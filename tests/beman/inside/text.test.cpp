// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Text: to_string, std::format, operator<<, parse and from_chars.

#include <beman/inside/inside.hpp>
#include <beman/inside/inside/casts.hpp>
#include <beman/inside/inside/detail/numbers.hpp>
#include <beman/inside/inside/detail/rational.hpp>
#include <beman/inside/inside/formats.hpp>
#include <beman/inside/inside/io.hpp>
#include <beman/inside/inside/predicates.hpp>
#include <beman/inside/math.hpp>

#include <gtest/gtest.h>

#include <cfloat>
#include <cmath>
#include <compare>
#include <cstdint>
#include <format>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

// format
namespace format_tests {
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

// rational to_string: no finite decimal prints N/D, the form parse reads
TEST(FormatTest, errc_as_text) {
    EXPECT_EQ(to_string(errc::division_by_zero), "division by zero");
    std::ostringstream os;
    os << errc::overflow;
    EXPECT_EQ(os.str(), errc_message(errc::overflow));
}

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
TEST(FormatTest, to_string_round_trips_through_parse) {
    using third = inside<{{-10, 10}, per<3>}>;
    for (const third v : {third{rational{7, 3}}, third{rational{-1, 3}}, third{rational{29, 3}}})
        ASSERT_EQ(parse<third>(beman::inside::to_string(v)), v);
    using fine   = inside<{{0, 1}, per<imax{1} << 52>}>;
    const fine e = fine::from_raw(1);
    ASSERT_EQ(parse<fine>(beman::inside::to_string(e)), e);
}

// rational max as integer formats without 1/
TEST(FormatTest, rational_max_as_integer_formats_without_1) {
    constexpr umax M = std::numeric_limits<umax>::max();
    ASSERT_EQ((beman::inside::to_string(rational{M, 1})), std::to_string(M));
}

// std::format integration is only exercised where <format> is available
// (libstdc++ from GCC 13). GCC 12 / C++20 builds skip these and keep the
// to_string()/operator<< coverage below.
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
    EXPECT_EQ(row.template operator()<round_toward_zero>(), "0.12 0.12 -0.12 -0.12");
    using floor_t = inside<{{-2, 2}, per<1000>}, round_floor>;
    EXPECT_EQ(std::format("{:.1e}", floor_t{rational{135, 1000}}), "1.3e-01");
    // The same digits a store at that precision gives.
    using cents_nearest = inside<{{-2, 2}, per<100>}, round_nearest>;
    using milli_nearest = inside<{{-2, 2}, per<1000>}, round_nearest>;
    const milli_nearest m{rational{125, 1000}};
    EXPECT_EQ(std::format("{:.2f}", m), beman::inside::to_string(cents_nearest{m}));
}

// A decimal notch prints every value with that notch's decimals; any other
// notch prints the value's shortest exact form.
TEST(FormatTest, decimal_notches_fix_the_digits) {
    using cents = inside<{{-100, 100}, per<100>}>;
    EXPECT_EQ(beman::inside::to_string(cents{rational{199, 10}}), "19.90");
    EXPECT_EQ(beman::inside::to_string(cents{2}), "2.00");
    EXPECT_EQ(beman::inside::to_string(cents{rational{-1, 20}}), "-0.05");
    EXPECT_EQ(beman::inside::to_string(cents{0}), "0.00");
    using nickel = inside<{{0, 1}, just<1, 20>}>; // 0.05: two decimals
    EXPECT_EQ(beman::inside::to_string(nickel{rational{1, 10}}), "0.10");
    using milli = inside<{{0, 1}, just<1, 200>}>; // 0.005: three
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
        EXPECT_EQ(parse<cents>(beman::inside::to_string(c)), c);
    EXPECT_EQ(std::format("{}", cents{rational{199, 10}}), "19.90");
}

