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
// inside_range — a range over a grid, and values<T>, the same range named by
// a type (every value of T; of a quantity type, its values as quantities).
// Walks by notch index (any non-zero notch), each `*it` computing the exact
// `Lower + index·Notch`; the iterator wraps modulo the slot count so a
// mid-range start visits every slot once. Up to 2^63 values a random-access
// sized view (view_interface adds [], front, back, empty): end() − begin(),
// an imax, is the count. Past that (uint64, int64, a grid past 64 bits) a
// bidirectional view without a size, its position a wide integer, as a
// cursor walks. A borrowed range: its iterators never refer to it, so an
// algorithm on a temporary still returns an iterator. iterator_category is
// input_iterator_tag because operator* returns by value; iterator_concept
// carries the capability.
//---------------------------------------------------------------------------
namespace beman::inside {
template <grid G, policy_flag P = checked, class V = inside<G, P>>
    requires(G.Notch != 0)
struct inside_range : std::ranges::view_interface<inside_range<G, P, V>> {
    using number_type = inside<G, P>;
    using value_type  = V; // number_type, or a quantity on it
    // Random access while end() − begin(), an imax, can be the count.
    static constexpr bool random_access =
        detail::max_index_representable(G) &&
        detail::max_index_of<number_type> < static_cast<umax>(std::numeric_limits<imax>::max());
    static constexpr umax slot_count = random_access ? detail::max_index_of<number_type> + 1 : 0;

  private:
    // The value at a slot (an offset from Lower).
    template <class S>
    static constexpr value_type value_at(const S& slot) {
        const auto n = number_type::from_raw(detail::raw_of_slot<number_type>(slot));
        if constexpr (std::same_as<V, number_type>)
            return n;
        else
            return V{n};
    }

    // The slot and position in 32 bits where the slot count fits: a loop over
    // the range then vectorizes in 32-bit lanes, as a native counting loop
    // does (64-bit ones hold half as many).
    static constexpr bool narrow = slot_count <= umax{0x7fff'ffff};
    using slot_t                 = std::conditional_t<narrow, std::uint32_t, umax>;
    using pos_t                  = std::conditional_t<narrow, std::int32_t, imax>;

    struct counted_iterator {
        using iterator_concept  = std::random_access_iterator_tag;
        using iterator_category = std::input_iterator_tag;
        using value_type        = V;
        using difference_type   = imax;

        slot_t Start{0}; // slot of the first element (the range wraps past the top)
        pos_t  Pos{0};   // position in [0, slot_count]; the loop variable

        constexpr counted_iterator() = default;
        constexpr counted_iterator(umax s, imax p) : Start{static_cast<slot_t>(s)}, Pos{static_cast<pos_t>(p)} {}

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

        constexpr counted_iterator& operator++() {
            ++Pos;
            return *this;
        }
        constexpr counted_iterator operator++(int) {
            auto t = *this;
            ++Pos;
            return t;
        }
        constexpr counted_iterator& operator--() {
            --Pos;
            return *this;
        }
        constexpr counted_iterator operator--(int) {
            auto t = *this;
            --Pos;
            return t;
        }
        constexpr counted_iterator& operator+=(difference_type n) {
            Pos = static_cast<pos_t>(Pos + n);
            return *this;
        }
        constexpr counted_iterator& operator-=(difference_type n) {
            Pos = static_cast<pos_t>(Pos - n);
            return *this;
        }

        [[nodiscard]] constexpr counted_iterator operator+(difference_type n) const {
            auto t = *this;
            t += n;
            return t;
        }
        [[nodiscard]] constexpr counted_iterator operator-(difference_type n) const {
            auto t = *this;
            t -= n;
            return t;
        }
        [[nodiscard]] friend constexpr counted_iterator operator+(difference_type n, counted_iterator it) {
            return it + n;
        }

        [[nodiscard]] constexpr difference_type operator-(counted_iterator o) const {
            return static_cast<difference_type>(Pos) - o.Pos;
        }
        [[nodiscard]] constexpr bool operator==(counted_iterator o) const { return Pos == o.Pos; }
        [[nodiscard]] constexpr auto operator<=>(counted_iterator o) const { return Pos <=> o.Pos; }
    };

    // Past 2^63 values: the slot and position in a signed wide integer that
    // holds the slot count (2^64 for uint64), stepping by one.
    using wide_pos_t                      = detail::wide_sint<detail::limbs_for_bits(G.slot_bits() + 2)>;
    static constexpr wide_pos_t pos_count = static_cast<wide_pos_t>(G.step_count() + detail::grid_int{1});

    struct stepping_iterator {
        using iterator_concept  = std::bidirectional_iterator_tag;
        using iterator_category = std::input_iterator_tag;
        using value_type        = V;
        using difference_type   = imax; // no distance is formed

        wide_pos_t Start{0}; // slot of the first element (the range wraps past the top)
        wide_pos_t Pos{0};   // position in [0, slot count]

        [[nodiscard]] constexpr value_type operator*() const {
            const wide_pos_t s = Start + Pos; // wrapped once: both below the count
            return value_at(pos_count < s || pos_count == s ? s - pos_count : s);
        }
        constexpr stepping_iterator& operator++() {
            Pos = Pos + wide_pos_t{1};
            return *this;
        }
        constexpr stepping_iterator operator++(int) {
            auto t = *this;
            ++*this;
            return t;
        }
        constexpr stepping_iterator& operator--() {
            Pos = Pos - wide_pos_t{1};
            return *this;
        }
        constexpr stepping_iterator operator--(int) {
            auto t = *this;
            --*this;
            return t;
        }
        [[nodiscard]] constexpr bool operator==(const stepping_iterator& o) const { return Pos == o.Pos; }
    };
    using start_t = std::conditional_t<random_access, umax, wide_pos_t>;

    // A grid value's notch index: its raw's offset (a wide index raw is it).
    static constexpr start_t start_of(const number_type& start) {
        if constexpr (detail::wide_index_storage<number_type>)
            return static_cast<start_t>(start.raw());
        else
            return static_cast<start_t>(detail::raw_offset<number_type>(start.raw()));
    }

  public:
    using iterator = std::conditional_t<random_access, counted_iterator, stepping_iterator>;

    start_t StartIndex{0};

    constexpr inside_range() = default;
    constexpr inside_range(number_type start) : StartIndex{start_of(start)} {}

    constexpr iterator begin() const {
        if constexpr (random_access)
            return {StartIndex, 0};
        else
            return {StartIndex, wide_pos_t{0}};
    }
    constexpr iterator end() const {
        if constexpr (random_access)
            return {StartIndex, static_cast<imax>(slot_count)};
        else
            return {StartIndex, pos_count};
    }

    constexpr std::size_t size() const
        requires random_access
    {
        return slot_count;
    }
};

// values<T>: every value of T, Lower to Upper (random access and sized up to
// 2^63 values, bidirectional past that); of a quantity type, its number's
// values as quantities. A cursor has none: its grid ends past T's Upper.
template <detail::steppable T>
    requires(notch_of<detail::step_number_t<T>> != 0 &&
             !has_flag(flags_of<detail::step_number_t<T>>, detail::cursor_marker))
inline constexpr inside_range<grid_of<detail::step_number_t<T>>, flags_of<detail::step_number_t<T>>, T> values{};

} // namespace beman::inside

template <beman::inside::grid G, beman::inside::policy_flag P, class V>
inline constexpr bool std::ranges::enable_borrowed_range<beman::inside::inside_range<G, P, V>> = true;

#endif // BEMAN_INSIDE_INSIDE_RANGE_HPP
