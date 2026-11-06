// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Run-time values are insides, never grid numbers: a grid number past 64 bits
// exists only at compile time, while an inside holds any value of its grid in
// a raw sized from it. index(), from_index, index_t, step_count_of and the
// numerator/denominator insides, on grids past 2^64 points and within them.
// The inputs pass through a volatile, so every computation runs at run time.

#include <beman/inside/inside.hpp>
#include <beman/inside/inside/formats.hpp>
#include <beman/inside/inside/io.hpp>

#include <gtest/gtest.h>

#include <expected>
#include <type_traits>

using namespace beman::inside;
using detail::big_int;
using detail::grid_rational;

namespace {
consteval big_int       pow2(int k) { return big_int{1} << k; }
consteval grid_rational g(big_int v) { return grid_rational{v}; }

using huge   = inside<{0, g(pow2(100))}>;                             // 2^100 + 1 points
using fine   = inside<{{0, 1}, grid_rational{big_int{1}, pow2(80)}}>; // notch 2^-80
using shuge  = inside<{-g(pow2(100)), g(pow2(100))}>;                 // signed, past 64 bits
using temp   = inside<{-40, 85}>;                                     // a value raw (int8_t)
using halves = inside<{{0, 5}, 0.5}>;                                 // an index raw (uint8_t)
using bins   = inside<{{0.5, 255.5}, 1}>;                             // off the notch's multiples
using five   = inside<{5, 5}>;                                        // a point

// A run-time integer the optimizer cannot see through.
int at_run_time(int v) {
    volatile int x = v;
    return x;
}
} // namespace

TEST(RuntimeInsides, counts_are_insides) {
    static_assert(std::is_same_v<index_t<huge>, huge>); // {0, 2^100} notch 1 is its own index grid
    static_assert(sizeof(index_t<fine>) == sizeof(fine));
    static_assert(std::is_same_v<index_t<fine>::raw_type, fine::raw_type>);
    static_assert(grid_of<index_t<temp>> == grid{0, 125});
    static_assert(grid_of<index_t<halves>> == grid{0, 10});
    static_assert(grid_of<index_t<five>> == grid{0});
    // The step count is a point: no storage, the value in the type, printable.
    static_assert(std::is_empty_v<std::remove_cvref_t<decltype(step_count_of<huge>)>>);
    EXPECT_EQ(to_string(step_count_of<huge>), "1267650600228229401496703205376");
    EXPECT_EQ(to_string(step_count_of<fine>), "1208925819614629174706176");
    EXPECT_TRUE(step_count_of<halves> == 10);
    // One more than the step count is the number of values, with an exact type.
    const auto values = step_count_of<huge> + 1_ins;
    EXPECT_EQ(to_string(values), "1267650600228229401496703205377");
}

TEST(RuntimeInsides, index_round_trips_past_64_bits) {
    const huge h = huge::from_index(at_run_time(7));
    EXPECT_TRUE(h == 7);
    const huge last = huge::from_index(step_count_of<huge>);
    EXPECT_TRUE(last == just<g(pow2(100))>);
    // Index arithmetic at run time, on wide raws.
    const auto gap = last.index() - h.index(); // {−2^100, 2^100}
    EXPECT_EQ(to_string(gap), "1267650600228229401496703205369");
    const huge mid = huge::from_index(div(last.index(), 2_ins, round_floor));
    EXPECT_TRUE(mid == just<g(pow2(99))>);
    EXPECT_TRUE(huge::from_index(mid.index()) == mid);

    const fine f = 0.5;
    EXPECT_TRUE(f.index() == just<g(pow2(79))>);
    EXPECT_TRUE(fine::from_index(f.index() + 1_ins) > f);

    const shuge s = -at_run_time(3);
    EXPECT_EQ(to_string(s.index()), "1267650600228229401496703205373"); // −3 + 2^100
    EXPECT_TRUE(shuge::from_index(s.index()) == -3);
}