// interval and grid to_string
TEST(FormatTest, interval_and_grid_to_string) {
    ASSERT_EQ((beman::inside::to_string(interval{0, 10})), "[0..10]");
    ASSERT_EQ((beman::inside::to_string(grid{interval{0, 10}, 1_r})), ("{[0..10], 1}"));
    ASSERT_EQ((beman::inside::to_string(grid{interval{0, 1}, max_denominator{7}})), ("{[0..1], 0, den<=7}"));
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

// type_name_of covers all raw types
TEST(FormatTest, type_name_covers_all_raw_types) {
    ASSERT_TRUE(type_name_of<std::uint8_t> == "uint8_t");
    ASSERT_TRUE(type_name_of<std::uint16_t> == "uint16_t");
    ASSERT_TRUE(type_name_of<std::uint32_t> == "uint32_t");
    ASSERT_TRUE(type_name_of<std::uint64_t> == "uint64_t");
    ASSERT_TRUE(type_name_of<std::int8_t> == "int8_t");
    ASSERT_TRUE(type_name_of<std::int16_t> == "int16_t");
    ASSERT_TRUE(type_name_of<std::int32_t> == "int32_t");
    ASSERT_TRUE(type_name_of<std::int64_t> == "int64_t");
    ASSERT_TRUE(type_name_of<rational> == "rational");
    ASSERT_TRUE(type_name_of<point_slot> == "point");
    ASSERT_TRUE(type_name_of<bool> == "unknown");
}

// A continuous grid with integer bounds still holds fractions: `{}` must not
// format it through the integer path.
TEST(FormatTest, continuous_grid_with_integer_bounds_formats_the_fraction) {
    using C = inside<{{0, 10}, max_denominator{1000}}>;
    const C c{rational{2, 3}};
    EXPECT_NE(std::format("{}", c), "0");
    EXPECT_EQ(std::format("{:.3f}", c), "0.667");
}
} // namespace format_tests

