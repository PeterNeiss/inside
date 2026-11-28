// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INSIDE_INSIDE_DETAIL_POLICY_REF_HPP
#define INSIDE_INSIDE_DETAIL_POLICY_REF_HPP

#include <inside/inside/detail/config.hpp>

#include <inside/inside/detail/assignment.hpp>
#include <inside/inside/detail/call_policy.hpp>
#include <inside/inside/detail/lift.hpp>

#include <expected>
#include <tuple>
#include <utility>

// policy_ref and policy_buffer, the proxies that b.with<F>(), b.on_error(ec) and on_*() return
// (inside/detail/core.hpp): an inside (by reference, or owned for a temporary) with a call_policy<…>
// and a tuple of on_* actions.
// Stores and compound ops go through them, which routes each action to the right callback at the right stage.
namespace ins
{
  // policy_ref is variadic in actions
  // (it stores std::tuple<As...>: an on_* hook gives one, a policy override none),
  // and picks the matching action per call path, so assignment keeps its single-A signature.
  namespace detail
  {
    // The one assignment-time action among actions (at most one such tag is present), or no_action.
    template <typename... As>
    constexpr decltype(auto) assignment_action(std::tuple<As...>& actions)
    {
      if constexpr (has_action<is_clamp_action, As...>)
        return pick_action_in<is_clamp_action>(actions);
      else if constexpr (has_action<is_wrap_action, As...>)
        return pick_action_in<is_wrap_action>(actions);
      else if constexpr (has_action<is_error_action, As...>)
        return pick_action_in<is_error_action>(actions);
      else
        return no_action{};
    }

    // The shared assignment dispatch: src stored into dst under the policy and that action.
    // It backs policy_ref (dst is the wrapped inside) and policy_buffer (dst is a fresh target),
    // so the store logic lives in one place.
    template <insidable Dst, numeric C, typename P, typename... As>
    constexpr Dst& dispatch_assign(Dst& dst, const C& src, P& policy, std::tuple<As...>& actions)
    {
      return assignment<Dst, C>::assign(dst, src, policy, assignment_action(actions));
    }

    // The same into a fresh Target, the value read-out of policy_ref and policy_buffer.
    // A failed store (error&, on_error) leaves it at its default value (0, or Lower), not garbage.
    template <insidable Target, numeric C, typename P, typename... As>
    constexpr Target dispatch_new(const C& src, P& policy, std::tuple<As...>& actions)
    {
      Target r{};
      dispatch_assign(r, src, policy, actions);
      return r;
    }

    // policy_buffer, the sibling of policy_ref that with<F>() returns on a temporary:
    // it owns the inside by value (the temporary moves in), so the rounded value can be returned or stored
    // without dangling, and `auto square(small n) { return (n*n).with<round_toward_zero>(); }` is safe.
    // Value read-out only (no operator=: assigning into a throwaway means nothing),
    // with policy_ref's constrained conversion, so it stays SFINAE-friendly.
    // A proxy store of a C into B is proven by the grids under the proxy's flags, or has a channel:
    // an error& policy or an on_error hook.
    template <insidable B, typename C, typename P, typename... As>
    inline constexpr bool proxy_store_ok =
      store_proven<B, C, policy_of<B> | policy_of<P>> || reports_into_errc<P> || has_action<is_error_action, As...>;

    // A builtin integer that stands for a number: no bool, no character type.
    // An index converts to one (core.hpp), and a compound assignment takes one.
    template <class T>
    concept index_integer =
      std::integral<T> && !std::same_as<T, bool> && !std::same_as<T, char> && !std::same_as<T, wchar_t>
      && !std::same_as<T, char8_t> && !std::same_as<T, char16_t> && !std::same_as<T, char32_t>;

    // A builtin integer on the right of x += n, x -= n, x *= n:
    // an inside on the whole range of its type (an int is an id<key{grid{INT_MIN, INT_MAX}}>),
    // so the result is stored back into x by the rules for any inside.
    // (The binary x + n has no such reading: its type would be that range.)
    template <index_integer A>
    using lifted_t = id<key{grid{interval_of<A>, grid_number{1}}}>;
    template <class A>
    concept step_integer = index_integer<A> && integer_value_storage<lifted_t<A>>;
    template <step_integer A>
    [[nodiscard]] constexpr lifted_t<A> lift_integer(A a) noexcept
    {
      return lifted_t<A>::from_raw(static_cast<raw_t<lifted_t<A>>>(a), unsafe);
    }

#define INSIDE_WHY_proxy_unproven                                                                             \
  "inside: the value may fall outside the target grid, or off its notch, at run time - add clamp, wrap or a " \
  "rounding mode to the policy, report with on_error(ec) or on_error(f), or opt out with unsafe"

