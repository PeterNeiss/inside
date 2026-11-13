// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Ranges and loops: values<T>, inside_range, cursor, the standard
// algorithms, index()/from_index, and uniform<B>.

#include <beman/inside/inside.hpp>
#include <beman/inside/inside/formats.hpp>
#include <beman/inside/inside/io.hpp>
#include <beman/inside/inside/random.hpp>
#include <beman/inside/inside/range.hpp>
#include <beman/inside/math.hpp>

#include <gtest/gtest.h>

#include "support.hpp"

using beman::inside::test::exact;

#include <algorithm>
#include <expected>
#include <iterator>
#include <numeric>
#include <random>
#include <ranges>
#include <set>
#include <type_traits>
#include <vector>

// range
namespace range_tests {
using namespace beman::inside;
using namespace beman::inside::detail;

namespace {
using small_grid = inside_range<{0, 4}>;        // 5 slots: 0,1,2,3,4
using frac_grid  = inside_range<{{0, 2}, 0.5}>; // 5 slots: 0,0.5,1,1.5,2
using small_it   = small_grid::iterator;
} // namespace

//---------------------------------------------------------------------------
// concept conformance
//---------------------------------------------------------------------------
static_assert(std::input_or_output_iterator<small_it>);
static_assert(std::forward_iterator<small_it>);
static_assert(std::bidirectional_iterator<small_it>);
static_assert(std::random_access_iterator<small_it>);

static_assert(std::ranges::range<small_grid>);
static_assert(std::ranges::forward_range<small_grid>);
static_assert(std::ranges::random_access_range<small_grid>);
static_assert(std::ranges::sized_range<small_grid>);

static_assert(std::ranges::random_access_range<frac_grid>);

//---------------------------------------------------------------------------
// basic iteration
//---------------------------------------------------------------------------
// inside_range: default iteration walks every slot once
TEST(RangeTest, inside_range_default_iteration_walks_every_slot_once) {
    small_grid       r;
    std::vector<int> seen;
    for (auto b : r)
        seen.push_back(int(to_value(b)));
    ASSERT_TRUE((seen == std::vector<int>{0, 1, 2, 3, 4}));
}

// inside_range: mid-range start wraps around
TEST(RangeTest, inside_range_mid_range_start_wraps_around) {
    small_grid       r{small_grid::value_type{2}};
    std::vector<int> seen;
    for (auto b : r)
        seen.push_back(int(to_value(b)));
    ASSERT_TRUE((seen == std::vector<int>{2, 3, 4, 0, 1}));
}

// inside_range: fractional notch iterates exact values
TEST(RangeTest, inside_range_fractional_notch_iterates_exact_values) {
    frac_grid             r;
    std::vector<rational> seen;
    for (auto b : r)
        seen.push_back(b); // inside -> rational is implicit
    ASSERT_TRUE((seen == std::vector<rational>{0_r, 0.5_r, 1_r, 1.5_r, 2_r}));
}

//---------------------------------------------------------------------------
// random-access arithmetic
//---------------------------------------------------------------------------
// inside_range: iterator arithmetic
TEST(RangeTest, inside_range_iterator_arithmetic) {
    small_grid r;
    auto       b = r.begin();
    auto       e = r.end();

    ASSERT_EQ(e - b, 5);
    ASSERT_EQ((b + 5), e);
    ASSERT_EQ((e - 5), b);
    ASSERT_EQ(int(to_value(*(b + 3))), 3);
    ASSERT_EQ(int(to_value(b[2])), 2);

    auto it = b + 2;
    ASSERT_EQ(int(to_value(*it)), 2);
    it += 2;
    ASSERT_EQ(int(to_value(*it)), 4);
    it -= 3;
    ASSERT_EQ(int(to_value(*it)), 1);
    ASSERT_EQ(it - b, 1);
}

// inside_range: ordering and equality
TEST(RangeTest, inside_range_ordering_and_equality) {
    small_grid r;
    auto       b = r.begin();
    auto       m = b + 2;
    auto       e = r.end();

    ASSERT_EQ(b, b);
    ASSERT_TRUE(b != m);
    ASSERT_TRUE(b < m);
    ASSERT_TRUE(m <= m);
    ASSERT_TRUE(e > m);
}

//---------------------------------------------------------------------------
// std::ranges interop
//---------------------------------------------------------------------------
// inside_range: std::ranges::size
TEST(RangeTest, inside_range_std_ranges_size) {
    small_grid r;
    ASSERT_EQ(std::ranges::size(r), 5);
}

// inside_range: std::views::take
TEST(RangeTest, inside_range_std_views_take) {
    small_grid       r;
    std::vector<int> seen;
    for (auto b : r | std::views::take(3))
        seen.push_back(int(to_value(b)));
    ASSERT_TRUE((seen == std::vector<int>{0, 1, 2}));
}

// inside_range: std::views::reverse
TEST(RangeTest, inside_range_std_views_reverse) {
    small_grid       r;
    std::vector<int> seen;
    for (auto b : r | std::views::reverse)
        seen.push_back(int(to_value(b)));
    ASSERT_TRUE((seen == std::vector<int>{4, 3, 2, 1, 0}));
}

// inside_range: backwards iteration via operator--
TEST(RangeTest, inside_range_backwards_iteration_via_operator) {
    small_grid       r;
    auto             it = r.end();
    std::vector<int> seen;
    while (it != r.begin()) {
        --it;
        seen.push_back(int(to_value(*it)));
    }
    ASSERT_TRUE((seen == std::vector<int>{4, 3, 2, 1, 0}));
}

// inside_range: postfix ++ / -- behave standard
TEST(RangeTest, inside_range_postfix_plus_plus_behave_standard) {
    small_grid r;
    auto       it   = r.begin();
    auto       prev = it++;
    ASSERT_EQ(int(to_value(*prev)), 0);
    ASSERT_EQ(int(to_value(*it)), 1);

    auto snap2 = it--;
    ASSERT_EQ(int(to_value(*snap2)), 1);
    ASSERT_EQ(int(to_value(*it)), 0);
}

// inside_range works with std::views::reverse
TEST(RangeTest, inside_range_works_with_std_views_reverse) {
    small_grid                r; // 0,1,2,3,4
    std::vector<std::int64_t> seen;
    for (auto v : std::views::reverse(r))
        seen.push_back(v.as<std::int64_t>());
    ASSERT_TRUE((seen == std::vector<std::int64_t>{4, 3, 2, 1, 0}));
}

// std::views::enumerate pairs each value with its position
TEST(RangeTest, inside_range_with_std_views_enumerate) {
    small_grid                                         r;
    std::vector<std::pair<std::int64_t, std::int64_t>> seen;
    for (auto [i, v] : std::views::enumerate(r))
        seen.emplace_back(i, v.as<std::int64_t>());
    ASSERT_TRUE((seen == std::vector<std::pair<std::int64_t, std::int64_t>>{{0, 0}, {1, 1}, {2, 2}, {3, 3}, {4, 4}}));
}

// std::views::stride visits every step-th value
TEST(RangeTest, inside_range_with_std_views_stride) {
    small_grid r; // 0,1,2,3,4
    auto       collect = [&](std::int64_t step) {
        std::vector<std::int64_t> seen;
        for (auto v : r | std::views::stride(step))
            seen.push_back(v.as<std::int64_t>());
        return seen;
    };
    ASSERT_TRUE((collect(1) == std::vector<std::int64_t>{0, 1, 2, 3, 4}));
    ASSERT_TRUE((collect(2) == std::vector<std::int64_t>{0, 2, 4}));
    ASSERT_TRUE((collect(3) == std::vector<std::int64_t>{0, 3}));
    ASSERT_TRUE(collect(5) == std::vector<std::int64_t>{0});

    // Fractional grid strides over notch values too.
    using quarters = inside_range<{{0, 1}, per<4>}>; // 0,.25,.5,.75,1
    std::vector<rational> fseen;
    for (auto v : quarters{} | std::views::stride(2))
        fseen.push_back(rational{v});
    ASSERT_TRUE((fseen == std::vector<rational>{rational{0}, rational{1, 2}, rational{1}}));
}

//---------------------------------------------------------------------------
// values<T>: the range of a type's values
//---------------------------------------------------------------------------
namespace {
using bin     = inside<{0, 9}>;
using quarter = inside<{{0, 1}, per<4>}, round_nearest>;

template <typename T>
concept has_values = requires { values<T>; };
} // namespace

static_assert(std::is_same_v<std::remove_const_t<decltype(values<bin>)>, inside_range<{0, 9}>>);
static_assert(
    std::is_same_v<std::remove_const_t<decltype(values<quarter>)>, inside_range<{{0, 1}, per<4>}, round_nearest>>);
static_assert(std::ranges::random_access_range<decltype(values<bin>)>);
static_assert(std::ranges::sized_range<decltype(values<bin>)>);
static_assert(std::is_same_v<std::ranges::range_value_t<decltype(values<quarter>)>, quarter>);
static_assert(has_values<bin>);
static_assert(!has_values<cursor<bin>>);                           // ends past bin's Upper
static_assert(!has_values<inside<{{0, 1}, max_denominator{10}}>>); // no notch
static_assert(std::ranges::size(values<bin>) == 10);
static_assert(std::ranges::count_if(values<bin>, [](bin b) { return b > 5_ins; }) == 4);

TEST(RangeTest, values_walks_every_value_of_a_type) {
    std::vector<rational> seen;
    for (quarter q : values<quarter>)
        seen.push_back(rational{q});
    ASSERT_TRUE((seen == std::vector<rational>{0, rational{1, 4}, rational{1, 2}, rational{3, 4}, 1}));

    auto it = std::ranges::find(values<bin>, bin{7});
    ASSERT_EQ(it - std::ranges::begin(values<bin>), 7);
    ASSERT_EQ(rational{*std::ranges::max_element(values<bin>)}, 9);
}

// A view (view_interface: [], front, back, empty) and a borrowed range.
static_assert(std::ranges::view<std::remove_const_t<decltype(values<bin>)>>);
static_assert(std::ranges::borrowed_range<small_grid>);
static_assert(values<bin>[7] == 7 && values<bin>.front() == 0 && values<bin>.back() == 9 && !values<bin>.empty());
static_assert(small_grid{3}[4] == 2 && small_grid{3}.back() == 2); // indexes from the start, wrapping

TEST(RangeTest, algorithm_on_a_temporary_returns_an_iterator) {
    // Borrowed: no std::ranges::dangling, though the range is a temporary.
    const auto it = std::ranges::partition_point(small_grid{}, [](auto v) { return v < 3_ins; });
    ASSERT_EQ(*it, 3);
}

//---------------------------------------------------------------------------
// storage-kind decode — operator* takes integer fast arms for index/value
// raws (see range.hpp); every arm must agree with the analytic
// Lower + index·Notch, and the ctor must invert it. [perf-paths] pins which
// arm each representative type dispatches to.
//---------------------------------------------------------------------------
namespace {
template <typename RangeType>
void require_decodes_analytically() {
    using value_type = typename RangeType::value_type;
    RangeType   r;
    std::size_t position = 0;
    for (auto b : r) {
        const exact expected =
            exact{detail::lower64<value_type>} + exact{position} * exact{detail::notch64<value_type>};
        ASSERT_EQ(exact{as_rational(b)}, expected);
        ++position;
    }
    ASSERT_EQ(position, r.size());
}
} // namespace

// inside_range: decode agrees with Lower + i*Notch on every storage kind
TEST(RangeTest, inside_range_decode_agrees_with_lower_plus_i_notch_on_every_storage_kind) {
    require_decodes_analytically<inside_range<{0, 999}>>();           // value raw
    require_decodes_analytically<inside_range<{-500, 500}>>();        // value raw, signed
    require_decodes_analytically<inside_range<{{0, 4}, per<256>}>>(); // index raw
    require_decodes_analytically<inside_range<{{-2, 2}, per<4>}>>();  // index raw, offset Lower
    require_decodes_analytically<inside_range<{0, 255}>>();           // full-width uint8 raw
    require_decodes_analytically<inside_range<{{0, 2}, per<3>}>>();   // index raw, notch 1/3
}

// inside_range: start ctor inverts the decode on every storage kind
TEST(RangeTest, inside_range_start_ctor_inverts_the_decode_on_every_storage_kind) {
    auto first_equals_start = [](auto range_tag, auto start_value) {
        using RangeType  = decltype(range_tag);
        const auto start = test::store<typename RangeType::value_type>(start_value);
        RangeType  r{start};
        ASSERT_EQ(as_rational(*r.begin()), as_rational(start));
    };
    first_equals_start(inside_range<{0, 999}>{}, 500);
    first_equals_start(inside_range<{{0, 4}, per<256>}>{}, 2);
    first_equals_start(inside_range<{{0, 2}, per<3>}>{}, 1);
}

// inside_range: a grid filling its raw type visits every value once
TEST(RangeTest, inside_range_full_width_raw_visits_every_value_once) {
    using r_t = inside_range<{0, 255}>;
    static_assert(sizeof(r_t::value_type) == 1);
    int expected = 0;
    for (auto b : r_t{}) {
        ASSERT_EQ(b, expected);
        ++expected;
    }
    ASSERT_EQ(expected, 256);
}

// inside_range: fast decode arms engage (dispatch pins)
TEST(RangeTest, inside_range_fast_decode_arms_engage_dispatch_pins) {
    // The operator* fast arms are gated on the storage kind; these pins fail if
    // a storage-selection change silently reroutes a type to another arm.
    static_assert(integer_value_storage<inside_range<{0, 999}>::value_type>);
    static_assert(index_storage<inside_range<{{0, 4}, per<256>}>::value_type>);
    static_assert(index_storage<inside_range<{{-2, 2}, per<4>}>::value_type>);
    static_assert(index_storage<inside_range<{{0, 2}, per<3>}>::value_type>);
}

// inside_range: random access and reverse on a mid-range start
TEST(RangeTest, inside_range_mid_start_random_access_and_reverse) {
    small_grid             r{small_grid::value_type{3}};
    const std::vector<int> want{3, 4, 0, 1, 2};
    auto                   b = r.begin();
    for (int k = 0; k < 5; ++k) {
        EXPECT_EQ(int(to_value(b[k])), want[k]);
        EXPECT_EQ(int(to_value(*(b + k))), want[k]);
    }
    auto e = r.end();
    EXPECT_EQ(int(to_value(*(e - 1))), 2);
    EXPECT_EQ(e - b, 5);
    std::vector<int> rev;
    for (auto v : std::views::reverse(r))
        rev.push_back(int(to_value(v)));
    EXPECT_TRUE((rev == std::vector<int>{2, 1, 0, 4, 3}));
}
} // namespace range_tests

