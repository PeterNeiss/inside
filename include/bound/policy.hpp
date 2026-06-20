//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDpolicyHPP
#define BNDpolicyHPP

#include "bound/detail/assignment.hpp"
#include "bound/detail/overflow.hpp"
#include "bound/policy_flag.hpp"

#include <type_traits>     // std::is_constant_evaluated

//---------------------------------------------------------------------------
// policy — runtime policy carrier, plus `policy_ref` for per-operation dispatch.
//   policy<F, E>  — compile-time flags F plus an optional error_ref E holding a
//                   bnd::errc& (EBO so the no-error-code form is zero-sized).
//   policy_ref    — wraps a bound& with a policy<...> and a tuple of on_* actions;
//                   compound ops flow through it, routing each action to the right
//                   callback at the right pipeline stage.
//---------------------------------------------------------------------------
namespace bnd
{
  //---------------------------------------------------------------------------
  // policy — derives from E for EBO: throwing form (E == empty_ref) is zero-sized;
  // `policy(ec)` makes E == error_ref carrying a `bnd::errc&`. No virtuals,
  // resolved at compile time. (The error-code channel reports `errc` directly —
  // there is no <system_error> dependency.)
  //---------------------------------------------------------------------------
  namespace detail
  {
    struct empty_ref{ };
    struct error_ref
    {
      constexpr error_ref(errc& ec):Code{ec} {}
      errc& Code;
    };
  }

  template<policy_flag W = none, typename E = detail::empty_ref>
  struct policy: E
  {
    constexpr policy() = default;
    constexpr policy(errc& ec) requires std::same_as<E, detail::error_ref>
    :E(ec) { }

    static constexpr bool test(policy_flag w)
    { return has_flag(W, w); }

    static constexpr bool domain_check()
    {
      if (std::is_constant_evaluated()) return true;
      return test(checked) && not test(ignore_domain);
    }

    static constexpr bool round_check()
    {
      if (std::is_constant_evaluated()) return true;
      return test(checked) && not test(snap);
    }

    // Cheap default report: no message construction. error_ref mode records the
    // code (sticky: keeps the first error); throw mode funnels through the
    // installed handler via an outlined cold helper. The constant-evaluation
    // guard names a fixed-string diagnostic for a clearer compile-time message
    // than "non-constexpr function called".
    constexpr void report(errc code)
    {
      if (std::is_constant_evaluated())
        detail::constexpr_error<
          "bound: value out of range during constant evaluation "
          "(checked policy hit; choose clamp/wrap/sentinel or widen the interval)">();
      if constexpr (std::is_same_v<E, detail::error_ref>)
        E::Code = E::Code != errc{} ? E::Code : code;
      else
        detail::raise(code);
    }
  };

  policy(errc&) -> policy<none, detail::error_ref>;

  //---------------------------------------------------------------------------
  // IsPolicy — true for policy<F,E> specializations, false otherwise.
  // Used to gate free-fn overloads so they don't accidentally bind P = action tag.
  //---------------------------------------------------------------------------
  namespace detail
  {
    template<typename T>             inline constexpr bool IsPolicy = false;
    template<policy_flag F, typename E> inline constexpr bool IsPolicy<policy<F,E>> = true;

    // Concept form of IsPolicy — pulls cvref off so the constraint matches
    // forwarded `policy<F,E>` references in template parameters.
    template<typename T>
    concept policy_like = IsPolicy<std::remove_cvref_t<T>>;

    // True for policy specializations that carry a bnd::errc& reference.
    // Free-fn arithmetic uses this to decide whether to call policy.report on
    // failure (which sets ec) vs. returning silent nullopt (no-arg form).
    template<typename T>             inline constexpr bool UsesErrorRef = false;
    template<policy_flag F>          inline constexpr bool UsesErrorRef<policy<F, error_ref>> = true;
  }

  //---------------------------------------------------------------------------
  // make_policy
  //---------------------------------------------------------------------------
  template<policy_flag F = none>
  [[nodiscard]] constexpr auto make_policy()
  { return policy<F,detail::empty_ref>{}; }

  template<policy_flag F = none>
  [[nodiscard]] constexpr auto make_policy(errc& ec)
  { return policy<F,detail::error_ref>{ec}; }

