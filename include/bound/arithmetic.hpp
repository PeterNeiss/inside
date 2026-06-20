//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDarithmeticHPP
#define BNDarithmeticHPP

#include "bound/core.hpp"
#include "bound/casts.hpp"

#include <algorithm>
#include <ranges>

//---------------------------------------------------------------------------
// Free-function arithmetic — wraps detail::addition/multiplication/division/
// modulo with caller-friendly overloads:
//   add(l, r) / add(l, r, policy<F>{}) / add(l, r, on_overflow(λ)) /
//   add(l, r, ec) / l + r
// Plus the variadic folds add_all/mul_all and *_into<Target>, and the
// slim::optional operator overloads (a nullopt operand propagates through).
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
                                   errc& ec, A&& action = {})
  { return detail::addition<L,R>::add(lhs, rhs, make_policy<checked>(ec),
      std::forward<A>(action)); }

  //---------------------------------------------------------------------------
  // operator+
  //---------------------------------------------------------------------------
  [[nodiscard]] constexpr auto operator+(boundable auto lhs, boundable auto rhs)
  { return add(lhs, rhs); }

  // One overload covers all three optional shapes; the lambda's `l + r` re-enters
  // resolution on the unwrapped values, inheriting whichever bare overload applies.
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
                                   errc& ec, A&& action = {})
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
                                   errc& ec, A&& action = {})
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
  // add_all / mul_all — variadic folds (pairwise widening, same as `a + b + c`
  // but reads cleaner; matches Chromium's `CheckAdd(a, b, c)`).
  //---------------------------------------------------------------------------
  template <boundable First, boundable... Rest>
  [[nodiscard]] constexpr auto add_all(First const& first, Rest const&... rest)
  { return (first + ... + rest); }

  template <boundable First, boundable... Rest>
  [[nodiscard]] constexpr auto mul_all(First const& first, Rest const&... rest)
  { return (first * ... * rest); }

  // add_all_into<Target> / mul_all_into<Target> — fold, then collapse the widened
  // intermediate into Target via clamp_cast (widen for exactness, then clip).
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
  // sum<Target> — bulk reduction with ONE deferred range check. Per-element
  // `target += b` re-validates every step (blocks vectorization); this
  // accumulates raws in imax and applies Target's policy once to the total
  // (semantic difference: the *total* is validated, not every prefix). Fast
  // path: ≤32-bit integer raws, flushed to a rational every 2^30 elements so the
  // accumulator can't overflow; wider/rational/real take the per-element fold.
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
        // value storage: raw IS the value. index: Σvalue = cnt·Lower + Σraw·Notch.
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
        // Branch-free inner block (≤ 2^30 elements keeps the imax accumulator
        // overflow-free) — the loop that vectorizes.
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
  // dot / cross / lerp — 2-D bound-space vector helpers. Each widens its result
  // grid like the underlying `+`/`*`, so no overflow and the result is a plain
  // `bound`. (cross is the z-component, useful for "which side" tests.)
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
  // std-vocabulary helpers — ADL-found `min` / `max` / `midpoint` for generic
  // code. min/max mirror std; midpoint returns the *exact* average on a refined
  // grid (so, unlike std::midpoint, it neither rounds nor overflows). There is
  // no free `bnd::clamp` (the name is the policy flag — use clamp_cast<Target>).
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
                                   errc& ec, A&& action = {})
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
                                   errc& ec, A&& action = {})
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

  template <class L, class R>
    requires (detail::is_slim_optional_v<L> || detail::is_slim_optional_v<R>)
          && (!detail::expected_like<L> && !detail::expected_like<R>)
          && (boundable<detail::unwrap_t<L>> || boundable<detail::unwrap_t<R>>)
          && requires(detail::unwrap_t<L> l, detail::unwrap_t<R> r) { l % r; }
  constexpr auto operator%(L const& lhs, R const& rhs)
  { return lift([](auto const& l, auto const& r){ return l % r; }, lhs, rhs); }

  //---------------------------------------------------------------------------
  // expected-lift operators — bridge bnd::math's expected results into chains, so
  // `math::tan(x) * gain + offset` stays an expected end to end (first error
  // short-circuits). An underlying nullopt maps to the operator's documented
  // cause: overflow for + − ×, division_by_zero for /. To drop the cause and
  // enter the optional world instead, convert with `bnd::ok(e)` (see lift.hpp).
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
    requires detail::expected_operands<L, R>
          && requires(detail::expected_value_t<L> l, detail::expected_value_t<R> r) { l % r; }
  constexpr auto operator%(L const& lhs, R const& rhs)
  { return lift_expected([](auto const& l, auto const& r){ return l % r; },
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

  template <class L, class R>
    requires detail::mixed_error_operands<L, R>
  constexpr auto operator%(L const&, R const&)
  { static_assert(detail::dependent_false<L, R>,
      "bnd: don't mix expected and optional operands in one expression — "
      "convert the expected side with bnd::ok(e) (drops the error cause) "
      "or unwrap explicitly"); }

  //---------------------------------------------------------------------------
  // Grid-less scalar operands are rejected. A raw int/double carries no grid, so
  // `bound op rawscalar` has no type-safe result; rather than silently escape
  // into rational/double, these guidance overloads make it ill-formed with a fix
  // (give the literal a grid: `1_b` / `just<1>`, or a bound over its range).
  // Comparisons and compound assignment with raw scalars are unaffected.
  //
  // Concrete (non-auto) return type on purpose: keeps these SFINAE-transparent,
  // so `requires { b + 1; }` stays well-formed and the static_assert fires only
  // on a real call.
  //---------------------------------------------------------------------------
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

  //---------------------------------------------------------------------------
  // Compound assignment with a raw scalar is ill-formed for the same reason —
  // a bound is mutated by another bound (or a rational), never a bare number.
  // These guidance overloads turn `b += 1` into a readable diagnostic instead
  // of a generic "no viable operator+=". Same SFINAE-transparent shape as the
  // binary operators above.
  //---------------------------------------------------------------------------
  template <boundable B, raw_scalar A> B& operator+=(B&, A) {
    static_assert(detail::dependent_false<B>,
      "add a bound, not a raw scalar: write `b += 1_b` / `b += just<1>`, or "
      "`b += bound<{lo,hi}>{n}` for a runtime value with a known range"); }
  template <boundable B, raw_scalar A> B& operator-=(B&, A) {
    static_assert(detail::dependent_false<B>,
      "subtract a bound, not a raw scalar: write `b -= 1_b` / `b -= just<1>`, "
      "or `b -= bound<{lo,hi}>{n}` for a runtime value with a known range"); }
  template <boundable B, raw_scalar A> B& operator*=(B&, A) {
    static_assert(detail::dependent_false<B>,
      "multiply by a bound, not a raw scalar: write `b *= 2_b` / `b *= just<2>`, "
      "or `b *= bound<{lo,hi}>{n}` for a runtime value with a known range"); }
  template <boundable B, raw_scalar A> B& operator/=(B&, A) {
    static_assert(detail::dependent_false<B>,
      "divide by a bound, not a raw scalar: write `b /= 2_b` / `b /= just<2>`, "
      "or `b /= bound<{lo,hi}>{n}` for a runtime value with a known range"); }
  template <boundable B, raw_scalar A> B& operator%=(B&, A) {
    static_assert(detail::dependent_false<B>,
      "take the modulus by a bound, not a raw scalar: write `b %= 2_b` / "
      "`b %= just<2>`, or `b %= bound<{lo,hi}>{n}` for a runtime value"); }

} // namespace bnd

#endif // BNDarithmeticHPP
