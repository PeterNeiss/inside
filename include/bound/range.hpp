//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDrangeHPP
#define BNDrangeHPP

#include "bound/bound.hpp"

//---------------------------------------------------------------------------
// bound_range — range-based for loop support
//
// Iteration walks the grid by notch index, so any grid with a non-zero
// notch works (integer or fractional). Each step computes the value
// `Lower + index * Notch`, which is exact by construction. The iterator
// wraps modulo the slot count so a mid-range start visits every slot
// exactly once before terminating.
//---------------------------------------------------------------------------
namespace bnd
{
  template <grid G, policy_flag P = checked>
    requires (G.Notch != 0_r)
  struct bound_range
  {
    using value_type = bound<G, P>;
    static constexpr umax slot_count = NotchCount<value_type> + 1;

    struct iterator
    {
      umax index;
      imax remaining;

      constexpr value_type operator*() const
      {
        // value = Lower + index * Notch  (always exact: lies on the grid).
        rational val = (G.Interval.Lower
                        + (rational{index} * G.Notch).value()).value();
        return value_type{val};
      }
      constexpr iterator& operator++()
      {
        --remaining;
        index = (index + 1) % slot_count;
        return *this;
      }
      constexpr bool operator!=(iterator o) const { return remaining != o.remaining; }
    };

    umax start_index_;

    constexpr bound_range() : start_index_{0} {}

    constexpr bound_range(value_type start)
    {
      // Map a grid value back to its notch index: (start - Lower) / Notch.
      // The result has integer denominator (start is on the grid) so the
      // numerator is the index directly.
      auto offset = ((as_rational(start) - G.Interval.Lower)
                     / G.Notch).value();
      start_index_ = offset.Numerator;
    }

    constexpr iterator begin() const
    { return {start_index_, static_cast<imax>(slot_count)}; }
    constexpr iterator end() const
    { return {start_index_, 0}; }
  };

} // namespace bnd

#endif // BNDrangeHPP
