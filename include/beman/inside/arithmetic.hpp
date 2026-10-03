//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_ARITHMETIC_HPP
#define BEMAN_INSIDE_ARITHMETIC_HPP

#include <beman/inside/core.hpp>
#include <beman/inside/casts.hpp>
#include <beman/inside/detail/rep.hpp>

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
namespace beman::inside
{
  //---------------------------------------------------------------------------
  // add
  //---------------------------------------------------------------------------
  template <insidable L, insidable R, detail::policy_like P = policy<>, typename A = no_action>
  [[nodiscard]] constexpr auto add(L const& lhs, R const& rhs, P&& policy = {}, A&& action = {})
  { return detail::addition<L,R>::add(lhs, rhs, std::forward<P>(policy), std::forward<A>(action)); }

  // Action-first form: 1+ tagged actions, at least one of which is on_overflow
  // (the only kind arithmetic itself fires; others are kept for forward-compat).
  template <insidable L, insidable R, typename... Actions>
    requires (sizeof...(Actions) >= 1)
          && detail::has_action<detail::IsOverflowActionPred, std::remove_cvref_t<Actions>...>
  [[nodiscard]] constexpr auto add(L const& lhs, R const& rhs, Actions&&... actions)
  { return detail::addition<L,R>::add(lhs, rhs,
      make_policy<detail::merged_implied_flags<Actions...>>(),
      detail::pick_action<detail::IsOverflowActionPred>(actions...)); }

  template <insidable L, insidable R, typename A = no_action>
  [[nodiscard]] constexpr auto add(L const& lhs, R const& rhs,
                                   errc& ec, A&& action = {})
  { return detail::addition<L,R>::add(lhs, rhs, make_policy<checked>(ec),
      std::forward<A>(action)); }

  //---------------------------------------------------------------------------
  // operator+
  //---------------------------------------------------------------------------
  [[nodiscard]] constexpr auto operator+(insidable auto lhs, insidable auto rhs)
  { return add(lhs, rhs); }

  // One overload covers all three optional shapes; the lambda's `l + r` re-enters
  // resolution on the unwrapped values, inheriting whichever bare overload applies.
  template <class L, class R>
    requires (detail::is_slim_optional_v<L> || detail::is_slim_optional_v<R>)
          && (!detail::expected_like<L> && !detail::expected_like<R>)
          && (insidable<detail::unwrap_t<L>> || insidable<detail::unwrap_t<R>>)
          && requires(detail::unwrap_t<L> l, detail::unwrap_t<R> r) { l + r; }
  constexpr auto operator+(L const& lhs, R const& rhs)
  { return lift([](auto const& l, auto const& r){ return l + r; }, lhs, rhs); }

  //---------------------------------------------------------------------------
  // sub
  //---------------------------------------------------------------------------
  template <insidable L, insidable R, detail::policy_like P = policy<>, typename A = no_action>
  [[nodiscard]] constexpr auto sub(L const& lhs, R const& rhs, P&& policy = {}, A&& action = {})
  { return add(lhs, -rhs, std::forward<P>(policy), std::forward<A>(action)); }

  template <insidable L, insidable R, typename... Actions>
    requires (sizeof...(Actions) >= 1)
          && detail::has_action<detail::IsOverflowActionPred, std::remove_cvref_t<Actions>...>
  [[nodiscard]] constexpr auto sub(L const& lhs, R const& rhs, Actions&&... actions)
  { return add(lhs, -rhs,
      make_policy<detail::merged_implied_flags<Actions...>>(),
      detail::pick_action<detail::IsOverflowActionPred>(actions...)); }

  template <insidable L, insidable R, typename A = no_action>
  [[nodiscard]] constexpr auto sub(L const& lhs, R const& rhs,
                                   errc& ec, A&& action = {})
  { return add(lhs, -rhs, make_policy<checked>(ec), std::forward<A>(action)); }

  //---------------------------------------------------------------------------
  // operator-
  //---------------------------------------------------------------------------
  [[nodiscard]] constexpr auto operator-(insidable auto lhs, insidable auto rhs)
  { return sub(lhs, rhs); }

