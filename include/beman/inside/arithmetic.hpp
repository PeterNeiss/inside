// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
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
// std::expected operator overloads (an error operand propagates through).
//---------------------------------------------------------------------------
namespace beman::inside
{
  //---------------------------------------------------------------------------
  // add / sub / mul / div / mod — each takes one of three trailing forms:
  //   op(l, r [, policy [, action]])   explicit per-call policy (+ action)
  //   op(l, r, on_overflow(λ), ...)    tagged actions (policy = their implied flags)
  //   op(l, r, ec [, action])          error-code form (checked, reports into ec)
  // detail::arith normalises the form to (policy, action) for the op's core.
  //---------------------------------------------------------------------------
  namespace detail
  {
    // Arithmetic fires only on_overflow; any other action would be silently
    // ignored, so it is rejected.
    template <class A>
    inline constexpr bool arith_action =
        std::same_as<std::remove_cvref_t<A>, no_action> || overflow_action<std::remove_cvref_t<A>>;

    template <class Op, class L, class R, policy_like P = policy<>, class A = no_action>
    constexpr auto arith(Op op, L const& l, R const& r, P&& pol = {}, A&& act = {})
    {
      static_assert(arith_action<A>,
        "add/sub/mul/div/mod fire only on_overflow; on_clamp / on_wrap / on_error "
        "apply to assignment — use them with b.with(...) or a cast");
      return op(l, r, std::forward<P>(pol), std::forward<A>(act));
    }

    // Action-first form: on_overflow actions (the only kind arithmetic fires;
    // any other tag is rejected below with a message, not a bare mismatch).
    template <class A>
    inline constexpr bool is_action_tag =
        is_overflow_action<A>::value || is_clamp_action<A>::value
        || is_wrap_action<A>::value || is_error_action<A>::value;

    template <class Op, class L, class R, class... Actions>
      requires (sizeof...(Actions) >= 1)
            && (is_action_tag<std::remove_cvref_t<Actions>> && ...)
    constexpr auto arith(Op op, L const& l, R const& r, Actions&&... acts)
    {
      static_assert((overflow_action<std::remove_cvref_t<Actions>> && ...),
        "add/sub/mul/div/mod fire only on_overflow; on_clamp / on_wrap / on_error "
        "apply to assignment — use them with b.with(...) or a cast");
      return op(l, r, make_policy<merged_implied_flags<Actions...>>(),
                pick_action<is_overflow_action>(acts...));
    }

    template <class Op, class L, class R, class A = no_action>
    constexpr auto arith(Op op, L const& l, R const& r, errc& ec, A&& act = {})
    {
      static_assert(arith_action<A>,
        "add/sub/mul/div/mod fire only on_overflow; on_clamp / on_wrap / on_error "
        "apply to assignment — use them with b.with(...) or a cast");
      return op(l, r, make_policy<checked>(ec), std::forward<A>(act));
    }

    template <class P> inline constexpr policy_flag flags_of = policy_flags_of<std::remove_cvref_t<P>>;

    struct add_op
    {
      template <class L, class R, class P, class A>
      constexpr auto operator()(L const& l, R const& r, P&& p, A&& a) const
      { return addition<L, R>::add(l, r, std::forward<P>(p), std::forward<A>(a)); }
    };
    struct sub_op
    {
      template <class L, class R, class P, class A>
      constexpr auto operator()(L const& l, R const& r, P&& p, A&& a) const
      { return add_op{}(l, -r, std::forward<P>(p), std::forward<A>(a)); }
    };
    struct mul_op
    {
      template <class L, class R, class P, class A>
      constexpr auto operator()(L const& l, R const& r, P&& p, A&& a) const
      { return multiplication<L, R>::mul(l, r, std::forward<P>(p), std::forward<A>(a)); }
    };
    struct div_op
    {
      template <class L, class R, class P, class A>
      constexpr auto operator()(L const& l, R const& r, P&& p, A&& a) const
      { return division<L, R, flags_of<P>>::div(l, r, p, std::forward<A>(a)); }
    };
    struct mod_op
    {
      template <class L, class R, class P, class A>
      constexpr auto operator()(L const& l, R const& r, P&& p, A&& a) const
      { return modulo<L, R, flags_of<P>>::mod(l, r, p, std::forward<A>(a)); }
    };
  }

#define BEMAN_INSIDE_ARITH_FN(name, op)                                              \
  template <insidable L, insidable R, class... Args>                                 \
    requires requires(L const& l, R const& r, Args&&... args)                        \
      { detail::arith(detail::op{}, l, r, std::forward<Args>(args)...); }            \
  [[nodiscard]] constexpr auto name(L const& lhs, R const& rhs, Args&&... args)      \
  { return detail::arith(detail::op{}, lhs, rhs, std::forward<Args>(args)...); }

