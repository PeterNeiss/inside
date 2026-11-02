// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_INSIDE_RANGE_HPP
#define BEMAN_INSIDE_INSIDE_RANGE_HPP

#include <beman/inside/inside/detail/config.hpp>

#include <beman/inside/inside/compare.hpp> // the inside type and its comparisons

#include <compare>
#include <cstddef>
#include <iterator>
#include <limits>
#include <ranges>

//---------------------------------------------------------------------------
// inside_range — random-access range over a grid, and values<T>, the same
// range named by a type (every value of T; of a quantity type, its values as
// quantities). Walks by notch index (any
// non-zero notch), each `*it` computing the exact `Lower + index·Notch`; the
// iterator wraps modulo the slot count so a mid-range start visits every slot
// once. A random-access sized view (view_interface adds [], front, back,
// empty) and a borrowed range: its iterators never refer to it, so an
// algorithm on a temporary still returns an iterator. iterator_category is
// input_iterator_tag because operator* returns by value; iterator_concept
// carries the random-access capability.
//---------------------------------------------------------------------------
namespace beman::inside {
template <grid G, policy_flag P = checked, class V = inside<G, P>>
    requires(G.Notch != 0)
struct inside_range : std::ranges::view_interface<inside_range<G, P, V>> {
    using number_type = inside<G, P>;
    using value_type  = V; // number_type, or a quantity on it
    // The iterator's difference is an imax: end() − begin() must be the count.
    static_assert(G.max_index_representable() &&
                      detail::max_index_of<number_type> < static_cast<umax>(std::numeric_limits<imax>::max()),
                  "inside_range: the grid has more points than an iterator difference (int64) can count");
    static constexpr umax slot_count = detail::max_index_of<number_type> + 1;

  private:
    // The slot and position in 32 bits where the slot count fits: a loop over
    // the range then vectorizes in 32-bit lanes, as a native counting loop
    // does (64-bit ones hold half as many).
    static constexpr bool narrow = slot_count <= umax{0x7fff'ffff};
    using slot_t                 = std::conditional_t<narrow, std::uint32_t, umax>;
    using pos_t                  = std::conditional_t<narrow, std::int32_t, imax>;

  public:
    struct iterator {
        using iterator_concept  = std::random_access_iterator_tag;
        using iterator_category = std::input_iterator_tag;
        using value_type        = V;
        using difference_type   = imax;

        slot_t Start{0}; // slot of the first element (the range wraps past the top)
        pos_t  Pos{0};   // position in [0, slot_count]; the loop variable

        constexpr iterator() = default;
        constexpr iterator(umax s, imax p) : Start{static_cast<slot_t>(s)}, Pos{static_cast<pos_t>(p)} {}

        // Grid slot of this position: Start + Pos, wrapped once (no overflow).
        constexpr umax slot() const {
            constexpr slot_t count = static_cast<slot_t>(slot_count);
            const slot_t     p     = static_cast<slot_t>(Pos);
            return p < count - Start ? Start + p : p - (count - Start);
        }

        [[nodiscard]] constexpr value_type operator*() const {
            // value = Lower + index * Notch: an integer raw (a range needs a
            // notch), slot() for an index raw, Lower + slot() for a value raw.
            const auto n = number_type::from_raw(detail::raw_from_offset<number_type>(slot()));
            if constexpr (std::same_as<V, number_type>)
                return n;
            else
                return V{n};
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
            Pos = static_cast<pos_t>(Pos + n);
            return *this;
        }
        constexpr iterator& operator-=(difference_type n) {
            Pos = static_cast<pos_t>(Pos - n);
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

        [[nodiscard]] constexpr difference_type operator-(iterator o) const {
            return static_cast<difference_type>(Pos) - o.Pos;
        }
        [[nodiscard]] constexpr bool operator==(iterator o) const { return Pos == o.Pos; }
        [[nodiscard]] constexpr auto operator<=>(iterator o) const { return Pos <=> o.Pos; }
    };

    umax StartIndex{0};

    constexpr inside_range() = default;
    // A grid value's notch index: its raw's offset.
    constexpr inside_range(number_type start) : StartIndex{detail::raw_offset<number_type>(start.raw())} {}

    constexpr iterator begin() const { return {StartIndex, 0}; }
    constexpr iterator end() const { return {StartIndex, static_cast<imax>(slot_count)}; }

    constexpr std::size_t size() const { return slot_count; }
};

// values<T>: every value of T, Lower to Upper, as a random-access sized range;
// of a quantity type, its number's values as quantities. A cursor has none:
// its grid ends past T's Upper.
template <detail::steppable T>
    requires(notch_of<detail::step_number_t<T>> != 0 &&
             !has_flag(flags_of<detail::step_number_t<T>>, detail::cursor_marker))
inline constexpr inside_range<grid_of<detail::step_number_t<T>>, flags_of<detail::step_number_t<T>>, T> values{};

} // namespace beman::inside

template <beman::inside::grid G, beman::inside::policy_flag P, class V>
inline constexpr bool std::ranges::enable_borrowed_range<beman::inside::inside_range<G, P, V>> = true;

#endif // BEMAN_INSIDE_INSIDE_RANGE_HPP