  template <class L, class R>
    requires (detail::is_slim_optional_v<L> || detail::is_slim_optional_v<R>)
          && (!detail::expected_like<L> && !detail::expected_like<R>)
          && (insidable<detail::unwrap_t<L>> || insidable<detail::unwrap_t<R>>)
          && requires(detail::unwrap_t<L> l, detail::unwrap_t<R> r) { l - r; }
  constexpr auto operator-(L const& lhs, R const& rhs)
  { return lift([](auto const& l, auto const& r){ return l - r; }, lhs, rhs); }

  //---------------------------------------------------------------------------
  // mul
  //---------------------------------------------------------------------------
  template <insidable L, insidable R, detail::policy_like P = policy<>, typename A = no_action>
  [[nodiscard]] constexpr auto mul(L const& lhs, R const& rhs, P&& policy = {}, A&& action = {})
  { return detail::multiplication<L,R>::mul(lhs, rhs, std::forward<P>(policy), std::forward<A>(action)); }

  template <insidable L, insidable R, typename... Actions>
    requires (sizeof...(Actions) >= 1)
          && detail::has_action<detail::IsOverflowActionPred, std::remove_cvref_t<Actions>...>
  [[nodiscard]] constexpr auto mul(L const& lhs, R const& rhs, Actions&&... actions)
  { return detail::multiplication<L,R>::mul(lhs, rhs,
      make_policy<detail::merged_implied_flags<Actions...>>(),
      detail::pick_action<detail::IsOverflowActionPred>(actions...)); }

  template <insidable L, insidable R, typename A = no_action>
  [[nodiscard]] constexpr auto mul(L const& lhs, R const& rhs,
                                   errc& ec, A&& action = {})
  { return detail::multiplication<L,R>::mul(lhs, rhs, make_policy<checked>(ec),
      std::forward<A>(action)); }

  //---------------------------------------------------------------------------
  // operator*
  //---------------------------------------------------------------------------
  [[nodiscard]] constexpr auto operator*(insidable auto lhs, insidable auto rhs)
  { return beman::inside::mul(lhs, rhs); }

  template <class L, class R>
    requires (detail::is_slim_optional_v<L> || detail::is_slim_optional_v<R>)
          && (!detail::expected_like<L> && !detail::expected_like<R>)
          && (insidable<detail::unwrap_t<L>> || insidable<detail::unwrap_t<R>>)
          && requires(detail::unwrap_t<L> l, detail::unwrap_t<R> r) { l * r; }
  constexpr auto operator*(L const& lhs, R const& rhs)
  { return lift([](auto const& l, auto const& r){ return l * r; }, lhs, rhs); }

  //---------------------------------------------------------------------------
  // add_all / mul_all — variadic folds (pairwise widening, same as `a + b + c`
  // but reads cleaner; matches Chromium's `CheckAdd(a, b, c)`).
  //---------------------------------------------------------------------------
  template <insidable First, insidable... Rest>
  [[nodiscard]] constexpr auto add_all(First const& first, Rest const&... rest)
  { return (first + ... + rest); }

  template <insidable First, insidable... Rest>
  [[nodiscard]] constexpr auto mul_all(First const& first, Rest const&... rest)
  { return (first * ... * rest); }

  // add_all_into<Target> / mul_all_into<Target> — fold, then collapse the widened
  // intermediate into Target via clamp_cast (widen for exactness, then clip).
  template <insidable Target, insidable First, insidable... Rest>
  [[nodiscard]] constexpr Target add_all_into(First const& first, Rest const&... rest)
  {
    auto sum = (first + ... + rest);
    if constexpr (requires { typename decltype(sum)::value_type; })
      return clamp_cast<Target>(sum.value());
    else
      return clamp_cast<Target>(sum);
  }

