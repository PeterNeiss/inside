//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDarithmeticHPP
#define BNDarithmeticHPP

#include "bound/bound.hpp"
#include "bound/casts.hpp"

//---------------------------------------------------------------------------
// Free-function arithmetic — wraps `addition<L,R>::add`,
// `multiplication<L,R>::mul`, `division<L,R,F>::div`, `modulo<L,R,F>::mod`
// with caller-friendly overloads:
//   add(l, r)                      // default policy
//   add(l, r, policy<F>{})         // explicit policy
//   add(l, r, on_overflow(λ))      // single action
//   add(l, r, ec)                  // std::error_code mode
//   l + r                          // operator sugar
//
// Includes the variadic folds `add_all` / `mul_all` and their `*_into<Target>`
// saturating variants used in audio-mix / sensor-sum pipelines.
//
// The slim::optional overloads of operator+/-/*// are here too so a single
// nullopt in any operand propagates through the whole expression.
//---------------------------------------------------------------------------
namespace bnd
{
  //---------------------------------------------------------------------------
  // add
  //---------------------------------------------------------------------------
  template <boundable L, boundable R, policy_like P = policy<>, typename A = no_action>
  [[nodiscard]] constexpr auto add(L const& lhs, R const& rhs, P&& policy = {}, A&& action = {})
  { return addition<L,R>::add(lhs, rhs, std::forward<P>(policy), std::forward<A>(action)); }

  // Action-first form: 1+ tagged actions, at least one of which is on_overflow
  // (the only kind arithmetic itself fires; others are kept for forward-compat).
  template <boundable L, boundable R, typename... Actions>
    requires (sizeof...(Actions) >= 1)
          && has_action<IsOverflowActionPred, std::remove_cvref_t<Actions>...>
  [[nodiscard]] constexpr auto add(L const& lhs, R const& rhs, Actions&&... actions)
  { return addition<L,R>::add(lhs, rhs,
      make_policy<merged_implied_flags<Actions...>>(),
      pick_action<IsOverflowActionPred>(actions...)); }

  template <boundable L, boundable R, typename A = no_action>
  [[nodiscard]] constexpr auto add(L const& lhs, R const& rhs,
                                   std::error_code& ec, A&& action = {})
  { return addition<L,R>::add(lhs, rhs, make_policy<checked>(ec),
      std::forward<A>(action)); }

  //---------------------------------------------------------------------------
  // operator+
  //---------------------------------------------------------------------------
  [[nodiscard]] constexpr auto operator+(boundable auto lhs, boundable auto rhs)
  { return add(lhs, rhs); }

  template <boundable L, boundable R>
  constexpr auto operator+(slim::optional<L> lhs, R rhs)
  { return lift([](auto l, auto r){ return l + r; }, lhs, rhs); }

  template <boundable L, boundable R>
  constexpr auto operator+(L lhs, slim::optional<R> rhs)
  { return lift([](auto l, auto r){ return l + r; }, lhs, rhs); }

  template <boundable L, boundable R>
  constexpr auto operator+(slim::optional<L> lhs, slim::optional<R> rhs)
  { return lift([](auto l, auto r){ return l + r; }, lhs, rhs); }

  //---------------------------------------------------------------------------
  // sub
  //---------------------------------------------------------------------------
  template <boundable L, boundable R, policy_like P = policy<>, typename A = no_action>
  [[nodiscard]] constexpr auto sub(L const& lhs, R const& rhs, P&& policy = {}, A&& action = {})
  { return add(lhs, -rhs, std::forward<P>(policy), std::forward<A>(action)); }

  template <boundable L, boundable R, typename... Actions>
    requires (sizeof...(Actions) >= 1)
          && has_action<IsOverflowActionPred, std::remove_cvref_t<Actions>...>
  [[nodiscard]] constexpr auto sub(L const& lhs, R const& rhs, Actions&&... actions)
  { return add(lhs, -rhs,
      make_policy<merged_implied_flags<Actions...>>(),
      pick_action<IsOverflowActionPred>(actions...)); }

