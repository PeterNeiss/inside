// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_RANGE_HPP
#define BEMAN_INSIDE_RANGE_HPP

#include <beman/inside/core.hpp>

#include <compare>
#include <cstddef>
#include <iterator>
#include <limits>
#include <ranges>

//---------------------------------------------------------------------------
// inside_range — random-access range over a grid. Walks by notch index (any
// non-zero notch), each `*it` computing the exact `Lower + index·Notch`; the
// iterator wraps modulo the slot count so a mid-range start visits every slot
// once. Models random_access_range + sized_range (so std::ranges algorithms
// work directly). iterator_category is input_iterator_tag because operator*
// returns by value; iterator_concept carries the random-access capability.
//---------------------------------------------------------------------------
namespace beman::inside {
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
            // value = Lower + index * Notch: an integer raw (a range needs a
            // notch), slot() for an index raw, Lower + slot() for a value raw.
            return value_type::from_raw(detail::raw_from_offset<value_type>(slot()));
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
        // Map a grid value back to its notch index: the raw's offset.
        StartIndex = static_cast<umax>(start.raw()) - static_cast<umax>(detail::raw_lo<value_type>);
    }

    constexpr iterator begin() const { return {StartIndex, 0}; }
    constexpr iterator end() const { return {StartIndex, static_cast<imax>(slot_count)}; }

    constexpr std::size_t size() const { return slot_count; }
};

// values<T>: every value of T, Lower to Upper, as a random-access sized range.
// A cursor has none: its grid ends past T's Upper.
template <insidable T>
    requires(notch_of<T> != 0 && !has_flag(policy_of<T>, detail::cursor_marker))
inline constexpr inside_range<grid_of<T>, policy_of<T>> values{};

} // namespace beman::inside

#endif // BEMAN_INSIDE_RANGE_HPP