  BEMAN_INSIDE_ARITH_FN(add, add_op)
  BEMAN_INSIDE_ARITH_FN(sub, sub_op)
  BEMAN_INSIDE_ARITH_FN(mul, mul_op)
  BEMAN_INSIDE_ARITH_FN(div, div_op)
  BEMAN_INSIDE_ARITH_FN(mod, mod_op)
#undef BEMAN_INSIDE_ARITH_FN

  // Binary operators: +, -, * use the default policy; / and % carry the
  // operands' own policies (snap/rounding select the native integer paths).
  [[nodiscard]] constexpr auto operator+(insidable auto lhs, insidable auto rhs)
  { return beman::inside::add(lhs, rhs); }

  [[nodiscard]] constexpr auto operator-(insidable auto lhs, insidable auto rhs)
  { return beman::inside::sub(lhs, rhs); }

  [[nodiscard]] constexpr auto operator*(insidable auto lhs, insidable auto rhs)
  { return beman::inside::mul(lhs, rhs); }

  [[nodiscard]] constexpr auto operator/(insidable auto lhs, insidable auto rhs)
  { return beman::inside::div(lhs, rhs, make_policy<policy_of<decltype(lhs)> | policy_of<decltype(rhs)>>()); }

  [[nodiscard]] constexpr auto operator%(insidable auto lhs, insidable auto rhs)
  { return beman::inside::mod(lhs, rhs, make_policy<policy_of<decltype(lhs)> | policy_of<decltype(rhs)>>()); }

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