    // A plain number a proxy's grids cannot prove (a double, a rational, an int off the lattice):
    // a constant, stored once while compiling under the proxy's flags F into B's grid,
    // so a store that would fail is a compile error;
    // at run time it is stored again with the proxy's hooks, which fire (an on_clamp hears its overshoot).
    // A run-time value does not compile: it needs a report channel (on_error(ec) or an on_error hook).
    template <insidable B, policy_flag F>
    struct constant_value
    {
      bool        Floating = false;
      long double Float    = 0;  // a float, double or long double, exactly
      rational    Exact{};       // an integer or a rational

      template <std::floating_point X>
        requires inside_assignable<id<key{grid_of<B>, F}>, X, F>
      consteval constant_value(X x) : Floating{true}, Float{x}
      {
        trial(x);
      }
      template <class X>
        requires(std::integral<X> || std::same_as<X, rational>) && inside_assignable<id<key{grid_of<B>, F}>, X, F>
      consteval constant_value(const X& x) : Exact{x}
      {
        trial(x);
      }

      template <class Store>
      constexpr decltype(auto) visit(Store store) const
      {
        return Floating ? store(Float) : store(Exact);
      }

    private:
      template <class X>
      static consteval void trial(const X& x)
      {
        id<key{grid_of<B>, F}> twin;
        assignment<id<key{grid_of<B>, F}>, X>::assign(twin, x, make_policy<F>());
      }
    };

    template <insidable B, typename P, typename... As>
    struct policy_buffer
    {
      B                                       Owned;
      P                                       Policy;
      [[no_unique_address]] std::tuple<As...> Actions;

      template <insidable Target>
        requires inside_assignable<Target, B, policy_of<Target> | policy_of<P>> && proxy_store_ok<Target, B, P, As...>
      constexpr operator Target()
      {
        return dispatch_new<Target>(Owned, Policy, Actions);
      }
    };

    template <insidable B, typename P, typename... As>
    struct policy_ref
    {
      // (At most one action: each on_* hook makes a ref with its own.)
      B& Ref;
      P  Policy;
      // [[no_unique_address]] matters: each captureless action lambda is an empty type,
      // which the tuple would pad to a byte without it.
      // With it, a policy_ref<B, P> with no actions is as large as policy_ref<B, P, no_action>.
      [[no_unique_address]] std::tuple<As...> Actions;

      // Stores go through dispatch_assign under this ref's Policy and actions:
      // into Ref for operator=, into a fresh target for the conversion below,
      // so a one-shot rounding can be read out as a value ((a * b).with<round_toward_zero>()).
      template <numeric C>
        requires proxy_store_ok<B, C, P, As...>
      constexpr B& operator=(const C& other)
      {
        return dispatch_assign(Ref, other, Policy, Actions);
      }
      // A constant the grids cannot prove: checked while compiling (above).
      constexpr B& operator=(const constant_value<B, policy_of<B> | plain_t<P>::Flags>& c)
      {
        return c.visit([&](const auto& v) -> B& { return dispatch_assign(Ref, v, Policy, Actions); });
      }
      template <insidable C>
        requires(!proxy_store_ok<B, C, P, As...>)
      B& operator=(const C&) = delete (INSIDE_WHY_proxy_unproven);

      // The value read-out: a one-shot policy ref converts to any inside the assignment could satisfy,
      // under the target's own policy (range) plus this ref's flags (notch, rounding), merged by uses_flag.
      // So `Target t = (a * b).with<round_toward_zero>();` and `return (a * b).with<round_toward_zero>();` compile.
      // Constrained, so the proxy stays SFINAE-friendly (no over-broad convertibility).
      template <insidable Target>
        requires inside_assignable<Target, B, policy_of<Target> | policy_of<P>> && proxy_store_ok<Target, B, P, As...>
      constexpr operator Target()
      {
        return dispatch_new<Target>(Ref, Policy, Actions);
      }

      // A std::expected is never stored implicitly, through a proxy either:
      // check it and store *e, or use B::try_make(e).
      template <numeric C>
      B& operator=(const std::expected<C, error>&) = delete ("inside: a std::expected is not stored implicitly - "
                                                             "check it and store *e, or use T::try_make(e), "
                                                             "which passes its error on");

    private:
      // The value an op's result holds (an expected's value type) assigns back.
      template <typename R>
      static constexpr bool stores_back = inside_assignable<B, detail::unwrap_t<R>, policy_of<B> | policy_of<P>>
                                       && proxy_store_ok<B, detail::unwrap_t<R>, P, As...>;
      // A divisor whose grid holds 0 needs a channel for the zero, or unsafe.
      template <insidable C>
      static constexpr bool divisor_ok =
        reports_into_errc<P> || has_action<is_error_action, As...>
        || !is_checked(policy_of<B> | plain_t<P>::Flags | policy_of<C>) || !grid_of<C>.representable(grid_number{0});

    public:
      // The compound ops with an inside: the inside's arithmetic, then dispatch_assign,
      // so the hooks fire on the narrowing back to B.
      // An expected result carrying an error (a zero divisor) reports through on_error if there is one,
      // else the policy.
      // Each is constrained on the free op it calls and on storing the result back under this policy,
      // so a requires probe sees what compiles: a 1/16 sum does not store into an integer grid without snap.
      template <insidable C>
        requires requires(const B& b, const C& c, P& p) { add(b, c, p); }
              && stores_back<decltype(add(std::declval<const B&>(), std::declval<const C&>(), std::declval<P&>()))>
      constexpr B& operator+=(const C& rhs)
      {
        return finalize_arith(add(Ref, rhs, Policy));
      }

