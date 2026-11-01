// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_INSIDE_CURSOR_HPP
#define BEMAN_INSIDE_INSIDE_CURSOR_HPP

#include <beman/inside/inside/detail/config.hpp>

#include <beman/inside/inside/detail/compare.hpp>

namespace beman::inside {
//---------------------------------------------------------------------------
// cursor<T, Step> — an inside on {{Lower, Upper + Step}, Step} of T: it steps
// (++ moves one notch) from Lower through Upper and one step past it, the
// end. It keeps T's checks and rounding, default construction starts at
// Lower, and it carries cursor_marker, which gives it
// `end(t)`. Step is a number (an integer, `rational`, `per<N>`), T's notch by
// default: a positive whole number of notches of T dividing its range, so
// every value but the end is a value of T.
//   for (cursor<time_t, per<4>> t; t != end(t); ++t) …
//---------------------------------------------------------------------------
namespace detail {
template <insidable T>
consteval grid cursor_grid(const auto& step) {
    const grid_rational s = grid{step}.Interval.Lower;
    if (!(s > 0))
        constexpr_error<"cursor<T, Step>: the step must be positive - a cursor steps forward, one past the end">();
    if (notch_of<T> == 0)
        constexpr_error<"cursor<T, Step>: T has no notch - a continuous grid has no lattice to step on">();
    if (!interval_of<T>.divides_evenly(s))
        constexpr_error<"cursor<T, Step>: the step must divide the range of T evenly">();
    if (!grid_divides_evenly(s, notch_of<T>))
        constexpr_error<"cursor<T, Step>: the step must be a whole number of notches of T">();
    return grid{interval{lower_of<T>, upper_of<T> + s}, s};
}
} // namespace detail

template <insidable T, auto Step = notch_of<T>>
using cursor = inside<detail::cursor_grid<T>(Step), flags_of<T> | detail::cursor_marker>;

// end(t): a cursor's past-the-end value, one step past T's Upper. Only a
// cursor has one — on any other inside its Upper is a value, not an end.
template <insidable B>
    requires(has_flag(flags_of<B>, detail::cursor_marker))
[[nodiscard]] constexpr auto end(const B&) noexcept {
    return just<upper_of<B>>;
}
} // namespace beman::inside

#endif // BEMAN_INSIDE_INSIDE_CURSOR_HPP
