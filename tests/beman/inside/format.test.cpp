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

// −0 prints as 0
TEST(FormatTest, negative_zero_prints_as_zero) {
    using Q = inside<{{-4, 4}, per<1024>}, round_nearest>;
    EXPECT_EQ(beman::inside::to_string(-Q{0}), "0");
}

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

// std::format numeric specs - by storage
TEST(FormatTest, std_format_numeric_specs_by_storage) {
    // A thirds grid: empty spec keeps the exact fraction; a numeric spec
    // routes through std::formatter<double> like any fractional grid.
    using ex = inside<{{0, 1}, per<3>}>;
    ex e{rational{2u, 3}};
    ASSERT_EQ((std::format("{}", e)), "2/3");
    ASSERT_EQ((std::format("{:.3f}", e)), (std::format("{:.3f}", static_cast<double>(rational{e}))));

    // A signed whole-number grid: specs format the VALUE (via
    // std::formatter<imax>).
    using ix = inside<{-5, 5}>;
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

    // Non-empty spec — rounded from the exact value (the same digits a double
    // formatter gives where the double is close enough).
    rational r = rational{1u, 3};
    ASSERT_EQ((std::format("{:.3f}", r)), (std::format("{:.3f}", static_cast<double>(r))));
    ASSERT_EQ((std::format("{:.6f}", r)), (std::format("{:.6f}", static_cast<double>(r))));
    ASSERT_EQ((std::format("{:e}", r)), (std::format("{:e}", static_cast<double>(r))));
    ASSERT_EQ((std::format("{:.25f}", r)), "0.3333333333333333333333333");
}

// Exact specs: digits rounded from the exact value, ties to even like printf.
TEST(FormatTest, std_format_exact_specs) {
    using q = inside<{{-2, 2}, per<1024>}>;
    EXPECT_EQ(std::format("{:.2f} {:.2f} {:.0f} {:.0f}", q{0.125}, q{0.375}, q{0.5}, q{1.5}), "0.12 0.38 0 2");
    EXPECT_EQ(std::format("{:e} {:.3g} {:G}", q{0.0009765625}, q{0.0009765625}, q{0.0009765625}),
              "9.765625e-04 0.000977 0.000976562");
    EXPECT_EQ(std::format("[{:>8.3f}] [{:<8.1f}] [{:*^9.1f}] [{:+.1f}] [{: .1f}] [{:08.2f}]",
                          q{-0.5},
                          q{0.5},
                          q{0.5},
                          q{0.5},
                          q{0.5},
                          q{-0.5}),
              "[  -0.500] [0.5     ] [***0.5***] [+0.5] [ 0.5] [-0000.50]");
    EXPECT_EQ(std::format("{:#.0f} {:#.0e}", q{1.5}, q{1.5}), "2. 2.e+00");
    // A width alone keeps the exact text.
    using third = inside<{{0, 10}, per<3>}>;
    EXPECT_EQ(std::format("[{:>6}] [{:.4f}]", third{rational{7, 3}}, third{rational{7, 3}}), "[   7/3] [2.3333]");
    // A wide value past 2^53: exact digits a double cannot give.
    using fine     = inside<{{0, umax{1} << 34}, per<(umax{1} << 32)>}>;
    const fine odd = fine::from_raw((detail::wide_uint<2>{1} << 65) + detail::wide_uint<2>{1});
    EXPECT_EQ(std::format("{:.12f}", odd), "8589934592.000000000233");
    EXPECT_EQ(std::format("{:.15e}", odd), "8.589934592000000e+09");
    EXPECT_THROW((void)std::vformat("{:x}", std::make_format_args(odd)), std::format_error);
}

