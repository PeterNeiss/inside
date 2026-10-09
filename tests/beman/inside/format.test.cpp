// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

#include <gtest/gtest.h>

#include <version> // __cpp_lib_format
#ifdef __cpp_lib_format
    #include <format>
#endif
#include <limits>
#include <sstream>

using namespace beman::inside;
using namespace beman::inside::detail;

// rational to_string: decimal forms
TEST(FormatTest, rational_to_string_decimal_forms) {
    ASSERT_EQ((beman::inside::to_string(rational{1u, 2})), "0.5");
    ASSERT_EQ((beman::inside::to_string(rational{43u, 2})), "21.5");
    ASSERT_EQ((beman::inside::to_string(rational{1u, 4})), "0.25");
    ASSERT_EQ((beman::inside::to_string(rational{3u, 8})), "0.375");
    ASSERT_EQ((beman::inside::to_string(rational{1u, 10})), "0.1");
    ASSERT_EQ((beman::inside::to_string(rational{15u, 100})), "0.15");

    // negative
    ASSERT_EQ((beman::inside::to_string(rational{1, -2})), "-0.5");
    ASSERT_EQ((beman::inside::to_string(rational{43, -2})), "-21.5");
}

// rational to_string: no finite decimal prints N/D, the form from_chars reads
TEST(FormatTest, rational_to_string_fraction_forms) {
    ASSERT_EQ((beman::inside::to_string(rational{7u, 3})), "7/3");
    ASSERT_EQ((beman::inside::to_string(rational{22u, 7})), "22/7");
    ASSERT_EQ((beman::inside::to_string(rational{1u, 3})), "1/3");
    ASSERT_EQ((beman::inside::to_string(rational{2u, 7})), "2/7");
    ASSERT_EQ((beman::inside::to_string(rational{5u, 1})), "5");
    ASSERT_EQ(beman::inside::to_string(0_r), "0");

    ASSERT_EQ((beman::inside::to_string(rational{7, -3})), "-7/3");
    ASSERT_EQ((beman::inside::to_string(rational{1, -3})), "-1/3");
}

// rational to_string: a terminating decimal prints in full however many digits
TEST(FormatTest, rational_to_string_long_decimals) {
    constexpr umax M = std::numeric_limits<umax>::max();
    ASSERT_EQ((beman::inside::to_string(rational{M, 2})), "9223372036854775807.5");
    ASSERT_EQ((beman::inside::to_string(rational{M, -2})), "-9223372036854775807.5");
    ASSERT_EQ((beman::inside::to_string(rational{M / 5, 2})), "1844674407370955161.5");
    ASSERT_EQ((beman::inside::to_string(rational{1u, imax{1} << 52})),
              "0.0000000000000002220446049250313080847263336181640625");
    ASSERT_EQ((beman::inside::to_string(rational{M, imax{1} << 62})),
              "3.99999999999999999978315956550289911319850943982601165771484375");
}

#ifndef BEMAN_INSIDE_MATH_NO_FP // f64 storage is compiled out under the integer engine
// A continuous f64 inside prints the double's exact decimal
TEST(FormatTest, continuous_f64_prints_the_exact_double) {
    using R = inside<{{0, 1}, 0}, f64>;
    ASSERT_EQ(beman::inside::to_string(R{0.1}), "0.1000000000000000055511151231257827021181583404541015625");
    ASSERT_EQ(beman::inside::to_string(R{0.5}), "0.5");
    ASSERT_EQ(beman::inside::to_string(R{0.0}), "0");
}
#endif

// to_string output parses back to the same value
TEST(FormatTest, to_string_round_trips_through_from_chars) {
    using third = inside<{{-10, 10}, per<3>}>;
    for (const third v : {third{rational{7, 3}}, third{rational{-1, 3}}, third{rational{29, 3}}})
        ASSERT_EQ(from_chars<third>(beman::inside::to_string(v)), v);
    using fine   = inside<{{0, 1}, per<imax{1} << 52>}>;
    const fine e = fine::from_raw(1);
    ASSERT_EQ(from_chars<fine>(beman::inside::to_string(e)), e);
}

