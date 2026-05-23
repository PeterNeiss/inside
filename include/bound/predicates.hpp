//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDpredicatesHPP
#define BNDpredicatesHPP

#include "bound/generic.hpp"
#include "bound/grid.hpp"

//---------------------------------------------------------------------------
// predicates — `will_conversion_*` / `is_conversion_lossy` for `bound<B>`.
//
// Pure inspection — none of these perform the conversion or modify state.
// Use them to branch *before* attempting a construction that might throw or
// land on the sentinel.
//
//   will_conversion_overflow<B>(v) — true if v falls outside B's interval.
//   will_conversion_truncate<B>(v) — true if v is in-range but doesn't land
//                                    on a notch of B (would round/truncate).
//   is_conversion_lossy<B>(v)      — OR of the two above; convenient single
//                                    test for "would this conversion lose
//                                    information?".
//---------------------------------------------------------------------------
namespace bnd
{
  template <boundable B, arithmetic A>
  [[nodiscard]] constexpr bool will_conversion_overflow(A value) noexcept
  {
    return not Interval<B>.includes(rational{value});
  }

  template <boundable B, boundable A>
  [[nodiscard]] constexpr bool will_conversion_overflow(A value) noexcept
  {
    return not Interval<B>.includes(as_rational(value));
  }

  template <boundable B, arithmetic A>
  [[nodiscard]] constexpr bool will_conversion_truncate(A value) noexcept
  {
    if constexpr (is_raw_rational<B>)
      return false;                       // rational raw stores any value exactly
    if (not Interval<B>.includes(rational{value}))
      return false;                       // out-of-range — overflow, not truncation
    // In-range: truncation occurs iff (value - Lower) / Notch is non-integer.
    auto offset = (rational{value} - Lower<B>) / Notch<B>;
    return !offset.has_value() || abs_den(offset->Denominator) != 1;
  }

  template <boundable B, boundable A>
  [[nodiscard]] constexpr bool will_conversion_truncate(A value) noexcept
  {
    if constexpr (is_raw_rational<B>) return false;
    rational r = value;
    if (not Interval<B>.includes(r)) return false;
    auto offset = (r - Lower<B>) / Notch<B>;
    return !offset.has_value() || abs_den(offset->Denominator) != 1;
  }

  template <boundable B, typename A>
  [[nodiscard]] constexpr bool is_conversion_lossy(A value) noexcept
  {
    return will_conversion_overflow<B>(value)
        || will_conversion_truncate<B>(value);
  }
} // namespace bnd

#endif // BNDpredicatesHPP