  template <boundable L, boundable R, typename A = no_action>
  [[nodiscard]] constexpr auto sub(L const& lhs, R const& rhs,
                                   std::error_code& ec, A&& action = {})
  { return add(lhs, -rhs, make_policy<checked>(ec), std::forward<A>(action)); }

  //---------------------------------------------------------------------------
  // operator-
  //---------------------------------------------------------------------------
  [[nodiscard]] constexpr auto operator-(boundable auto lhs, boundable auto rhs)
  { return sub(lhs, rhs); }

  template <boundable L, boundable R>
  constexpr auto operator-(slim::optional<L> lhs, R rhs)
  { return lift([](auto l, auto r){ return l - r; }, lhs, rhs); }

  template <boundable L, boundable R>
  constexpr auto operator-(L lhs, slim::optional<R> rhs)
  { return lift([](auto l, auto r){ return l - r; }, lhs, rhs); }

  template <boundable L, boundable R>
  constexpr auto operator-(slim::optional<L> lhs, slim::optional<R> rhs)
  { return lift([](auto l, auto r){ return l - r; }, lhs, rhs); }

  //---------------------------------------------------------------------------
  // mul
  //---------------------------------------------------------------------------
  template <boundable L, boundable R, policy_like P = policy<>, typename A = no_action>
  [[nodiscard]] constexpr auto mul(L const& lhs, R const& rhs, P&& policy = {}, A&& action = {})
  { return multiplication<L,R>::mul(lhs, rhs, std::forward<P>(policy), std::forward<A>(action)); }

  template <boundable L, boundable R, typename... Actions>
    requires (sizeof...(Actions) >= 1)
          && has_action<IsOverflowActionPred, std::remove_cvref_t<Actions>...>
  [[nodiscard]] constexpr auto mul(L const& lhs, R const& rhs, Actions&&... actions)
  { return multiplication<L,R>::mul(lhs, rhs,
      make_policy<merged_implied_flags<Actions...>>(),
      pick_action<IsOverflowActionPred>(actions...)); }

  template <boundable L, boundable R, typename A = no_action>
  [[nodiscard]] constexpr auto mul(L const& lhs, R const& rhs,
                                   std::error_code& ec, A&& action = {})
  { return multiplication<L,R>::mul(lhs, rhs, make_policy<checked>(ec),
      std::forward<A>(action)); }

  //---------------------------------------------------------------------------
  // operator*
  //---------------------------------------------------------------------------
  [[nodiscard]] constexpr auto operator*(boundable auto lhs, boundable auto rhs)
  { return bnd::mul(lhs, rhs); }

  template <boundable L, boundable R>
  constexpr auto operator*(slim::optional<L> lhs, R rhs)
  { return lift([](auto l, auto r){ return l * r; }, lhs, rhs); }

  template <boundable L, boundable R>
  constexpr auto operator*(L lhs, slim::optional<R> rhs)
  { return lift([](auto l, auto r){ return l * r; }, lhs, rhs); }

  template <boundable L, boundable R>
  constexpr auto operator*(slim::optional<L> lhs, slim::optional<R> rhs)
  { return lift([](auto l, auto r){ return l * r; }, lhs, rhs); }

  //---------------------------------------------------------------------------
  // add_all / mul_all — variadic folds
  //
  // `a + b + c + d` works today via pairwise `add`, threading four grid
  // widenings. `add_all(a, b, c, d)` is the variadic equivalent — same
  // pairwise widening, but the fold reads cleaner and matches Chromium's
  // `CheckAdd(a, b, c)` idiom.
  //---------------------------------------------------------------------------
  template <boundable First, boundable... Rest>
  [[nodiscard]] constexpr auto add_all(First const& first, Rest const&... rest)
  { return (first + ... + rest); }

  template <boundable First, boundable... Rest>
  [[nodiscard]] constexpr auto mul_all(First const& first, Rest const&... rest)
  { return (first * ... * rest); }