// Text input: parse<B> (exact, through B::try_make) and operator>>.
namespace parse_tests {
using namespace beman::inside;

namespace {
template <class B>
rational value_of(std::string_view s) {
    auto r = parse<B>(s);
    EXPECT_TRUE(r.has_value()) << s;
    return r ? rational{*r} : rational{};
}
template <class B>
errc error_of(std::string_view s) {
    auto r = parse<B>(s);
    EXPECT_FALSE(r.has_value()) << s;
    return r ? errc{} : r.error();
}
} // namespace

TEST(ParseTest, accepts_the_literal_grammar_signs_and_fractions) {
    using wide = inside<{{-100000, 100000}, per<1024>}, round_nearest>;
    EXPECT_EQ(value_of<wide>("42"), rational{42});
    EXPECT_EQ(value_of<wide>("-1.25"), (rational{-5, 4}));
    EXPECT_EQ(value_of<wide>("+0.5"), (rational{1, 2}));
    EXPECT_EQ(value_of<wide>("1'000"), rational{1000});
    EXPECT_EQ(value_of<wide>("1.5e2"), rational{150});
    EXPECT_EQ(value_of<wide>("25e-2"), (rational{1, 4}));
    EXPECT_EQ(value_of<wide>("0xff"), rational{255});
    EXPECT_EQ(value_of<wide>("0b1010"), rational{10});
    EXPECT_EQ(value_of<wide>("0x1.8p3"), rational{12});
    EXPECT_EQ(value_of<wide>("0x1p-10"), (rational{1, 1024}));
    EXPECT_EQ(value_of<wide>("-3/4"), (rational{-3, 4}));
    using thirds = inside<{{-10, 10}, per<3>}>;
    EXPECT_EQ(value_of<thirds>("7/3"), (rational{7, 3})); // exact: 0.333… has no decimal
    using tenth = inside<{{0, 1}, per<10>}>;
    EXPECT_EQ(value_of<tenth>("0.1"), (rational{1, 10})); // exact, unlike the double 0.1
}

TEST(ParseTest, reports_malformed_text_range_and_notch_errors) {
    using pct = inside<{0, 100}>;
    for (std::string_view bad : {"", "-", "abc", "1.2.3", "12x", "1e", "0x", " 5", "5 ", "0b102", "1/"})
        EXPECT_EQ(error_of<pct>(bad), errc::invalid_format) << bad;
    EXPECT_EQ(error_of<pct>("150"), errc::overflow);
    EXPECT_EQ(error_of<pct>("-1"), errc::overflow);
    EXPECT_EQ(error_of<pct>("2.5"), errc::rounding_error);
    EXPECT_EQ(error_of<pct>("1/0"), errc::division_by_zero);
    EXPECT_EQ(error_of<pct>("99999999999999999999999"), errc::overflow);

    // The policy applies: rounding and clamping instead of errors.
    using rpct = inside<{0, 100}, round_nearest | clamp>;
    EXPECT_EQ(value_of<rpct>("2.5"), rational{3});
    EXPECT_EQ(value_of<rpct>("150"), rational{100});
}

TEST(ParseTest, stream_extraction_round_trips_to_string) {
    using q = inside<{{-10, 10}, per<12>}>;
    for (auto v : {rational{-5, 4}, rational{0}, rational{7, 3}, rational{1, 12}, rational{-10}}) {
        const q           x{v};
        std::stringstream ss{to_string(x)};
        if (to_string(x).find(' ') != std::string::npos)
            continue; // mixed "2 1/3" is two tokens
        q y{0};
        ss >> y;
        EXPECT_FALSE(ss.fail()) << to_string(x);
        EXPECT_EQ(y, x) << to_string(x);
    }

    std::stringstream in{"12 abc 7"};
    inside<{0, 100}>  a{0}, b{5};
    in >> a >> b;
    EXPECT_EQ(a, 12);
    EXPECT_TRUE(in.fail());
    EXPECT_EQ(b, 5); // unchanged on error
}

TEST(ParseTest, from_chars_is_constexpr) {
    constexpr const char text[] = "0.75";
    constexpr auto       r      = parse<inside<{{0, 1}, per<4>}>>(text, text + 4);
    static_assert(r.has_value() && rational{*r} == rational{3, 4});
    constexpr const char bad[] = "1.5";
    static_assert(parse<inside<{{0, 1}, per<4>}>>(bad, bad + 3).error() == errc::overflow);
}

// Fractional trailing zeros and the exponent of zero carry no value: they must
// not overflow the 64-bit numerator / denominator.
TEST(ParseTest, trailing_zeros_and_zero_exponent) {
    using B = inside<{{0, 10}, per<100>}>;
    EXPECT_EQ(value_of<B>("1.000000000000000000000"), 1);
    EXPECT_EQ(value_of<B>("1.0500000000000000000000000"), (rational{105, 100}));
    EXPECT_EQ(value_of<B>("0.000000000000000000000000"), 0);
    EXPECT_EQ(value_of<B>("0e-30"), 0);
    EXPECT_EQ(value_of<B>("0x0p99"), 0);
    EXPECT_EQ(value_of<B>("0x1.80000000000000000p1"), 3);
    EXPECT_EQ(value_of<B>("2.50e0"), (rational{5, 2}));
    // Past the 64-bit rational: parsed exactly, then off the 1/100 grid.
    EXPECT_EQ(error_of<B>("1.00000000000000000000001"), errc::rounding_error);
    using N = inside<{{0, 10}, per<100>}, round_nearest>;
    EXPECT_EQ(value_of<N>("1.004999999999999999999999"), 1);
    static_assert(1.250000000000000000000_r == rational{5, 4});
}

TEST(ParseTest, per_call_flags) {
    using cents = inside<{{0, 100}, per<100>}>; // checked: off-grid is an error
    EXPECT_EQ(parse<cents>("1.005").error(), errc::rounding_error);
    EXPECT_EQ((rational{*parse<cents, round_nearest>("1.005")}), (rational{101, 100}));
    EXPECT_EQ((rational{*parse<cents, round_floor>("1.009")}), (rational{1}));
    EXPECT_EQ((rational{*parse<cents, clamp>("150")}), rational{100});
    // Long decimals take the exact parse, with the same flags.
    EXPECT_EQ((rational{*parse<cents, round_nearest>("1.004999999999999999999999")}), rational{1});
}

TEST(ParseTest, exact_rejects_what_the_policy_would_round_or_clamp) {
    using price = inside<{{0, 100}, per<100>}, round_nearest | clamp>;
    EXPECT_EQ(rational{*parse<price>("1.005")}, (rational{101, 100})); // rounded
    EXPECT_EQ(parse_exact<price>("1.005").error(), errc::rounding_error);
    EXPECT_EQ(parse_exact<price>("150").error(), errc::overflow);
    EXPECT_EQ(parse_exact<price>("1.0x").error(), errc::invalid_format);
    EXPECT_EQ(rational{*parse_exact<price>("12.34")}, (rational{1234, 100}));
    static_assert(std::is_same_v<decltype(parse_exact<price>("1"))::value_type, price>);
    EXPECT_EQ(parse_exact<price>("1.004999999999999999999999").error(), errc::rounding_error);
}

TEST(ParseTest, try_make_takes_an_expected) {
    using volts = inside<{{-1, 1}, per<1000>}, round_nearest>;
    using any   = inside<{0, 100}>;
    EXPECT_EQ(rational{*volts::try_make(any{1} / any{3})}, (rational{333, 1000}));
    EXPECT_EQ(volts::try_make(any{1} / any{0}).error(), errc::division_by_zero);
    EXPECT_EQ(volts::try_make(any{3} / any{2}).error(), errc::overflow);
    using strict = inside<{{-1, 1}, per<1000>}>;
    EXPECT_EQ(strict::try_make(any{1} / any{3}).error(), errc::rounding_error);
    EXPECT_EQ(rational{*strict::try_make<round_nearest>(any{1} / any{3})}, (rational{333, 1000}));
}
} // namespace parse_tests

