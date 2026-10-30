// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// cursor<T, Step>: a loop variable that steps through T's grid and one step
// past its Upper, so the classic loop needs no value T cannot hold.

#include <beman/inside/inside.hpp>
#include <beman/inside/cmath.hpp>
#include <beman/inside/io.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <iterator>
#include <ranges>
#include <type_traits>
#include <vector>

using namespace beman::inside;
using namespace beman::inside::detail;

namespace {
using time_t_ = inside<{{-4, 4}, per<1024>}, round_nearest>;
using cur     = cursor<time_t_, per<4>>;

template <typename B>
concept has_end = requires(B b) { end(b); };

std::vector<rational> quarter_steps() {
    std::vector<rational> v;
    for (int k = -16; k <= 16; ++k)
        v.push_back(rational{k, 4});
    return v;
}
} // namespace

TEST(Cursor, type) {
    static_assert(grid_of<cur> == grid{{-4, 4.25_r}, 0.25_r});
    static_assert(has_flag(flags_of<cur>, round_nearest));
    static_assert(has_flag(flags_of<cur>, cursor_marker));
    static_assert(index_storage<cur>);
    static_assert(std::is_same_v<raw_t<cur>, std::uint8_t>);
    // a whole-number cursor stores its value, and still starts at Lower
    static_assert(integer_value_storage<cursor<inside<{5, 100}>>>);
    static_assert(rational{cursor<inside<{5, 100}>>{}} == 5);
    // the step as a rational, per<N> or an integer; T's notch by default
    static_assert(std::is_same_v<cursor<time_t_, 0.25_r>, cur>);
    static_assert(std::is_same_v<cursor<inside<{0, 10}>, 2>, cursor<inside<{0, 10}>, rational{2}>>);
    static_assert(grid_of<cursor<inside<{0, 10}>>> == grid{{0, 11}, 1});
    // a cursor over an `uninitialized` T still starts at Lower
    static_assert(rational{cursor<inside<{5, 100}, uninitialized>>{}} == 5);
    static_assert(!std::is_trivially_default_constructible_v<cursor<inside<{5, 100}, uninitialized>>>);
}

TEST(Cursor, three_loop_forms) {
    const auto            want = quarter_steps();
    std::vector<rational> a, b, c;
    for (cur t = -4_ins; t <= 4_ins; ++t)
        a.push_back(static_cast<rational>(t));
    for (cur t = -4_ins; t != end(t); ++t)
        b.push_back(static_cast<rational>(t));
    for (cur t; t != end(t); ++t)
        c.push_back(static_cast<rational>(t));
    EXPECT_EQ(a, want);
    EXPECT_EQ(b, want);
    EXPECT_EQ(c, want);

    cur t;
    for (; t <= 4_ins; ++t) {
    }
    EXPECT_EQ(static_cast<rational>(t), 4.25_r);
    EXPECT_TRUE(t == end(t));
    EXPECT_EQ(static_cast<rational>(end(t)), 4.25_r);
}

TEST(Cursor, default_starts_at_lower) {
    static_assert([] {
        cur t;
        return t == -4_ins;
    }());
    using off = cursor<inside<{{0.25_r, 1.25_r}, per<4>}>, 0.5_r>;
    static_assert([] {
        off t;
        return t == 0.25_ins;
    }());
    std::vector<rational> got;
    for (off t; t != end(t); ++t)
        got.push_back(static_cast<rational>(t));
    EXPECT_EQ(got, (std::vector<rational>{0.25_r, 0.75_r, 1.25_r}));

    using neg = cursor<inside<{-7, -1}>, 3>;
    neg n;
    EXPECT_EQ(static_cast<rational>(n), rational{-7});
}

TEST(Cursor, steps_and_ends) {
    cur t;
    EXPECT_THROW(--t, inside_error); // nothing before Lower
    cur u = 4_ins;
    ++u;                             // the end
    EXPECT_THROW(++u, inside_error); // nothing past the end
    // the values convert back to T exactly
    for (cur v; v != end(v); ++v) {
        const time_t_ x = v;
        EXPECT_EQ(static_cast<rational>(x), static_cast<rational>(v));
    }
    // a cursor works where an inside does
    using amp = inside<{{0, 64}, per<16384>}, round_nearest>;
    cur w     = 1_ins;
    EXPECT_EQ(math::exp_into<amp>(w), math::exp_into<amp>(time_t_{1}));
    EXPECT_EQ(to_string(w), "1");
}

