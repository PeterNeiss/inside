// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Defects from the ninth bug-hunt round. The runtime cases stored or rounded
// wrongly before the fixes; the concept checks were hard errors inside the
// operator bodies, not false.

#include <beman/inside/chrono.hpp>
#include <beman/inside/units.hpp>

#include <gtest/gtest.h>

#include <bit>
#include <cfloat>
#include <chrono>
#include <cstdint>

using namespace beman::inside;

namespace {
template <class T>
T launder(T v) {
    volatile T x = v;
    return x;
}
} // namespace

// A floating count with a period other than 1 s went through a 64-bit
// rational: 0.1 ms is 3602879701896397/2^55 ms, and the product by 1/1000
// passed 64 bits, an overflow for a value well inside the grid.
TEST(BugHuntRound9, from_duration_of_a_fractional_floating_count) {
    using namespace std::chrono;
    using us_t = inside<{{0, 1}, per<1000000>}>;
    EXPECT_EQ((rational{*from_duration<us_t, round_nearest>(duration<double, std::milli>{launder(0.1)})}),
              rational(1, 10000));
    // Off the microsecond grid without a rounding flag: no overflow.
    EXPECT_EQ(from_duration<us_t>(duration<double, std::milli>{launder(0.1)}).error(), errc::rounding_error);
    EXPECT_EQ((*from_duration<inside<{0, 100}>, round_nearest>(duration<double, std::ratio<60>>{launder(0.3)})), 18);
    static_assert(*from_duration<us_t, round_nearest>(duration<double, std::milli>{0.1}) == rational(1, 10000));
    // Huge counts reach the policy: clamp, and an overflow without it.
    EXPECT_EQ((*from_duration<inside<{0, 1}, clamp>>(duration<double, std::milli>{launder(1e300)})), 1);
    EXPECT_EQ(from_duration<us_t>(duration<double, std::milli>{launder(1e300)}).error(), errc::overflow);
}

// A long double count went through a double; it is taken exactly now.
TEST(BugHuntRound9, from_duration_of_a_long_double_count) {
    using namespace std::chrono;
    using fine_t        = inside<{{0, 1100}, per<(1ll << 50)>}>;
    const long double c = launder(0x1p20L + 0x1p-36L); // ms; the double holds 2^20 only
    const auto        r = from_duration<fine_t, round_floor>(duration<long double, std::milli>{c});
    const auto        d = from_duration<fine_t, round_floor>(duration<double, std::milli>{0x1p20});
    if constexpr (LDBL_MANT_DIG > DBL_MANT_DIG)
        EXPECT_GT(rational{*r}, rational{*d}); // 2^-36/1000 s is past the 2^-50 notch
    else
        EXPECT_EQ(rational{*r}, rational{*d});
    EXPECT_TRUE(r.has_value());
}

// to<float>() rounded through a double: 1 + 2^-24 + 2^-60 rounded to the
// double 1 + 2^-24, a tie the float broke to even (1), not up.
TEST(BugHuntRound9, to_float_rounds_once) {
    using T     = inside<{{0, 2}, per<(1ll << 60)>}>;
    const T x   = T::from_raw(launder((std::uint64_t{1} << 60) + (std::uint64_t{1} << 36) + 1));
    const T low = T::from_raw(launder((std::uint64_t{1} << 60) + (std::uint64_t{1} << 36) - 1));
    const T tie = T::from_raw(launder((std::uint64_t{1} << 60) + (std::uint64_t{1} << 36)));
    EXPECT_EQ(*x.to<float>(), 1.0f + 0x1p-23f);
    EXPECT_EQ(*low.to<float>(), 1.0f);
    EXPECT_EQ(*tie.to<float>(), 1.0f); // a true tie: to even
    static_assert(*T::from_raw((std::uint64_t{1} << 60) + (std::uint64_t{1} << 36) + 1).to<float>() ==
                  1.0f + 0x1p-23f);
}

// Quantity operators named only the units in their constraints, so a
// requires-expression on a number op inside's rules reject (an off-notch
// +=, a product past 64-bit grids, a factor past 64 bits) was a hard error.
namespace honest {
namespace units = beman::inside::units;
namespace si    = units::si;

template <class A, class B>
concept can_add_assign = requires(A a, const B& b) { a += b; };
template <class A, class B>
concept can_mul_assign = requires(A a, const B& b) { a *= b; };
template <class A, class B>
concept can_add = requires(const A& a, const B& b) { a + b; };
template <class A, class B>
concept can_mod = requires(const A& a, const B& b) { a % b; };
template <class A, class B>
concept can_mul = requires(const A& a, const B& b) { a * b; };
template <class A, class B>
concept can_div = requires(const A& a, const B& b) { a / b; };
template <class A, class B>
concept can_eq = requires(const A& a, const B& b) { a == b; };

template <class V>
using metres = units::quantity<si::metre, V>;
using wrap_m = metres<inside<{0, 1000, bounds::half_open}, wrap>>;
using mile_q = decltype(just<3> * units::international::mile); // 4828.032 m
using huge   = inside<{0, 1ll << 62}>;
using fine   = inside<{{0, 1ll << 62}, per<(1ll << 62)>}>;
using small  = inside<{1, 3}>;

// Off the metre notch: inside rejects it, so does the quantity.
static_assert(!can_add_assign<wrap_m, mile_q>);
static_assert(can_add_assign<wrap_m, decltype(just<3> * si::kilo(si::metre))>);
static_assert(can_mul_assign<metres<small>, inside<{-3, 3}>> == can_mul_assign<small, inside<{-3, 3}>>);
// The rest follows inside's own operators, both ways.
static_assert(can_add<metres<huge>, metres<fine>> == can_add<huge, fine>);
static_assert(can_mod<metres<huge>, metres<fine>> == can_mod<huge, fine>);
static_assert(can_mul<metres<huge>, metres<fine>> == can_mul<huge, fine>);
static_assert(can_div<metres<huge>, metres<fine>> == can_div<huge, fine>);
static_assert(can_eq<metres<huge>, metres<fine>> == can_eq<huge, fine>);
static_assert(can_mul<metres<huge>, fine> == can_mul<huge, fine>);
static_assert(can_div<fine, metres<huge>> == can_div<fine, huge>);
#if !BEMAN_INSIDE_BIG_GRIDS
// au → nm is 1.5·10^20: no 64-bit grid number, so no common number.
static_assert(!can_add<units::quantity<si::astronomical_unit, inside<{0, 100}>>,
                       units::quantity<si::nano(si::metre), inside<{0, 1000}>>>);
#endif
// Points follow their quantities.
using celsius = decltype(units::point<si::degree_Celsius>(just<20>));
static_assert(can_add<celsius, decltype(just<1> * si::kelvin)>);
} // namespace honest