      template <insidable C>
        requires requires(const B& b, const C& c, P& p) { sub(b, c, p); }
              && stores_back<decltype(sub(std::declval<const B&>(), std::declval<const C&>(), std::declval<P&>()))>
      constexpr B& operator-=(const C& rhs)
      {
        return finalize_arith(sub(Ref, rhs, Policy));
      }

      template <insidable C>
        requires requires(const B& b, const C& c, P& p) { mul(b, c, p); }
              && stores_back<decltype(mul(std::declval<const B&>(), std::declval<const C&>(), std::declval<P&>()))>
      constexpr B& operator*=(const C& rhs)
      {
        return finalize_arith(mul(Ref, rhs, Policy));
      }

      // A builtin integer steps as the inside on its type's range (above).
      template <step_integer A>
        requires requires(policy_ref& r, const lifted_t<A>& v) { r += v; }
      constexpr B& operator+=(A a)
      {
        return *this += lift_integer(a);
      }
      template <step_integer A>
        requires requires(policy_ref& r, const lifted_t<A>& v) { r -= v; }
      constexpr B& operator-=(A a)
      {
        return *this -= lift_integer(a);
      }
      template <step_integer A>
        requires requires(policy_ref& r, const lifted_t<A>& v) { r *= v; }
      constexpr B& operator*=(A a)
      {
        return *this *= lift_integer(a);
      }

      // A zero divisor is reported (on_error, unless unsafe appears) and leaves Ref unchanged,
      // as inside::operator/= does: div and mod under unsafe skip their own check,
      // so dividing here would divide by zero.
      template <insidable C>
        requires requires(const B& b, const C& c, P& p) { div(b, c, p); }
              && stores_back<decltype(div(std::declval<const B&>(), std::declval<const C&>(), std::declval<P&>()))>
              && divisor_ok<C>
      constexpr B& operator/=(const C& rhs)
      {
        if (rhs == 0)
        {
          if constexpr (is_checked(policy_of<C>)) // either operand silences it
            report_error(error::division_by_zero, "policy_ref::operator/= division by zero");
          return Ref;
        }
        return finalize_arith(div(Ref, rhs, Policy));
      }

      template <insidable C>
        requires requires(const B& b, const C& c, P& p) { mod(b, c, p); }
              && stores_back<decltype(mod(std::declval<const B&>(), std::declval<const C&>(), std::declval<P&>()))>
              && divisor_ok<C>
      constexpr B& operator%=(const C& rhs)
      {
        if (rhs == 0)
        {
          if constexpr (is_checked(policy_of<C>)) // either operand silences it
            report_error(error::division_by_zero, "policy_ref::operator%= division by zero");
          return Ref;
        }
        return finalize_arith(mod(Ref, rhs, Policy));
      }

      // A rational has no grid: as on the inside itself, a compound assignment takes an inside
      // (0.5_ins, just<1, 2>).
#define INSIDE_NO_RATIONAL_ASSIGN(op)                                                                    \
  template <std::same_as<rational> C>                                                                    \
  B& operator op(const C&) = delete ("an inside cannot be combined with a rational: write the constant " \
                                     "as just<1, 2> or 0.5_ins (a grid), or store the rational into an " \
                                     "inside first");
      INSIDE_NO_RATIONAL_ASSIGN(+=)
      INSIDE_NO_RATIONAL_ASSIGN(-=)
      INSIDE_NO_RATIONAL_ASSIGN(*=)
      INSIDE_NO_RATIONAL_ASSIGN(/=)
      INSIDE_NO_RATIONAL_ASSIGN(%=)
#undef INSIDE_NO_RATIONAL_ASSIGN

    private:
      // A failed op leaves Ref unchanged: on_error takes the failure, else the policy reports it
      // (but unsafe skips a zero divisor).
      constexpr void report_error(error code, const char* what)
      {
        if constexpr (has_action<is_error_action, As...>)
          pick_action_in<is_error_action>(Actions).Fn(Ref, code, what);
        else if (code != error::division_by_zero || is_checked(policy_of<B> | plain_t<P>::Flags))
          Policy.report(code);
      }

      // An expected result (/ and % by a grid that holds 0) holds a value here:
      // its only error is the zero divisor, which /= and %= report first.
      template <typename R>
      constexpr B& finalize_arith(R&& result)
      {
        if constexpr (expected_like<R>)
          return dispatch_assign(Ref, *result, Policy, Actions);
        else
          return dispatch_assign(Ref, std::forward<R>(result), Policy, Actions);
      }
    };
#undef INSIDE_WHY_proxy_unproven
  } // namespace detail
} // namespace ins

#endif // INSIDE_INSIDE_DETAIL_POLICY_REF_HPP