  template <insidable Target, insidable First, insidable... Rest>
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
  template <insidable Target, std::ranges::input_range Rng>
    requires insidable<std::remove_cvref_t<std::ranges::range_reference_t<Rng>>>
  [[nodiscard]] constexpr Target sum(Rng&& r)
  {
    using B = std::remove_cvref_t<std::ranges::range_reference_t<Rng>>;
    using beman::inside::detail::rational;
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
  // dot / cross / lerp — 2-D inside-space vector helpers. Each widens its result
  // grid like the underlying `+`/`*`, so no overflow and the result is a plain
  // `inside`. (cross is the z-component, useful for "which side" tests.)
  //---------------------------------------------------------------------------
  [[nodiscard]] constexpr auto dot(insidable auto ax, insidable auto ay,
                                   insidable auto bx, insidable auto by)
  { return ax * bx + ay * by; }

  [[nodiscard]] constexpr auto cross(insidable auto ax, insidable auto ay,
                                     insidable auto bx, insidable auto by)
  { return ax * by - ay * bx; }

  // lerp(a, b, t) = a + (b - a) * t. `t` is itself an inside (typically a
  // [0, 1] fixed-point grid), so the interpolation never leaves inside-space.
  [[nodiscard]] constexpr auto lerp(insidable auto a, insidable auto b,
                                    insidable auto t)
  { return a + (b - a) * t; }

  //---------------------------------------------------------------------------
  // common_inside — the "hull" type able to hold every value of L and R exactly:
  // interval hull + notch gcd (grid `hull`), representation propagated by the
  // same widest-wins rule as arithmetic results (detail::fp_rep). Backs the
  // std::common_type specialisation (numeric_limits.hpp) and mixed-grid
  // min/max below. The primary has no `type` when the hull grid is
  // unrepresentable, so common_type_t SFINAEs away instead of erroring.
  //---------------------------------------------------------------------------
  namespace detail
  {
    template <insidable Lhs, insidable Rhs>
    struct common_inside {};

    // Same type stays itself (policy included) — mirrors std::common_type<T, T>.
    template <insidable Same>
    struct common_inside<Same, Same> { using type = Same; };

    template <insidable Lhs, insidable Rhs>
      requires (!std::same_as<Lhs, Rhs>) && (hull(Grid<Lhs>, Grid<Rhs>).has_value())
    struct common_inside<Lhs, Rhs>
    {
      static constexpr grid hull_grid = *hull(Grid<Lhs>, Grid<Rhs>);
      using type = inside<hull_grid,
                         fp_rep<Lhs, Rhs, hull_grid, /*AllowContinuous=*/true>::result_policy>;
    };
  }

  template <insidable Lhs, insidable Rhs>
  using common_inside_t = typename detail::common_inside<Lhs, Rhs>::type;

  //---------------------------------------------------------------------------
  // std-vocabulary helpers — ADL-found `min` / `max` / `midpoint` for generic
  // code. min/max mirror std; midpoint returns the *exact* average on a refined
  // grid (so, unlike std::midpoint, it neither rounds nor overflows). There is
  // no free `beman::inside::clamp` (the name is the policy flag — use clamp_cast<Target>).
  //---------------------------------------------------------------------------
  template <insidable T>
  [[nodiscard]] constexpr T min(T a, T b) { return (b < a) ? b : a; }

  template <insidable T>
  [[nodiscard]] constexpr T max(T a, T b) { return (a < b) ? b : a; }

  // Mixed-grid forms return the common hull type (both operands convert
  // losslessly — the hull is assignable from each by construction).
  template <insidable Lhs, insidable Rhs> requires (!std::same_as<Lhs, Rhs>)
  [[nodiscard]] constexpr auto min(Lhs a, Rhs b) -> common_inside_t<Lhs, Rhs>
  { common_inside_t<Lhs, Rhs> ca{a}, cb{b}; return (cb < ca) ? cb : ca; }

  template <insidable Lhs, insidable Rhs> requires (!std::same_as<Lhs, Rhs>)
  [[nodiscard]] constexpr auto max(Lhs a, Rhs b) -> common_inside_t<Lhs, Rhs>
  { common_inside_t<Lhs, Rhs> ca{a}, cb{b}; return (ca < cb) ? cb : ca; }

  template <insidable T>
  [[nodiscard]] constexpr auto midpoint(T a, T b) { return (a + b) * just<frac<1, 2>>; }

  //---------------------------------------------------------------------------
  // div
  //---------------------------------------------------------------------------
  template <insidable L, insidable R, policy_flag F = none, typename A = no_action>
  [[nodiscard]] constexpr auto div(L lhs, R rhs, policy<F> pol = {}, A&& action = {})
  { return detail::division<L, R, F>::div(lhs, rhs, pol, std::forward<A>(action)); }

  template <insidable L, insidable R, typename... Actions>
    requires (sizeof...(Actions) >= 1)
          && detail::has_action<detail::IsOverflowActionPred, std::remove_cvref_t<Actions>...>
  [[nodiscard]] constexpr auto div(L lhs, R rhs, Actions&&... actions)
  { return detail::division<L, R, detail::merged_implied_flags<Actions...>>::div(lhs, rhs,
      make_policy<detail::merged_implied_flags<Actions...>>(),
      detail::pick_action<detail::IsOverflowActionPred>(actions...)); }

  template <insidable L, insidable R, typename A = no_action>
  [[nodiscard]] constexpr auto div(L lhs, R rhs,
                                   errc& ec, A&& action = {})
  { return detail::division<L, R, checked>::div(lhs, rhs, make_policy<checked>(ec),
      std::forward<A>(action)); }

  //---------------------------------------------------------------------------
  // operator/
  //---------------------------------------------------------------------------
  [[nodiscard]] constexpr auto operator/(insidable auto lhs, insidable auto rhs)
  {
    constexpr policy_flag F = InsidePolicy<decltype(lhs)> | InsidePolicy<decltype(rhs)>;
    return beman::inside::div(lhs, rhs, make_policy<F>());
  }

  template <class L, class R>
    requires (detail::is_slim_optional_v<L> || detail::is_slim_optional_v<R>)
          && (!detail::expected_like<L> && !detail::expected_like<R>)
          && (insidable<detail::unwrap_t<L>> || insidable<detail::unwrap_t<R>>)
          && requires(detail::unwrap_t<L> l, detail::unwrap_t<R> r) { l / r; }
  constexpr auto operator/(L const& lhs, R const& rhs)
  { return lift([](auto const& l, auto const& r){ return l / r; }, lhs, rhs); }

  //---------------------------------------------------------------------------
  // mod
  //---------------------------------------------------------------------------
  template <insidable L, insidable R, policy_flag F = none, typename A = no_action>
  [[nodiscard]] constexpr auto mod(L lhs, R rhs, policy<F> pol = {}, A&& action = {})
  { return detail::modulo<L, R, F>::mod(lhs, rhs, pol, std::forward<A>(action)); }

  template <insidable L, insidable R, typename... Actions>
    requires (sizeof...(Actions) >= 1)
          && detail::has_action<detail::IsOverflowActionPred, std::remove_cvref_t<Actions>...>
  [[nodiscard]] constexpr auto mod(L lhs, R rhs, Actions&&... actions)
  { return detail::modulo<L, R, detail::merged_implied_flags<Actions...>>::mod(lhs, rhs,
      make_policy<detail::merged_implied_flags<Actions...>>(),
      detail::pick_action<detail::IsOverflowActionPred>(actions...)); }

  template <insidable L, insidable R, typename A = no_action>
  [[nodiscard]] constexpr auto mod(L lhs, R rhs,
                                   errc& ec, A&& action = {})
  { return detail::modulo<L, R, checked>::mod(lhs, rhs, make_policy<checked>(ec),
      std::forward<A>(action)); }

  //---------------------------------------------------------------------------
  // operator%
  //---------------------------------------------------------------------------
  [[nodiscard]] constexpr auto operator%(insidable auto lhs, insidable auto rhs)
  {
    constexpr policy_flag F = InsidePolicy<decltype(lhs)> | InsidePolicy<decltype(rhs)>;
    return beman::inside::mod(lhs, rhs, make_policy<F>());
  }

  template <class L, class R>
    requires (detail::is_slim_optional_v<L> || detail::is_slim_optional_v<R>)
          && (!detail::expected_like<L> && !detail::expected_like<R>)
          && (insidable<detail::unwrap_t<L>> || insidable<detail::unwrap_t<R>>)
          && requires(detail::unwrap_t<L> l, detail::unwrap_t<R> r) { l % r; }
  constexpr auto operator%(L const& lhs, R const& rhs)
  { return lift([](auto const& l, auto const& r){ return l % r; }, lhs, rhs); }

  //---------------------------------------------------------------------------
  // expected-lift operators — bridge beman::inside::math's expected results into chains, so
  // `math::tan(x) * gain + offset` stays an expected end to end (first error
  // short-circuits). An underlying nullopt maps to the operator's documented
  // cause: overflow for + − ×, division_by_zero for /. To drop the cause and
  // enter the optional world instead, convert with `beman::inside::ok(e)` (see lift.hpp).
  //---------------------------------------------------------------------------
  namespace detail
  {
    template <class L, class R>
    concept expected_operands =
        (expected_like<L> || expected_like<R>)
        && !is_slim_optional_v<L> && !is_slim_optional_v<R>
        && (insidable<expected_value_t<L>> || insidable<expected_value_t<R>>);

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
      "inside: don't mix expected and optional operands in one expression — "
      "convert the expected side with beman::inside::ok(e) (drops the error cause) "
      "or unwrap explicitly"); }

