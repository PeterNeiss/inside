// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_PREDICATES_HPP
#define BEMAN_INSIDE_PREDICATES_HPP

#include <beman/inside/generic.hpp>
#include <beman/inside/grid.hpp>

//---------------------------------------------------------------------------
// predicates — pure inspection (no conversion, no state change) to branch
// before a construction that might throw or report an error:
//   will_conversion_overflow<B>(v) — v falls outside B's interval.
//   will_conversion_trunc<B>(v) — v is in-range but off-notch (would round).
//   is_conversion_lossy<B>(v)      — OR of the two.
//---------------------------------------------------------------------------
namespace beman::inside
{
  template <insidable B, numeric A>
  [[nodiscard]] constexpr bool will_conversion_overflow(A value) noexcept
  {
    if constexpr (std::floating_point<A>)
      if (!(value - value == 0)) return true;   // NaN / ±inf fit no grid (and must not raise here)
    return not includes(interval_of<B>, detail::as_rational(value));
  }

  template <insidable B, numeric A>
  [[nodiscard]] constexpr bool will_conversion_trunc(A value) noexcept
  {
    if constexpr (notch_of<B> == 0)
      return false;                       // continuous grid: no notch to miss
    if constexpr (std::floating_point<A>)
      if (!(value - value == 0)) return false;   // non-finite — overflow, not truncation
    detail::rational r = detail::as_rational(value);
    if (not includes(interval_of<B>, r))
      return false;                       // out-of-range — overflow, not truncation
    // In-range: truncation occurs iff (value - Lower) / Notch is non-integer.
    auto offset = (r - lower_of<B>) / notch_of<B>;
    return !offset.has_value() || detail::abs_den(offset->Denominator) != 1;
  }

  template <insidable B, typename A>
  [[nodiscard]] constexpr bool is_conversion_lossy(A value) noexcept
  {
    return will_conversion_overflow<B>(value)
        || will_conversion_trunc<B>(value);
  }
} // namespace beman::inside

#endif // BEMAN_INSIDE_PREDICATES_HPP