  //---------------------------------------------------------------------------
  // report_or_nullopt — uniform "rational arithmetic failed" handler shared by
  // addition/multiplication/division/modulo. Three compile-time behaviors:
  // overflow_action<A> → fire it on a default Result; UsesErrorRef<P> →
  // policy.report then nullopt; plain throw-policy → nullopt.
  //---------------------------------------------------------------------------
  namespace detail
  {
  template <boundable Result, typename A, typename P>
  constexpr auto report_or_nullopt(A&& action, P&& policy, errc code,
                                   [[maybe_unused]] const char* what)
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
        policy.report(code);
      return slim::nullopt;
    }
  }
  } // namespace detail

  //---------------------------------------------------------------------------
  // Named convenience policies — let user code skip `make_policy<F>()` entirely
  // for the per-call flag form on free arithmetic functions.
  //---------------------------------------------------------------------------
  inline constexpr auto truncated        = make_policy<snap>();
  inline constexpr auto round_to_nearest = make_policy<round_nearest>();
  inline constexpr auto clamped          = make_policy<clamp>();
  inline constexpr auto wrapped          = make_policy<wrap>();

  //---------------------------------------------------------------------------
  // policy_ref — variadic in actions (stores std::tuple<As...>). policy_ref
  // pre-picks the matching action per call path, so assignment/arithmetic keep
  // their single-A signatures. Payoff: the imax-probe and narrowing stages of a
  // compound op can each fire a different action (e.g. on_overflow + on_clamp).
  //---------------------------------------------------------------------------
  namespace detail
  {
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

    // optional<C> sink — unwrap once at the proxy boundary so callers can chain
    // checked arithmetic into `.with_clamp() = ...` without per-step `.value()`.
    template <numeric C>
    constexpr B& operator=(slim::optional<C> const& other)
    { return assign_with_picked(other.value()); }

    private:
    constexpr void report_zero(errc code, const char* what)
    {
      if constexpr (has_action<IsErrorActionPred, As...>)
        pick_action_in<IsErrorActionPred>(Actions).fn(Ref, code, what);
      else if constexpr (!HasPolicy<B, P, ignore_zero>)
        Policy.report(code);
    }

    public:
    //-------------------------------------------------------------------------
    // boundable RHS overloads — route through the bound's arithmetic, then
    // assign via assign_with_picked so callbacks fire on the narrowing back to B.
    // An optional<bound> result (rational-raw overflow) surfaces errc::overflow
    // through on_overflow if registered, else report.
    //-------------------------------------------------------------------------
    private:
    template <typename R>
    constexpr B& finalise_arith(R&& result, [[maybe_unused]] const char* msg)
    {
      if constexpr (requires { typename plain<R>::value_type; })
      {
        if (!result.has_value()) [[unlikely]]
        {
          if constexpr (has_action<IsOverflowActionPred, As...>)
            pick_action_in<IsOverflowActionPred>(Actions).fn(Ref, errc::overflow);
          else
            Policy.report(errc::overflow);
          return Ref;
        }
        return assign_with_picked(result.value());
      }
      else
        return assign_with_picked(std::forward<R>(result));
    }

    // Shared body for the fractional `+=`/`-=`/`*=`/`/=` operators: the rational
    // RHS lifts Ref to rational and routes the checked result through
    // `finalise_arith`; any other fractional RHS lifts both sides to double.
    template <fractional C, typename RatOp, typename DblOp>
    constexpr B& fractional_assign(C const& rhs, RatOp rat_op, DblOp dbl_op,
                                   const char* msg)
    {
      if constexpr (std::same_as<C, rational>)
        return finalise_arith(rat_op(rational{Ref}, rhs), msg);
      else
        return assign_with_picked(dbl_op(static_cast<double>(Ref),
                                         static_cast<double>(rhs)));
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
    // rational RHS overloads — the only non-bound operand a compound assign
    // accepts. Lets callers write `b += rational{1,3}`. Raw int/float/double are
    // ill-formed: give the scalar a grid (`1_b` / `just<1>` / `bound<{lo,hi}>{n}`).
    // Lift Ref to rational, checked op, finalise_arith.
    //-------------------------------------------------------------------------
    template <std::same_as<rational> C>
    constexpr B& operator+=(C const& rhs)
    {
      return fractional_assign(rhs, [](rational a, rational b){ return a + b; },
        [](double a, double b){ return a + b; }, "policy_ref::operator+= overflow");
    }

    template <std::same_as<rational> C>
    constexpr B& operator-=(C const& rhs)
    {
      return fractional_assign(rhs, [](rational a, rational b){ return a - b; },
        [](double a, double b){ return a - b; }, "policy_ref::operator-= overflow");
    }

    template <std::same_as<rational> C>
    constexpr B& operator*=(C const& rhs)
    {
      return fractional_assign(rhs, [](rational a, rational b){ return a * b; },
        [](double a, double b){ return a * b; }, "policy_ref::operator*= overflow");
    }

    template <std::same_as<rational> C>
    constexpr B& operator/=(C const& rhs)
    {
      if (is_canonical_zero(rhs))
      {
        report_zero(errc::division_by_zero, "policy_ref::operator/= division by zero");
        return Ref;
      }
      return fractional_assign(rhs, [](rational a, rational b){ return a / b; },
        [](double a, double b){ return a / b; }, "policy_ref::operator/= division/overflow");
    }
  };
  } // namespace detail

} // namespace bnd

#endif // BNDpolicyHPP
