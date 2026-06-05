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

  // Single overload covers all three optional shapes (optional×T, T×optional,
  // optional×optional) for any combination of boundable / rational operands.
  // The lambda's `l + r` re-enters overload resolution on the *unwrapped*
  // values, so we inherit whichever bare overload (bound×bound, bound×rational,
  // bound×integral, …) would normally apply.
  template <class L, class R>
    requires (is_slim_optional_v<L> || is_slim_optional_v<R>)
          && (boundable<unwrap_t<L>> || boundable<unwrap_t<R>>)
          && requires(unwrap_t<L> l, unwrap_t<R> r) { l + r; }
  constexpr auto operator+(L const& lhs, R const& rhs)
  { return lift([](auto const& l, auto const& r){ return l + r; }, lhs, rhs); }

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

  template <class L, class R>
    requires (is_slim_optional_v<L> || is_slim_optional_v<R>)
          && (boundable<unwrap_t<L>> || boundable<unwrap_t<R>>)
          && requires(unwrap_t<L> l, unwrap_t<R> r) { l - r; }
  constexpr auto operator-(L const& lhs, R const& rhs)
  { return lift([](auto const& l, auto const& r){ return l - r; }, lhs, rhs); }

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

  template <class L, class R>
    requires (is_slim_optional_v<L> || is_slim_optional_v<R>)
          && (boundable<unwrap_t<L>> || boundable<unwrap_t<R>>)
          && requires(unwrap_t<L> l, unwrap_t<R> r) { l * r; }
  constexpr auto operator*(L const& lhs, R const& rhs)
  { return lift([](auto const& l, auto const& r){ return l * r; }, lhs, rhs); }

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

  template <class L, class R>
    requires (is_slim_optional_v<L> || is_slim_optional_v<R>)
          && (boundable<unwrap_t<L>> || boundable<unwrap_t<R>>)
          && requires(unwrap_t<L> l, unwrap_t<R> r) { l / r; }
  constexpr auto operator/(L const& lhs, R const& rhs)
  { return lift([](auto const& l, auto const& r){ return l / r; }, lhs, rhs); }

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

  //---------------------------------------------------------------------------
  // Grid-less scalar operands are rejected.
  //
  // A raw `int`/`double` carries no grid, so `bound op rawscalar` has no
  // type-safe result: widening to the scalar's full type range would balloon
  // the grid, and a runtime value has no compile-time point grid to widen
  // from. Rather than silently escape into `rational`/`double` (the old
  // behavior), these guidance overloads make the expression ill-formed with a
  // message that hands the caller the fix: give the literal a grid (`1_b` /
  // `just<1>`), or build a bound over the value's known range.
  //
  // Only `std::integral`/`std::floating_point` are caught here; `rational` is
  // also `arithmetic` but keeps its exact, deliberate mixed-mode operators
  // above. Comparisons (`a == 1`, `a < 5`) and compound assignment (`a += 1`)
  // with raw scalars are unaffected — they don't manufacture a new value/type
  // and so never leave the bounded world.
  //---------------------------------------------------------------------------
  // Concrete (non-`auto`) return type on purpose: it keeps these overloads
  // SFINAE-transparent — probing `requires { b + 1; }` stays well-formed and
  // the `static_assert` fires only on real instantiation (an actual call),
  // exactly as the old `rational`/`double`-returning mixed-mode operators did.
  template <typename A> concept raw_scalar = std::integral<A> || std::floating_point<A>;

  template <boundable B, raw_scalar A> B operator+(B const&, A) {
    static_assert(dependent_false<B>,
      "a bound can only be added to another bound: give the scalar a grid — "
      "write `a + 1_b` (or `a + just<1>`), or `a + bound<{lo,hi}>{n}` for a "
      "runtime value with a known range"); }
  template <raw_scalar A, boundable B> B operator+(A, B const&) {
    static_assert(dependent_false<B>,
      "a scalar can only be added to a bound that is itself a bound: write "
      "`1_b + a` / `just<1> + a`, or `bound<{lo,hi}>{n} + a`"); }

  template <boundable B, raw_scalar A> B operator-(B const&, A) {
    static_assert(dependent_false<B>,
      "subtract a bound, not a raw scalar: write `a - 1_b` / `a - just<1>`, "
      "or `a - bound<{lo,hi}>{n}` for a runtime value with a known range"); }
  template <raw_scalar A, boundable B> B operator-(A, B const&) {
    static_assert(dependent_false<B>,
      "subtract from a bound, not a raw scalar: write `1_b - a` / `just<1> - "
      "a`, or `bound<{lo,hi}>{n} - a`"); }

  template <boundable B, raw_scalar A> B operator*(B const&, A) {
    static_assert(dependent_false<B>,
      "multiply by a bound, not a raw scalar: write `a * 2_b` / `a * just<2>`, "
      "or `a * bound<{lo,hi}>{n}` for a runtime value with a known range"); }
  template <raw_scalar A, boundable B> B operator*(A, B const&) {
    static_assert(dependent_false<B>,
      "multiply a bound by a bound, not a raw scalar: write `2_b * a` / "
      "`just<2> * a`, or `bound<{lo,hi}>{n} * a`"); }

  template <boundable B, raw_scalar A> B operator/(B const&, A) {
    static_assert(dependent_false<B>,
      "divide by a bound, not a raw scalar: write `a / 2_b` / `a / just<2>`, "
      "or `a / bound<{lo,hi}>{n}` for a runtime value with a known range"); }
  template <raw_scalar A, boundable B> B operator/(A, B const&) {
    static_assert(dependent_false<B>,
      "divide a bound by a bound, not a raw scalar: write `6_b / a` / "
      "`just<6> / a`, or `bound<{lo,hi}>{n} / a`"); }

} // namespace bnd

#endif // BNDarithmeticHPP
