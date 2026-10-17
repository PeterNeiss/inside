// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_DETAIL_REP_HPP
#define BEMAN_INSIDE_DETAIL_REP_HPP

#include <beman/inside/detail/config.hpp>
BEMAN_INSIDE_WARNINGS_PUSH

#include <beman/inside/generic.hpp>
#include <beman/inside/grid.hpp>
#include <beman/inside/policy_flag.hpp>

namespace beman::inside::detail {
enum class exact_op : unsigned char { add, mul, div };

// An exact L op R into the continuous Result that the grids prove cannot
// overflow. Result carries a denominator bound (grid.hpp: den_bound), so its
// raw holds every value: a fraction raw is sized from the bound; a 64-bit
// rational needs the unchecked kernels' intermediates in range too —
//   +: each operand over the common denominator D_L·D_R fits umax;
//   ×: nothing more (numerator ≤ num_bound(Result), denominator ≤ D_L·D_R);
//   ÷: the divisor's numerator fits imax (1/r moves it to the denominator).
template <exact_op Op, insidable Result, insidable L, insidable R>
inline constexpr bool exact_fits = [] {
    if constexpr (grid_of<Result>.MaxDenominator == 0)
        return false;
    else if constexpr (fraction_storage<Result>)
        return true;
    else if constexpr (!rational_storage<Result>)
        return false;
    else {
        const grid_int umax_max{std::numeric_limits<umax>::max()}, imax_max{std::numeric_limits<imax>::max()};
        if constexpr (Op == exact_op::add) {
            const grid_int d = den_bound(grid_of<L>) * den_bound(grid_of<R>);
            return !(umax_max < num_bound(interval_of<L>, d)) && !(umax_max < num_bound(interval_of<R>, d));
        } else if constexpr (Op == exact_op::div)
            return !(imax_max < num_bound(grid_of<R>));
        else
            return true;
    }
}();

// Whether a + or × into Result runs its overflow check. The result grid
// holds every result, so only a continuous result can overflow, and not one
// whose denominator bound proves it fits (exact_fits): otherwise a fraction
// raw always may (its denominator grows); a rational raw under a checked
// policy (a continuous operand bounds no denominator).
template <exact_op Op, insidable Result, insidable L, insidable R, policy_flag F>
inline constexpr bool lattice_op_checked =
    !exact_fits<Op, Result, L, R> &&
    (fraction_storage<Result> ||
     (rational_storage<Result> && (has_any_flag(F, checked) || is_checked(flags_of<L>) || is_checked(flags_of<R>))));

// `x * just<c>` and `x / just<c>` (c != 0): the result lattice is x's lattice
// scaled by c or 1/c (grid operator* / operator/), so the result offset IS x's
// offset — counted from the far end when the scale is negative. No multiply
// or divide at all.
template <insidable Point, insidable X, insidable Result>
inline constexpr bool point_scale =
    point_grid<Point> && lower_of<Point> != 0 && notched<X> && !wide_valued<X> && !wide_valued<Result>;

template <insidable Result, bool Negate, insidable X>
constexpr Result scale_by_point(const X& x) noexcept {
    static_assert(max_index_of<Result> == max_index_of<X>);
    const umax off = static_cast<umax>(x.raw()) - static_cast<umax>(raw_lo<X>); // raw_lo is 0 for an index
    return Result::from_raw(raw_from_offset<Result>(Negate ? max_index_of<X> - off : off));
}
} // namespace beman::inside::detail
BEMAN_INSIDE_WARNINGS_POP

#endif // BEMAN_INSIDE_DETAIL_REP_HPP