// Pinned defects: each case failed before its fix.
namespace pinned_tests {
using namespace beman::inside;
using namespace beman::inside::detail;

// Text past the 64-bit parse: the wide fallback reads the whole grammar
// (hex, binary, p exponents, separators), and a value too fine even for it
// stores by a same-sign stand-in below every grid step.
TEST(TextTest, parse_past_64_bits) {
    using H  = inside<{{0, 1}, per<100>}, round_nearest>;
    using HC = inside<{{0, 1}, per<100>}, round_ceil>;
    using S  = inside<{{0, 1}, per<100>}>;
    EXPECT_EQ(*parse<H>(std::string_view{"1e-200"}), 0);
    EXPECT_EQ(*parse<HC>(std::string_view{"1e-200"}), rational(1, 100));
    EXPECT_EQ(parse<S>(std::string_view{"1e-200"}).error(), errc::rounding_error);
    EXPECT_EQ(*parse<HC>(std::string_view{"-1e-200"}), 0);
    EXPECT_EQ(*parse<HC>(std::string_view{"1e-5000"}), rational(1, 100)); // past the wide parse
    EXPECT_EQ(parse_exact<S>(std::string_view{"1e-5000"}).error(), errc::rounding_error);
    EXPECT_TRUE(*parse<uint64>(std::string_view{"0x1p63"}) == (umax{1} << 63));
    EXPECT_EQ(*parse<H>(std::string_view{"0x1p-70"}), 0);
    EXPECT_TRUE(*parse<uint64>(std::string_view{"18'446'744'073'709'551'615"}) == ~umax{0});
    const std::string third = "0." + std::string(299, '3') + "4";
    EXPECT_EQ(*parse<H>(std::string_view{third}), rational(33, 100));
    const std::string over_half = "0.5" + std::string(298, '0') + "1";
    EXPECT_EQ((*parse<inside<{0, 1}, round_nearest>>(std::string_view{over_half})), 1);
}

// Text past the wide parse ignored clamp (1e1300 overflowed) and N/D text
// had no tiny stand-in.
TEST(TextTest, from_chars_stand_ins_for_huge_and_fraction_text) {
    using price = inside<{{0, 1'000'000}, per<100>}, round_nearest | clamp>;
    EXPECT_EQ(*parse<price>("1e1300"), 1'000'000);
    EXPECT_EQ(*parse<price>("-1e1300"), 0);
    EXPECT_EQ(*parse<price>("1/3e5000"), 0);
    EXPECT_EQ(*parse<price>("1e-5000"), 0);
    using checked_price = inside<{{0, 1'000'000}, per<100>}>;
    EXPECT_EQ(parse<checked_price>("1e1300").error(), errc::overflow);
    EXPECT_EQ(parse_exact<price>("1/3e5000").error(), errc::rounding_error);
    EXPECT_EQ(parse_exact<price>("1e1300").error(), errc::overflow);
}

// parse_exact rounded off-grid wide text onto a bounded continuous
// grid under a rounding policy instead of reporting rounding_error.
TEST(TextTest, from_chars_exact_bounded_continuous) {
    using B = inside<grid{{0, 1}, max_denominator{5}}, round_nearest>;
    EXPECT_EQ(parse_exact<B>("0.40000000000000000000000000001").error(), errc::rounding_error);
    EXPECT_EQ(parse_exact<B>("1e-5000").error(), errc::rounding_error);
    EXPECT_EQ(parse_exact<B>("0.41").error(), errc::rounding_error);
    EXPECT_EQ(*parse_exact<B>("0.4"), rational(2, 5));
}

//---------------------------------------------------------------------------
// to_string(rational) (io.hpp) — a decimal fraction needs leading-zero padding.
//---------------------------------------------------------------------------
// rational to_string zero-pads short decimal fractions
TEST(TextTest, rational_to_string_zero_pads_short_decimal_fractions) {
    ASSERT_EQ((beman::inside::to_string(rational{1u, 16})), "0.0625"); // frac "625" -> "0625"
    ASSERT_EQ((beman::inside::to_string(rational{1u, 100})), "0.01");  // frac  "1"  -> "01"
    ASSERT_EQ((beman::inside::to_string(rational{3u, 16})), "0.1875");
}

} // namespace pinned_tests