TEST(RuntimeInsides, index_within_64_bits) {
    const temp t = at_run_time(-39);
    EXPECT_TRUE(t.index() == 1);
    EXPECT_TRUE(temp::from_index(t.index()) == -39);
    const halves h = rational{5, 2};
    EXPECT_TRUE(h.index() == 5);
    EXPECT_TRUE((halves::from_index(h.index()) == rational{5, 2}));
    const bins b = 3.5;
    EXPECT_TRUE(b.index() == 3);
    EXPECT_TRUE(bins::from_index(at_run_time(3)) == b);
    EXPECT_TRUE(five{}.index() == 0);
    EXPECT_TRUE(five::from_index(0_ins) == 5);
    // Every value of a small grid round-trips.
    for (int k = 0; k <= 10; ++k)
        EXPECT_TRUE(halves::from_index(at_run_time(k)).index() == k);
}

TEST(RuntimeInsides, from_index_out_of_range) {
    const int past = at_run_time(11); // halves has indices 0 … 10
    EXPECT_THROW((void)halves::from_index(past), inside_error);
    EXPECT_TRUE(halves::from_index(past, clamp) == 5);
    EXPECT_TRUE((halves::from_index(past + 1, wrap) == rational{1, 2})); // 12 folds to 1
    EXPECT_TRUE(halves::from_index(-at_run_time(1), clamp) == 0);
    EXPECT_EQ(halves::try_from_index(past).error(), errc::overflow);
    EXPECT_TRUE(halves::try_from_index(at_run_time(4)).value() == 2);
    EXPECT_THROW((void)five::from_index(at_run_time(1)), inside_error); // a point has index 0 only

    // Past 2^64 points: one past the last index.
    const auto beyond = step_count_of<huge> + inside<{0, 1}>{at_run_time(1)};
    EXPECT_THROW((void)huge::from_index(beyond), inside_error);
    EXPECT_TRUE(huge::from_index(beyond, clamp) == just<g(pow2(100))>);
    EXPECT_TRUE(huge::from_index(beyond, wrap) == 0);
    EXPECT_EQ(huge::try_from_index(beyond).error(), errc::overflow);
}

TEST(RuntimeInsides, numerator_and_denominator_are_insides) {
    const q8_8 q = 1.5;
    static_assert(grid_of<decltype(q.numerator())> == grid{0, 65535});
    static_assert(grid_of<decltype(q.denominator())> == grid{1, 256});
    EXPECT_TRUE(q.numerator() == 3 && q.denominator() == 2);
    EXPECT_EQ(q.numerator().as<int>(), 3);

    const temp t = -at_run_time(7);
    static_assert(std::is_same_v<decltype(t.numerator()), temp>); // the value itself
    EXPECT_TRUE(t.numerator() == -7 && t.denominator() == 1);

    using money   = inside<{{-100, 100}, rational{1, 100}}>;
    const money m = rational{-5, 4};
    static_assert(grid_of<decltype(m.numerator())> == grid{-10000, 10000});
    EXPECT_TRUE(m.numerator() == -5 && m.denominator() == 4);

    using ratio   = inside<{{0, 1}, max_denominator{1000}}>;
    const ratio r = rational{at_run_time(2), 6};
    EXPECT_TRUE(r.numerator() == 1 && r.denominator() == 3);

    // Past 64 bits: wide raws, exact.
    const fine f = fine::from_raw(fine::raw_type{3});
    EXPECT_TRUE(f.numerator() == 3 && f.denominator() == just<g(pow2(80))>);
    EXPECT_EQ(to_string(f.denominator()), "1208925819614629174706176");

    // A point's parts are points.
    constexpr auto third = just<1, 3>;
    static_assert(std::is_empty_v<std::remove_cvref_t<decltype(third.numerator())>>);
    EXPECT_TRUE(third.numerator() == 1 && third.denominator() == 3);
}
