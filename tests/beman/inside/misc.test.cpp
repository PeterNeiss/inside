// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>
#include <beman/inside/numeric_limits.hpp>

#include <gtest/gtest.h>

using namespace beman::inside;
using namespace beman::inside::detail;

// implicit cast to imax
TEST(MiscTest, implicit_cast_to_imax) {
    using idx = inside<{0, 9}>;
    idx  a{5};
    imax val = a;
    ASSERT_EQ(val, 5);

    using idx2 = inside<{1, 10}>;
    idx2 b{7};
    imax val2 = b;
    ASSERT_EQ(val2, 7);

    // array subscript without explicit cast
    int arr[] = {10, 20, 30, 40, 50};
    using ai  = inside<{0, 4}>;
    ai c{3};
    ASSERT_EQ(arr[c], 40);
}

// inside_range sequential
TEST(MiscTest, inside_range_sequential) {
    int  count = 0;
    imax sum   = 0;
    for (auto i : inside_range<{0, 9}>{}) {
        imax v = i; // implicit inside -> imax (direct-init picks imax over size_t)
        sum += v;
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
            first = i;
        last = i;
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
            first = i;
        last = i;
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
    imax av = a; // implicit
    ASSERT_EQ(av, 5);

    using point_neg = inside<{-3, -3}>;
    point_neg b{-3};
    ASSERT_EQ(b, -3);
    imax bv = b;
    ASSERT_EQ(bv, -3);

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
    using r01 = inside<{{0_r, 1_r}, 0}>;
    static_assert(std::numeric_limits<r01>::epsilon() == 0);

    // 0 is *outside* the interval — fall back to Lower (the closest representable).
    using above_zero = inside<{10, 100}>;
    static_assert(std::numeric_limits<above_zero>::epsilon() == 10);
    static_assert(std::numeric_limits<above_zero>::round_error() == 10);
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
