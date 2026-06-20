//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDpredicatesHPP
#define BNDpredicatesHPP

#include "bound/generic.hpp"
#include "bound/grid.hpp"

//---------------------------------------------------------------------------
// predicates — pure inspection (no conversion, no state change) to branch
// before a construction that might throw or land on the sentinel:
//   will_conversion_overflow<B>(v) — v falls outside B's interval.
//   will_conversion_trunc<B>(v) — v is in-range but off-notch (would round).
//   is_conversion_lossy<B>(v)      — OR of the two.
//---------------------------------------------------------------------------
namespace bnd
{
  template <boundable B, numeric A>
  [[nodiscard]] constexpr bool will_conversion_overflow(A value) noexcept
  {
    return not includes(Interval<B>, detail::as_rational(value));
  }

  template <boundable B, numeric A>
  [[nodiscard]] constexpr bool will_conversion_trunc(A value) noexcept
  {
    if constexpr (detail::rational_raw<B>)
      return false;                       // rational raw stores any value exactly
    bnd::detail::rational r = detail::as_rational(value);
    if (not includes(Interval<B>, r))
      return false;                       // out-of-range — overflow, not truncation
    // In-range: truncation occurs iff (value - Lower) / Notch is non-integer.
    auto offset = (r - Lower<B>) / Notch<B>;
    return !offset.has_value() || detail::abs_den(offset->Denominator) != 1;
  }

  template <boundable B, typename A>
  [[nodiscard]] constexpr bool is_conversion_lossy(A value) noexcept
  {
    return will_conversion_overflow<B>(value)
        || will_conversion_trunc<B>(value);
  }
} // namespace bnd

#endif // BNDpredicatesHPP
