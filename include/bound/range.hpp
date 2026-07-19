//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDrangeHPP
#define BNDrangeHPP

#include "bound/core.hpp"

#include <compare>
#include <cstddef>
#include <iterator>
#include <ranges>
#include <utility>

//---------------------------------------------------------------------------
// bound_range — random-access range over a grid. Walks by notch index (any
// non-zero notch), each `*it` computing the exact `Lower + index·Notch`; the
// iterator wraps modulo the slot count so a mid-range start visits every slot
// once. Models random_access_range + sized_range (so std::ranges algorithms
// work directly). iterator_category is input_iterator_tag because operator*
// returns by value; iterator_concept carries the real random-access capability.
//---------------------------------------------------------------------------
namespace bnd
{
  namespace detail
  {
    // enumerate_view — C++20 stand-in for std::views::enumerate (C++23), yielding
    // pair<index, value> by value (all indexed() needs).
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

    // stride_view — C++20 stand-in for std::views::stride (C++23). Visits every
    // `step`-th element; forward-only, and the advance checks `end` so a length
    // that isn't a multiple of the stride still terminates.
    template <class R>
    struct stride_view
    {
      R base_;
      std::size_t step_{1};

      struct iterator
      {
        std::ranges::iterator_t<const R> it{};
        std::ranges::iterator_t<const R> end{};
        std::size_t step{1};

        using value_type      = std::ranges::range_value_t<R>;
        using difference_type = std::ptrdiff_t;

        constexpr value_type operator*() const { return *it; }
        constexpr iterator& operator++()
        {
          for (std::size_t k = 0; k < step && it != end; ++k) ++it;
          return *this;
        }
        constexpr iterator operator++(int) { auto t = *this; ++*this; return t; }
        constexpr bool operator==(iterator const& o) const { return it == o.it; }
      };

      constexpr iterator begin() const
      { return {std::ranges::begin(base_), std::ranges::end(base_), step_}; }
      constexpr iterator end() const
      { return {std::ranges::end(base_), std::ranges::end(base_), step_}; }
    };
  } // namespace detail

  template <grid G, policy_flag P = checked>
    requires (G.Notch != 0)
  struct bound_range
  {
    using value_type = bound<G, P>;
    static constexpr umax slot_count = detail::NotchCount<value_type> + 1;

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
        // value = Lower + index * Notch (always exact: lies on the grid).
        // Integer-backed storages decode without the rational/assignment
        // engine: for index storage the iterator index IS the raw (it stays in
        // [0, NotchCount] and the sentinel slot is a numeric_limits extreme
        // outside that span); integer-grid value storage is a multiply-add in
        // raw space. Rational/fp raws keep the exact generic path.
        if constexpr (bnd::detail::index_raw<value_type>)
          return value_type::from_raw(
              static_cast<typename value_type::raw_type>(index));
        else if constexpr (bnd::detail::value_raw<value_type>
                           && bnd::detail::abs_den(Notch<value_type>.Denominator) == 1
                           && bnd::detail::abs_den(Lower<value_type>.Denominator) == 1)
        {
          constexpr imax notch_step = static_cast<imax>(Notch<value_type>.Numerator);
          return value_type::from_raw(static_cast<typename value_type::raw_type>(
              bnd::detail::LowerImax<value_type>
              + static_cast<imax>(index) * notch_step));
        }
        else
        {
          bnd::detail::rational val = (G.Interval.Lower
                          + (bnd::detail::rational{index} * G.Notch).value()).value();
          return value_type{val};
        }
      }

      constexpr value_type operator[](difference_type n) const
      { return *(*this + n); }

      // Advance / retreat: `remaining` is the position counter (advancing
      // decreases it), so the <=> below flips the comparison. Single steps
      // wrap by compare instead of the euclidean mod in `+= n` (index stays
      // in [0, slot_count) — the mod would cost two divides per element).
      constexpr iterator& operator++()
      {
        index = (index + 1 == slot_count) ? 0 : index + 1;
        --remaining;
        return *this;
      }
      constexpr iterator  operator++(int) { auto t = *this; ++*this; return t; }
      constexpr iterator& operator--()
      {
        index = (index == 0) ? slot_count - 1 : index - 1;
        ++remaining;
        return *this;
      }
      constexpr iterator  operator--(int) { auto t = *this; --*this; return t; }

      constexpr iterator& operator+=(difference_type n)
      {
        constexpr imax M = slot_count;
        imax i = static_cast<imax>(index) + n;
        // euclidean mod so negative n still lands in [0, slot_count)
        i = ((i % M) + M) % M;
        index = i;
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
      // Same storage split as iterator::operator* — index raw already is the
      // notch index; integer-grid value raw divides out the (integer) step.
      if constexpr (detail::index_raw<value_type>)
        start_index_ = static_cast<umax>(start.raw());
      else if constexpr (detail::value_raw<value_type>
                         && detail::abs_den(Notch<value_type>.Denominator) == 1
                         && detail::abs_den(Lower<value_type>.Denominator) == 1)
      {
        constexpr imax notch_step = static_cast<imax>(Notch<value_type>.Numerator);
        start_index_ = static_cast<umax>(
            (static_cast<imax>(start.raw()) - detail::LowerImax<value_type>)
            / notch_step);
      }
      else
      {
        // The result has integer denominator (start is on the grid) so the
        // numerator is the index directly.
        auto offset = ((detail::as_rational(start) - G.Interval.Lower)
                       / G.Notch).value();
        start_index_ = offset.Numerator;
      }
    }

    constexpr iterator begin() const
    { return {start_index_, static_cast<imax>(slot_count)}; }
    constexpr iterator end() const
    { return {start_index_, 0}; }

    constexpr std::size_t size() const { return slot_count; }

    // `indexed()` pairs each value with its zero-based position (≈ C++23
    // std::views::enumerate), via detail::enumerate_view for C++20.
    constexpr auto indexed() const { return detail::enumerate_view<bound_range>{*this}; }

    // `strided(step)` visits every `step`-th grid value (≈ C++23 std::views::
    // stride). `std::views::reverse` already works directly, so there's no reverse().
    constexpr auto strided(std::size_t step) const
    { return detail::stride_view<bound_range>{*this, step}; }
  };

} // namespace bnd

#endif // BNDrangeHPP
