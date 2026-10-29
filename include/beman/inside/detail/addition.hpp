// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_DETAIL_ADDITION_HPP
#define BEMAN_INSIDE_DETAIL_ADDITION_HPP

#include <beman/inside/detail/config.hpp>

#include <beman/inside/detail/rep.hpp>
#include <beman/inside/generic.hpp>
#include <beman/inside/detail/wide_value.hpp>
#include <beman/inside/grid.hpp>
#include <beman/inside/policy.hpp>

#include <utility>

//---------------------------------------------------------------------------
// addition — `add(L, R, policy, action) -> inside<G>`, G = grid_of<L> + grid_of<R>.
// The grid arithmetic is sound by construction (the result interval contains
// every runtime sum), so overflow can only happen on continuous results.
// Shapes: a continuous result (exact fraction, or a rational raw), a point,
// or integer raws added as value counts in a common unit.
//---------------------------------------------------------------------------
namespace beman::inside::detail {
template <insidable L, insidable R = L>
struct addition {
    static constexpr grid result_grid = grid_of<L> + grid_of<R>;
    // A result is checked, whatever its operands' policies.
    using result = inside<result_grid>;

    template <policy_flag F>
    static constexpr bool needs_overflow_check = lattice_op_checked<exact_op::add, result, L, R, F>;

    // Plain result when an overflow action takes the failure or no check is
    // needed; else std::expected<result, errc>.
    template <policy_flag F, typename A>
    using return_t = std::
        conditional_t<overflow_action<plain_t<A>> || !needs_overflow_check<F>, result, std::expected<result, errc>>;

    template <policy_flag F = none, typename E = empty_ref, typename A = no_action>
    static constexpr auto add(L lhs, R rhs, policy<F, E> policy = {}, A&& action = {}) -> return_t<F, A> {
        if constexpr (fraction_storage<result>) {
            const auto sum = continuous_raw_of<raw_t<result>>(exact_of(lhs) + exact_of(rhs));
            if constexpr (needs_overflow_check<F>)
                if (!sum) [[unlikely]]
                    return report_or_unexpected<result>(action, policy, errc::overflow);
            return result::from_raw(*sum);
        } else if constexpr (rational_storage<result>) {
            if constexpr (needs_overflow_check<F>) {
                auto sum = as_rational(lhs) + as_rational(rhs);
                if (!sum) [[unlikely]]
                    return report_or_unexpected<result>(action, policy, errc::overflow);
                return result::from_raw(*sum);
            } else
                return result::from_raw(rational::add_unchecked(lhs, rhs));
        } else if constexpr (point_storage<result>)
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
// hard error): no wide-index operand with a continuous result. (F, the
// call's flags, is taken for symmetry with div_defined; a sum ignores it.)
template <insidable L, insidable R, policy_flag = none>
inline constexpr bool add_defined =
    !rational_storage<typename addition<L, R>::result> || (!wide_valued<L> && !wide_valued<R>);

// The same for `l - r` (l + (−r)).
template <insidable L, insidable R, policy_flag = none>
inline constexpr bool sub_defined = add_defined<L, decltype(-std::declval<const R&>())>;
} // namespace beman::inside::detail

#endif // BEMAN_INSIDE_DETAIL_ADDITION_HPP