// cursor<T, Step>: a loop variable that steps through T's grid and one step
// past its Upper, so the classic loop needs no value T cannot hold.
namespace cursor_tests {
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

TEST(CursorTest, type) {
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
    // a cursor over an unsafe T still starts at Lower
    static_assert(rational{cursor<inside<{5, 100}, unsafe>>{}} == 5);
}

TEST(CursorTest, three_loop_forms) {
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

TEST(CursorTest, default_starts_at_lower) {
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

TEST(CursorTest, steps_and_ends) {
    cur t;
    EXPECT_DEATH(--t, "beman::inside: value does not fit"); // nothing before Lower: a precondition
    cur u = 4_ins;
    ++u;                                                    // the end
    EXPECT_DEATH(++u, "beman::inside: value does not fit"); // nothing past the end: a precondition
    // the values convert back to T exactly
    for (cur v; v != end(v); ++v) {
        const time_t_ x = *v; // (a cursor's value: without the end)
        EXPECT_EQ(static_cast<rational>(x), static_cast<rational>(v));
    }
    // a cursor works where an inside does
    using amp = inside<{{0, 64}, per<16384>}, round_nearest>;
    cur w     = 1_ins;
    EXPECT_EQ((math::exp<amp>(w)), (math::exp<amp>(time_t_{1})));
    EXPECT_EQ(to_string(w), "1");
}

TEST(CursorTest, marker_does_not_leak) {
    cur t = 1_ins;
    static_assert(!has_flag(flags_of<decltype(t + t)>, cursor_marker));
    static_assert(!has_flag(flags_of<decltype(t * t)>, cursor_marker));
    static_assert(!has_flag(flags_of<decltype(-t)>, cursor_marker));
    static_assert(!has_flag(flags_of<decltype(math::exp(t))>, cursor_marker));
    static_assert(!has_flag(flags_of<decltype(math::abs(t))>, cursor_marker));
    static_assert(!has_end<time_t_>); // only a cursor has an end
    static_assert(has_end<cur>);
}

TEST(CursorTest, increment_moves_one_notch) {
    // (++ on a checked grid may step out: clamp names its range handling)
    static_assert(!test::can_step<inside<{{0, 2}, per<4>}>>);
    inside<{{0, 2}, per<4>}, clamp> q{0.5};
    ++q;
    EXPECT_EQ(static_cast<rational>(q), 0.75_r);
    q--;
    EXPECT_EQ(static_cast<rational>(q), 0.5_r);
    inside<{0, 10}, clamp> i{3};
    ++i;
    EXPECT_EQ(static_cast<rational>(i), rational{4});
}

TEST(CursorTest, point_compare_is_a_slot_compare) {
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
static_assert(std::same_as<std::iter_difference_t<cur>, std::int64_t>);
static_assert(std::same_as<std::iter_value_t<cur>, inside<{{-4, 4}, per<4>}, round_nearest>>);
static_assert(std::convertible_to<std::iter_value_t<cur>, time_t_>);
// plain insides stay numbers, not iterators
static_assert(!std::weakly_incrementable<time_t_>);
static_assert(!std::indirectly_readable<time_t_>);
static_assert(!std::ranges::range<time_t_>);

TEST(CursorTest, is_an_iterator) {
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
    EXPECT_DEATH((void)*e, "beman::inside: value does not fit"); // the end has no value: a precondition
}
} // namespace cursor_tests

// algo
namespace algo_tests {
using namespace beman::inside;
using namespace beman::inside::detail;

namespace rng = std::ranges;

namespace {
using cell = beman::inside::inside<{0, 100, 0.5}>;
// (an accumulator of std algorithms stores each prefix back: clamp names it)
using accum = beman::inside::inside<{0, 1000, 0.5}, beman::inside::clamp>;
} // namespace

// algo: ranges algorithms over vector<inside>
TEST(AlgoTest, algo_ranges_algorithms_over_vector_inside) {
    std::vector<cell> v;
    v.resize(10);
    rng::generate(v, [counter = 7]() mutable { return test::store<cell>(++counter); });
    ASSERT_EQ(v.size(), 10);

    // reduce/accumulate
    auto sum = std::reduce(v.begin(), v.end(), accum{0}, std::plus<>{});
    ASSERT_EQ(sum, 8 + 9 + 10 + 11 + 12 + 13 + 14 + 15 + 16 + 17);

    // sort
    rng::sort(v);
    ASSERT_TRUE(rng::is_sorted(v));

    // find
    auto it = rng::find(v, cell{12});
    ASSERT_TRUE(it != v.end());

    // count_if
    auto above = rng::count_if(v, [](cell x) { return x > 10; });
    ASSERT_EQ(above, 7);

    // min/max
    ASSERT_EQ(*rng::min_element(v), 8);
    ASSERT_EQ(*rng::max_element(v), 17);

    // transform
    std::vector<accum> shifted(v.size());
    rng::transform(v, shifted.begin(), [](cell x) { return x + cell{1}; });
    ASSERT_EQ(shifted.front(), 9);
    ASSERT_EQ(shifted.back(), 18);

    // copy_if
    std::vector<cell> filtered;
    rng::copy_if(v, std::back_inserter(filtered), [](cell x) { return x > 12; });
    ASSERT_EQ(filtered.size(), 5);

    ASSERT_TRUE((rng::all_of(v, [](cell x) { return x > 0; })));
    ASSERT_TRUE((rng::any_of(v, [](cell x) { return x == 10; })));
    ASSERT_TRUE((rng::none_of(v, [](cell x) { return x > 100; })));
}

// algo: classic STL algorithms
TEST(AlgoTest, algo_classic_stl_algorithms) {
    std::vector<cell> v = {50, 10, 30, 20, 40};

    std::sort(v.begin(), v.end());
    ASSERT_EQ(v.front(), 10);
    ASSERT_EQ(v.back(), 50);

    std::stable_sort(v.begin(), v.end(), std::greater<>{});
    ASSERT_EQ(v.front(), 50);

    std::vector<cell> v3 = {70, 10, 50, 30, 90};
    std::nth_element(v3.begin(), v3.begin() + 2, v3.end());
    ASSERT_EQ(v3[2], 50);

    std::vector<cell> v4 = {20, 80, 10, 60, 90, 40};
    std::partial_sort(v4.begin(), v4.begin() + 3, v4.end(), std::greater<>{});
    ASSERT_EQ(v4[0], 90);
    ASSERT_EQ(v4[1], 80);
    ASSERT_EQ(v4[2], 60);

    // accumulate
    std::vector<cell> v5  = {1, 2, 3, 4, 5};
    auto              acc = std::accumulate(v5.begin(), v5.end(), accum{0}, std::plus<>{});
    ASSERT_EQ(acc, 15);

    // reverse
    std::vector<cell> v6 = {1, 2, 3};
    std::reverse(v6.begin(), v6.end());
    ASSERT_EQ(v6[0], 3);
    ASSERT_EQ(v6[2], 1);

    // rotate
    std::vector<cell> v7 = {1, 2, 3, 4, 5};
    std::rotate(v7.begin(), v7.begin() + 2, v7.end());
    ASSERT_EQ(v7[0], 3);
    ASSERT_EQ(v7.back(), 2);

    // partition
    std::vector<cell> v8    = {10, 50, 20, 60, 30, 70};
    auto              pivot = std::partition(v8.begin(), v8.end(), [](cell x) { return x > 30; });
    for (auto p = v8.begin(); p != pivot; ++p)
        ASSERT_TRUE(*p > 30);
    for (auto p = pivot; p != v8.end(); ++p)
        ASSERT_TRUE(*p <= 30);

    // equal / mismatch
    std::vector<cell> a = {10, 20, 30};
    std::vector<cell> b = {10, 20, 30};
    ASSERT_TRUE((std::equal(a.begin(), a.end(), b.begin())));
    b[1]    = 99;
    auto mm = std::mismatch(a.begin(), a.end(), b.begin());
    ASSERT_EQ(*mm.first, 20);
    ASSERT_EQ(*mm.second, 99);

    // iota: ++ moves one notch (1/2 on this grid); ++ names its range handling
    using step_cell = beman::inside::inside<{0, 100, 0.5}, beman::inside::clamp>;
    std::vector<step_cell> iota_v(5);
    std::iota(iota_v.begin(), iota_v.end(), step_cell{10});
    ASSERT_EQ(iota_v.front(), 10);
    ASSERT_EQ(iota_v.back(), 12);
}

// beman::inside::min / max / midpoint over bounds
TEST(AlgoTest, beman_inside_min_max_midpoint_over_bounds) {
    using pct = inside<{0, 100}>;
    pct a{30}, b{70};
    ASSERT_EQ((beman::inside::min(a, b)), 30);
    ASSERT_EQ((beman::inside::max(a, b)), 70);
    // midpoint is exact: (30+70)/2 = 50, lands on a refined grid.
    ASSERT_EQ((rational{beman::inside::midpoint(a, b)}), 50);

    // Exact midpoint of integer endpoints whose average is fractional.
    pct c{0}, d{1};
    ASSERT_EQ((rational{beman::inside::midpoint(c, d)}), (rational{1, 2})); // no rounding, no overflow

    // ADL: unqualified call resolves to beman::inside::min for inside arguments.
    ASSERT_EQ((min(a, b)), 30);

    // Fractional grid.
    using s = inside<{{-1, 1}, per<16384>}, round_nearest>;
    s x{rational{1, 4}}, y{rational{3, 4}};
    ASSERT_EQ((rational{beman::inside::midpoint(x, y)}), (rational{1, 2}));
    ASSERT_EQ((beman::inside::max(x, y)), (rational{3, 4}));
}
} // namespace algo_tests

// Run-time values are insides, never grid numbers: a grid number past 64 bits
// exists only at compile time, while an inside holds any value of its grid in
// a raw sized from it. index(), from_index, index_t, step_count_of and the
// numerator/denominator insides, on grids past 2^64 points and within them.
// The inputs pass through a volatile, so every computation runs at run time.
namespace runtime_insides_tests {
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

TEST(IndexTest, counts_are_insides) {
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

TEST(IndexTest, index_round_trips_past_64_bits) {
    const huge h = *huge::from_index(at_run_time(7)); // (an int index may miss: an expected)
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
    EXPECT_TRUE(*fine::from_index(f.index() + 1_ins) > f);

    const shuge s = -at_run_time(3);
    EXPECT_EQ(to_string(s.index()), "1267650600228229401496703205373"); // −3 + 2^100
    EXPECT_TRUE(shuge::from_index(s.index()) == -3);
}

TEST(IndexTest, index_within_64_bits) {
    const temp t = test::store<temp>(at_run_time(-39));
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
        EXPECT_TRUE(halves::from_index(at_run_time(k))->index() == k); // (an int index: an expected)
}

TEST(IndexTest, from_index_out_of_range) {
    const int past = at_run_time(11); // halves has indices 0 … 10
    EXPECT_EQ(halves::from_index(past).error(), errc::overflow);
    EXPECT_TRUE(halves::from_index(past, clamp) == 5);
    EXPECT_TRUE((halves::from_index(past + 1, wrap) == rational{1, 2})); // 12 folds to 1
    EXPECT_TRUE(halves::from_index(-at_run_time(1), clamp) == 0);
    EXPECT_EQ(halves::try_from_index(past).error(), errc::overflow);
    EXPECT_TRUE(halves::try_from_index(at_run_time(4)).value() == 2);
    EXPECT_EQ(five::from_index(at_run_time(1)).error(), errc::overflow); // a point has index 0 only

    // Past 2^64 points: one past the last index.
    const auto beyond = step_count_of<huge> + test::store<inside<{0, 1}>>(at_run_time(1));
    EXPECT_EQ(huge::from_index(beyond).error(), errc::overflow);
    EXPECT_TRUE(huge::from_index(beyond, clamp) == just<g(pow2(100))>);
    EXPECT_TRUE(huge::from_index(beyond, wrap) == 0);
    EXPECT_EQ(huge::try_from_index(beyond).error(), errc::overflow);
}

TEST(IndexTest, numerator_and_denominator_are_insides) {
    const q8_8 q = 1.5;
    static_assert(grid_of<decltype(q.numerator())> == grid{0, 65535});
    static_assert(grid_of<decltype(q.denominator())> == grid{1, 256});
    EXPECT_TRUE(q.numerator() == 3 && q.denominator() == 2);
    EXPECT_EQ(q.numerator().as<int>(), 3);

    const temp t = test::store<temp>(-at_run_time(7));
    static_assert(std::is_same_v<decltype(t.numerator()), temp>); // the value itself
    EXPECT_TRUE(t.numerator() == -7 && t.denominator() == 1);

    using money   = inside<{{-100, 100}, rational{1, 100}}>;
    const money m = rational{-5, 4};
    static_assert(grid_of<decltype(m.numerator())> == grid{-10000, 10000});
    EXPECT_TRUE(m.numerator() == -5 && m.denominator() == 4);

    using ratio   = inside<{{0, 1}, max_denominator{1000}}>;
    const ratio r = test::store<ratio>(rational{at_run_time(2), 6});
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
} // namespace runtime_insides_tests

// Pinned defects: each case failed before its fix.
namespace pinned_tests {
using namespace beman::inside;
using namespace beman::inside::detail;

template <class B>
std::set<rational> sample(int n) {
    std::mt19937_64    rng{12345};
    std::set<rational> seen;
    for (int i = 0; i < n; ++i) {
        const B        b = uniform<B>(rng);
        const rational v = b;
        EXPECT_TRUE(v >= lower_of<B> && v <= upper_of<B>);
        seen.insert(v);
    }
    return seen;
}

// inside_range sequential
TEST(RangeTest, inside_range_sequential) {
    int          count = 0;
    std::int64_t sum   = 0;
    for (auto i : inside_range<{0, 9}>{}) {
        sum += i.as<std::int64_t>();
        ++count;
    }
    ASSERT_EQ(count, 10);
    ASSERT_EQ(sum, 45);
}

// inside_range wrapping start
TEST(RangeTest, inside_range_wrapping_start) {
    int          count = 0;
    std::int64_t first = -1, last = -1;
    for (auto i : inside_range<{0, 9}>{7}) {
        if (count == 0)
            first = i.as<std::int64_t>();
        last = i.as<std::int64_t>();
        ++count;
    }
    ASSERT_EQ(count, 10);
    ASSERT_EQ(first, 7);
    ASSERT_EQ(last, 6);
}

// inside_range over signed inside
TEST(RangeTest, inside_range_over_signed_inside) {
    int          count = 0;
    std::int64_t first = 0, last = 0;
    for (auto i : inside_range<{-2, 2}>{}) {
        if (count == 0)
            first = i.as<std::int64_t>();
        last = i.as<std::int64_t>();
        ++count;
    }
    ASSERT_EQ(count, 5);
    ASSERT_EQ(first, -2);
    ASSERT_EQ(last, 2);
}

// inside_range size 1
TEST(RangeTest, inside_range_size_1) {
    int count = 0;
    for (auto i : inside_range<{5, 5}>{}) {
        ASSERT_EQ(i, 5);
        ++count;
    }
    ASSERT_EQ(count, 1);
}

// inside_range size 1 with negative point
TEST(RangeTest, inside_range_size_1_with_negative_point) {
    int count = 0;
    for (auto i : inside_range<{-3, -3}>{}) {
        ASSERT_EQ(i, -3);
        ++count;
    }
    ASSERT_EQ(count, 1);
}

TEST(RangeTest, uniform_covers_every_slot_and_stays_on_the_grid) {
    using ints    = inside<{0, 9}>;
    using idx     = inside<{-5, 5}>;
    using quarter = inside<{{-1, 1}, per<4>}>;
    using thirds  = inside<{{0, 1}, per<3>}>;
    EXPECT_EQ(sample<ints>(2000).size(), 10u);
    EXPECT_EQ(sample<idx>(2000).size(), 11u);
    const auto q = sample<quarter>(2000);
    EXPECT_EQ(q.size(), 9u);
    for (auto v : q) {
        const exact slots = (exact{v} + exact{1}) / exact{rational{1, 4}};
        EXPECT_EQ(slots, floor(slots)); // on the 1/4 lattice
    }
    EXPECT_EQ(sample<thirds>(2000).size(), 4u);
    EXPECT_EQ(sample<decltype(5_ins)>(10).size(), 1u); // a point
}

// The draw is the library's own, so these sequences are the same on every
// standard library: a 64-bit generator (one output per word) and a 31-bit one
// (bits gathered from several outputs).
TEST(RangeTest, uniform_draws_are_pinned_across_standard_libraries) {
    using idx = inside<{-5, 5}>;
    std::mt19937_64 mt{42};
    for (int v : {1, 3, 5, 0, -5, -5, 1, -4})
        EXPECT_EQ(uniform<idx>(mt), v);
    std::minstd_rand ms{42};
    for (int v : {1, 0, -2, 3, 0, 5, -3, -2})
        EXPECT_EQ(uniform<idx>(ms), v);

    using quarter = inside<{{-1, 1}, per<4>}>;
    std::mt19937_64 mq{42};
    for (int q : {2, 4, 1, -4, -4, 2, -3, 2}) // in quarters
        EXPECT_EQ((rational{uniform<quarter>(mq)}), (rational{q, 4}));

    using big = inside<{0, 1'000'000}>;
    std::minstd_rand mb{42};
    for (int v : {876198, 72741, 579955, 669995, 974328, 246765, 169285, 905162})
        EXPECT_EQ(uniform<big>(mb), v);
}

} // namespace pinned_tests

// Past 2^63 values (uint64, int64, a grid past 64 bits) values<T> is a
// bidirectional view without a size: its position is a wide integer.
namespace big_range_tests {
using namespace beman::inside;

TEST(RangeTest, values_past_2_to_the_63) {
    using WS = inside<{-1180591620717411303424_g, 1180591620717411303424_g}>; // 2^71 + 1 values
    using U  = std::remove_const_t<decltype(values<uint64>)>;
    using R  = std::remove_const_t<decltype(values<WS>)>;
    static_assert(std::ranges::bidirectional_range<U> && std::ranges::view<U> && std::ranges::borrowed_range<U>);
    static_assert(!std::ranges::random_access_range<U> && !std::ranges::sized_range<U>);
    static_assert(std::ranges::bidirectional_range<R> && !std::ranges::sized_range<R>);
    static_assert(std::ranges::random_access_range<decltype(values<inside<{0, 100}>>)>);

    std::vector<std::uint64_t> head;
    for (const auto v : values<uint64> | std::views::take(3))
        head.push_back(v.to<std::uint64_t>());
    EXPECT_EQ(head, (std::vector<std::uint64_t>{0, 1, 2}));
    EXPECT_TRUE(*std::ranges::prev(values<uint64>.end()) == 18446744073709551615u);
    EXPECT_TRUE(*values<WS>.begin() == WS{-1180591620717411303424_g});
    EXPECT_TRUE(*std::ranges::prev(values<WS>.end()) == WS{1180591620717411303424_g});
    EXPECT_TRUE(*std::ranges::next(values<WS>.begin(), 2) == WS{-1180591620717411303422_g});

    // A mid-range start wraps past the top, as on a smaller grid.
    const inside_range<grid_of<uint64>> from_top{uint64{18446744073709551614u}};
    auto                                it = from_top.begin();
    EXPECT_TRUE(*it++ == 18446744073709551614u);
    EXPECT_TRUE(*it++ == 18446744073709551615u);
    EXPECT_TRUE(*it == 0);
    EXPECT_TRUE(*--it == 18446744073709551615u);
    EXPECT_TRUE(*std::ranges::prev(from_top.end()) == 18446744073709551613u);
    EXPECT_FALSE(from_top.empty());
}
} // namespace big_range_tests