  template <class L, class R>
    requires detail::mixed_error_operands<L, R>
  constexpr auto operator-(L const&, R const&)
  { static_assert(detail::dependent_false<L, R>,
      "inside: don't mix expected and optional operands in one expression — "
      "convert the expected side with beman::inside::ok(e) (drops the error cause) "
      "or unwrap explicitly"); }

  template <class L, class R>
    requires detail::mixed_error_operands<L, R>
  constexpr auto operator*(L const&, R const&)
  { static_assert(detail::dependent_false<L, R>,
      "inside: don't mix expected and optional operands in one expression — "
      "convert the expected side with beman::inside::ok(e) (drops the error cause) "
      "or unwrap explicitly"); }

  template <class L, class R>
    requires detail::mixed_error_operands<L, R>
  constexpr auto operator/(L const&, R const&)
  { static_assert(detail::dependent_false<L, R>,
      "inside: don't mix expected and optional operands in one expression — "
      "convert the expected side with beman::inside::ok(e) (drops the error cause) "
      "or unwrap explicitly"); }

  template <class L, class R>
    requires detail::mixed_error_operands<L, R>
  constexpr auto operator%(L const&, R const&)
  { static_assert(detail::dependent_false<L, R>,
      "inside: don't mix expected and optional operands in one expression — "
      "convert the expected side with beman::inside::ok(e) (drops the error cause) "
      "or unwrap explicitly"); }