TEST(Cursor, marker_does_not_leak) {
    cur t = 1_ins;
    static_assert(!has_flag(flags_of<decltype(t + t)>, cursor_marker));
    static_assert(!has_flag(flags_of<decltype(t * t)>, cursor_marker));
    static_assert(!has_flag(flags_of<decltype(-t)>, cursor_marker));
    static_assert(!has_flag(flags_of<decltype(math::exp(t))>, cursor_marker));
    static_assert(!has_flag(flags_of<decltype(math::abs(t))>, cursor_marker));
    static_assert(!has_end<time_t_>); // only a cursor has an end
    static_assert(has_end<cur>);
}

TEST(Cursor, increment_moves_one_notch) {
    inside<{{0, 2}, per<4>}> q{0.5};
    ++q;
    EXPECT_EQ(static_cast<rational>(q), 0.75_r);
    q--;
    EXPECT_EQ(static_cast<rational>(q), 0.5_r);
    inside<{0, 10}> i{3};
    ++i;
    EXPECT_EQ(static_cast<rational>(i), rational{4});
}

TEST(Cursor, point_compare_is_a_slot_compare) {
    static_assert(point_is_slot<cur, decltype(4_ins)>);
    static_assert(point_is_slot<cur, decltype(end(cur{}))>);
    static_assert(!point_is_slot<cur, decltype(0.1_ins)>); // not a slot
    for (cur t; t != end(t); ++t) {
        const rational v = static_cast<rational>(t);
        EXPECT_EQ(t < 1.5_ins, v < 1.5_r);
        EXPECT_EQ(t == 1.5_ins, v == 1.5_r);
        EXPECT_EQ(2_ins <= t, rational{2} <= v);
        EXPECT_EQ(t < 0.1_ins, v < 0.1_r); // the general path
    }
}

// A cursor is its own bidirectional iterator; end(t) is its sentinel.
static_assert(std::bidirectional_iterator<cur>);
static_assert(!std::random_access_iterator<cur>); // t - u is arithmetic, not a distance
static_assert(std::sentinel_for<decltype(end(cur{})), cur>);
static_assert(!std::sized_sentinel_for<cur, cur>);
static_assert(std::same_as<std::iter_difference_t<cur>, imax>);
static_assert(std::same_as<std::iter_value_t<cur>, inside<{{-4, 4}, per<4>}, round_nearest>>);
static_assert(std::convertible_to<std::iter_value_t<cur>, time_t_>);
// plain insides stay numbers, not iterators
static_assert(!std::weakly_incrementable<time_t_>);
static_assert(!std::indirectly_readable<time_t_>);
static_assert(!std::ranges::range<time_t_>);

TEST(Cursor, is_an_iterator) {
    const auto            want = quarter_steps();
    std::vector<rational> a, b, c;
    cur                   t;
    for (time_t_ x : std::ranges::subrange(t, end(t)))
        a.push_back(static_cast<rational>(x));
    for (cur u : std::views::iota(cur{}, end(cur{})))
        b.push_back(static_cast<rational>(u));
    std::ranges::for_each(std::ranges::subrange(t, end(t)), [&](auto x) { c.push_back(static_cast<rational>(x)); });
    EXPECT_EQ(a, want);
    EXPECT_EQ(b, want);
    EXPECT_EQ(c, want);
    EXPECT_EQ(std::ranges::distance(t, end(t)), 33);
    EXPECT_EQ(static_cast<rational>(*std::ranges::find(t, end(t), 1.5_ins)), 1.5_r);

    static_assert([] {
        cursor<inside<{0, 9}>, 3> i;
        return std::ranges::count_if(i, end(i), [](auto v) { return v > 2_ins; }) == 3;
    }());

    cur e = end(t);
    EXPECT_THROW((void)*e, inside_error); // the end has no value
}