  // `add_all_into<Target>` / `mul_all_into<Target>` — variadic fold that
  // collapses the widening intermediate back into a caller-chosen target
  // grid via `clamp_cast<Target>`. The standard audio-mix / sensor-sum
  // idiom: pairwise widen for exactness, then clip to the bus.
  template <boundable Target, boundable First, boundable... Rest>
  [[nodiscard]] constexpr Target add_all_into(First const& first, Rest const&... rest)
  {
    auto sum = (first + ... + rest);
    if constexpr (requires { typename decltype(sum)::value_type; })
      return clamp_cast<Target>(sum.value());
    else
      return clamp_cast<Target>(sum);
  }

  template <boundable Target, boundable First, boundable... Rest>
  [[nodiscard]] constexpr Target mul_all_into(First const& first, Rest const&... rest)
  {
    auto prod = (first * ... * rest);
    if constexpr (requires { typename decltype(prod)::value_type; })
      return clamp_cast<Target>(prod.value());
    else
      return clamp_cast<Target>(prod);
  }

  //---------------------------------------------------------------------------
  // div
  //---------------------------------------------------------------------------
  template <boundable L, boundable R, policy_flag F = none, typename A = no_action>
  [[nodiscard]] constexpr auto div(L lhs, R rhs, policy<F> pol = {}, A&& action = {})
  { return division<L, R, F>::div(lhs, rhs, pol, std::forward<A>(action)); }

  template <boundable L, boundable R, typename... Actions>
    requires (sizeof...(Actions) >= 1)
          && has_action<IsOverflowActionPred, std::remove_cvref_t<Actions>...>
  [[nodiscard]] constexpr auto div(L lhs, R rhs, Actions&&... actions)
  { return division<L, R, merged_implied_flags<Actions...>>::div(lhs, rhs,
      make_policy<merged_implied_flags<Actions...>>(),
      pick_action<IsOverflowActionPred>(actions...)); }

  template <boundable L, boundable R, typename A = no_action>
  [[nodiscard]] constexpr auto div(L lhs, R rhs,
                                   std::error_code& ec, A&& action = {})
  { return division<L, R, checked>::div(lhs, rhs, make_policy<checked>(ec),
      std::forward<A>(action)); }

  //---------------------------------------------------------------------------
  // operator/
  //---------------------------------------------------------------------------
  [[nodiscard]] constexpr auto operator/(boundable auto lhs, boundable auto rhs)
  {
    constexpr policy_flag F = BoundPolicy<decltype(lhs)> | BoundPolicy<decltype(rhs)>;
    return bnd::div(lhs, rhs, make_policy<F>());
  }

  template <boundable L, boundable R>
  constexpr auto operator/(slim::optional<L> lhs, R rhs)
  { return lift([](auto l, auto r){ return l / r; }, lhs, rhs); }

  template <boundable L, boundable R>
  constexpr auto operator/(L lhs, slim::optional<R> rhs)
  { return lift([](auto l, auto r){ return l / r; }, lhs, rhs); }

  template <boundable L, boundable R>
  constexpr auto operator/(slim::optional<L> lhs, slim::optional<R> rhs)
  { return lift([](auto l, auto r){ return l / r; }, lhs, rhs); }

  //---------------------------------------------------------------------------
  // mod
  //---------------------------------------------------------------------------
  template <boundable L, boundable R, policy_flag F = none, typename A = no_action>
  [[nodiscard]] constexpr auto mod(L lhs, R rhs, policy<F> pol = {}, A&& action = {})
  { return modulo<L, R, F>::mod(lhs, rhs, pol, std::forward<A>(action)); }

  template <boundable L, boundable R, typename... Actions>
    requires (sizeof...(Actions) >= 1)
          && has_action<IsOverflowActionPred, std::remove_cvref_t<Actions>...>
  [[nodiscard]] constexpr auto mod(L lhs, R rhs, Actions&&... actions)
  { return modulo<L, R, merged_implied_flags<Actions...>>::mod(lhs, rhs,
      make_policy<merged_implied_flags<Actions...>>(),
      pick_action<IsOverflowActionPred>(actions...)); }

  template <boundable L, boundable R, typename A = no_action>
  [[nodiscard]] constexpr auto mod(L lhs, R rhs,
                                   std::error_code& ec, A&& action = {})
  { return modulo<L, R, checked>::mod(lhs, rhs, make_policy<checked>(ec),
      std::forward<A>(action)); }

