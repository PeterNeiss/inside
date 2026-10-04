// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_PREDICATES_HPP
#define BEMAN_INSIDE_PREDICATES_HPP

#include <beman/inside/generic.hpp>
#include <beman/inside/grid.hpp>
#include <beman/inside/policy.hpp>

//---------------------------------------------------------------------------
// predicates — pure inspection (no conversion, no state change) to branch
// before a construction that might throw or report an error:
//   conversion_overflows<B>(v) — v falls outside B's interval.
//   conversion_rounds<B>(v)    — v is in-range but off-notch (would round).
//   conversion_is_lossy<B>(v)  — either of the two.
//---------------------------------------------------------------------------
namespace beman::inside
{
  template <insidable B, numeric A>
  [[nodiscard]] constexpr bool conversion_overflows(A value) noexcept
  {
    if constexpr (std::floating_point<A>)
      if (!(value - value == 0)) return true;   // NaN / ±inf fit no grid (and must not raise here)
    if constexpr (std::floating_point<A>)
      if (!(value < 0x1p64 && value > -0x1p64)) return true;   // beyond every grid, no rational form
    const detail::rational r = detail::as_rational(value);
    if (includes(interval_of<B>, r))
      return false;
    // B's policy rounds before it range-checks: a value that rounds onto the
    // grid does not overflow.
    detail::rational rounded;
    return !detail::rounds_into_range<B, policy<>>(r, rounded);
  }

  template <insidable B, numeric A>
  [[nodiscard]] constexpr bool conversion_rounds(A value) noexcept
  {
    if constexpr (::beman::inside::detail::notch64<B> == 0)
      return false;                       // continuous grid: no notch to miss
    if constexpr (std::floating_point<A>)
      if (!(value - value == 0)) return false;   // non-finite — overflow, not truncation
    if constexpr (std::floating_point<A>)
      if (!(value < 0x1p64 && value > -0x1p64)) return false;   // overflow, not truncation
    detail::rational r = detail::as_rational(value);
    if (not includes(interval_of<B>, r))
    {
      // Out of range: a rounding policy that brings it onto the grid rounds;
      // anything else is overflow, not rounding.
      detail::rational rounded;
      return detail::rounds_into_range<B, policy<>>(r, rounded);
    }
    // In-range: truncation occurs iff (value - Lower) / Notch is non-integer.
    auto offset = (r - ::beman::inside::detail::lower64<B>) / ::beman::inside::detail::notch64<B>;
    return !offset.has_value() || detail::abs_den(offset->Denominator) != 1;
  }

  template <insidable B, numeric A>
  [[nodiscard]] constexpr bool conversion_is_lossy(A value) noexcept
  {
    return conversion_overflows<B>(value)
        || conversion_rounds<B>(value);
  }
} // namespace beman::inside

#endif // BEMAN_INSIDE_PREDICATES_HPP