// rational max as integer formats without 1/
TEST(FormatTest, rational_max_as_integer_formats_without_1) {
    constexpr umax M = std::numeric_limits<umax>::max();
    ASSERT_EQ((beman::inside::to_string(rational{M, 1})), std::to_string(M));
}

// std::format integration is only exercised where <format> is available
// (libstdc++ from GCC 13). GCC 12 / C++20 builds skip these and keep the
// to_string()/operator<< coverage below.
#ifdef __cpp_lib_format
// std::format integration
TEST(FormatTest, std_format_integration) {
    ASSERT_EQ((std::format("{}", inside<{0, 99}>{42})), "42");
    ASSERT_EQ((std::format("[{:>6}]", inside<{0, 99}>{42})), "[    42]");
    ASSERT_EQ((std::format("[{:<6}]", inside<{0, 99}>{42})), "[42    ]");
    ASSERT_EQ((std::format("[{:0>4}]", inside<{0, 99}>{42})), "[0042]");
    ASSERT_EQ((std::format("{}", rational{1u, 2})), "0.5");
}

// std::format numeric specs - integer-aligned inside
TEST(FormatTest, std_format_numeric_specs_integer_aligned_inside) {
    // Integer-aligned bounds route through std::formatter<imax>, so every
    // standard integer spec flows through and matches a direct integer format.
    using pct = inside<{0, 100}>;
    pct  x{42};
    imax v = to_value(x);

    ASSERT_EQ((std::format("{:5}", x)), (std::format("{:5}", v)));
    ASSERT_EQ((std::format("{:+}", x)), (std::format("{:+}", v)));
    ASSERT_EQ((std::format("{:#x}", x)), (std::format("{:#x}", v)));
    ASSERT_EQ((std::format("{:o}", x)), (std::format("{:o}", v)));
    ASSERT_EQ((std::format("{:b}", x)), (std::format("{:b}", v)));
    ASSERT_EQ((std::format("{:08}", x)), (std::format("{:08}", v)));

    // signed inside
    using s100 = inside<{-100, 100}>;
    s100 y{-7};
    ASSERT_EQ((std::format("{:+}", y)), "-7");
    ASSERT_EQ((std::format("{:4}", y)), "  -7");
}

// std::format numeric specs - fractional inside goes through double
TEST(FormatTest, std_format_numeric_specs_fractional_inside_goes_through_double) {
    // Fractional grids fall to std::formatter<double> when a spec is present.
    using frac = inside<{{0, 2}, rational{1u, 4}}>; // 0.25 notch
    frac   f{rational{5u, 4}};                      // value = 1.25
    double d = static_cast<double>(rational{f});

    ASSERT_EQ((std::format("{:.2f}", f)), (std::format("{:.2f}", d)));
    ASSERT_EQ((std::format("{:.4f}", f)), (std::format("{:.4f}", d)));
    ASSERT_EQ((std::format("{:e}", f)), (std::format("{:e}", d)));

    // empty spec preserves exact-rational form
    ASSERT_EQ((std::format("{}", f)), "1.25");
}

// std::format numeric specs - representation flags
TEST(FormatTest, std_format_numeric_specs_representation_flags) {
    // `exact` on a notched grid: empty spec keeps the exact fraction; a
    // numeric spec routes through std::formatter<double> like any fractional
    // grid.
    using ex = inside<{{0, 1}, per<3>}, exact>;
    ex e{rational{2u, 3}};
    ASSERT_EQ((std::format("{}", e)), "2/3");
    ASSERT_EQ((std::format("{:.3f}", e)), (std::format("{:.3f}", static_cast<double>(rational{e}))));

    // `indexed` keeps the integer-grid routing: specs format the VALUE (via
    // std::formatter<imax>), never the raw index.
    using ix = inside<{-5, 5}, indexed>;
    ix i{-3};
    ASSERT_EQ((std::format("{}", i)), "-3");
    ASSERT_EQ((std::format("{:>4}", i)), "  -3");
    ASSERT_EQ((std::format("{:+}", i)), "-3");
}

