//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDpolicyHPP
#define BNDpolicyHPP

#include "bound/assignment.hpp"
#include "bound/detail/overflow.hpp"
#include "bound/policy_flag.hpp"

#include <system_error>

//---------------------------------------------------------------------------
// policy — runtime policy carrier, plus `policy_ref` for per-operation
// dispatch.
//
//   policy<F, E>   — compile-time flags F (see policy_flag.hpp) plus an
//                    optional `error_ref` E that holds an `std::error_code&`.
//                    Empty Base Optimization (`empty_ref`) is used so the
//                    no-error-code form is zero-sized.
//   policy_ref     — wraps a `bound&` together with a `policy<...>` and a
//                    tuple of `on_*` actions. Compound `+=`, `-=`, `*=`,
//                    `/=`, `%=` flow through `policy_ref`, which routes the
//                    overflow/clamp/wrap/error/sentinel action to the right
//                    callback at the right point in the pipeline.
//---------------------------------------------------------------------------
namespace bnd
{
  //---------------------------------------------------------------------------
  // policy
  //---------------------------------------------------------------------------
  // `policy<F, E>` derives from `E` to enable EBO: when the user picks the
  // throwing-on-error form, `E == empty_ref` is an empty base and `policy`
  // carries zero bytes per instance. When the user calls `policy(ec)`, `E`
  // becomes `error_ref` and the same struct carries an `std::error_code&`
  // — no virtuals, no dispatch tables, choice resolved at compile time.
  struct empty_ref{ };
  struct error_ref
  {
    constexpr error_ref(std::error_code& ec):Code{ec} {}
    std::error_code& Code;
  };

  template<policy_flag W = none, typename E = empty_ref>
  struct policy: E
  {
    constexpr policy() = default;
    constexpr policy(std::error_code& ec) requires std::same_as<E, error_ref>
    :E(ec) { }

    static constexpr bool test(policy_flag w)
    { return (W & w) == w; }

    static constexpr bool domain_check()
    {
      if consteval { return true; }
      return test(checked) && not test(ignore_domain);
    }

    static constexpr bool round_check()
    {
      if consteval { return true; }
      return test(checked) && not test(ignore_round);
    }

    constexpr void report(errc code, std::string what)
    {
      // Constant-evaluation guard: neither `make_error_code` nor the
      // `std::system_error` constructor are constexpr, so a `checked`
      // policy hitting this path at compile time would otherwise produce
      // an opaque "non-constexpr function called" diagnostic. The string-
      // literal throw aborts constant evaluation with a clearer pointer.
      if consteval
      {
        throw "bound: value out of range during constant evaluation "
              "(checked policy hit; choose clamp/wrap/sentinel or widen the interval)";
      }
      if constexpr (std::is_same_v<E, error_ref>)
        E::Code = E::Code ? E::Code : make_error_code(code);
      else
      {
#ifdef BOUND_HAS_STACKTRACE
        what += ": \n" + std::to_string(std::stacktrace::current());
#endif
        throw std::system_error(make_error_code(code), what);
      }
    }
  };

  policy(std::error_code&) -> policy<none, error_ref>;

  //---------------------------------------------------------------------------
  // IsPolicy — true for policy<F,E> specializations, false otherwise.
  // Used to gate free-fn overloads so they don't accidentally bind P = action tag.
  //---------------------------------------------------------------------------
  template<typename T>             inline constexpr bool IsPolicy = false;
  template<policy_flag F, typename E> inline constexpr bool IsPolicy<policy<F,E>> = true;

  // Concept form of IsPolicy — pulls cvref off so the constraint matches
  // forwarded `policy<F,E>` references in template parameters.
  template<typename T>
  concept policy_like = IsPolicy<std::remove_cvref_t<T>>;

  // True for policy specializations that carry an std::error_code& reference.
  // Free-fn arithmetic uses this to decide whether to call policy.report on
  // failure (which sets ec) vs. returning silent nullopt (no-arg form).
  template<typename T>             inline constexpr bool UsesErrorRef = false;
  template<policy_flag F>          inline constexpr bool UsesErrorRef<policy<F, error_ref>> = true;

  //---------------------------------------------------------------------------
  // make_policy
  //---------------------------------------------------------------------------
  template<policy_flag F = none>
  [[nodiscard]] constexpr auto make_policy()
  { return policy<F,empty_ref>{}; }

