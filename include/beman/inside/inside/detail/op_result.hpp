// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_INSIDE_DETAIL_OP_RESULT_HPP
#define BEMAN_INSIDE_INSIDE_DETAIL_OP_RESULT_HPP

#include <beman/inside/inside/detail/config.hpp>

#include <beman/inside/inside/generic.hpp>
#include <beman/inside/inside/grid.hpp>
#include <beman/inside/inside/policy_flag.hpp>

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
    if constexpr (fraction_storage<Result>)
        return true;
    else if constexpr (!rational_storage<Result>)
        return false;
    else {
        if constexpr (Op == exact_op::add) {
            const grid_int d = den_bound(grid_of<L>) * den_bound(grid_of<R>);
            return num_bound(interval_of<L>, d).fits_limb() && num_bound(interval_of<R>, d).fits_limb();
        } else if constexpr (Op == exact_op::div)
            return num_bound(grid_of<R>) <= std::numeric_limits<imax>::max();
        else
            return true;
    }
}();

// A continuous Result holds every exact result of + and ×: its raw is sized
// from its bound. Only the 64-bit kernels' intermediates (each operand over
// the common denominator) may pass 64 bits, so where exact_fits is false the
// op computes exactly in wide integers and stores that — no check, no
// expected.
template <exact_op Op, insidable Result, insidable L, insidable R>
inline constexpr bool exact_path_fits = rational_storage<Result> && !exact_fits<Op, Result, L, R>;

// `x * just<c>` and `x / just<c>` (c != 0): the result lattice is x's lattice
// scaled by c or 1/c (grid operator* / operator/), so the result offset IS x's
// offset — counted from the far end when the scale is negative. No multiply
// or divide at all.
template <insidable Point, insidable X, insidable Result>
inline constexpr bool point_scale =
    point_grid<Point> && lower_of<Point> != 0 && notched<X> && !wide_valued<X> && !wide_valued<Result>;

template <insidable Result, insidable Point, insidable X>
constexpr Result scale_by_point(const X& x) noexcept {
    static_assert(max_index_of<Result> == max_index_of<X>);
    const umax off = raw_offset<X>(x.raw());
    return Result::from_raw(raw_from_offset<Result>(lower_of<Point> < 0 ? max_index_of<X> - off : off));
}
} // namespace beman::inside::detail

#endif // BEMAN_INSIDE_INSIDE_DETAIL_OP_RESULT_HPP
