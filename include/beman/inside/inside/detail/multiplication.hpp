// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_INSIDE_DETAIL_MULTIPLICATION_HPP
#define BEMAN_INSIDE_INSIDE_DETAIL_MULTIPLICATION_HPP

#include <beman/inside/inside/detail/config.hpp>

#include <beman/inside/inside/detail/op_result.hpp>
#include <beman/inside/inside/generic.hpp>
#include <beman/inside/inside/detail/wide_value.hpp>
#include <beman/inside/inside/grid.hpp>
#include <beman/inside/inside/detail/call_policy.hpp>

//---------------------------------------------------------------------------
// multiplication — `mul(L, R, policy, action) -> inside<grid_of<L> * grid_of<R>>`.
// Integer raws multiply their value indices (wide_value.hpp), in imax when the
// grids' bounds allow, else by wrapping arithmetic as wide as the result raw.
// Point scaling and rational results have their own branches.
//---------------------------------------------------------------------------
namespace beman::inside::detail {
template <insidable L, insidable R = L>
struct multiplication {
    static constexpr grid result_grid = grid_of<L> * grid_of<R>;
    // A result is checked, whatever its operands' policies.
    using result = inside<result_grid>;

    // Total, as a sum: the policy has nothing to decide.
    template <policy_flag F = none, typename E = empty_ref>
    static constexpr result mul(L lhs, R rhs, call_policy<F, E> = {}) {
        if constexpr (point_scale<R, L, result>) // x * just<c>: x's offset (see op_result.hpp)
            return scale_by_point<result, R>(lhs);
        else if constexpr (point_scale<L, R, result>)
            return scale_by_point<result, L>(rhs);
        else if constexpr (fraction_storage<result>)
            return result::from_raw(*continuous_raw_of<raw_t<result>>(exact_of(lhs) * exact_of(rhs)));
        else if constexpr (exact_path_fits<exact_op::mul, result, L, R>)
            return result::from_raw(*try_rational(exact_of(lhs) * exact_of(rhs)));
        else if constexpr (rational_storage<result>)
            return result::from_raw(rational::mul_unchecked(as_rational(lhs), as_rational(rhs)));
        else if constexpr (point_storage<result>) // point × point, or × 0: the grid's one value
            return result::from_raw(raw_t<result>{});
        else {
            // Integer raws (a notched result has no continuous operand): multiply the operands' values in their own
            // units (a notch or value unit, or |c| for a point c), in imax or by wrapping arithmetic (wide_value.hpp).
            // The product of the unit counts counts the product in the product of the units — on anchored grids the
            // result notch — exact for every grid and sign, at any width.
            static_assert(integer_storage<L> && integer_storage<R>);
            constexpr grid_rational U = unit_of<L> * unit_of<R>;
            using W                   = index_work_t<result, L, unit_of<L>, R, unit_of<R>, U>;
            return from_value_in_units<result, U>(value_in_units<W, unit_of<L>>(lhs) *
                                                  value_in_units<W, unit_of<R>>(rhs));
        }
    }
};

// Whether `l * r` has a result, as a constraint of mul / * / *=: a result
// grid with a bound, and no wide-index operand with a continuous result.
template <insidable L, insidable R, policy_flag = none>
inline constexpr bool mul_defined = [] {
    using result = typename multiplication<L, R>::result;
    if constexpr (!bounded_grid(grid_of<result>))
        return false;
    else
        return point_scale<R, L, result> || point_scale<L, R, result> || !rational_storage<result> ||
               (!wide_valued<L> && !wide_valued<R>);
}();
} // namespace beman::inside::detail

#endif // BEMAN_INSIDE_INSIDE_DETAIL_MULTIPLICATION_HPP