  //---------------------------------------------------------------------------
  // Grid-less scalar operands are rejected. A raw int/double carries no grid, so
  // `inside op rawscalar` has no type-safe result; rather than silently escape
  // into rational/double, these guidance overloads make it ill-formed with a fix
  // (give the literal a grid: `1_ins` / `just<1>`, or an inside over its range).
  // Comparisons and compound assignment with raw scalars are unaffected.
  //
  // Concrete (non-auto) return type on purpose: keeps these SFINAE-transparent,
  // so `requires { b + 1; }` stays well-formed and the static_assert fires only
  // on a real call.
  //---------------------------------------------------------------------------
  template <typename A> concept raw_scalar = std::integral<A> || std::floating_point<A>;

  template <insidable B, raw_scalar A> B operator+(B const&, A) {
    static_assert(detail::dependent_false<B>,
      "an inside can only be added to another inside: give the scalar a grid — "
      "write `a + 1_ins` (or `a + just<1>` / `a + one`), or `a + inside<{lo,hi}>{n}` "
      "for a runtime value with a known range"); }
  template <raw_scalar A, insidable B> B operator+(A, B const&) {
    static_assert(detail::dependent_false<B>,
      "a scalar can only be added to an inside that is itself an inside: write "
      "`1_ins + a` / `just<1> + a` / `one + a`, or `inside<{lo,hi}>{n} + a`"); }