  template<policy_flag F = none>
  [[nodiscard]] constexpr auto make_policy(std::error_code& ec)
  { return policy<F,error_ref>{ec}; }

  //---------------------------------------------------------------------------
  // report_or_nullopt — uniform "rational arithmetic failed" handler shared by
  // addition.hpp / multiplication.hpp / division.hpp / modulo. Three behaviors,
  // selected at compile time by the action / policy types:
  //   - `overflow_action<A>` present  → default-init a Result, fire the action
  //                                     on it (may overwrite), return Result.
  //   - `UsesErrorRef<P>` (error_code mode) → policy.report(...), return nullopt.
  //   - plain throw-policy            → return nullopt (caller already threw or
  //                                     the policy is configured to be silent).
  //---------------------------------------------------------------------------
  template <boundable Result, typename A, typename P>
  constexpr auto report_or_nullopt(A&& action, P&& policy, errc code, const char* what)
    -> std::conditional_t<overflow_action<A>, Result, slim::optional<Result>>
  {
    if constexpr (overflow_action<A>)
    {
      Result res;
      action.fn(res, code);
      return res;
    }
    else
    {
      if constexpr (UsesErrorRef<std::remove_cvref_t<P>>)
        policy.report(code, what);
      return slim::nullopt;
    }
  }

  //---------------------------------------------------------------------------
  // Named convenience policies — let user code skip `make_policy<F>()` entirely
  // for the per-call flag form on free arithmetic functions.
  //---------------------------------------------------------------------------
  inline constexpr auto truncated        = make_policy<ignore_round>();
  inline constexpr auto round_to_nearest = make_policy<round_nearest>();
  inline constexpr auto clamped          = make_policy<clamp>();
  inline constexpr auto wrapped          = make_policy<wrap>();

  //---------------------------------------------------------------------------
  // policy_ref
  //
  // Variadic in actions: stores a `std::tuple<As...>`. Single-action callers
  // (the bound::on_* methods) instantiate with a 1-element pack and aggregate-
  // init the tuple from the tag. The bound::with(...) entry point and the
  // free-fn pack overloads instantiate with N-element packs.
  //
  // assignment::assign and addition/multiplication/etc. keep their single-A
  // signatures unchanged; policy_ref pre-picks the appropriate action for each
  // call path. The compound-op payoff: the imax-probe stage and the narrowing
  // stage can each fire a different action (e.g. on_overflow + on_clamp).
  //---------------------------------------------------------------------------
  template<boundable B, typename P, typename... As>
  struct policy_ref
  {
    private:
    // Conflict diagnostics: at most one assignment-time tag (clamp / wrap /
    // sentinel / error), at most one of each kind, no clamp+wrap.
    static constexpr unsigned _clamp_count    = count_action_matches<IsClampActionPred,    As...>;
    static constexpr unsigned _wrap_count     = count_action_matches<IsWrapActionPred,     As...>;
    static constexpr unsigned _sentinel_count = count_action_matches<IsSentinelActionPred, As...>;
    static constexpr unsigned _error_count    = count_action_matches<IsErrorActionPred,    As...>;
    static constexpr unsigned _overflow_count = count_action_matches<IsOverflowActionPred, As...>;

    static_assert(_clamp_count + _wrap_count + _sentinel_count + _error_count <= 1,
      "on_clamp / on_wrap / on_sentinel / on_error are mutually exclusive in a single policy_ref");
    static_assert(_clamp_count    <= 1, "duplicate on_clamp");
    static_assert(_wrap_count     <= 1, "duplicate on_wrap");
    static_assert(_sentinel_count <= 1, "duplicate on_sentinel");
    static_assert(_error_count    <= 1, "duplicate on_error");
    static_assert(_overflow_count <= 1, "duplicate on_overflow");

    public:
    B& Ref;
    P Policy;
    // `[[no_unique_address]]` is load-bearing: each captureless action lambda
    // is an empty type, and without this attribute the tuple would pad each
    // one out to a byte. With it, `policy_ref<B, P>` carrying no actions has
    // the same size as `policy_ref<B, P, no_action>`.
    [[no_unique_address]] std::tuple<As...> Actions;

