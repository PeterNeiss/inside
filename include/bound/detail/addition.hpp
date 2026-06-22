//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDadditionHPP
#define BNDadditionHPP

#include "bound/generic.hpp"
#include "bound/grid.hpp"
#include "bound/policy.hpp"

//---------------------------------------------------------------------------
// addition — `add(L, R, policy, action) -> bound<G>`, G = Grid<L> + Grid<R>.
// The grid arithmetic is sound by construction (the result interval contains
// every runtime sum), so overflow can only happen on rational-raw results.
// Specialises on the storage shapes: rational result, mixed rational/integer,
// direct integer-space add, or both notch-offset (scale via lhs/rhs_widen).
//---------------------------------------------------------------------------
namespace bnd::detail
{
  template <boundable L, boundable R = L>
  struct addition
  {
    static_assert((Grid<L> + Grid<R>).has_value(),
      "addition: result grid's notch/interval exceeds the representable rational "
      "range — coarsen the operand grids");
    static constexpr grid result_grid = (Grid<L> + Grid<R>).value();
    // Propagate fp storage only when the result grid stays exactly representable
    // in the chosen width; otherwise demote (f32→f64) or drop it so storage_pick
    // deduces an exact representation (the fp sum would diverge from the exact sum
    // — see grid::double_exact / float_exact). Widest-wins: prefer f32 only when
    // both operands are f32-only and the result fits float; an f64 operand or a
    // too-fine-for-float result widens to f64; too fine for double → exact.
    static constexpr bool any_f64 =
        (BoundPolicy<L> & bnd::real) == bnd::real || (BoundPolicy<R> & bnd::real) == bnd::real;
    static constexpr bool any_f32 =
        (BoundPolicy<L> & bnd::f32) == bnd::f32 || (BoundPolicy<R> & bnd::f32) == bnd::f32;
    static constexpr bool keep_f32 = any_f32 && !any_f64 && float_exact<result_grid>;
    static constexpr bool keep_f64 = !keep_f32 && (any_f64 || any_f32) && double_exact<result_grid>;
    // Carry both operands' representation flags (widest-wins at storage selection).
    static constexpr policy_flag rep =
        ((BoundPolicy<L> | BoundPolicy<R>) & (bnd::exact | bnd::direct | bnd::indexed))
        | (keep_f64 ? bnd::real : none) | (keep_f32 ? bnd::f32 : none);
    using result = bound<result_grid, rep != none ? rep : checked>;

    template <policy_flag F>
    static constexpr bool needs_overflow_check =
        rational_raw<result>
        && ((F | BoundPolicy<L> | BoundPolicy<R>) & checked)
        && !rational_add_is_safe(Grid<L>, Grid<R>);

    template <policy_flag F = none>
    using return_type_for = std::conditional_t<needs_overflow_check<F>,
                                               slim::optional<result>,
                                               result>;

    template <policy_flag F, typename A>
    using add_return_t = std::conditional_t<overflow_action<plain<A>>,
                                            result,
                                            return_type_for<F>>;

    // Result notch is gcd(NL, NR); scale each raw up to it before adding —
    // lhs_widen = NL/Nresult, rhs_widen = NR/Nresult (exact, Nresult divides both).
    // Guard the continuous-grid case (Notch<result> == 0): the rational divide-by-zero
    // path returns nullopt on GCC/Clang but MSVC's constexpr evaluator rejects it
    // (C2131). widen is unused on the continuous/rational result path, so 1 is fine.
    static constexpr imax lhs_widen = (Notch<result> == 0) ? imax{1}
        : (Notch<L> / Notch<result>).value_or(rational{1}).Numerator;
    static constexpr imax rhs_widen = (Notch<result> == 0) ? imax{1}
        : (Notch<R> / Notch<result>).value_or(rational{1}).Numerator;

    // Defined inline (not out-of-line): MSVC mishandles out-of-line member
    // templates of constrained partial specializations.
    template <policy_flag F = none, typename E = empty_ref, typename A = no_action>
    static constexpr auto add(L lhs, R rhs, policy<F, E> policy = {}, A&& action = {}) -> add_return_t<F, A>
  {
    result res;
    if constexpr (fp_raw<result>)
    {
      res = result::from_raw(raw_cast<result>(Grid<result>.snap_double(as_double(lhs) + as_double(rhs))));
    }
    else if constexpr (rational_raw<result>)
    {
      if constexpr (needs_overflow_check<F>)
      {
        auto sum = rational::add(lhs,rhs);
        if (!sum) [[unlikely]]
          return report_or_nullopt<result>(action, policy, errc::overflow,
                                           "rational overflow in add");
        res = result::from_raw(*sum);
      }
      else
        res = result::from_raw(rational::add_unchecked(lhs, rhs));
    }
    else if constexpr (rational_raw<L> || rational_raw<R>
                       || !((IsIntegerAligned<L> && IsIntegerAligned<R>)
                            || (index_raw<L> && index_raw<R>)))
    {
      // Rational store: a rational-raw operand, or a direct integer bound mixed
      // with a fractional notch-offset one (where neither to_value nor offset-widen
      // is exact — to_value would truncate the fractional operand). Compute the
      // exact rational sum and convert to result's raw via raw_from_offset.
      auto sum = rational::add_unchecked(lhs,rhs);
      res = result::from_raw(raw_from_offset<result>(
          ((sum - Lower<result>) / Notch<result>).value().Numerator));
    }
    else if constexpr (IsIntegerAligned<L> && IsIntegerAligned<R>)
    {
      // Both operands are integer-valued (Notch and Lower integers), so the
      // value-space add is exact.
      from_value(res, to_value(lhs) + to_value(rhs));
    }
    else
    {
      // Both notch-offset: scale each raw to the result notch and add in offset
      // space (offsets compose because result Lower = Lower<L> + Lower<R>).
      res = result::from_raw(raw_cast<result>(raw_imax(lhs) * lhs_widen + raw_imax(rhs) * rhs_widen));
    }
    return res;
  }
  };
} // namespace bnd::detail

#endif // BNDadditionHPP
