// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_DETAIL_REP_HPP
#define BEMAN_INSIDE_DETAIL_REP_HPP

#include <beman/inside/generic.hpp>
#include <beman/inside/grid.hpp>
#include <beman/inside/policy_flag.hpp>

namespace beman::inside::detail {
// Whether a + or × into Result runs its overflow check. The result grid
// holds every result, so only a continuous result can overflow: a fraction
// raw always may (its denominator grows); a rational raw under a checked
// policy (a continuous operand bounds no denominator).
template <insidable Result, insidable L, insidable R, policy_flag F>
inline constexpr bool lattice_op_checked =
    fraction_storage<Result> ||
    (rational_storage<Result> && (has_any_flag(F, checked) || is_checked(policy_of<L>) || is_checked(policy_of<R>)));

// `x * just<c>` and `x / just<c>` (c != 0): the result lattice is x's lattice
// scaled by c or 1/c (grid operator* / operator/), so the result offset IS x's
// offset — counted from the far end when the scale is negative. No multiply
// or divide at all.
template <insidable Point, insidable X, insidable Result>
inline constexpr bool point_scale =
    point_grid<Point> && lower_of<Point> != 0 && notched<X> && !wide_valued<X> && !wide_valued<Result>;

template <insidable Result, bool Negate, insidable X>
constexpr Result scale_by_point(const X& x) noexcept {
    static_assert(max_index_v<Result> == max_index_v<X>);
    umax off;
    if constexpr (index_storage<X>)
        off = static_cast<umax>(x.raw());
    else
        off = static_cast<umax>(x.raw()) - static_cast<umax>(raw_lo<X>);
    return Result::from_raw(raw_from_offset<Result>(Negate ? max_index_v<X> - off : off));
}
} // namespace beman::inside::detail
#endif