    private:
    // Pre-pick the assignment-time action that matches the policy_ref's pack,
    // and forward to the single-action assignment::assign. At most one of the
    // four assignment-time tags is in the pack (enforced by static_assert), so
    // exactly one branch fires; the rest fall through to no-action.
    template <numeric C>
    constexpr B& assign_with_picked(C const& other)
    {
      if constexpr (has_action<IsClampActionPred, As...>)
        return assignment<B, C>::assign(Ref, other, Policy,
          pick_action_in<IsClampActionPred>(Actions));
      else if constexpr (has_action<IsWrapActionPred, As...>)
        return assignment<B, C>::assign(Ref, other, Policy,
          pick_action_in<IsWrapActionPred>(Actions));
      else if constexpr (has_action<IsSentinelActionPred, As...>)
        return assignment<B, C>::assign(Ref, other, Policy,
          pick_action_in<IsSentinelActionPred>(Actions));
      else if constexpr (has_action<IsErrorActionPred, As...>)
        return assignment<B, C>::assign(Ref, other, Policy,
          pick_action_in<IsErrorActionPred>(Actions));
      else
        return assignment<B, C>::assign(Ref, other, Policy);
    }

    public:
    template <numeric C>
    constexpr B& operator=(C const& other)
    { return assign_with_picked(other); }

    private:
    constexpr void report_zero(errc code, const char* what)
    {
      if constexpr (has_action<IsErrorActionPred, As...>)
        pick_action_in<IsErrorActionPred>(Actions).fn(Ref, code, std::string_view(what));
      else if constexpr (!HasPolicy<B, P, ignore_zero>)
        Policy.report(code, what);
    }

    public:
    template <std::integral C>
    constexpr B& operator+=(C rhs)
    {
      imax l = to_value(Ref), r = static_cast<imax>(rhs), result;
      if constexpr (has_action<IsOverflowActionPred, As...>
                 || has_action<IsErrorActionPred,    As...>)
      {
        if (add_overflow(l, r, &result))
        {
          if constexpr (has_action<IsOverflowActionPred, As...>)
            pick_action_in<IsOverflowActionPred>(Actions).fn(Ref, errc::overflow);
          else
            pick_action_in<IsErrorActionPred>(Actions)
              .fn(Ref, errc::overflow, std::string_view("operator+= overflow"));
          return Ref;
        }
        return assign_with_picked(result);
      }
      else
        return assign_with_picked(static_cast<imax>(l + r));
    }

    template <std::integral C>
    constexpr B& operator-=(C rhs)
    {
      imax l = to_value(Ref), r = static_cast<imax>(rhs), result;
      if constexpr (has_action<IsOverflowActionPred, As...>
                 || has_action<IsErrorActionPred,    As...>)
      {
        if (sub_overflow(l, r, &result))
        {
          if constexpr (has_action<IsOverflowActionPred, As...>)
            pick_action_in<IsOverflowActionPred>(Actions).fn(Ref, errc::overflow);
          else
            pick_action_in<IsErrorActionPred>(Actions)
              .fn(Ref, errc::overflow, std::string_view("operator-= overflow"));
          return Ref;
        }
        return assign_with_picked(result);
      }
      else
        return assign_with_picked(static_cast<imax>(l - r));
    }

    template <std::integral C>
    constexpr B& operator*=(C rhs)
    {
      imax l = to_value(Ref), r = static_cast<imax>(rhs), result;
      if constexpr (has_action<IsOverflowActionPred, As...>
                 || has_action<IsErrorActionPred,    As...>)
      {
        if (mul_overflow(l, r, &result))
        {
          if constexpr (has_action<IsOverflowActionPred, As...>)
            pick_action_in<IsOverflowActionPred>(Actions).fn(Ref, errc::overflow);
          else
            pick_action_in<IsErrorActionPred>(Actions)
              .fn(Ref, errc::overflow, std::string_view("operator*= overflow"));
          return Ref;
        }
        return assign_with_picked(result);
      }
      else
        return assign_with_picked(static_cast<imax>(l * r));
    }

    template <std::integral C>
    constexpr B& operator/=(C rhs)
    {
      if (rhs == 0)
      {
        report_zero(errc::division_by_zero, "operator/= division by zero");
        return Ref;
      }
      return assign_with_picked(static_cast<imax>(to_value(Ref) / static_cast<imax>(rhs)));
    }

    template <std::integral C>
    constexpr B& operator%=(C rhs)
    {
      if (rhs == 0)
      {
        report_zero(errc::division_by_zero, "operator%= division by zero");
        return Ref;
      }
      return assign_with_picked(static_cast<imax>(to_value(Ref) % static_cast<imax>(rhs)));
    }