  //---------------------------------------------------------------------------
  // operator%
  //---------------------------------------------------------------------------
  [[nodiscard]] constexpr auto operator%(boundable auto lhs, boundable auto rhs)
  {
    constexpr policy_flag F = BoundPolicy<decltype(lhs)> | BoundPolicy<decltype(rhs)>;
    return bnd::mod(lhs, rhs, make_policy<F>());
  }

  //---------------------------------------------------------------------------
  // Mixed-mode bound op real
  //
  // `boundable op real` (and the symmetric `real op boundable`) lift the
  // bound into rational (for rational RHS) or double (for floating-point
  // RHS) and route through the corresponding scalar operator. The result
  // type is `rational` or `double` — there is no widening-bound result,
  // because runtime real arithmetic can't reconstruct a bound type.
  //
  // The rational path goes through the checked `rational::operator op`,
  // then unwraps via `.value()` — overflow surfaces as
  // `slim::bad_optional_access`, matching `rational::mul_unchecked`'s
  // panic semantics rather than the silent-truncation that
  // `static_cast<imax>(rational)` would otherwise do.
  //---------------------------------------------------------------------------
  template <boundable B>
  [[nodiscard]] constexpr rational operator+(B const& lhs, rational const& rhs)
  { return (static_cast<rational>(lhs) + rhs).value(); }

  template <boundable B>
  [[nodiscard]] constexpr rational operator+(rational const& lhs, B const& rhs)
  { return (lhs + static_cast<rational>(rhs)).value(); }

  template <boundable B>
  [[nodiscard]] constexpr rational operator-(B const& lhs, rational const& rhs)
  { return (static_cast<rational>(lhs) - rhs).value(); }

  template <boundable B>
  [[nodiscard]] constexpr rational operator-(rational const& lhs, B const& rhs)
  { return (lhs - static_cast<rational>(rhs)).value(); }

  template <boundable B>
  [[nodiscard]] constexpr rational operator*(B const& lhs, rational const& rhs)
  { return (static_cast<rational>(lhs) * rhs).value(); }

  template <boundable B>
  [[nodiscard]] constexpr rational operator*(rational const& lhs, B const& rhs)
  { return (lhs * static_cast<rational>(rhs)).value(); }

  template <boundable B>
  [[nodiscard]] constexpr rational operator/(B const& lhs, rational const& rhs)
  { return (static_cast<rational>(lhs) / rhs).value(); }

  template <boundable B>
  [[nodiscard]] constexpr rational operator/(rational const& lhs, B const& rhs)
  { return (lhs / static_cast<rational>(rhs)).value(); }

  // optional<bound> × rational — propagate via lift so callers can chain
  // checked-arithmetic results into a rational expression without unwrapping.
  template <boundable B>
  [[nodiscard]] constexpr auto operator+(slim::optional<B> const& lhs, rational const& rhs)
  { return lift([](B b, rational r){ return static_cast<rational>(b) + r; }, lhs, rhs); }

  template <boundable B>
  [[nodiscard]] constexpr auto operator+(rational const& lhs, slim::optional<B> const& rhs)
  { return lift([](rational r, B b){ return r + static_cast<rational>(b); }, lhs, rhs); }

  template <boundable B>
  [[nodiscard]] constexpr auto operator-(slim::optional<B> const& lhs, rational const& rhs)
  { return lift([](B b, rational r){ return static_cast<rational>(b) - r; }, lhs, rhs); }

  template <boundable B>
  [[nodiscard]] constexpr auto operator-(rational const& lhs, slim::optional<B> const& rhs)
  { return lift([](rational r, B b){ return r - static_cast<rational>(b); }, lhs, rhs); }

  template <boundable B>
  [[nodiscard]] constexpr auto operator*(slim::optional<B> const& lhs, rational const& rhs)
  { return lift([](B b, rational r){ return static_cast<rational>(b) * r; }, lhs, rhs); }

  template <boundable B>
  [[nodiscard]] constexpr auto operator*(rational const& lhs, slim::optional<B> const& rhs)
  { return lift([](rational r, B b){ return r * static_cast<rational>(b); }, lhs, rhs); }