// Arms no other test reaches, found by the reachability audit (gcov and an
// `if constexpr` probe over the suite).
namespace reached_arms_tests {
using namespace beman::inside;
using detail::rational;

// parse_exact takes a fraction exactly: onto a lattice, onto a point, and
// N/0 is a zero denominator.
TEST(TextArmTest, parse_exact_of_a_fraction) {
    EXPECT_EQ((rational{*parse_exact<inside<{{0, 1}, per<4>}>>("1/2")}), (rational{1, 2}));
    EXPECT_TRUE((*parse_exact<inside<{5, 5}>>("10/2") == 5));
    EXPECT_EQ((parse_exact<inside<{{0, 1}, per<4>}>>("1/0").error()), errc::division_by_zero);
}
} // namespace reached_arms_tests

namespace reached_arms_tests {
using namespace beman::inside;
using detail::rational;

// Run-time parse: upper-case hex digits, a separator in an exponent, a dot in
// a binary number, and fractions past the 64-bit rationals (an over-long hex
// fraction, a p or decimal exponent past the denominator) taken exactly.
TEST(TextArmTest, parse_of_the_scanner_edge_cases) {
    EXPECT_TRUE((*parse<inside<{0, 1000}>>("0xFF") == 255));
    EXPECT_TRUE((*parse<inside<{0, 100000}>>("1e0'3") == 1000));
    EXPECT_EQ((parse<inside<{0, 1000}>>("0b1.1").error()), errc::invalid_format);
    using F = inside<{{0, 1}, max_denominator{1000000000000000000_g}}>;            // 10^18
    EXPECT_EQ(parse<F>("0x0.0000000000000001p0").error(), errc::rounding_error);   // 2^-64
    EXPECT_EQ(parse<F>("0x1.8p-63").error(), errc::rounding_error);                // 3·2^-64
    EXPECT_EQ(parse<F>("0.0000000000000000000001").error(), errc::rounding_error); // 10^-22
    EXPECT_TRUE((*parse<F>("0x1p-2") == rational{1, 4}));
}

// format: a rounding carry in fixed and scientific forms, %g's scientific
// choice, the sign kept out of the padded shortest form, and a precision
// without digits.
TEST(TextArmTest, format_edge_cases) {
    using Q = inside<{{0, 20}, per<1000>}>;
    EXPECT_EQ(std::format("{:.2f}", (Q{rational{9999, 1000}})), "10.00");
    EXPECT_EQ(std::format("{:.0e}", (Q{rational{96, 10}})), "1e+01");
    using G = inside<{{0, 1000000}, per<100000>}>;
    EXPECT_EQ(std::format("{:g}", (G{rational{1, 100000}})), "1e-05");
    EXPECT_EQ(std::format("{:g}", G{1000000}), "1e+06");
    const inside<{{-2, 2}, per<2>}> h{-1.5};
    EXPECT_EQ(std::format("{:>6}", h), "  -1.5");
    const std::string spec = "{:.}";
    EXPECT_THROW((void)std::vformat(spec, std::make_format_args(h)), std::format_error);
}

// >> on an empty stream sets failbit and leaves the value.
TEST(TextArmTest, extraction_from_an_empty_stream) {
    std::istringstream        in{""};
    inside<{{-2, 2}, per<2>}> v{1};
    in >> v;
    EXPECT_TRUE((in.fail() && v == 1));
}

// A grid number past 4096 bits prints its numerator and denominator.
TEST(TextArmTest, to_string_of_a_huge_grid_number) {
    constexpr detail::grid_rational big = detail::grid_rational{detail::grid_int{1} << 5000} / 3;
    const std::string               s   = to_string(big);
    EXPECT_EQ(s.substr(s.size() - 2), "/3");
    EXPECT_EQ(s.substr(0, 10), "1412467032");
}
} // namespace reached_arms_tests

