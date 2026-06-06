//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDrangeHPP
#define BNDrangeHPP

#include "bound/bound.hpp"

#include <compare>
#include <cstddef>
#include <iterator>
#include <ranges>
#include <utility>

//---------------------------------------------------------------------------
// bound_range — random-access range over a grid.
//
// Iteration walks the grid by notch index, so any grid with a non-zero
// notch works (integer or fractional). Each `*it` computes the value
// `Lower + index * Notch`, which is exact by construction. The iterator
// wraps modulo the slot count so a mid-range start visits every slot
// exactly once before terminating.
//
// Models `std::ranges::random_access_range` and `std::ranges::sized_range`
// so `std::ranges::sort`, `std::views::take`, `std::views::reverse` etc.
// work without falling back to `common_iterator`.
//
// `iterator_category` is `input_iterator_tag` (operator* returns by value,
// which is fine for random_access — `iterator_concept` carries the real
// capability per the std::ranges concept hierarchy).
//---------------------------------------------------------------------------
namespace bnd
{
  namespace detail
  {
    // enumerate_view — uniform C++20 stand-in for `std::views::enumerate`
    // (C++23). Wraps a range and yields `pair<index, value>` by value, which
    // is all `indexed()` needs: bound_range's iterator already returns each
    // value by value, so there is nothing to bind a reference to. One code
    // path on every compiler (no __cpp_lib_ranges_enumerate branch).
    template <class R>
    struct enumerate_view
    {
      R base_;

      struct iterator
      {
        std::ranges::iterator_t<const R> it{};
        std::size_t index{0};

        using value_type      = std::pair<std::size_t, std::ranges::range_value_t<R>>;
        using difference_type  = std::ptrdiff_t;

        constexpr value_type operator*() const { return {index, *it}; }
        constexpr iterator& operator++() { ++it; ++index; return *this; }
        constexpr iterator  operator++(int) { auto t = *this; ++*this; return t; }
        constexpr bool operator==(iterator const& o) const { return it == o.it; }
      };

      constexpr iterator begin() const { return {std::ranges::begin(base_), 0}; }
      constexpr iterator end()   const { return {std::ranges::end(base_), 0}; }
    };
  } // namespace detail

  template <grid G, policy_flag P = checked>
    requires (G.Notch != 0)
  struct bound_range
  {
    using value_type = bound<G, P>;
    static constexpr umax slot_count = NotchCount<value_type> + 1;

    struct iterator
    {
      using iterator_concept  = std::random_access_iterator_tag;
      using iterator_category = std::input_iterator_tag;
      using value_type        = bound<G, P>;
      using difference_type   = imax;

      umax index     {0};
      imax remaining {0};

      constexpr iterator() = default;
      constexpr iterator(umax i, imax r) : index{i}, remaining{r} {}

      constexpr value_type operator*() const
      {
        // value = Lower + index * Notch  (always exact: lies on the grid).
        bnd::detail::rational val = (G.Interval.Lower
                        + (bnd::detail::rational{index} * G.Notch).value()).value();
        return value_type{val};
      }

      constexpr value_type operator[](difference_type n) const
      { return *(*this + n); }

      // Advance / retreat: `remaining` is the position counter (advancing
      // decreases it), so the <=> below flips the comparison.
      constexpr iterator& operator++() { return *this += 1; }
      constexpr iterator  operator++(int) { auto t = *this; ++*this; return t; }
      constexpr iterator& operator--() { return *this -= 1; }
      constexpr iterator  operator--(int) { auto t = *this; --*this; return t; }

      constexpr iterator& operator+=(difference_type n)
      {
        constexpr imax M = static_cast<imax>(slot_count);
        imax i = static_cast<imax>(index) + n;
        // euclidean mod so negative n still lands in [0, slot_count)
        i = ((i % M) + M) % M;
        index = static_cast<umax>(i);
        remaining -= n;
        return *this;
      }
      constexpr iterator& operator-=(difference_type n) { return *this += -n; }

      constexpr iterator operator+(difference_type n) const { auto t = *this; t += n; return t; }
      constexpr iterator operator-(difference_type n) const { auto t = *this; t -= n; return t; }
      friend constexpr iterator operator+(difference_type n, iterator it) { return it + n; }

      constexpr difference_type operator-(iterator o) const
      { return o.remaining - remaining; }

      constexpr bool operator==(iterator o) const { return remaining == o.remaining; }
      constexpr auto operator<=>(iterator o) const { return o.remaining <=> remaining; }
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

    constexpr std::size_t size() const { return static_cast<std::size_t>(slot_count); }

    // `indexed()` pairs each value with its zero-based position, mirroring
    // the C++23 `std::views::enumerate` adapter. Convenient sugar for the
    // common "index alongside value" pattern in lookup-table examples.
    // Uses detail::enumerate_view so C++20 / GCC 12 builds work unchanged.
    constexpr auto indexed() const { return detail::enumerate_view<bound_range>{*this}; }
  };

} // namespace bnd

#endif // BNDrangeHPP
