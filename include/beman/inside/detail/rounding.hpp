// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_DETAIL_ROUNDING_HPP
#define BEMAN_INSIDE_DETAIL_ROUNDING_HPP

//---------------------------------------------------------------------------
// The rounding decision every integer rounding path shares. A path
// truncates toward zero, classifies the dropped remainder, and asks
// rounds_away whether the mode moves the quotient one unit away from zero.
// The arithmetic (builtin, wide, rational, double) stays with the caller;
// only the decision lives here. With a constant mode it folds to the one
// comparison that mode needs.
//---------------------------------------------------------------------------
namespace beman::inside::detail {

// Ties of `nearest` go half away from zero. rounding_of (policy_flag.hpp)
// maps a flag set to its mode.
enum class round_mode { trunc, nearest, floor, ceil, half_even };

// Where a truncation's dropped remainder sits against half a unit. Only the
// nearest modes read the half; for the others every nonzero remainder
// classifies as below_half (they need only "inexact").
enum class remainder_class : unsigned char { zero, below_half, half, above_half };

// The remainder magnitude r of a divisor magnitude d (0 ≤ r < d), classified
// for m. Compares r with d − r, so 2·r never has to fit T.
template <typename T>
[[nodiscard]] constexpr remainder_class classify_remainder(round_mode m, const T& r, const T& d) noexcept {
    if (r == T{0})
        return remainder_class::zero;
    if (m != round_mode::nearest && m != round_mode::half_even)
        return remainder_class::below_half;
    const T rest = d - r;
    return r < rest ? remainder_class::below_half : rest < r ? remainder_class::above_half : remainder_class::half;
}

// Whether rounding by m moves a truncated quotient one unit away from zero.
// `negative`: the exact value's sign; `odd`: the truncated quotient's parity.
[[nodiscard]] constexpr bool rounds_away(round_mode m, bool negative, remainder_class r, bool odd) noexcept {
    if (r == remainder_class::zero)
        return false;
    switch (m) {
    case round_mode::floor:
        return negative;
    case round_mode::ceil:
        return !negative;
    case round_mode::nearest:
        return r != remainder_class::below_half;
    case round_mode::half_even:
        return r == remainder_class::above_half || (r == remainder_class::half && odd);
    default:
        return false;
    }
}
// The same decision for a quotient rounded down (toward −∞) instead of
// toward zero: whether rounding by m moves it one unit up. `negative`: the
// rounded VALUE is below zero, which on an unanchored grid need not be the
// quotient's sign (toward zero is down for a value ≥ 0, up below 0, and a
// tie of `nearest` goes away from zero — up at 0 itself); `odd`: the floor's
// parity as a lattice index.
[[nodiscard]] constexpr bool rounds_up(round_mode m, bool negative, remainder_class r, bool odd) noexcept {
    if (r == remainder_class::zero)
        return false;
    switch (m) {
    case round_mode::floor:
        return false;
    case round_mode::ceil:
        return true;
    case round_mode::nearest:
        return r == remainder_class::above_half || (r == remainder_class::half && !negative);
    case round_mode::half_even:
        return r == remainder_class::above_half || (r == remainder_class::half && odd);
    default: // toward zero
        return negative;
    }
}
} // namespace beman::inside::detail

#endif // BEMAN_INSIDE_DETAIL_ROUNDING_HPP
