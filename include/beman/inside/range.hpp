// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_RANGE_HPP
#define BEMAN_INSIDE_RANGE_HPP

#include <beman/inside/core.hpp>

#include <compare>
#include <cstddef>
#include <iterator>
#include <limits>
#include <ranges>
#include <utility>

//---------------------------------------------------------------------------
// inside_range — random-access range over a grid. Walks by notch index (any
// non-zero notch), each `*it` computing the exact `Lower + index·Notch`; the
// iterator wraps modulo the slot count so a mid-range start visits every slot
// once. Models random_access_range + sized_range (so std::ranges algorithms
// work directly). iterator_category is input_iterator_tag because operator*
// returns by value; iterator_concept carries the random-access capability.
//---------------------------------------------------------------------------
namespace beman::inside {
namespace detail {
// enumerate_view — C++20 stand-in for std::views::enumerate (C++23), yielding
// pair<index, value> by value (all indexed() needs).
template <class R>
struct enumerate_view {
    R Base;

    struct iterator {
        std::ranges::iterator_t<const R> It{};
        std::size_t                      Index{0};

        using value_type      = std::pair<std::size_t, std::ranges::range_value_t<R>>;
        using difference_type = std::ptrdiff_t;

        [[nodiscard]] constexpr value_type operator*() const { return {Index, *It}; }
        constexpr iterator&                operator++() {
            ++It;
            ++Index;
            return *this;
        }
        constexpr iterator operator++(int) {
            auto t = *this;
            ++*this;
            return t;
        }
        [[nodiscard]] constexpr bool operator==(const iterator& o) const { return It == o.It; }
    };

    constexpr iterator begin() const { return {std::ranges::begin(Base), 0}; }
    constexpr iterator end() const { return {std::ranges::end(Base), 0}; }
};

// stride_view — stand-in for std::views::stride (likewise). Visits every
// `step`-th element; forward-only, and the advance checks `end` so a length
// that isn't a multiple of the stride still terminates.
template <class R>
struct stride_view {
    R           Base;
    std::size_t Step{1};

    struct iterator {
        std::ranges::iterator_t<const R> It{};
        std::ranges::iterator_t<const R> End{};
        std::size_t                      Step{1};

        using value_type      = std::ranges::range_value_t<R>;
        using difference_type = std::ptrdiff_t;

        [[nodiscard]] constexpr value_type operator*() const { return *It; }
        constexpr iterator&                operator++() {
            for (std::size_t k = 0; k < Step && It != End; ++k)
                ++It;
            return *this;
        }
        constexpr iterator operator++(int) {
            auto t = *this;
            ++*this;
            return t;
        }
        [[nodiscard]] constexpr bool operator==(const iterator& o) const { return It == o.It; }
    };

    constexpr iterator begin() const { return {std::ranges::begin(Base), std::ranges::end(Base), Step}; }
    constexpr iterator end() const { return {std::ranges::end(Base), std::ranges::end(Base), Step}; }
};
} // namespace detail

template <grid G, policy_flag P = checked>
    requires(G.Notch != 0)
struct inside_range {
    using value_type = inside<G, P>;
    static_assert(G.max_index_representable() && detail::max_index_v<value_type> < std::numeric_limits<umax>::max(),
                  "inside_range: the grid has more points than a 64-bit count can hold");
    static constexpr umax slot_count = detail::max_index_v<value_type> + 1;

    struct iterator {
        using iterator_concept  = std::random_access_iterator_tag;
        using iterator_category = std::input_iterator_tag;
        using value_type        = inside<G, P>;
        using difference_type   = imax;

        umax Start{0}; // slot of the first element (the range wraps past the top)
        imax Pos{0};   // position in [0, slot_count]; the loop variable

        constexpr iterator() = default;
        constexpr iterator(umax s, imax p) : Start{s}, Pos{p} {}

        // Grid slot of this position: Start + Pos, wrapped once (no overflow).
        constexpr umax slot() const {
            const umax p = static_cast<umax>(Pos);
            return p < slot_count - Start ? Start + p : p - (slot_count - Start);
        }

