// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_INTERVAL_HPP
#define BEMAN_INSIDE_INTERVAL_HPP

#include <beman/inside/lift.hpp>
#include <beman/inside/detail/rational.hpp>
#include <beman/inside/detail/grid_rational.hpp>

#include <initializer_list>

namespace beman::inside {
//---------------------------------------------------------------------------
// interval — structural NTTP type (public members only) with inclusive Lower
// and Upper bounds. Like `grid`, its operator+/-/*// computes result intervals
// at compile time; division returns errc::division_by_zero when the divisor straddles zero
// (grid::operator/ re-runs on the two zero-free halves and unions them).
//---------------------------------------------------------------------------
inline namespace BEMAN_INSIDE_GRID_ABI {
struct interval {
    detail::grid_rational Lower;
    detail::grid_rational Upper;

    interval() = default;

    constexpr interval(detail::grid_rational lower, detail::grid_rational upper) : Lower{lower}, Upper{upper} {}
    constexpr interval(detail::arithmetic auto lower, detail::arithmetic auto upper) : Lower{lower}, Upper{upper} {}

    template <auto I>
    static constexpr bool validate() {
        static_assert(I.Lower <= I.Upper);
        return true;
    }

    [[nodiscard]] constexpr bool     operator==(const interval& rhs) const = default;
    [[nodiscard]] constexpr interval operator-() const { return interval{-Upper, -Lower}; }

    // With 64-bit grid numbers a span past the rational range (an interval
    // reaching past int64 on both sides) is tested endpoint by endpoint:
    // equal residues mod notch.
    [[nodiscard]] constexpr bool divides_evenly(const detail::grid_rational& notch) const {
#if BEMAN_INSIDE_BIG_GRIDS
        return detail::grid_divides_evenly(Upper - Lower, notch);
#else
        if (const auto span = detail::try_sub(Upper, Lower))
            return detail::divides_evenly(*span, notch);
        return detail::divides_evenly(Lower, notch) && detail::divides_evenly(Upper, notch);
#endif
    }

    // The span in units of `notch` (the slot count of a grid).
    [[nodiscard]] constexpr std::expected<detail::grid_rational, errc>
    operator/(const detail::grid_rational& notch) const {
#if BEMAN_INSIDE_BIG_GRIDS
        if (notch == 0)
            return std::unexpected{errc::division_by_zero};
        return (Upper - Lower) / notch;
#else
        return (Upper - Lower) / notch;
#endif
    }
};
} // namespace BEMAN_INSIDE_GRID_ABI

// Containment / disjointness — free functions over the public endpoints
// (siblings of the binary interval operators below).
[[nodiscard]] constexpr bool includes(const interval& iv, const interval& rhs) noexcept {
    return iv.Lower <= rhs.Lower && rhs.Upper <= iv.Upper;
}

[[nodiscard]] constexpr bool includes(const interval& iv, const detail::rational& r) noexcept {
    return iv.Lower <= r && r <= iv.Upper;
}

#if BEMAN_INSIDE_BIG_GRIDS
[[nodiscard]] constexpr bool includes(const interval& iv, const detail::grid_rational& r) noexcept {
    return iv.Lower <= r && r <= iv.Upper;
}
#endif

[[nodiscard]] constexpr bool includes(const interval& iv, detail::arithmetic auto a) noexcept {
    return includes(iv, detail::grid_rational{a});
}

// `excludes` means *strictly disjoint* — the intervals share no value.
// `!includes()` is weaker: it only rules out total containment, so two
// overlapping intervals are `!includes` AND `!excludes`.
[[nodiscard]] constexpr bool excludes(const interval& iv, const interval& rhs) noexcept {
    return rhs.Upper < iv.Lower || iv.Upper < rhs.Lower;
}

// The `includes(rhs, iv)` clause catches rhs wholly containing iv (where
// neither rhs endpoint lands in iv, so the other checks would miss it).
[[nodiscard]] constexpr bool overlaps(const interval& iv, const interval& rhs) noexcept {
    return includes(rhs, iv) || includes(iv, rhs.Lower) || includes(iv, rhs.Upper);
}

// The min/max hull of four endpoint combinations — the result interval of an
// interval product or quotient (interval arithmetic's four-corner rule).
namespace detail {
[[nodiscard]] constexpr interval
corner_hull(grid_rational a, grid_rational b, grid_rational c, grid_rational d) noexcept {
    const grid_rational lo1 = a < b ? a : b, hi1 = a < b ? b : a;
    const grid_rational lo2 = c < d ? c : d, hi2 = c < d ? d : c;
    return interval{lo1 < lo2 ? lo1 : lo2, hi1 < hi2 ? hi2 : hi1};
}
} // namespace detail

[[nodiscard]] constexpr std::expected<interval, errc> operator+(const interval&, const interval&);
[[nodiscard]] constexpr std::expected<interval, errc> operator-(const interval&, const interval&);
[[nodiscard]] constexpr std::expected<interval, errc> operator*(const interval&, const interval&);
[[nodiscard]] constexpr std::expected<interval, errc> operator/(const interval&, const interval&);
[[nodiscard]] constexpr auto operator<=>(const interval&, const interval&) -> std::partial_ordering;

//---------------------------------------------------------------------------
// operator+
//---------------------------------------------------------------------------
[[nodiscard]] inline constexpr std::expected<interval, errc> operator+(const interval& lhs, const interval& rhs) {
    return detail::lift([](detail::grid_rational l, detail::grid_rational u) { return interval{l, u}; },
                        lhs.Lower + rhs.Lower,
                        lhs.Upper + rhs.Upper);
}

//---------------------------------------------------------------------------
// operator-
//---------------------------------------------------------------------------
[[nodiscard]] inline constexpr std::expected<interval, errc> operator-(const interval& lhs, const interval& rhs) {
    return operator+(lhs, -rhs);
}

//---------------------------------------------------------------------------
// operator*
//---------------------------------------------------------------------------
[[nodiscard]] inline constexpr std::expected<interval, errc> operator*(const interval& lhs, const interval& rhs) {
    return detail::lift(detail::corner_hull,
                        lhs.Lower * rhs.Lower,
                        lhs.Lower * rhs.Upper,
                        lhs.Upper * rhs.Lower,
                        lhs.Upper * rhs.Upper);
}

//---------------------------------------------------------------------------
// operator/
//---------------------------------------------------------------------------
[[nodiscard]] inline constexpr std::expected<interval, errc> operator/(const interval& lhs, const interval& rhs) {
    if (includes(rhs, 0))
        return std::unexpected{errc::division_by_zero};

    return detail::lift(detail::corner_hull,
                        lhs.Lower / rhs.Lower,
                        lhs.Lower / rhs.Upper,
                        lhs.Upper / rhs.Lower,
                        lhs.Upper / rhs.Upper);
}

//---------------------------------------------------------------------------
// operator<=>
//---------------------------------------------------------------------------
[[nodiscard]] inline constexpr auto operator<=>(const interval& lhs, const interval& rhs) -> std::partial_ordering {
    if (lhs.Upper < rhs.Lower)
        return std::partial_ordering::less;

    if (lhs.Lower > rhs.Upper)
        return std::partial_ordering::greater;

    if (lhs.Lower == rhs.Lower && lhs.Upper == rhs.Upper)
        return std::partial_ordering::equivalent;

    return std::partial_ordering::unordered;
}

} // namespace beman::inside

#endif // BEMAN_INSIDE_INTERVAL_HPP
