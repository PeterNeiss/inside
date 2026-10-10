// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_DETAIL_REP_HPP
#define BEMAN_INSIDE_DETAIL_REP_HPP

#include <beman/inside/generic.hpp>
#include <beman/inside/grid.hpp>
#include <beman/inside/policy_flag.hpp>

namespace beman::inside::detail {
// Whether a + or × into Result runs its overflow check. The result grid
// holds every result, so only a rational or fraction raw can overflow: a
// fraction raw always may (its denominator grows); a rational raw under a
// checked policy, unless the grids prove the op fits
// (RationalSafe: rational_add_is_safe / rational_mul_is_safe).
template <insidable Result, insidable L, insidable R, policy_flag F, bool RationalSafe>
inline constexpr bool lattice_op_checked =
    fraction_storage<Result> ||
    (rational_storage<Result> && (has_any_flag(F, checked) || is_checked(policy_of<L>) || is_checked(policy_of<R>)) &&
     !RationalSafe);
} // namespace beman::inside::detail
#endif
