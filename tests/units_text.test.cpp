// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Quantities and points read from text with their units: parse, parse_exact, >> into an expected.

#include <inside/units.hpp>
#include <inside/units/io.hpp>

#include "units_text_other.hpp"

#include <gtest/gtest.h>

#include <expected>
#include <sstream>
#include <string>
#include <string_view>

namespace units_text_tests {
using namespace ins;

namespace {
using metres   = quantity<si::metre, inside<{{0, 100'000}, per<1000>}>>;
using minutes  = quantity<si::minute, inside<{{0, 1000}, per<10>}>>;
using speeds   = quantity<si::metre / si::second, inside<{{0, 100}, per<100>}>>;
using areas    = quantity<square(si::metre), inside<{{0, 1'000'000}, per<1000>}>>;
using energies = quantity<si::joule, inside<{{0, 1'000'000}, per<1000>}>>;
using percents = quantity<percent, inside<{{0, 100}, per<10>}>>;
using ones     = quantity<one, inside<{{0, 10}, per<4>}>>;
using hertzes  = quantity<si::hertz, inside<{0, 100}>>;
using becqs    = quantity<si::becquerel, inside<{0, 100}>>;
using radians  = quantity<si::radian, inside<{{0, 7}, per<1000>}, round_half_away>>;
using degrees  = quantity<si::degree, inside<{0, 360}>>;
using kelvins  = quantity<si::kelvin, inside<{{0, 100}, per<10>}>>;
using rounding = quantity<si::metre, inside<{{0, 100}, per<100>}, round_half_away>>;
using celsius  = quantity_point<si::degree_Celsius, si::ice_point, inside<{{-300, 300}, max_denominator{180}}>>;

inline constexpr auto level_kind = kind<"level", isq::dimensionless, {.distinct = true}>;
inline constexpr auto dB         = unit<"dB", one, {.kind = level_kind}>;
using levels                     = quantity<dB, inside<{{-100, 100}, per<10>}>>;

// Seen by this translation unit only (units_text_other.cpp does not see it).
inline constexpr auto cent = unit<"¢", 0.01_ins * units_text_other::USD, {.ascii = "c"}>;
inline constexpr auto ftm  = unit<"ftm", 1.8288_ins * si::metre>;

template <class Q, class Tag>
concept can_parse_in = requires(Tag u) { parse<Q>("1", u); };

template <class Q>
std::string read(std::string_view text) {
    const auto r = parse<Q>(text);
    return r ? to_string(*r) : std::string{"error: "} + error_message(r.error());
}
} // namespace

TEST(UnitsTextTest, to_string_reads_back) {
    const auto check = [](const auto& q) {
        using Q      = std::remove_cvref_t<decltype(q)>;
        const auto r = parse<Q>(to_string(q));
        ASSERT_TRUE(r) << to_string(q);
        EXPECT_TRUE(*r == q) << to_string(q);
        const auto a = parse<Q>(to_string(q, text_encoding::ascii));
        ASSERT_TRUE(a) << to_string(q, text_encoding::ascii);
        EXPECT_TRUE(*a == q) << to_string(q, text_encoding::ascii);
    };
    check(metres{12.5, si::metre});
    check(speeds{12.34_ins, si::metre / si::second});
    check(areas{2.5, square(si::metre)});
    check(energies{5, si::joule});
    check(percents{12.5, percent});
    check(degrees{90, si::degree});
    check(levels{-3.5, dB});
    check(quantity<si::kilo(si::metre) / si::hour, inside<{0, 300}>>{120, si::kilo(si::metre) / si::hour});
    check(quantity<units_text_other::USD / units_text_other::item, inside<{{0, 100}, per<100>}>>{
        19.99_ins, units_text_other::USD / units_text_other::item});
    check(quantity<si::micro(si::second), inside<{0, 1000}>>{250, si::micro(si::second)});
    check(celsius{21.5, si::degree_Celsius});
    check(quantity_point<international::degree_Fahrenheit, international::zeroth_degree_Fahrenheit,
                         inside<{0, 600}>>{350, international::degree_Fahrenheit});
    // The common unit of a sum: a scaled unit, [1/381 ft].
    check(just<1> * international::foot + just<1> * si::metre);
    // Unit one prints no unit, and reads a bare number.
    check(ones{2.25, one});
}

TEST(UnitsTextTest, any_unit_that_converts_exactly) {
    EXPECT_EQ(read<metres>("12.5 km"), "12500.000 m");
    EXPECT_EQ(read<metres>("12000 µm"), "0.012 m");
    EXPECT_EQ(read<metres>("12000 um"), "0.012 m");
    EXPECT_EQ(read<metres>("1 mi"), "1609.344 m");
    EXPECT_EQ(read<metres>("5 ftm"), "9.144 m"); // a unit of your own
    EXPECT_EQ(read<minutes>("1.5 h"), "90.0 min");
    EXPECT_EQ(read<minutes>("90 s"), "1.5 min");
    EXPECT_EQ(read<speeds>("36 km/h"), "10.00 m/s");
    EXPECT_EQ(read<speeds>("36 km h^-1"), "10.00 m/s");
    EXPECT_EQ(read<speeds>("36 km h⁻¹"), "10.00 m/s");
    EXPECT_EQ(read<areas>("0.003 km²"), "3000.000 m²");
    EXPECT_EQ(read<areas>("2.5 m^2"), "2.500 m²");
    EXPECT_EQ(read<energies>("5 kJ"), "5000.000 J");
    EXPECT_EQ(read<energies>("5 kg m²/s²"), "5.000 J");
    EXPECT_EQ(read<energies>("5 kg·m²/s²"), "5.000 J");
    EXPECT_EQ(read<energies>("5 N*m"), "5.000 J");
    EXPECT_EQ(read<energies>("5 kg m^2 s^-2"), "5.000 J");
    EXPECT_EQ(read<percents>("150 ‰"), "15.0 %");
    EXPECT_EQ(read<percents>("150 permille"), "15.0 %");
    EXPECT_EQ(read<hertzes>("5 1/s"), "5 Hz"); // a compound unit's kind is the target's
    EXPECT_EQ(read<kelvins>("5 ℃"), "5.0 K");  // a quantity is a difference: no origin
    EXPECT_EQ(read<units_text_other::dollars>("1999 ¢"), "19.99 USD");
    EXPECT_EQ(read<units_text_other::dollars>("1999 c"), "19.99 USD");
}

TEST(UnitsTextTest, the_two_apart_and_a_unit_in_code) {
    EXPECT_EQ(to_string(*parse<metres>("12.5", "km")), "12500.000 m");
    EXPECT_EQ(to_string(*parse<metres>("12.5", si::kilo(si::metre))), "12500.000 m");
    EXPECT_EQ(to_string(*parse<celsius>("350", international::degree_Fahrenheit)), "530/3 ℃");
    EXPECT_EQ(parse<metres>("12.5", "").error(), error::missing_unit);
    // A unit named in code that does not convert exactly does not compile.
    static_assert(can_parse_in<metres, decltype(si::kilo(si::metre))>);
    static_assert(!can_parse_in<metres, decltype(si::second)>);
    static_assert(!can_parse_in<radians, decltype(si::degree)>);
    static_assert(!can_parse_in<celsius, decltype(si::metre)>);
}

TEST(UnitsTextTest, errors_name_the_cause) {
    EXPECT_EQ(parse<metres>("12.5").error(), error::missing_unit);
    EXPECT_EQ(parse<metres>("12.5 zz").error(), error::unknown_unit);
    EXPECT_EQ(parse<metres>("12.5 km/").error(), error::unknown_unit);
    EXPECT_EQ(parse<metres>("12.5 m^99").error(), error::unknown_unit);
    EXPECT_EQ(parse<metres>("12.5 m m m m m m m").error(), error::unknown_unit); // more than 6 symbols
    EXPECT_EQ(parse<metres>("12.5 s").error(), error::incompatible_unit);
    EXPECT_EQ(parse<metres>("1 min").error(), error::incompatible_unit); // the minute, no milli-inch
    EXPECT_EQ(parse<becqs>("5 Hz").error(), error::incompatible_unit);   // a frequency is no activity
    EXPECT_EQ(parse<levels>("15 %").error(), error::incompatible_unit);  // a level is no plain ratio
    EXPECT_EQ(parse<radians>("90°").error(), error::incompatible_unit);  // a factor of π
    EXPECT_EQ(parse<celsius>("5 m").error(), error::incompatible_unit);
    EXPECT_EQ(parse<metres>("12.5x m").error(), error::invalid_format);
    EXPECT_EQ(parse<metres>(" m").error(), error::invalid_format);
    EXPECT_EQ(parse<metres>("200 km").error(), error::overflow);
}

TEST(UnitsTextTest, the_number_type_stores) {
    EXPECT_EQ(parse<metres>("1.0005 m").error(), error::rounding_error); // no rounding mode
    EXPECT_EQ(to_string(*parse<rounding>("1.005 m")), "1.01 m");          // rounded once, by the type
    EXPECT_EQ(parse_exact<rounding>("1.005 m").error(), error::rounding_error);
    EXPECT_EQ(to_string(*parse_exact<rounding>("100 cm")), "1.00 m");
    EXPECT_EQ(to_string(*parse<metres, clamp>("200 km")), "100000.000 m");
}

TEST(UnitsTextTest, points_shift_exactly) {
    EXPECT_EQ(to_string(*parse<celsius>("21.5 ℃")), "21.5 ℃");
    EXPECT_EQ(to_string(*parse<celsius>("21.5 `C")), "21.5 ℃");
    EXPECT_EQ(to_string(*parse<celsius>("350 ℉")), "530/3 ℃");
    EXPECT_EQ(to_string(*parse<celsius>("300 K")), "26.85 ℃");
    EXPECT_EQ(to_string(*parse<celsius>("300000 mK")), "26.85 ℃");
}

TEST(UnitsTextTest, a_stream_reads_into_an_expected) {
    std::istringstream           in{"12.5 km 90° 7 zz 3"};
    std::expected<metres, error> m;
    in >> m;
    EXPECT_TRUE(m && to_string(*m) == "12500.000 m");
    std::expected<degrees, error> d;
    in >> d;
    EXPECT_TRUE(d && to_string(*d) == "90°");
    in >> m;
    EXPECT_EQ(m.error(), error::unknown_unit);
    EXPECT_TRUE(in.good()); // a parse error stays in the expected
    in >> m;
    EXPECT_EQ(m.error(), error::missing_unit);
    EXPECT_TRUE(in.fail()); // the input ran out

    // A compound unit prints as one token (its factors joined by ·), so >> reads what << writes.
    constexpr auto                 kg_m2_s2 = si::kilogram * square(si::metre) / square(si::second);
    const energies                 e{5, si::joule};
    std::stringstream              io;
    io << quantity<kg_m2_s2, inside<{0, 10}>>{5, kg_m2_s2} << " " << e;
    std::expected<energies, error> a, b;
    io >> a >> b;
    EXPECT_EQ(io.str(), "5 kg·m²/s² 5.000 J");
    EXPECT_TRUE(a && *a == e);
    EXPECT_TRUE(b && *b == e);
}

TEST(UnitsTextTest, constant_expressions) {
    static_assert(*parse<metres>("1.5 km") == just<1500> * si::metre);
    static_assert(parse<metres>("1.5 s").error() == error::incompatible_unit);
}

TEST(UnitsTextTest, each_call_sees_the_units_declared_before_it) {
    // units_text_other.cpp parses the same type without seeing the cent: its own specialization.
    EXPECT_EQ(to_string(*parse<units_text_other::dollars>("150 ¢")), "1.50 USD");
    EXPECT_EQ(units_text_other::parse_dollars("150 ¢").error(), error::unknown_unit);
    EXPECT_EQ(to_string(*units_text_other::parse_dollars("1.5 USD")), "1.50 USD");
}
} // namespace units_text_tests
