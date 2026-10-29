// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Defects from the ninth bug-hunt round. The runtime cases rounded wrongly
// before the fixes; the concept checks were hard errors inside the
// operator bodies, not false.

#include <beman/inside/units.hpp>
#include <beman/inside/units_math.hpp>

#include <gtest/gtest.h>

#include <cstdint>

using namespace beman::inside;

namespace {
template <class T>
T launder(T v) {
    volatile T x = v;
    return x;
}
} // namespace

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
// Points follow their quantities.
using celsius = decltype(units::point<si::degree_Celsius>(just<20>));
static_assert(can_add<celsius, decltype(just<1> * si::kelvin)>);
// pow, atan2 and hypot follow their number ops too.
template <class A>
concept can_pow2 = requires(const A& a) { units::pow<2>(a); };
template <class A, class B>
concept can_atan2 = requires(const A& a, const B& b) { units::atan2(a, b); };
template <class A, class B>
concept can_hypot = requires(const A& a, const B& b) { units::hypot(a, b); };
using rounding_m  = metres<inside<{{-10, 10}, per<8>}, round_nearest>>;
static_assert(can_pow2<rounding_m> && can_atan2<rounding_m, rounding_m> && can_hypot<rounding_m, rounding_m>);
static_assert(can_pow2<metres<huge>> == can_mul<huge, huge>);
} // namespace honest
