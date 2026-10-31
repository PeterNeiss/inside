// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include <beman/inside/inside.hpp>
#include <beman/inside/inside/io.hpp>

#include <gtest/gtest.h>

// Whether numeric_limits<T> has epsilon() / round_error(): only where 0 is a value.
template <class T>
concept has_epsilon = requires { std::numeric_limits<T>::epsilon(); };
template <class T>
concept has_round_error = requires { std::numeric_limits<T>::round_error(); };

using namespace beman::inside;
using namespace beman::inside::detail;

// No implicit integer conversion: as<T>() reads the value, with no check
// left where T holds the whole grid.
template <class T>
concept casts_to_imax = requires(T t) { static_cast<imax>(t); };
template <class T>
concept indexes_a_vector = requires(std::vector<int>& v, T t) { v[t]; };
TEST(MiscTest, no_implicit_integer_conversion) {
    using idx = inside<{0, 9}>;
    static_assert(!std::is_convertible_v<idx, imax>);
    static_assert(!std::is_convertible_v<idx, int>);
    static_assert(!std::is_constructible_v<int, idx>);
    static_assert(!std::is_convertible_v<inside<{{0, 9}, per<2>}, round_nearest>, imax>);
    static_assert(!std::is_convertible_v<inside<{0, 10'000'000'000}>, int>); // would narrow silently
    static_assert(!casts_to_imax<idx>);
    static_assert(!indexes_a_vector<idx>);

    idx  a{5};
    imax val = a.as<imax>();
    ASSERT_EQ(val, 5);

    using idx2 = inside<{1, 10}>;
    idx2 b{7};
    ASSERT_EQ(b.as<int>(), 7);

    // an array subscript names the index type
    int arr[] = {10, 20, 30, 40, 50};
    using ai  = inside<{0, 4}>;
    ai c{3};
    ASSERT_EQ(arr[c.as<std::size_t>()], 40);
}

// inside_range sequential
TEST(MiscTest, inside_range_sequential) {
    int  count = 0;
    imax sum   = 0;
    for (auto i : inside_range<{0, 9}>{}) {
        sum += i.as<imax>();
        ++count;
    }
    ASSERT_EQ(count, 10);
    ASSERT_EQ(sum, 45);
}

// inside_range wrapping start
TEST(MiscTest, inside_range_wrapping_start) {
    int  count = 0;
    imax first = -1, last = -1;
    for (auto i : inside_range<{0, 9}>{7}) {
        if (count == 0)
            first = i.as<imax>();
        last = i.as<imax>();
        ++count;
    }
    ASSERT_EQ(count, 10);
    ASSERT_EQ(first, 7);
    ASSERT_EQ(last, 6);
}

// inside_range over signed inside
TEST(MiscTest, inside_range_over_signed_inside) {
    int  count = 0;
    imax first = 0, last = 0;
    for (auto i : inside_range<{-2, 2}>{}) {
        if (count == 0)
            first = i.as<imax>();
        last = i.as<imax>();
        ++count;
    }
    ASSERT_EQ(count, 5);
    ASSERT_EQ(first, -2);
    ASSERT_EQ(last, 2);
}

// inside_range size 1
TEST(MiscTest, inside_range_size_1) {
    int count = 0;
    for (auto i : inside_range<{5, 5}>{}) {
        ASSERT_EQ(i, 5);
        ++count;
    }
    ASSERT_EQ(count, 1);
}

// inside_range size 1 with negative point
TEST(MiscTest, inside_range_size_1_with_negative_point) {
    int count = 0;
    for (auto i : inside_range<{-3, -3}>{}) {
        ASSERT_EQ(i, -3);
        ++count;
    }
    ASSERT_EQ(count, 1);
}

// inside<{x,x}> singleton round-trips its value
TEST(MiscTest, inside_x_x_singleton_round_trips_its_value) {
    using point_pos = inside<{5, 5}>;
    point_pos a{5};
    ASSERT_EQ(a, 5);
    ASSERT_EQ(a.as<imax>(), 5);

    using point_neg = inside<{-3, -3}>;
    point_neg b{-3};
    ASSERT_EQ(b, -3);
    ASSERT_EQ(b.as<imax>(), -3);

    // Float assignment to single-value rational-storage grid (notch=0)
    using point_fp = inside<{2.5_r}>;
    point_fp c     = 2.5;
    ASSERT_EQ(c, 2.5_r);
}

// default-constructed inside is well-formed
TEST(MiscTest, default_constructed_inside_is_well_formed) {
    inside<> b;
    (void)b;
    SUCCEED() << "default ctor compiles";
}

// numeric_limits epsilon / round_error report 0 for exact types
TEST(MiscTest, numeric_limits_epsilon_round_error_report_0_for_exact_types) {
    // 0 is in the interval and on the grid → epsilon == 0.
    using u8 = inside<{0, 255}>;
    static_assert(std::numeric_limits<u8>::epsilon() == 0);
    static_assert(std::numeric_limits<u8>::round_error() == 0);

    using i8 = inside<{-100, 100}>;
    static_assert(std::numeric_limits<i8>::epsilon() == 0);
    static_assert(std::numeric_limits<i8>::round_error() == 0);

    using half = inside<{{-40, 60}, 0.5_r}>;
    static_assert(std::numeric_limits<half>::epsilon() == 0);
    static_assert(std::numeric_limits<half>::round_error() == 0);

    // Rational raw (Notch = 0) — still exact, epsilon = 0.
    using r01 = inside<{{0_r, 1_r}, max_denominator{1000}}>;
    static_assert(std::numeric_limits<r01>::epsilon() == 0);

    // 0 is *outside* the interval: no epsilon, as 0 is no value.
    using above_zero = inside<{10, 100}>;
    static_assert(!has_epsilon<above_zero>);
    static_assert(!has_round_error<above_zero>);

    // digits from the grid: the narrowest integer numbering every value, less
    // a sign bit for a grid with negatives — as for uint8_t / int8_t.
    static_assert(std::numeric_limits<u8>::digits == 8 && std::numeric_limits<u8>::digits10 == 2);
    static_assert(std::numeric_limits<inside<{-128, 127}>>::digits == 7);
    static_assert(std::numeric_limits<inside<{0, 100}>>::digits == 6);
    static_assert(std::numeric_limits<i8>::digits == 6);   // 201 values, signed
    static_assert(std::numeric_limits<half>::digits == 6); // 201 values, signed
}

// errc_message stringifies every errc
TEST(MiscTest, errc_message_stringifies_every_errc) {
    ASSERT_EQ(std::string_view{errc_message(errc::domain_error)}, "argument outside the function's domain");
    ASSERT_EQ(std::string_view{errc_message(errc::division_by_zero)}, "division by zero");
    ASSERT_EQ(std::string_view{errc_message(errc::overflow)}, "value does not fit its range");
    ASSERT_EQ(std::string_view{errc_message(errc::rounding_error)}, "value is not on the grid");
    ASSERT_EQ(std::string_view{errc_message(errc::not_finite)}, "non-finite floating-point value");
    ASSERT_EQ(std::string_view{errc_message(errc::invalid_format)}, "malformed number");
    ASSERT_EQ(std::string_view{errc_message(static_cast<errc>(999))}, "unknown inside error");
}