    //-------------------------------------------------------------------------
    // boundable RHS overloads — route through the bound's arithmetic and
    // assign the result via `assign_with_picked` so the user's callbacks fire
    // on the final narrowing back to B. Handles the case `+`/`*` returns
    // `slim::optional<bound>` (rational-raw overflow) by surfacing
    // `errc::overflow` through `on_overflow` if registered, else `report`.
    //-------------------------------------------------------------------------
    private:
    template <typename R>
    constexpr B& finalise_arith(R&& result, const char* msg)
    {
      if constexpr (requires { typename plain<R>::value_type; })
      {
        if (!result.has_value())
        {
          if constexpr (has_action<IsOverflowActionPred, As...>)
            pick_action_in<IsOverflowActionPred>(Actions).fn(Ref, errc::overflow);
          else
            Policy.report(errc::overflow, msg);
          return Ref;
        }
        return assign_with_picked(result.value());
      }
      else
        return assign_with_picked(std::forward<R>(result));
    }
    public:

    template <boundable C>
    constexpr B& operator+=(C const& rhs)
    { return finalise_arith(Ref + rhs, "policy_ref::operator+= overflow"); }

    template <boundable C>
    constexpr B& operator-=(C const& rhs)
    { return finalise_arith(Ref - rhs, "policy_ref::operator-= overflow"); }

    template <boundable C>
    constexpr B& operator*=(C const& rhs)
    { return finalise_arith(Ref * rhs, "policy_ref::operator*= overflow"); }

    template <boundable C>
    constexpr B& operator/=(C const& rhs)
    { return finalise_arith(Ref / rhs, "policy_ref::operator/= division/overflow"); }

    template <boundable C>
    constexpr B& operator%=(C const& rhs)
    { return finalise_arith(mod(Ref, rhs, Policy), "policy_ref::operator%= division/overflow"); }

    //-------------------------------------------------------------------------
    // real RHS overloads — rational, float, double. The `real` concept
    // excludes both `boundable` and `std::integral`, so these don't overlap
    // with the existing overloads above. Lets callers write `b += 1.5` or
    // `b += rational{1, 3}` without lifting the RHS into a bound first.
    //
    // rational path: lift Ref to rational, run the checked op, route the
    // optional<rational> result through `finalise_arith` so any overflow
    // surfaces through `on_overflow` / Policy.report.
    //
    // floating-point path: lift both sides to double and assign — the bound
    // must carry a round policy (operator double() requires it).
    //-------------------------------------------------------------------------
    template <real C>
    constexpr B& operator+=(C const& rhs)
    {
      if constexpr (std::same_as<C, rational>)
        return finalise_arith(rational{Ref} + rhs, "policy_ref::operator+= overflow");
      else
        return assign_with_picked(static_cast<double>(Ref) + static_cast<double>(rhs));
    }

    template <real C>
    constexpr B& operator-=(C const& rhs)
    {
      if constexpr (std::same_as<C, rational>)
        return finalise_arith(rational{Ref} - rhs, "policy_ref::operator-= overflow");
      else
        return assign_with_picked(static_cast<double>(Ref) - static_cast<double>(rhs));
    }

    template <real C>
    constexpr B& operator*=(C const& rhs)
    {
      if constexpr (std::same_as<C, rational>)
        return finalise_arith(rational{Ref} * rhs, "policy_ref::operator*= overflow");
      else
        return assign_with_picked(static_cast<double>(Ref) * static_cast<double>(rhs));
    }

    template <real C>
    constexpr B& operator/=(C const& rhs)
    {
      // Canonical-zero check: rational stores zero as {0, 1}, so Numerator == 0
      // catches every canonical zero (rational{} is the sentinel, not a zero).
      bool is_zero;
      if constexpr (std::same_as<C, rational>) is_zero = (rhs.Numerator == 0);
      else                                     is_zero = (rhs == C{0});
      if (is_zero)
      {
        report_zero(errc::division_by_zero, "policy_ref::operator/= division by zero");
        return Ref;
      }

      if constexpr (std::same_as<C, rational>)
        return finalise_arith(rational{Ref} / rhs, "policy_ref::operator/= division/overflow");
      else
        return assign_with_picked(static_cast<double>(Ref) / static_cast<double>(rhs));
    }
  };

} // namespace bnd

#endif // BNDpolicyHPP
