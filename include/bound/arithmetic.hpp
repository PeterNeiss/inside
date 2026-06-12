//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDarithmeticHPP
#define BNDarithmeticHPP

#include "bound/bound.hpp"
#include "bound/casts.hpp"

#include <algorithm>
#include <ranges>

//---------------------------------------------------------------------------
// Free-function arithmetic — wraps `detail::addition<L,R>::add`,
// `detail::multiplication<L,R>::mul`, `detail::division<L,R,F>::div`, `detail::modulo<L,R,F>::mod`
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
  template <boundable L, boundable R, detail::policy_like P = policy<>, typename A = no_action>
  [[nodiscard]] constexpr auto add(L const& lhs, R const& rhs, P&& policy = {}, A&& action = {})
  { return detail::addition<L,R>::add(lhs, rhs, std::forward<P>(policy), std::forward<A>(action)); }

  // Action-first form: 1+ tagged actions, at least one of which is on_overflow
  // (the only kind arithmetic itself fires; others are kept for forward-compat).
  template <boundable L, boundable R, typename... Actions>
    requires (sizeof...(Actions) >= 1)
          && detail::has_action<detail::IsOverflowActionPred, std::remove_cvref_t<Actions>...>
  [[nodiscard]] constexpr auto add(L const& lhs, R const& rhs, Actions&&... actions)
  { return detail::addition<L,R>::add(lhs, rhs,
      make_policy<detail::merged_implied_flags<Actions...>>(),
      detail::pick_action<detail::IsOverflowActionPred>(actions...)); }

  template <boundable L, boundable R, typename A = no_action>
  [[nodiscard]] constexpr auto add(L const& lhs, R const& rhs,
                                   std::error_code& ec, A&& action = {})
  { return detail::addition<L,R>::add(lhs, rhs, make_policy<checked>(ec),
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
    requires (detail::is_slim_optional_v<L> || detail::is_slim_optional_v<R>)
          && (!detail::expected_like<L> && !detail::expected_like<R>)
          && (boundable<detail::unwrap_t<L>> || boundable<detail::unwrap_t<R>>)
          && requires(detail::unwrap_t<L> l, detail::unwrap_t<R> r) { l + r; }
  constexpr auto operator+(L const& lhs, R const& rhs)
  { return lift([](auto const& l, auto const& r){ return l + r; }, lhs, rhs); }

  //---------------------------------------------------------------------------
  // sub
  //---------------------------------------------------------------------------
  template <boundable L, boundable R, detail::policy_like P = policy<>, typename A = no_action>
  [[nodiscard]] constexpr auto sub(L const& lhs, R const& rhs, P&& policy = {}, A&& action = {})
  { return add(lhs, -rhs, std::forward<P>(policy), std::forward<A>(action)); }

  template <boundable L, boundable R, typename... Actions>
    requires (sizeof...(Actions) >= 1)
          && detail::has_action<detail::IsOverflowActionPred, std::remove_cvref_t<Actions>...>
  [[nodiscard]] constexpr auto sub(L const& lhs, R const& rhs, Actions&&... actions)
  { return add(lhs, -rhs,
      make_policy<detail::merged_implied_flags<Actions...>>(),
      detail::pick_action<detail::IsOverflowActionPred>(actions...)); }

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
    requires (detail::is_slim_optional_v<L> || detail::is_slim_optional_v<R>)
          && (!detail::expected_like<L> && !detail::expected_like<R>)
          && (boundable<detail::unwrap_t<L>> || boundable<detail::unwrap_t<R>>)
          && requires(detail::unwrap_t<L> l, detail::unwrap_t<R> r) { l - r; }
  constexpr auto operator-(L const& lhs, R const& rhs)
  { return lift([](auto const& l, auto const& r){ return l - r; }, lhs, rhs); }

  //---------------------------------------------------------------------------
  // mul
  //---------------------------------------------------------------------------
  template <boundable L, boundable R, detail::policy_like P = policy<>, typename A = no_action>
  [[nodiscard]] constexpr auto mul(L const& lhs, R const& rhs, P&& policy = {}, A&& action = {})
  { return detail::multiplication<L,R>::mul(lhs, rhs, std::forward<P>(policy), std::forward<A>(action)); }

  template <boundable L, boundable R, typename... Actions>
    requires (sizeof...(Actions) >= 1)
          && detail::has_action<detail::IsOverflowActionPred, std::remove_cvref_t<Actions>...>
  [[nodiscard]] constexpr auto mul(L const& lhs, R const& rhs, Actions&&... actions)
  { return detail::multiplication<L,R>::mul(lhs, rhs,
      make_policy<detail::merged_implied_flags<Actions...>>(),
      detail::pick_action<detail::IsOverflowActionPred>(actions...)); }

  template <boundable L, boundable R, typename A = no_action>
  [[nodiscard]] constexpr auto mul(L const& lhs, R const& rhs,
                                   std::error_code& ec, A&& action = {})
  { return detail::multiplication<L,R>::mul(lhs, rhs, make_policy<checked>(ec),
      std::forward<A>(action)); }

  //---------------------------------------------------------------------------
  // operator*
  //---------------------------------------------------------------------------
  [[nodiscard]] constexpr auto operator*(boundable auto lhs, boundable auto rhs)
  { return bnd::mul(lhs, rhs); }

  template <class L, class R>
    requires (detail::is_slim_optional_v<L> || detail::is_slim_optional_v<R>)
          && (!detail::expected_like<L> && !detail::expected_like<R>)
          && (boundable<detail::unwrap_t<L>> || boundable<detail::unwrap_t<R>>)
          && requires(detail::unwrap_t<L> l, detail::unwrap_t<R> r) { l * r; }
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
  // sum<Target> — bulk reduction with ONE deferred range check
  //
  // Per-element `target += b` re-validates the running total on every step,
  // which blocks vectorization (the 4× gap on checked accumulation in
  // bench.cpp). `sum<Target>(range)` accumulates raws in imax — exact, no
  // per-element branch, auto-vectorizable — and applies Target's policy ONCE
  // to the final total. Deliberate semantic difference from the += loop: the
  // *total* is validated/clamped, not every running prefix.
  //
  // Fast path: integer raws up to 32 bits, flushed into an exact rational
  // every 2^30 elements so the imax accumulator cannot overflow. Wider raws
  // and rational/real storage take the exact per-element rational fold (same
  // single final check).
  //---------------------------------------------------------------------------
  template <boundable Target, std::ranges::input_range Rng>
    requires boundable<std::remove_cvref_t<std::ranges::range_reference_t<Rng>>>
  [[nodiscard]] constexpr Target sum(Rng&& r)
  {
    using B = std::remove_cvref_t<std::ranges::range_reference_t<Rng>>;
    using bnd::detail::rational;
    rational total{0};

    if constexpr ((detail::value_raw<B> || detail::index_raw<B>)
                  && sizeof(detail::raw_t<B>) <= 4)
    {
      auto flush = [&](imax acc, imax cnt)
      {
        // value storage: raw IS the value. index storage:
        // Σvalue = cnt·Lower + Σraw·Notch.
        rational part = [&]
        {
          if constexpr (detail::index_raw<B>)
            return ((rational{acc} * Notch<B>).value()
                    + (rational{cnt} * Lower<B>).value()).value();
          else
            return rational{acc};
        }();
        total = (total + part).value();
      };
      auto it  = std::ranges::begin(r);
      auto end = std::ranges::end(r);
      while (it != end)
      {
        // Branch-free inner block (≤ 2^30 elements: |raw| < 2^32 keeps the
        // imax accumulator overflow-free) — this is the loop that vectorizes.
        imax acc = 0, cnt = 0;
        if constexpr (std::ranges::random_access_range<Rng>)
        {
          const imax block =
              std::min<imax>(end - it, imax{1} << 30);
          for (imax j = 0; j < block; ++j)
            acc += detail::raw_imax(it[j]);
          it += block;
          cnt = block;
        }
        else
        {
          for (; it != end && cnt < (imax{1} << 30); ++it, ++cnt)
            acc += detail::raw_imax(*it);
        }
        flush(acc, cnt);
      }
    }
    else
    {
      for (auto const& b : r)
        total = (total + detail::as_rational(b)).value();
    }
    return Target{total};
  }

  //---------------------------------------------------------------------------
  // dot / cross / lerp — small bound-space vector helpers
  //
  // These keep 2-D geometry inside the bounded world: a consumer steering or
  // measuring against a target never has to drop to a raw scalar to form a dot
  // product, a 2-D cross (the z-component, useful for "which side" tests), or a
  // linear interpolation. Each widens its result grid like the underlying
  // `+`/`*`, so no overflow is possible and the result is a plain `bound`.
  //---------------------------------------------------------------------------
  [[nodiscard]] constexpr auto dot(boundable auto ax, boundable auto ay,
                                   boundable auto bx, boundable auto by)
  { return ax * bx + ay * by; }

  [[nodiscard]] constexpr auto cross(boundable auto ax, boundable auto ay,
                                     boundable auto bx, boundable auto by)
  { return ax * by - ay * bx; }

  // lerp(a, b, t) = a + (b - a) * t. `t` is itself a bound (typically a
  // [0, 1] fixed-point grid), so the interpolation never leaves bound-space.
  [[nodiscard]] constexpr auto lerp(boundable auto a, boundable auto b,
                                    boundable auto t)
  { return a + (b - a) * t; }

  //---------------------------------------------------------------------------
  // std-vocabulary helpers — ADL-found `min` / `max` / `midpoint`.
  //
  // These let bounds drop into generic code that calls unqualified
  // `min` / `max` / `midpoint` (resolved by ADL). `min` / `max` mirror the std
  // signatures and return the same bound type. `midpoint` returns the *exact*
  // average on a refined grid: the true midpoint of two grid points need not
  // land on the grid, so — unlike `std::midpoint` on integers — it neither
  // rounds nor overflows. (`lerp` above is the third std-vocabulary verb.)
  //
  // There is no free `bnd::clamp`: the name is the `clamp` *policy flag*. To
  // clamp a value into a grid use the `clamp` policy or `clamp_cast<Target>`.
  //---------------------------------------------------------------------------
  template <boundable T>
  [[nodiscard]] constexpr T min(T a, T b) { return (b < a) ? b : a; }

  template <boundable T>
  [[nodiscard]] constexpr T max(T a, T b) { return (a < b) ? b : a; }

  template <boundable T>
  [[nodiscard]] constexpr auto midpoint(T a, T b) { return (a + b) * just<frac<1, 2>>; }

  //---------------------------------------------------------------------------
  // div
  //---------------------------------------------------------------------------
  template <boundable L, boundable R, policy_flag F = none, typename A = no_action>
  [[nodiscard]] constexpr auto div(L lhs, R rhs, policy<F> pol = {}, A&& action = {})
  { return detail::division<L, R, F>::div(lhs, rhs, pol, std::forward<A>(action)); }

  template <boundable L, boundable R, typename... Actions>
    requires (sizeof...(Actions) >= 1)
          && detail::has_action<detail::IsOverflowActionPred, std::remove_cvref_t<Actions>...>
  [[nodiscard]] constexpr auto div(L lhs, R rhs, Actions&&... actions)
  { return detail::division<L, R, detail::merged_implied_flags<Actions...>>::div(lhs, rhs,
      make_policy<detail::merged_implied_flags<Actions...>>(),
      detail::pick_action<detail::IsOverflowActionPred>(actions...)); }

  template <boundable L, boundable R, typename A = no_action>
  [[nodiscard]] constexpr auto div(L lhs, R rhs,
                                   std::error_code& ec, A&& action = {})
  { return detail::division<L, R, checked>::div(lhs, rhs, make_policy<checked>(ec),
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
    requires (detail::is_slim_optional_v<L> || detail::is_slim_optional_v<R>)
          && (!detail::expected_like<L> && !detail::expected_like<R>)
          && (boundable<detail::unwrap_t<L>> || boundable<detail::unwrap_t<R>>)
          && requires(detail::unwrap_t<L> l, detail::unwrap_t<R> r) { l / r; }
  constexpr auto operator/(L const& lhs, R const& rhs)
  { return lift([](auto const& l, auto const& r){ return l / r; }, lhs, rhs); }

  //---------------------------------------------------------------------------
  // mod
  //---------------------------------------------------------------------------
  template <boundable L, boundable R, policy_flag F = none, typename A = no_action>
  [[nodiscard]] constexpr auto mod(L lhs, R rhs, policy<F> pol = {}, A&& action = {})
  { return detail::modulo<L, R, F>::mod(lhs, rhs, pol, std::forward<A>(action)); }

  template <boundable L, boundable R, typename... Actions>
    requires (sizeof...(Actions) >= 1)
          && detail::has_action<detail::IsOverflowActionPred, std::remove_cvref_t<Actions>...>
  [[nodiscard]] constexpr auto mod(L lhs, R rhs, Actions&&... actions)
  { return detail::modulo<L, R, detail::merged_implied_flags<Actions...>>::mod(lhs, rhs,
      make_policy<detail::merged_implied_flags<Actions...>>(),
      detail::pick_action<detail::IsOverflowActionPred>(actions...)); }

  template <boundable L, boundable R, typename A = no_action>
  [[nodiscard]] constexpr auto mod(L lhs, R rhs,
                                   std::error_code& ec, A&& action = {})
  { return detail::modulo<L, R, checked>::mod(lhs, rhs, make_policy<checked>(ec),
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
  // expected-lift operators — bridge bnd::math's expected results into chains
  //---------------------------------------------------------------------------
  // `math::tan(x) * gain + offset` stays an `expected<bound, errc>` end to
  // end: the first (left) error short-circuits the chain. When the underlying
  // operator itself returns an optional (a division whose divisor grid spans
  // zero), its nullopt maps to the operator's single documented cause —
  // overflow for + − ×, division_by_zero for / (a rational-arithmetic
  // overflow inside a division chain also reports division_by_zero; the
  // optional vocabulary doesn't carry the distinction).
  //
  // To drop the cause and enter the zero-cost optional world instead, convert
  // with `bnd::ok(e)` — see lift.hpp.
  //---------------------------------------------------------------------------
  namespace detail
  {
    template <class L, class R>
    concept expected_operands =
        (expected_like<L> || expected_like<R>)
        && !is_slim_optional_v<L> && !is_slim_optional_v<R>
        && (boundable<expected_value_t<L>> || boundable<expected_value_t<R>>);

    // Mixing the two vocabularies in one expression is refused: the optional
    // operand's original cause is unknowable, so we won't invent one.
    template <class L, class R>
    concept mixed_error_operands =
        (expected_like<L> && is_slim_optional_v<R>)
        || (is_slim_optional_v<L> && expected_like<R>);
  }

  template <class L, class R>
    requires detail::expected_operands<L, R>
          && requires(detail::expected_value_t<L> l, detail::expected_value_t<R> r) { l + r; }
  constexpr auto operator+(L const& lhs, R const& rhs)
  { return lift_expected([](auto const& l, auto const& r){ return l + r; },
                         errc::overflow, lhs, rhs); }

  template <class L, class R>
    requires detail::expected_operands<L, R>
          && requires(detail::expected_value_t<L> l, detail::expected_value_t<R> r) { l - r; }
  constexpr auto operator-(L const& lhs, R const& rhs)
  { return lift_expected([](auto const& l, auto const& r){ return l - r; },
                         errc::overflow, lhs, rhs); }

  template <class L, class R>
    requires detail::expected_operands<L, R>
          && requires(detail::expected_value_t<L> l, detail::expected_value_t<R> r) { l * r; }
  constexpr auto operator*(L const& lhs, R const& rhs)
  { return lift_expected([](auto const& l, auto const& r){ return l * r; },
                         errc::overflow, lhs, rhs); }

  template <class L, class R>
    requires detail::expected_operands<L, R>
          && requires(detail::expected_value_t<L> l, detail::expected_value_t<R> r) { l / r; }
  constexpr auto operator/(L const& lhs, R const& rhs)
  { return lift_expected([](auto const& l, auto const& r){ return l / r; },
                         errc::division_by_zero, lhs, rhs); }

  template <class L, class R>
    requires detail::mixed_error_operands<L, R>
  constexpr auto operator+(L const&, R const&)
  { static_assert(detail::dependent_false<L, R>,
      "bnd: don't mix expected and optional operands in one expression — "
      "convert the expected side with bnd::ok(e) (drops the error cause) "
      "or unwrap explicitly"); }

  template <class L, class R>
    requires detail::mixed_error_operands<L, R>
  constexpr auto operator-(L const&, R const&)
  { static_assert(detail::dependent_false<L, R>,
      "bnd: don't mix expected and optional operands in one expression — "
      "convert the expected side with bnd::ok(e) (drops the error cause) "
      "or unwrap explicitly"); }

  template <class L, class R>
    requires detail::mixed_error_operands<L, R>
  constexpr auto operator*(L const&, R const&)
  { static_assert(detail::dependent_false<L, R>,
      "bnd: don't mix expected and optional operands in one expression — "
      "convert the expected side with bnd::ok(e) (drops the error cause) "
      "or unwrap explicitly"); }

  template <class L, class R>
    requires detail::mixed_error_operands<L, R>
  constexpr auto operator/(L const&, R const&)
  { static_assert(detail::dependent_false<L, R>,
      "bnd: don't mix expected and optional operands in one expression — "
      "convert the expected side with bnd::ok(e) (drops the error cause) "
      "or unwrap explicitly"); }

  //---------------------------------------------------------------------------
  // (Removed) Mixed-mode `bound op rational`
  //
  // These eight overloads used to let a bound be added to / multiplied by a
  // raw `rational`, returning a bare `rational` (unwrapped via `.value()`).
  // They were the main way a consumer got pulled out of bound-space and into
  // the optional-tax of rational arithmetic. They are gone: combine bounds
  // with bounds (wrap a scalar with `just<>` / `_b`), and read an exact value
  // back out with `numerator()` / `denominator()`. `rational` is now an
  // internal representation type, not part of the arithmetic surface.
  //---------------------------------------------------------------------------

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
    static_assert(detail::dependent_false<B>,
      "a bound can only be added to another bound: give the scalar a grid — "
      "write `a + 1_b` (or `a + just<1>` / `a + one`), or `a + bound<{lo,hi}>{n}` "
      "for a runtime value with a known range"); }
  template <raw_scalar A, boundable B> B operator+(A, B const&) {
    static_assert(detail::dependent_false<B>,
      "a scalar can only be added to a bound that is itself a bound: write "
      "`1_b + a` / `just<1> + a` / `one + a`, or `bound<{lo,hi}>{n} + a`"); }

  template <boundable B, raw_scalar A> B operator-(B const&, A) {
    static_assert(detail::dependent_false<B>,
      "subtract a bound, not a raw scalar: write `a - 1_b` / `a - just<1>` / "
      "`a - one`, or `a - bound<{lo,hi}>{n}` for a runtime value with a known range"); }
  template <raw_scalar A, boundable B> B operator-(A, B const&) {
    static_assert(detail::dependent_false<B>,
      "subtract from a bound, not a raw scalar: write `1_b - a` / `just<1> - "
      "a` / `one - a`, or `bound<{lo,hi}>{n} - a`"); }

  template <boundable B, raw_scalar A> B operator*(B const&, A) {
    static_assert(detail::dependent_false<B>,
      "multiply by a bound, not a raw scalar: write `a * 2_b` / `a * just<2>`, "
      "or `a * bound<{lo,hi}>{n}` for a runtime value with a known range"); }
  template <raw_scalar A, boundable B> B operator*(A, B const&) {
    static_assert(detail::dependent_false<B>,
      "multiply a bound by a bound, not a raw scalar: write `2_b * a` / "
      "`just<2> * a`, or `bound<{lo,hi}>{n} * a`"); }

  template <boundable B, raw_scalar A> B operator/(B const&, A) {
    static_assert(detail::dependent_false<B>,
      "divide by a bound, not a raw scalar: write `a / 2_b` / `a / just<2>`, "
      "or `a / bound<{lo,hi}>{n}` for a runtime value with a known range"); }
  template <raw_scalar A, boundable B> B operator/(A, B const&) {
    static_assert(detail::dependent_false<B>,
      "divide a bound by a bound, not a raw scalar: write `6_b / a` / "
      "`just<6> / a`, or `bound<{lo,hi}>{n} / a`"); }

} // namespace bnd

#endif // BNDarithmeticHPP
