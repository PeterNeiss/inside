// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_DETAIL_ADDITION_HPP
#define BEMAN_INSIDE_DETAIL_ADDITION_HPP

#include <beman/inside/detail/rep.hpp>
#include <beman/inside/generic.hpp>
#include <beman/inside/detail/wide_value.hpp>
#include <beman/inside/grid.hpp>
#include <beman/inside/policy.hpp>

//---------------------------------------------------------------------------
// addition — `add(L, R, policy, action) -> inside<G>`, G = grid_of<L> + grid_of<R>.
// The grid arithmetic is sound by construction (the result interval contains
// every runtime sum), so overflow can only happen on rational-raw results.
// Specialises on the storage shapes: rational result, mixed rational/integer,
// direct integer-space add, or both notch-offset (scale via lhs/rhs_widen).
//---------------------------------------------------------------------------
namespace beman::inside::detail {
template <insidable L, insidable R = L>
struct addition {
    static_assert(grid_sum_fits(grid_of<L>, grid_of<R>),
                  "addition: the result grid exceeds the 64-bit grid numbers — coarsen the "
                  "operand grids, or build with C++26 big grids");
    // (Falls back to L's grid when the assertion failed, so the build stops at
    // that message instead of the rational overflow behind it.)
    static constexpr grid result_grid =
        grid_sum_fits(grid_of<L>, grid_of<R>) ? (grid_of<L> + grid_of<R>).value() : grid_of<L>;
    // Representation propagation — shared rule in detail/rep.hpp.
    using rep_t  = result_rep<L, R, result_grid>;
    using result = inside<result_grid, rep_t::result_policy>;

    template <policy_flag F>
    static constexpr bool needs_overflow_check =
        lattice_op_checked<result, L, R, F, rational_add_is_safe(grid_of<L>, grid_of<R>)>;

    // Plain result when an overflow action takes the failure or no check is
    // needed; else std::expected<result, errc>.
    template <policy_flag F, typename A>
    using return_t = std::
        conditional_t<overflow_action<plain_t<A>> || !needs_overflow_check<F>, result, std::expected<result, errc>>;

    template <policy_flag F = none, typename E = empty_ref, typename A = no_action>
    static constexpr auto add(L lhs, R rhs, policy<F, E> policy = {}, A&& action = {}) -> return_t<F, A> {
        result res;
        if constexpr (fraction_storage<result>) {
            const auto sum = frac_raw_of<raw_t<result>>(exact_of(lhs) + exact_of(rhs));
            if (!sum) [[unlikely]]
                return report_or_unexpected<result>(action, policy, errc::overflow, "fraction overflow in add");
            res = result::from_raw(*sum);
        } else if constexpr (rational_storage<result>) {
            static_assert(!wide_valued<L> && !wide_valued<R>,
                          "addition: a wide-index operand with a continuous result is not supported yet");
            if constexpr (needs_overflow_check<F>) {
                auto sum = rational::add(lhs, rhs);
                if (!sum) [[unlikely]]
                    return report_or_unexpected<result>(action, policy, errc::overflow, "rational overflow in add");
                res = result::from_raw(*sum);
            } else
                res = result::from_raw(rational::add_unchecked(lhs, rhs));
        } else if constexpr (point_storage<result>)
            res = result::from_raw(raw_t<result>{}); // point + point: a point
        else if constexpr (integer_storage<L> && integer_storage<R>) {
            // Integer raws: add the values in a unit dividing both operands' (the
            // result notch gcd(N_L, N_R) on anchored grids), in imax or by wrapping
            // arithmetic (wide_value.hpp). Exact for every grid, at any width.
            constexpr grid_rational U = grid_gcd_of(unit_of<L>, unit_of<R>);
            using W                   = index_work_t<result, L, U, R, U, U>;
            res = from_value_in_units<result, U>(value_in_units<W, U>(lhs) + value_in_units<W, U>(rhs));
        } else if constexpr (wide_valued<result>)
            // A rational operand into a result with more than 2^64 slots.
            res = exact_result<result>(exact_of(lhs) + exact_of(rhs));
        else {
            // A rational operand into an integer result: the exact rational
            // sum, converted to the result's raw.
            auto sum = rational::add_unchecked(lhs, rhs);
            res      = result::from_raw(raw_of_lattice_value<result>(sum));
        }
        return res;
    }
};
} // namespace beman::inside::detail

#endif // BEMAN_INSIDE_DETAIL_ADDITION_HPP