namespace reached_arms_tests {
using namespace beman::inside;

// Text past the wide parse stands in by its magnitude: tiny rounds to 0,
// huge is past the grid.
TEST(TextArmTest, numbers_past_the_wide_parse) {
    using I = inside<{{0, 10}, per<1024>}, round_nearest>;
    EXPECT_TRUE((*parse<I>("1e-5000") == 0));
    EXPECT_TRUE((*parse<I>("0x1p-5000") == 0));
    EXPECT_TRUE((*parse<I>("0.0001e-5000") == 0));
    EXPECT_EQ(parse<I>("1e5000").error(), errc::overflow);
}
} // namespace reached_arms_tests

namespace reached_arms_tests {
using namespace beman::inside;

// parse_exact of a fraction onto a notch-0 point.
TEST(TextArmTest, parse_exact_onto_a_notch_zero_point) { EXPECT_TRUE((*parse_exact<inside<grid{5}>>("10/2") == 5)); }
} // namespace reached_arms_tests

namespace reached_arms_tests {
using namespace beman::inside;

// 1e-19 reduces to a denominator past imax: taken exactly, off a small bound;
// a grid number of 512 to 4096 bits prints in full.
TEST(TextArmTest, denominator_past_imax_and_a_kilobit_grid_number) {
    EXPECT_EQ((parse<inside<{{0, 1}, max_denominator{1000}}>>("1e-19").error()), errc::rounding_error);
    constexpr auto g = detail::grid_rational{detail::grid_int{1} << 1000} / 7;
    EXPECT_EQ(to_string(g).substr(0, 12), "107150860718");
}
} // namespace reached_arms_tests