  template <boundable B>
  [[nodiscard]] constexpr auto operator/(slim::optional<B> const& lhs, rational const& rhs)
  { return lift([](B b, rational r){ return static_cast<rational>(b) / r; }, lhs, rhs); }

  template <boundable B>
  [[nodiscard]] constexpr auto operator/(rational const& lhs, slim::optional<B> const& rhs)
  { return lift([](rational r, B b){ return r / static_cast<rational>(b); }, lhs, rhs); }

  template <boundable B, std::floating_point F>
  [[nodiscard]] constexpr double operator+(B const& lhs, F rhs)
  { return static_cast<double>(lhs) + static_cast<double>(rhs); }

  template <boundable B, std::floating_point F>
  [[nodiscard]] constexpr double operator+(F lhs, B const& rhs)
  { return static_cast<double>(lhs) + static_cast<double>(rhs); }

  template <boundable B, std::floating_point F>
  [[nodiscard]] constexpr double operator-(B const& lhs, F rhs)
  { return static_cast<double>(lhs) - static_cast<double>(rhs); }

  template <boundable B, std::floating_point F>
  [[nodiscard]] constexpr double operator-(F lhs, B const& rhs)
  { return static_cast<double>(lhs) - static_cast<double>(rhs); }

  template <boundable B, std::floating_point F>
  [[nodiscard]] constexpr double operator*(B const& lhs, F rhs)
  { return static_cast<double>(lhs) * static_cast<double>(rhs); }

  template <boundable B, std::floating_point F>
  [[nodiscard]] constexpr double operator*(F lhs, B const& rhs)
  { return static_cast<double>(lhs) * static_cast<double>(rhs); }

  template <boundable B, std::floating_point F>
  [[nodiscard]] constexpr double operator/(B const& lhs, F rhs)
  { return static_cast<double>(lhs) / static_cast<double>(rhs); }

  template <boundable B, std::floating_point F>
  [[nodiscard]] constexpr double operator/(F lhs, B const& rhs)
  { return static_cast<double>(lhs) / static_cast<double>(rhs); }

  // Integral RHS overloads — without these, `bound + 1` is ambiguous: int
  // can convert to rational (the mixed-mode op above) and bound can convert
  // to rational (rational's `arithmetic + rational` lift). Adding an
  // explicit `(boundable, integral)` form gives a zero-user-conversion
  // candidate that wins overload resolution. Returns rational.
  template <boundable B, std::integral I>
  [[nodiscard]] constexpr rational operator+(B const& lhs, I rhs)
  { return (static_cast<rational>(lhs) + rational{rhs}).value(); }

  template <std::integral I, boundable B>
  [[nodiscard]] constexpr rational operator+(I lhs, B const& rhs)
  { return (rational{lhs} + static_cast<rational>(rhs)).value(); }

  template <boundable B, std::integral I>
  [[nodiscard]] constexpr rational operator-(B const& lhs, I rhs)
  { return (static_cast<rational>(lhs) - rational{rhs}).value(); }

  template <std::integral I, boundable B>
  [[nodiscard]] constexpr rational operator-(I lhs, B const& rhs)
  { return (rational{lhs} - static_cast<rational>(rhs)).value(); }

  template <boundable B, std::integral I>
  [[nodiscard]] constexpr rational operator*(B const& lhs, I rhs)
  { return (static_cast<rational>(lhs) * rational{rhs}).value(); }

  template <std::integral I, boundable B>
  [[nodiscard]] constexpr rational operator*(I lhs, B const& rhs)
  { return (rational{lhs} * static_cast<rational>(rhs)).value(); }

  template <boundable B, std::integral I>
  [[nodiscard]] constexpr rational operator/(B const& lhs, I rhs)
  { return (static_cast<rational>(lhs) / rational{rhs}).value(); }

  template <std::integral I, boundable B>
  [[nodiscard]] constexpr rational operator/(I lhs, B const& rhs)
  { return (rational{lhs} / static_cast<rational>(rhs)).value(); }

} // namespace bnd

#endif // BNDarithmeticHPP
