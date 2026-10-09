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
// fp results, point scaling and rational results have their own branches.
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
    // fp / representation propagation — shared rule in detail/rep.hpp. The product
    // grid (notch = N_L·N_R) is finer, so demotion/dropping is the common case.
    using rep_t                      = fp_rep<L, R, result_grid>;
    static constexpr bool dropped_fp = rep_t::dropped_fp;
    using result                     = inside<result_grid, rep_t::result_policy>;

    // The dropped-fp case lands on a rational result when the product grid outgrows
    // uint index space; its product numerator can exceed `umax`, so check it (the
    // result carries `checked`) rather than wrap.
    // (A wide fraction raw may always overflow, as for addition.)
    template <policy_flag F>
    static constexpr bool needs_overflow_check =
        frac_raw<result> || (rational_raw<result> &&
                             (has_any_flag(F, checked) || is_checked(policy_of<L>) || is_checked(policy_of<R>) ||
                              has_any_flag(F | policy_of<L> | policy_of<R>, exact) || dropped_fp) &&
                             !rational_mul_is_safe(grid_of<L>, grid_of<R>));

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
        lower_of<Point> == upper_of<Point> && lower_of<Point> != 0 && !rational_raw<X> && !fp_raw<X> &&
        notch_of<X> != 0 && !rational_raw<result> && !fp_raw<result> && !exact_valued<X> && !exact_valued<result>;

    // An operand's unit in the product grid (grid operator*): its notch, or
    // |c| for a point c.
    template <insidable X>
    static constexpr grid_rational unit_of = (lower_of<X> == upper_of<X>) ? abs(lower_of<X>) : notch_of<X>;

    template <bool Negate, insidable X>
    static constexpr result scale_by_point(const X& x) {
        static_assert(max_index_v<result> == max_index_v<X>);
        umax off;
        if constexpr (index_raw<X>)
            off = static_cast<umax>(x.raw());
        else
            off = static_cast<umax>(x.raw()) - static_cast<umax>(raw_lo<X>);
        return result::from_raw(raw_from_offset<result>(Negate ? max_index_v<X> - off : off));
    }

    template <typename P, typename A = no_action>
    static constexpr auto mul(L lhs, R rhs, P&& policy, A&& action = {}) -> return_t<policy_flags_of<plain_t<P>>, A> {
        if constexpr (fp_raw<result>) {
            // Exact by construction, no snap (see addition.hpp): operands are notch
            // multiples, the product index |ia·ib| stays under the double_exact 2^53
            // gate, so the double multiply is exact and on the result lattice.
            return result::from_raw(raw_cast<result>(as_double(lhs) * as_double(rhs)));
        } else if constexpr (point_scale<R, L>)
            return scale_by_point<(lower_of<R> < 0)>(lhs);
        else if constexpr (point_scale<L, R>)
            return scale_by_point<(lower_of<L> < 0)>(rhs);
        else if constexpr (frac_raw<result>) {
            const auto prod = frac_raw_of<raw_t<result>>(exact_of(lhs) * exact_of(rhs));
            if (!prod) [[unlikely]]
                return report_or_unexpected<result>(action, policy, errc::overflow, "fraction overflow in mul");
            return result::from_raw(*prod);
        } else if constexpr (rational_raw<result>) {
            static_assert(!exact_valued<L> && !exact_valued<R>,
                          "multiplication: a wide-index operand with a continuous result is not supported yet");
            if constexpr (needs_overflow_check<policy_flags_of<plain_t<P>>>) {
                auto prod = as_rational(lhs) * as_rational(rhs);
                if (!prod) [[unlikely]]
                    return report_or_unexpected<result>(action, policy, errc::overflow, "rational overflow in mul");
                return result::from_raw(raw_cast<result>(*prod));
            } else
                return result::from_raw(raw_cast<result>(rational::mul_unchecked(as_rational(lhs), as_rational(rhs))));
        } else if constexpr (point_raw<result>)
            return result::from_raw(raw_t<result>{}); // a product with 0: the point 0
        else if constexpr (integer_raw<L> && integer_raw<R>) {
            // Integer raws: multiply the operands' values in their own units, in
            // imax or by wrapping arithmetic (wide_value.hpp). The product notch is the product
            // of those units (a notch, or |c| for a point c), so the product of the
            // unit counts is the result's value index — exact for every grid and
            // sign, at any width.
            using W = index_work_t<result, L, unit_of<L>, R, unit_of<R>>;
            static_assert(
                wide_numerator(unit_of<L>) * wide_numerator(unit_of<R>) * wide_denominator(notch_of<result>) ==
                    wide_denominator(unit_of<L>) * wide_denominator(unit_of<R>) * wide_numerator(notch_of<result>),
                "multiplication: the product notch is the product of the operand units");
            return from_value_index<result>(value_in_units<W, unit_of<L>>(lhs) * value_in_units<W, unit_of<R>>(rhs));
        } else if constexpr (exact_valued<result>)
            // An fp or rational operand into a result with more than 2^64 slots.
            return exact_result<result>(exact_of(lhs) * exact_of(rhs));
        else {
            // An fp or rational operand into an integer result (reached when `f64`
            // was dropped from a result grid that is not double-exact): the exact
            // rational product, converted to the result's raw.
            auto prod = rational::mul_unchecked(as_rational(lhs), as_rational(rhs));
            return result::from_raw(raw_from_offset<result>(
                ((prod - detail::lower64<result>) / detail::notch64<result>).value().Numerator));
        }
    }
};
} // namespace beman::inside::detail

#endif // BEMAN_INSIDE_DETAIL_MULTIPLICATION_HPP