// A spec rounds by the type's own rounding mode, as storing at that
// precision would; without one, ties to even.
TEST(FormatTest, std_format_specs_follow_the_rounding_policy) {
    auto row = []<policy_flag P>() {
        using T = inside<{{-2, 2}, per<1000>}, P>;
        return std::format("{:.2f} {:.2f} {:.2f} {:.2f}",
                           T{rational{125, 1000}},
                           T{rational{129, 1000}},
                           T{rational{-125, 1000}},
                           T{rational{-129, 1000}});
    };
    EXPECT_EQ(row.template operator()<checked>(), "0.12 0.13 -0.12 -0.13");
    EXPECT_EQ(row.template operator()<round_half_even>(), "0.12 0.13 -0.12 -0.13");
    EXPECT_EQ(row.template operator()<round_nearest>(), "0.13 0.13 -0.13 -0.13");
    EXPECT_EQ(row.template operator()<round_floor>(), "0.12 0.12 -0.13 -0.13");
    EXPECT_EQ(row.template operator()<round_ceil>(), "0.13 0.13 -0.12 -0.12");
    EXPECT_EQ(row.template operator()<snap>(), "0.12 0.12 -0.12 -0.12");
    using floor_t = inside<{{-2, 2}, per<1000>}, round_floor>;
    EXPECT_EQ(std::format("{:.1e}", floor_t{rational{135, 1000}}), "1.3e-01");
    // The same digits a store at that precision gives.
    using cents_nearest = inside<{{-2, 2}, per<100>}, round_nearest>;
    using milli_nearest = inside<{{-2, 2}, per<1000>}, round_nearest>;
    const milli_nearest m{rational{125, 1000}};
    EXPECT_EQ(std::format("{:.2f}", m), beman::inside::to_string(cents_nearest{m}));
}
#endif // __cpp_lib_format

// A decimal notch prints every value with that notch's decimals; any other
// notch prints the value's shortest exact form.
TEST(FormatTest, decimal_notches_fix_the_digits) {
    using cents = inside<{{-100, 100}, per<100>}>;
    EXPECT_EQ(beman::inside::to_string(cents{rational{199, 10}}), "19.90");
    EXPECT_EQ(beman::inside::to_string(cents{2}), "2.00");
    EXPECT_EQ(beman::inside::to_string(cents{rational{-1, 20}}), "-0.05");
    EXPECT_EQ(beman::inside::to_string(cents{0}), "0.00");
    using nickel = inside<{{0, 1}, frac<1, 20>}>; // 0.05: two decimals
    EXPECT_EQ(beman::inside::to_string(nickel{rational{1, 10}}), "0.10");
    using milli = inside<{{0, 1}, frac<1, 200>}>; // 0.005: three
    EXPECT_EQ(beman::inside::to_string(milli{rational{1, 2}}), "0.500");
    // Binary and other notches: the value decides.
    using quarter = inside<{{0, 4}, per<4>}>;
    EXPECT_EQ(beman::inside::to_string(quarter{1.5}), "1.5");
    EXPECT_EQ(beman::inside::to_string(quarter{2}), "2");
    using sixth = inside<{{0, 4}, per<6>}>;
    EXPECT_EQ(beman::inside::to_string(sixth{rational{1, 2}}), "0.5");
    EXPECT_EQ(beman::inside::to_string(sixth{rational{1, 3}}), "1/3");
    // Every form reads back.
    for (const cents c : {cents{rational{199, 10}}, cents{-1}, cents{0}})
        EXPECT_EQ(from_chars<cents>(beman::inside::to_string(c)), c);
#ifdef __cpp_lib_format
    EXPECT_EQ(std::format("{}", cents{rational{199, 10}}), "19.90");
#endif
}

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

// type_name_v covers all raw types
TEST(FormatTest, type_name_covers_all_raw_types) {
    ASSERT_TRUE(type_name_v<std::uint8_t> == "uint8_t");
    ASSERT_TRUE(type_name_v<std::uint16_t> == "uint16_t");
    ASSERT_TRUE(type_name_v<std::uint32_t> == "uint32_t");
    ASSERT_TRUE(type_name_v<std::uint64_t> == "uint64_t");
    ASSERT_TRUE(type_name_v<std::int8_t> == "int8_t");
    ASSERT_TRUE(type_name_v<std::int16_t> == "int16_t");
    ASSERT_TRUE(type_name_v<std::int32_t> == "int32_t");
    ASSERT_TRUE(type_name_v<std::int64_t> == "int64_t");
    ASSERT_TRUE(type_name_v<rational> == "rational");
    ASSERT_TRUE(type_name_v<point_slot> == "point");
    ASSERT_TRUE(type_name_v<bool> == "unknown");
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