        [[nodiscard]] constexpr value_type operator*() const {
            // value = Lower + index * Notch (always exact: lies on the grid).
            // Integer-backed storages decode without the rational/assignment
            // engine: for index storage the iterator index IS the raw (it stays in
            // [0, max_index_v], which the raw type holds); integer-grid value
            // storage is a multiply-add in raw space. Rational/fp raws keep the exact generic path.
            if constexpr (detail::index_storage<value_type>)
                return value_type::from_raw(static_cast<typename value_type::raw_type>(slot()));
            else if constexpr (detail::integer_value_storage<value_type> &&
                               detail::abs_den(::beman::inside::detail::notch64<value_type>.Denominator) == 1 &&
                               detail::abs_den(::beman::inside::detail::lower64<value_type>.Denominator) == 1) {
                constexpr imax notch_step = static_cast<imax>(::beman::inside::detail::notch64<value_type>.Numerator);
                return value_type::from_raw(static_cast<typename value_type::raw_type>(
                    detail::lower_imax<value_type> + static_cast<imax>(slot()) * notch_step));
            } else {
                detail::rational val = (detail::to_rational(G.Interval.Lower) +
                                        (detail::rational{slot()} * detail::to_rational(G.Notch)).value())
                                           .value();
                return value_type{val};
            }
        }

        [[nodiscard]] constexpr value_type operator[](difference_type n) const { return *(*this + n); }

        constexpr iterator& operator++() {
            ++Pos;
            return *this;
        }
        constexpr iterator operator++(int) {
            auto t = *this;
            ++Pos;
            return t;
        }
        constexpr iterator& operator--() {
            --Pos;
            return *this;
        }
        constexpr iterator operator--(int) {
            auto t = *this;
            --Pos;
            return t;
        }
        constexpr iterator& operator+=(difference_type n) {
            Pos += n;
            return *this;
        }
        constexpr iterator& operator-=(difference_type n) {
            Pos -= n;
            return *this;
        }

        [[nodiscard]] constexpr iterator operator+(difference_type n) const {
            auto t = *this;
            t += n;
            return t;
        }
        [[nodiscard]] constexpr iterator operator-(difference_type n) const {
            auto t = *this;
            t -= n;
            return t;
        }
        [[nodiscard]] friend constexpr iterator operator+(difference_type n, iterator it) { return it + n; }

        [[nodiscard]] constexpr difference_type operator-(iterator o) const { return Pos - o.Pos; }
        [[nodiscard]] constexpr bool            operator==(iterator o) const { return Pos == o.Pos; }
        [[nodiscard]] constexpr auto            operator<=>(iterator o) const { return Pos <=> o.Pos; }
    };

    umax StartIndex;

    constexpr inside_range() : StartIndex{0} {}

    constexpr inside_range(value_type start) {
        // Map a grid value back to its notch index: (start - Lower) / Notch.
        // Same storage split as iterator::operator* — index raw already is the
        // notch index; integer-grid value raw divides out the (integer) step.
        if constexpr (detail::index_storage<value_type>)
            StartIndex = static_cast<umax>(start.raw());
        else if constexpr (detail::integer_value_storage<value_type> &&
                           detail::abs_den(::beman::inside::detail::notch64<value_type>.Denominator) == 1 &&
                           detail::abs_den(::beman::inside::detail::lower64<value_type>.Denominator) == 1) {
            constexpr imax notch_step = static_cast<imax>(::beman::inside::detail::notch64<value_type>.Numerator);
            StartIndex =
                static_cast<umax>((static_cast<imax>(start.raw()) - detail::lower_imax<value_type>) / notch_step);
        } else {
            // The result has integer denominator (start is on the grid) so the
            // numerator is the index directly.
            auto offset =
                ((detail::as_rational(start) - detail::to_rational(G.Interval.Lower)) / detail::to_rational(G.Notch))
                    .value();
            StartIndex = offset.Numerator;
        }
    }

    constexpr iterator begin() const { return {StartIndex, 0}; }
    constexpr iterator end() const { return {StartIndex, static_cast<imax>(slot_count)}; }

    constexpr std::size_t size() const { return slot_count; }

    // `indexed()` pairs each value with its zero-based position (≈ C++23
    // std::views::enumerate), via detail::enumerate_view for C++20.
    constexpr auto indexed() const { return detail::enumerate_view<inside_range>{*this}; }

    // `strided(step)` visits every `step`-th grid value (≈ C++23 std::views::
    // stride). `std::views::reverse` already works directly, so there's no reverse().
    constexpr auto strided(std::size_t step) const { return detail::stride_view<inside_range>{*this, step}; }
};

} // namespace beman::inside

#endif // BEMAN_INSIDE_RANGE_HPP