// std::format numeric specs - rational
TEST(FormatTest, std_format_numeric_specs_rational) {
    // Empty spec — exact via to_string.
    ASSERT_EQ((std::format("{}", rational{1u, 3})), "1/3");
    ASSERT_EQ((std::format("{}", rational{7u, 3})), "7/3");

    // Non-empty spec — double formatter.
    rational r = rational{1u, 3};
    ASSERT_EQ((std::format("{:.3f}", r)), (std::format("{:.3f}", static_cast<double>(r))));
    ASSERT_EQ((std::format("{:.6f}", r)), (std::format("{:.6f}", static_cast<double>(r))));
    ASSERT_EQ((std::format("{:e}", r)), (std::format("{:e}", static_cast<double>(r))));
}
#endif // __cpp_lib_format

// interval and grid to_string
TEST(FormatTest, interval_and_grid_to_string) {
    ASSERT_EQ((beman::inside::to_string(interval{0, 10})), "[0..10]");
    ASSERT_EQ((beman::inside::to_string(grid{interval{0, 10}, 1_r})), ("{[0..10], 1}"));
}

// ostream operator<< for rational
TEST(FormatTest, ostream_operator_for_rational) {
    std::ostringstream os;
    os << rational{3u, 4};
    ASSERT_EQ(os.str(), "0.75");

    std::ostringstream os2;
    os2 << rational{1, -3};
    ASSERT_EQ(os2.str(), "-1/3");
}

// ostream operator<< for inside
TEST(FormatTest, ostream_operator_for_inside) {
    std::ostringstream os;
    os << inside<{0, 100}>{42};
    ASSERT_EQ(os.str(), "42");

    std::ostringstream os2;
    os2 << inside<{{-5, 5}, 0.5}>{2.5};
    ASSERT_EQ(os2.str(), "2.5");
}

// to_string_debug emits raw, type, and grid
TEST(FormatTest, to_string_debug_emits_raw_type_and_grid) {
    using pct = inside<{0, 100}>;
    pct  x{42};
    auto s = to_string_debug(x);

    // sanity: value, raw type, and grid all surface in the debug string
    ASSERT_TRUE(s.find("42") != std::string::npos);
    ASSERT_TRUE(s.find("uint8_t") != std::string::npos);
    ASSERT_TRUE(s.find("[0..100]") != std::string::npos);

    // signed-storage path — raw type should show the chosen signed integer
    using s8 = inside<{-100, 100}>;
    s8   y{-7};
    auto sd = to_string_debug(y);
    ASSERT_TRUE(sd.find("-7") != std::string::npos);
    ASSERT_TRUE(sd.find("int8_t") != std::string::npos);
}

// type_name covers all raw types
TEST(FormatTest, type_name_covers_all_raw_types) {
    ASSERT_TRUE(type_name<std::uint8_t>() == "uint8_t");
    ASSERT_TRUE(type_name<std::uint16_t>() == "uint16_t");
    ASSERT_TRUE(type_name<std::uint32_t>() == "uint32_t");
    ASSERT_TRUE(type_name<std::uint64_t>() == "uint64_t");
    ASSERT_TRUE(type_name<std::int8_t>() == "int8_t");
    ASSERT_TRUE(type_name<std::int16_t>() == "int16_t");
    ASSERT_TRUE(type_name<std::int32_t>() == "int32_t");
    ASSERT_TRUE(type_name<std::int64_t>() == "int64_t");
    ASSERT_TRUE(type_name<rational>() == "rational");
    ASSERT_TRUE(type_name<float>() == "unknown");
}

#ifdef __cpp_lib_format
// A continuous grid with integer bounds still holds fractions: `{}` must not
// format it through the integer path.
TEST(FormatTest, continuous_grid_with_integer_bounds_formats_the_fraction) {
    using C = inside<{{0, 10}, rational{0}}>;
    const C c{rational{2, 3}};
    EXPECT_NE(std::format("{}", c), "0");
    EXPECT_EQ(std::format("{:.3f}", c), "0.667");
}
#endif