  template <insidable B, raw_scalar A> B operator-(B const&, A) {
    static_assert(detail::dependent_false<B>,
      "subtract an inside, not a raw scalar: write `a - 1_ins` / `a - just<1>` / "
      "`a - one`, or `a - inside<{lo,hi}>{n}` for a runtime value with a known range"); }
  template <raw_scalar A, insidable B> B operator-(A, B const&) {
    static_assert(detail::dependent_false<B>,
      "subtract from an inside, not a raw scalar: write `1_ins - a` / `just<1> - "
      "a` / `one - a`, or `inside<{lo,hi}>{n} - a`"); }

  template <insidable B, raw_scalar A> B operator*(B const&, A) {
    static_assert(detail::dependent_false<B>,
      "multiply by an inside, not a raw scalar: write `a * 2_ins` / `a * just<2>`, "
      "or `a * inside<{lo,hi}>{n}` for a runtime value with a known range"); }
  template <raw_scalar A, insidable B> B operator*(A, B const&) {
    static_assert(detail::dependent_false<B>,
      "multiply an inside by an inside, not a raw scalar: write `2_ins * a` / "
      "`just<2> * a`, or `inside<{lo,hi}>{n} * a`"); }

  template <insidable B, raw_scalar A> B operator/(B const&, A) {
    static_assert(detail::dependent_false<B>,
      "divide by an inside, not a raw scalar: write `a / 2_ins` / `a / just<2>`, "
      "or `a / inside<{lo,hi}>{n}` for a runtime value with a known range"); }
  template <raw_scalar A, insidable B> B operator/(A, B const&) {
    static_assert(detail::dependent_false<B>,
      "divide an inside by an inside, not a raw scalar: write `6_ins / a` / "
      "`just<6> / a`, or `inside<{lo,hi}>{n} / a`"); }

  //---------------------------------------------------------------------------
  // Compound assignment with a raw scalar is ill-formed for the same reason —
  // an inside is mutated by another inside (or a rational), never a bare number.
  // These guidance overloads turn `b += 1` into a readable diagnostic instead
  // of a generic "no viable operator+=". Same SFINAE-transparent shape as the
  // binary operators above.
  //---------------------------------------------------------------------------
  template <insidable B, raw_scalar A> B& operator+=(B&, A) {
    static_assert(detail::dependent_false<B>,
      "add an inside, not a raw scalar: write `b += 1_ins` / `b += just<1>`, or "
      "`b += inside<{lo,hi}>{n}` for a runtime value with a known range"); }
  template <insidable B, raw_scalar A> B& operator-=(B&, A) {
    static_assert(detail::dependent_false<B>,
      "subtract an inside, not a raw scalar: write `b -= 1_ins` / `b -= just<1>`, "
      "or `b -= inside<{lo,hi}>{n}` for a runtime value with a known range"); }
  template <insidable B, raw_scalar A> B& operator*=(B&, A) {
    static_assert(detail::dependent_false<B>,
      "multiply by an inside, not a raw scalar: write `b *= 2_ins` / `b *= just<2>`, "
      "or `b *= inside<{lo,hi}>{n}` for a runtime value with a known range"); }
  template <insidable B, raw_scalar A> B& operator/=(B&, A) {
    static_assert(detail::dependent_false<B>,
      "divide by an inside, not a raw scalar: write `b /= 2_ins` / `b /= just<2>`, "
      "or `b /= inside<{lo,hi}>{n}` for a runtime value with a known range"); }
  template <insidable B, raw_scalar A> B& operator%=(B&, A) {
    static_assert(detail::dependent_false<B>,
      "take the modulus by an inside, not a raw scalar: write `b %= 2_ins` / "
      "`b %= just<2>`, or `b %= inside<{lo,hi}>{n}` for a runtime value"); }

} // namespace beman::inside

#endif // BEMAN_INSIDE_ARITHMETIC_HPP