  //---------------------------------------------------------------------------
  // sum<Target> — bulk reduction with ONE deferred range check. Per-element
  // `target += b` re-validates every step (blocks vectorization); this
  // accumulates raws in imax and applies Target's policy once to the total
  // (semantic difference: the *total* is validated, not every prefix). Fast
  // path: ≤32-bit integer raws, flushed to a rational every 2^30 elements so the
  // accumulator can't overflow; wider/rational/f64 take the per-element fold.
  //---------------------------------------------------------------------------
  template <insidable Target, std::ranges::input_range Rng>
    requires insidable<std::remove_cvref_t<std::ranges::range_reference_t<Rng>>>
  [[nodiscard]] constexpr Target sum(Rng&& r)
  {
    using B = std::remove_cvref_t<std::ranges::range_reference_t<Rng>>;
    using detail::rational;
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
            return ((rational{acc} * notch_of<B>).value()
                    + (rational{cnt} * lower_of<B>).value()).value();
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
      requires (!std::same_as<Lhs, Rhs>) && (hull(grid_of<Lhs>, grid_of<Rhs>).has_value())
    struct common_inside<Lhs, Rhs>
    {
      static constexpr grid hull_grid = *hull(grid_of<Lhs>, grid_of<Rhs>);
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

  // Mixed grids: the exact average on the refined sum grid, like the same-type form.
  template <insidable Lhs, insidable Rhs> requires (!std::same_as<Lhs, Rhs>)
  [[nodiscard]] constexpr auto midpoint(Lhs a, Rhs b) { return (a + b) * just<frac<1, 2>>; }

  //---------------------------------------------------------------------------
  // expected-lift operators — fallible results (division, modulo, checked
  // rational arithmetic, beman::inside::math) chain without per-step unwrapping:
  // `a / b * gain + offset` and `math::tan(x) * gain` stay a
  // std::expected<inside, errc> end to end. The first error short-circuits and
  // keeps its cause; an operation that fails inside the chain reports its own
  // (overflow, division_by_zero, ...). One overload per operator covers all
  // three shapes (expected op inside, inside op expected, expected op expected);
  // the lambda's `l + r` re-enters resolution on the unwrapped values,
  // inheriting whichever bare overload applies.
  //---------------------------------------------------------------------------
  namespace detail
  {
    template <class L, class R>
    concept expected_operands =
        (is_expected_v<L> || is_expected_v<R>)
        && (insidable<unwrap_t<L>> || insidable<unwrap_t<R>>);
  }

#define BEMAN_INSIDE_LIFT_OP(op)                                                     \
  template <class L, class R>                                                        \
    requires detail::expected_operands<L, R>                                         \
          && requires(detail::unwrap_t<L> l, detail::unwrap_t<R> r) { l op r; }      \
  [[nodiscard]] constexpr auto operator op(L const& lhs, R const& rhs)               \
  { return detail::lift([](auto const& l, auto const& r) { return l op r; }, lhs, rhs); }

  BEMAN_INSIDE_LIFT_OP(+)
  BEMAN_INSIDE_LIFT_OP(-)
  BEMAN_INSIDE_LIFT_OP(*)
  BEMAN_INSIDE_LIFT_OP(/)
  BEMAN_INSIDE_LIFT_OP(%)
#undef BEMAN_INSIDE_LIFT_OP

  //---------------------------------------------------------------------------
  // Grid-less scalar operands are rejected. A raw int/double carries no grid, so
  // `inside op rawscalar` has no type-safe result; rather than silently escape
  // into rational/double, these guidance overloads make it ill-formed with a fix
  // (give the literal a grid: `1_ins` / `just<1>`, or an inside over its range).
  // Compound assignment with a raw scalar is rejected the same way; comparisons
  // with raw scalars are unaffected.
  //
  // Concrete (non-auto) return type on purpose: keeps these SFINAE-transparent,
  // so `requires { b + 1; }` stays well-formed and the static_assert fires only
  // on a f64 call.
  //---------------------------------------------------------------------------
  template <typename A> concept raw_scalar = std::integral<A> || std::floating_point<A>;

#define BEMAN_INSIDE_SCALAR_MSG                                                      \
  "an inside cannot be combined with a raw scalar: give the scalar a grid — "        \
  "`1_ins`, `just<1>`, `one`, or `inside<{lo,hi}>{n}` for a runtime value with a "   \
  "known range"
#define BEMAN_INSIDE_NO_SCALAR(op)                                                   \
  template <insidable B, raw_scalar A> B operator op(B const&, A)                    \
  { static_assert(detail::dependent_false<B>, BEMAN_INSIDE_SCALAR_MSG); }            \
  template <raw_scalar A, insidable B> B operator op(A, B const&)                    \
  { static_assert(detail::dependent_false<B>, BEMAN_INSIDE_SCALAR_MSG); }            \
  template <insidable B, raw_scalar A> B& operator op##=(B&, A)                      \
  { static_assert(detail::dependent_false<B>, BEMAN_INSIDE_SCALAR_MSG); }

  BEMAN_INSIDE_NO_SCALAR(+)
  BEMAN_INSIDE_NO_SCALAR(-)
  BEMAN_INSIDE_NO_SCALAR(*)
  BEMAN_INSIDE_NO_SCALAR(/)
  BEMAN_INSIDE_NO_SCALAR(%)
#undef BEMAN_INSIDE_NO_SCALAR
#undef BEMAN_INSIDE_SCALAR_MSG

} // namespace beman::inside

#endif // BEMAN_INSIDE_ARITHMETIC_HPP
