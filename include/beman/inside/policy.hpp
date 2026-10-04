// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_POLICY_HPP
#define BEMAN_INSIDE_POLICY_HPP

#include <beman/inside/detail/assignment.hpp>
#include <beman/inside/detail/overflow.hpp>
#include <beman/inside/policy_flag.hpp>

#include <type_traits>     // std::is_constant_evaluated

//---------------------------------------------------------------------------
// policy — runtime policy carrier, plus `policy_ref` for per-operation dispatch.
//   policy<F, E>  — compile-time flags F plus an optional error_ref E holding a
//                   beman::inside::errc& (EBO so the no-error-code form is zero-sized).
//   policy_ref    — wraps an inside& with a policy<...> and a tuple of on_* actions;
//                   compound ops flow through it, routing each action to the right
//                   callback at the right pipeline stage.
//---------------------------------------------------------------------------
namespace beman::inside
{
  //---------------------------------------------------------------------------
  // policy — derives from E for EBO: throwing form (E == empty_ref) is zero-sized;
  // `policy(ec)` makes E == error_ref carrying a `beman::inside::errc&`. No virtuals,
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

    static constexpr bool range_check()
    {
      if (std::is_constant_evaluated()) return true;
      return is_checked(W) && not test(ignore_range);
    }

    static constexpr bool round_check()
    {
      if (std::is_constant_evaluated()) return true;
      return is_checked(W) && not test(snap);
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
          "inside: value out of range during constant evaluation "
          "(checked policy hit; choose clamp/wrap or widen the interval)">();
      if constexpr (std::is_same_v<E, detail::error_ref>)
        E::Code = E::Code != errc{} ? E::Code : code;
      else
        detail::raise(code);
    }
  };

  policy(errc&) -> policy<none, detail::error_ref>;

  //---------------------------------------------------------------------------
  // is_policy — true for policy<F,E> specializations, false otherwise.
  // Used to gate free-fn overloads so they don't accidentally bind P = action tag.
  //---------------------------------------------------------------------------
  namespace detail
  {
    template<typename T>             inline constexpr bool is_policy = false;
    template<policy_flag F, typename E> inline constexpr bool is_policy<policy<F,E>> = true;

    // Concept form of is_policy — pulls cvref off so the constraint matches
    // forwarded `policy<F,E>` references in template parameters.
    template<typename T>
    concept policy_like = is_policy<std::remove_cvref_t<T>>;

    // policy_flags_of<T> — the flag-set a one-shot `policy<F,E>` carries (else
    // `none`). Lets the value+policy constructor and policy_ref's conversion fold
    // the per-call flags into their `inside_assignable` check, so a one-shot
    // clamp/round actually relaxes the constraint it enables.
    template<typename T>                inline constexpr policy_flag policy_flags_of = none;
    template<policy_flag F, typename E> inline constexpr policy_flag policy_flags_of<policy<F,E>> = F;

    // True for policy specializations that carry a beman::inside::errc& reference.
    // Free-fn arithmetic uses this to decide whether to call policy.report on
    // failure (which sets ec) vs. returning a silent std::unexpected (no-arg form).
    template<typename T>             inline constexpr bool uses_error_ref = false;
    template<policy_flag F>          inline constexpr bool uses_error_ref<policy<F, error_ref>> = true;
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
  // report_or_unexpected — uniform "rational arithmetic failed" handler shared
  // by addition/multiplication/division/modulo. Three compile-time behaviors:
  // overflow_action<A> → fire it on a default Result; uses_error_ref<P> →
  // policy.report then std::unexpected{code}; plain throw-policy →
  // std::unexpected{code}.
  //---------------------------------------------------------------------------
  namespace detail
  {
  template <insidable Result, typename A, typename P>
  constexpr auto report_or_unexpected(A&& action, P&& policy, errc code,
                                      [[maybe_unused]] const char* what)
    -> std::conditional_t<overflow_action<A>, Result, std::expected<Result, errc>>
  {
    if constexpr (overflow_action<A>)
    {
      Result res;
      action.Fn(res, code);
      return res;
    }
    else
    {
      if constexpr (uses_error_ref<std::remove_cvref_t<P>>)
        policy.report(code);
      return std::unexpected{code};
    }
  }
  } // namespace detail

  //---------------------------------------------------------------------------
  // Named convenience policies — let user code skip `make_policy<F>()` entirely
  // for the per-call flag form on free arithmetic functions.
  //---------------------------------------------------------------------------
  // Each is named after its flag: snapped = snap (truncate toward zero),
  // rounded_* = round_*, clamped = clamp, wrapped = wrap.
  inline constexpr auto snapped           = make_policy<snap>();
  inline constexpr auto rounded_nearest   = make_policy<round_nearest>();
  inline constexpr auto rounded_floor     = make_policy<round_floor>();
  inline constexpr auto rounded_ceil      = make_policy<round_ceil>();
  inline constexpr auto rounded_half_even = make_policy<round_half_even>();
  inline constexpr auto clamped           = make_policy<clamp>();
  inline constexpr auto wrapped           = make_policy<wrap>();

  //---------------------------------------------------------------------------
  // policy_ref — variadic in actions (stores std::tuple<As...>). policy_ref
  // pre-picks the matching action per call path, so assignment/arithmetic keep
  // their single-A signatures. Payoff: the arithmetic and narrowing stages of a
  // compound op can each fire a different action (e.g. on_overflow + on_clamp).
  //---------------------------------------------------------------------------
  namespace detail
  {
  // Shared assignment dispatch: store `src` into `dst` under `policy` + the single
  // matching action from `actions` (at most one assignment-time tag is present).
  // Backs both policy_ref (dst = the wrapped inside) and policy_buffer (dst = a fresh
  // target), so the conversion/assignment logic lives in exactly one place.
  template <insidable Dst, numeric C, typename P, typename... As>
  constexpr Dst& dispatch_assign(Dst& dst, C const& src, P& policy, std::tuple<As...>& actions)
  {
    if constexpr (has_action<is_clamp_action, As...>)
      return assignment<Dst, C>::assign(dst, src, policy, pick_action_in<is_clamp_action>(actions));
    else if constexpr (has_action<is_wrap_action, As...>)
      return assignment<Dst, C>::assign(dst, src, policy, pick_action_in<is_wrap_action>(actions));
    else if constexpr (has_action<is_error_action, As...>)
      return assignment<Dst, C>::assign(dst, src, policy, pick_action_in<is_error_action>(actions));
    else
      return assignment<Dst, C>::assign(dst, src, policy);
  }

  // policy_buffer — the rvalue-receiver sibling of policy_ref. `with_snap()` etc.
  // on a *temporary* return this instead: it OWNS the inside by value (the temporary
  // is moved in), so the snapped value can be returned/stored without dangling —
  // `auto square(small n){ return (n*n).with_snap(); }` is safe. Value read-out only
  // (no operator=: assigning into a throwaway is meaningless). Same constrained
  // conversion as policy_ref, so it stays SFINAE-friendly.
  template<insidable B, typename P, typename... As>
  struct policy_buffer
  {
    B Owned;
    P Policy;
    [[no_unique_address]] std::tuple<As...> Actions;

    template <insidable Target>
      requires inside_assignable<Target, B, policy_of<Target> | policy_flags_of<P>>
    constexpr operator Target()
    {
      Target r;
      dispatch_assign(r, Owned, Policy, Actions);
      return r;
    }
  };

  template<insidable B, typename P, typename... As>
  struct policy_ref
  {
    private:
    // Conflict diagnostics: at most one assignment-time tag (clamp / wrap /
    // error), at most one of each kind, no clamp+wrap.
    static constexpr unsigned ClampCount    = count_action_matches<is_clamp_action,    As...>;
    static constexpr unsigned WrapCount     = count_action_matches<is_wrap_action,     As...>;
    static constexpr unsigned ErrorCount    = count_action_matches<is_error_action,    As...>;
    static constexpr unsigned OverflowCount = count_action_matches<is_overflow_action, As...>;

    static_assert(ClampCount + WrapCount + ErrorCount <= 1,
      "on_clamp / on_wrap / on_error are mutually exclusive in a single policy_ref");
    static_assert(ClampCount    <= 1, "duplicate on_clamp");
    static_assert(WrapCount     <= 1, "duplicate on_wrap");
    static_assert(ErrorCount    <= 1, "duplicate on_error");
    static_assert(OverflowCount <= 1, "duplicate on_overflow");

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
    // Generic assignment: store `src` into `dst` under this ref's Policy + picked
    // action. `operator=` uses it with dst = Ref (the inside this ref wraps); the
    // conversion operator below uses it with a fresh target, so a one-shot snap can
    // be read out as a value (`(a * b).with_snap()`), not only assigned.
    template <insidable Dst, numeric C>
    constexpr Dst& assign_into(Dst& dst, C const& src)
    { return dispatch_assign(dst, src, Policy, Actions); }

    template <numeric C>
    constexpr B& assign_with_picked(C const& other)
    { return assign_into(Ref, other); }

    public:
    template <numeric C>
    constexpr B& operator=(C const& other)
    { return assign_with_picked(other); }

    // Value read-out: a one-shot policy ref converts to any inside the assignment
    // could satisfy, applying the target's own policy (range) plus this ref's
    // carried flags (notch/rounding) via has_policy's merge. Makes
    // `Target t = (a * b).with_snap();` / `return (a * b).with_snap();` compile.
    // Constrained so the proxy stays SFINAE-friendly (no over-broad convertibility).
    template <insidable Target>
      requires inside_assignable<Target, B, policy_of<Target> | policy_flags_of<P>>
    constexpr operator Target()
    {
      Target r;
      assign_into(r, Ref);
      return r;
    }

    // expected<C> sink — unwrap once at the proxy boundary so callers can chain
    // checked arithmetic into `.with_clamp() = ...` without per-step `.value()`.
    template <numeric C>
    constexpr B& operator=(std::expected<C, errc> const& other)
    { return assign_with_picked(other.value()); }

    private:
    constexpr void report_zero(errc code, const char* what)
    {
      if constexpr (has_action<is_error_action, As...>)
        pick_action_in<is_error_action>(Actions).Fn(Ref, code, what);
      else if constexpr (!has_policy<B, P, ignore_zero>)
        Policy.report(code);
    }

    public:
    //-------------------------------------------------------------------------
    // insidable RHS overloads — route through the inside's arithmetic, then
    // assign via assign_with_picked so callbacks fire on the narrowing back to B.
    // An expected<inside> result carrying an error (rational-raw overflow,
    // division by zero) surfaces its errc through on_overflow if registered,
    // else report.
    //-------------------------------------------------------------------------
    private:
    template <typename R>
    constexpr B& finalise_arith(R&& result, [[maybe_unused]] const char* msg)
    {
      if constexpr (is_expected_v<R>)
      {
        if (!result.has_value()) [[unlikely]]
        {
          if (result.error() == errc::division_by_zero)
            report_zero(errc::division_by_zero, msg);       // on_error / ignore_zero, like rational /=
          else if constexpr (has_action<is_overflow_action, As...>)
            pick_action_in<is_overflow_action>(Actions).Fn(Ref, result.error());
          else
            Policy.report(result.error());
          return Ref;
        }
        return assign_with_picked(result.value());
      }
      else
        return assign_with_picked(std::forward<R>(result));
    }

    // Shared body for the rational `+=`/`-=`/`*=`/`/=` operators: lift Ref to
    // rational and route the checked result through `finalise_arith`.
    template <typename RatOp>
    constexpr B& rational_assign(rational const& rhs, RatOp rat_op, const char* msg)
    { return finalise_arith(rat_op(rational{Ref}, rhs), msg); }
    public:

    template <insidable C>
    constexpr B& operator+=(C const& rhs)
    { return finalise_arith(add(Ref, rhs, Policy), "policy_ref::operator+= overflow"); }

    template <insidable C>
    constexpr B& operator-=(C const& rhs)
    { return finalise_arith(sub(Ref, rhs, Policy), "policy_ref::operator-= overflow"); }

    template <insidable C>
    constexpr B& operator*=(C const& rhs)
    { return finalise_arith(mul(Ref, rhs, Policy), "policy_ref::operator*= overflow"); }

    template <insidable C>
    constexpr B& operator/=(C const& rhs)
    { return finalise_arith(div(Ref, rhs, Policy), "policy_ref::operator/= division/overflow"); }

    template <insidable C>
    constexpr B& operator%=(C const& rhs)
    { return finalise_arith(mod(Ref, rhs, Policy), "policy_ref::operator%= division/overflow"); }

    //-------------------------------------------------------------------------
    // rational RHS overloads — the only non-inside operand a compound assign
    // accepts. Lets callers write `b += rational{1,3}`. Raw int/float/double are
    // ill-formed: give the scalar a grid (`1_ins` / `just<1>` / `inside<{lo,hi}>{n}`).
    // Lift Ref to rational, checked op, finalise_arith.
    //-------------------------------------------------------------------------
    template <std::same_as<rational> C>
    constexpr B& operator+=(C const& rhs)
    {
      return rational_assign(rhs, [](rational a, rational b){ return a + b; }, "policy_ref::operator+= overflow");
    }

    template <std::same_as<rational> C>
    constexpr B& operator-=(C const& rhs)
    {
      return rational_assign(rhs, [](rational a, rational b){ return a - b; }, "policy_ref::operator-= overflow");
    }

    template <std::same_as<rational> C>
    constexpr B& operator*=(C const& rhs)
    {
      return rational_assign(rhs, [](rational a, rational b){ return a * b; }, "policy_ref::operator*= overflow");
    }

    template <std::same_as<rational> C>
    constexpr B& operator/=(C const& rhs)
    {
      if (is_canonical_zero(rhs))
      {
        report_zero(errc::division_by_zero, "policy_ref::operator/= division by zero");
        return Ref;
      }
      return rational_assign(rhs, [](rational a, rational b){ return a / b; }, "policy_ref::operator/= division/overflow");
    }
  };
  } // namespace detail

} // namespace beman::inside

#endif // BEMAN_INSIDE_POLICY_HPP
