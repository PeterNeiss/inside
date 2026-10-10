// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_DETAIL_MULTIPLICATION_HPP
#define BEMAN_INSIDE_DETAIL_MULTIPLICATION_HPP

#include <beman/inside/detail/rep.hpp>
#include <beman/inside/generic.hpp>
#include <beman/inside/detail/wide_value.hpp>
#include <beman/inside/grid.hpp>
#include <beman/inside/policy.hpp>

//---------------------------------------------------------------------------
// multiplication — `mul(L, R, policy, action) -> inside<grid_of<L> * grid_of<R>>`.
// Integer raws multiply their value indices (wide_value.hpp), in imax when the
// grids' bounds allow, else by wrapping arithmetic as wide as the result raw.
// Point scaling and rational results have their own branches.
//---------------------------------------------------------------------------
namespace beman::inside::detail {
template <insidable L, insidable R = L>
struct multiplication {
    static_assert(grid_product_fits(grid_of<L>, grid_of<R>),
                  "multiplication: the result grid exceeds the 64-bit grid numbers — round the "
                  "product into a coarser type with mul_into<Out>(a, b), coarsen the operand "
                  "grids, or build with C++26 big grids");
    // (Falls back to L's grid when the assertion failed, so the build stops at
    // that message instead of the rational overflow behind it.)
    static constexpr grid result_grid =
        grid_product_fits(grid_of<L>, grid_of<R>) ? (grid_of<L> * grid_of<R>).value() : grid_of<L>;
    // A result is checked, whatever its operands' policies.
    using result = inside<result_grid>;

    template <policy_flag F>
    static constexpr bool needs_overflow_check = lattice_op_checked<result, L, R, F>;

    // Plain result when an overflow action takes the failure or no check is
    // needed; else std::expected<result, errc>.
    template <policy_flag F, typename A>
    using return_t = std::
        conditional_t<overflow_action<plain_t<A>> || !needs_overflow_check<F>, result, std::expected<result, errc>>;

    // `x * just<c>` (c != 0): the result lattice is x's lattice scaled by c
    // (see grid operator*), so the result offset IS x's offset — counted from
    // the far end when c < 0. No multiply at all.
    template <insidable Point, insidable X>
    static constexpr bool point_scale =
        point_grid<Point> && lower_of<Point> != 0 && notched<X> && !wide_valued<X> && !wide_valued<result>;

    template <bool Negate, insidable X>
    static constexpr result scale_by_point(const X& x) {
        static_assert(max_index_v<result> == max_index_v<X>);
        umax off;
        if constexpr (index_storage<X>)
            off = static_cast<umax>(x.raw());
        else
            off = static_cast<umax>(x.raw()) - static_cast<umax>(raw_lo<X>);
        return result::from_raw(raw_from_offset<result>(Negate ? max_index_v<X> - off : off));
    }

    template <typename P, typename A = no_action>
    static constexpr auto mul(L lhs, R rhs, P&& policy, A&& action = {}) -> return_t<policy_flags_of<plain_t<P>>, A> {
        if constexpr (point_scale<R, L>)
            return scale_by_point<(lower_of<R> < 0)>(lhs);
        else if constexpr (point_scale<L, R>)
            return scale_by_point<(lower_of<L> < 0)>(rhs);
        else if constexpr (fraction_storage<result>) {
            const auto prod = frac_raw_of<raw_t<result>>(exact_of(lhs) * exact_of(rhs));
            if (!prod) [[unlikely]]
                return report_or_unexpected<result>(action, policy, errc::overflow, "fraction overflow in mul");
            return result::from_raw(*prod);
        } else if constexpr (rational_storage<result>) {
            static_assert(!wide_valued<L> && !wide_valued<R>,
                          "multiplication: a wide-index operand with a continuous result is not supported yet");
            if constexpr (needs_overflow_check<policy_flags_of<plain_t<P>>>) {
                auto prod = as_rational(lhs) * as_rational(rhs);
                if (!prod) [[unlikely]]
                    return report_or_unexpected<result>(action, policy, errc::overflow, "rational overflow in mul");
                return result::from_raw(*prod);
            } else
                return result::from_raw(rational::mul_unchecked(as_rational(lhs), as_rational(rhs)));
        } else if constexpr (point_storage<result>)
            return result::from_raw(raw_t<result>{}); // a product with 0: the point 0
        else {
            // Integer raws (a notched result has no continuous operand): multiply the operands' values in their own
            // units (a notch or value unit, or |c| for a point c), in imax or by wrapping arithmetic (wide_value.hpp).
            // The product of the unit counts counts the product in the product of the units — on anchored grids the
            // result notch — exact for every grid and sign, at any width.
            static_assert(integer_storage<L> && integer_storage<R>);
            constexpr grid_rational U = grid_mul(unit_of<L>, unit_of<R>);
            using W                   = index_work_t<result, L, unit_of<L>, R, unit_of<R>, U>;
            return from_value_in_units<result, U>(value_in_units<W, unit_of<L>>(lhs) *
                                                  value_in_units<W, unit_of<R>>(rhs));
        }
    }
};
} // namespace beman::inside::detail

#endif // BEMAN_INSIDE_DETAIL_MULTIPLICATION_HPP
