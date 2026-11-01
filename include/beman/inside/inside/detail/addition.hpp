// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_INSIDE_DETAIL_ADDITION_HPP
#define BEMAN_INSIDE_INSIDE_DETAIL_ADDITION_HPP

#include <beman/inside/inside/detail/config.hpp>

#include <beman/inside/inside/detail/op_result.hpp>
#include <beman/inside/inside/generic.hpp>
#include <beman/inside/inside/detail/wide_value.hpp>
#include <beman/inside/inside/grid.hpp>
#include <beman/inside/inside/detail/call_policy.hpp>

#include <utility>

//---------------------------------------------------------------------------
// addition — `add(L, R, policy, action) -> inside<G>`, G = grid_of<L> + grid_of<R>.
// The grid arithmetic is sound by construction (the result interval contains
// every runtime sum, and a continuous result's raw is sized from its
// denominator bound), so a sum never fails. Shapes: a continuous result
// (exact fraction, or a rational raw), a point, or integer raws added as
// value counts in a common unit.
//---------------------------------------------------------------------------
namespace beman::inside::detail {
template <insidable L, insidable R = L>
struct addition {
    static constexpr grid result_grid = grid_of<L> + grid_of<R>;
    // A result is checked, whatever its operands' policies.
    using result = inside<result_grid>;

    // Total: the policy has nothing to decide.
    template <policy_flag F = none, typename E = empty_ref>
    static constexpr result add(L lhs, R rhs, call_policy<F, E> = {}) {
        if constexpr (fraction_storage<result>)
            return result::from_raw(*continuous_raw_of<raw_t<result>>(exact_of(lhs) + exact_of(rhs)));
        else if constexpr (exact_path_fits<exact_op::add, result, L, R>)
            return result::from_raw(*try_rational(exact_of(lhs) + exact_of(rhs)));
        else if constexpr (rational_storage<result>)
            return result::from_raw(rational::add_unchecked(lhs, rhs));
        else if constexpr (point_storage<result>)
            return result::from_raw(raw_t<result>{}); // point + point: a point
        else {
            // Integer raws (a notched result has no continuous operand): add the
            // values in a unit dividing both operands' (the result notch
            // gcd(N_L, N_R) on anchored grids), in imax or by wrapping arithmetic
            // (wide_value.hpp). Exact for every grid, at any width.
            static_assert(integer_storage<L> && integer_storage<R>);
            constexpr grid_rational U = gcd(unit_of<L>, unit_of<R>);
            using W                   = index_work_t<result, L, U, R, U, U>;
            return from_value_in_units<result, U>(value_in_units<W, U>(lhs) + value_in_units<W, U>(rhs));
        }
    }
};

// Whether `l + r` has a result, as a constraint of add / + / += (so a
// requires-expression or `if constexpr` probe gets the answer instead of a
// hard error): a result grid with a bound (its fractions within
// BEMAN_INSIDE_MAX_FRACTION_BITS), and no wide-index operand with a
// continuous result. (F, the call's flags, is taken for symmetry with
// div_defined; a sum ignores it.)
template <insidable L, insidable R, policy_flag = none>
inline constexpr bool add_defined = [] {
    using result = typename addition<L, R>::result;
    if constexpr (!bounded_grid(grid_of<result>))
        return false;
    else
        return !rational_storage<result> || (!wide_valued<L> && !wide_valued<R>);
}();

// The same for `l - r` (l + (−r)).
template <insidable L, insidable R, policy_flag = none>
inline constexpr bool sub_defined = add_defined<L, decltype(-std::declval<const R&>())>;
} // namespace beman::inside::detail

#endif // BEMAN_INSIDE_INSIDE_DETAIL_ADDITION_HPP
