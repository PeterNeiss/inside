// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Storage: the raw a grid gets, the default value, the predefined formats,
// numeric_limits, and that every member of the class templates compiles.

#include <beman/inside/inside.hpp>
#include <beman/inside/inside/casts.hpp>
#include <beman/inside/inside/detail/rational.hpp>
#include <beman/inside/inside/formats.hpp>
#include <beman/inside/inside/formats.hpp> // beman::inside::byte
#include <beman/inside/inside/grid.hpp>
#include <beman/inside/inside/io.hpp>
#include <beman/inside/inside/predicates.hpp>
#include <beman/inside/math.hpp>
#include <beman/inside/units.hpp>

#include <gtest/gtest.h>

#include <cfloat>
#include <cmath>
#include <compare>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

// Storage deduced from the grid, and how the behaviour policies act on it.
namespace storage_tests {
using namespace beman::inside;

// The default ctor stores a value of the grid (0, else Lower; see
// default_value.test.cpp) on every storage.
TEST(StorageTest, default_ctor_stores_a_grid_value) {
    static_assert(!std::is_trivially_default_constructible_v<inside<{0, 10}>>);    // index
    static_assert(!std::is_trivially_default_constructible_v<inside<{-100, -5}>>); // signed value
    static_assert(!std::is_trivially_default_constructible_v<inside<{{0, 10}, per<4>}, round_nearest>>);
    static_assert(!std::is_trivially_default_constructible_v<inside<{0, 10}, unsafe>>);
    static_assert(inside<{-100, -5}>{} == -100_ins && inside<grid{5}>{} == 5_ins);
}

// behaviour policies on a notched grid
TEST(StorageTest, behaviour_policies_on_a_notched_grid) {
    // clamp: out-of-range snaps to the endpoint.
    using EC = inside<{{0, 10}, per<4>}, clamp | round_nearest>;
    ASSERT_EQ(rational{EC{rational{15}}}, 10);
    ASSERT_EQ(rational{EC{rational{-3}}}, 0);

    // wrap: modular reduction onto the grid
    // (range = Upper − Lower + Notch = 10.25, so 11.25 wraps to 1).
    using EW = inside<{{0, 10}, per<4>}, wrap | round_nearest>;
    ASSERT_EQ((rational{EW{rational{45, 4}}}), 1);

    // try_make: out-of-range yields errc::overflow.
    using DS  = inside<{5, 100}, checked>;
    auto ok   = DS::try_make(42);
    auto fail = DS::try_make(200);
    ASSERT_TRUE(ok.has_value());
    ASSERT_EQ(*ok, 42);
    ASSERT_TRUE(!fail.has_value());
    ASSERT_EQ(fail.error(), errc::overflow);
}

// a dyadic grid runs the full out-of-range policy cascade
TEST(StorageTest, dyadic_grid_runs_the_full_out_of_range_policy_cascade) {
    // clamp: saturate to the (grid-point) endpoint.
    using RC = inside<{{0, 4}, per<256>}, clamp>;
    ASSERT_TRUE(static_cast<double>(rational{RC{9.5}}) == 4.0);
    ASSERT_TRUE(static_cast<double>(rational{RC{-1.5}}) == 0.0);

    // wrap: fold into [Lower, Lower + span + notch) — same convention as the
    // fractional path. Span 0..359 with notch 1 wraps 370 → 10, -10 → 350.
    using RW = inside<{{0, 359}, 1}, wrap>;
    ASSERT_TRUE(static_cast<double>(rational{RW{370.0}}) == 10.0);
    ASSERT_TRUE(static_cast<double>(rational{RW{-10.0}}) == 350.0);

    // checked: out-of-range reports (throws) instead of silently storing.
    using RK = inside<{{0, 4}, per<256>}, checked>;
    ASSERT_THROW((void)(RK{9.5}), beman::inside::inside_error);
    ASSERT_TRUE(static_cast<double>(rational{RK{2.5}}) == 2.5);

    // try_make: out-of-range yields errc::overflow.
    ASSERT_TRUE(RK::try_make(2.0).has_value());
    ASSERT_TRUE(!RK::try_make(9.5).has_value());

    // no flags: checked like every policy without `unsafe`; in range, stores the value.
    using RU = inside<{{0, 4}, per<256>}>;
    ASSERT_TRUE(static_cast<double>(rational{RU{2.5}}) == 2.5);
}

// non-finite doubles are rejected, both engines
TEST(StorageTest, non_finite_doubles_are_rejected_both_engines) {
    // Default engine: store_real guards before rounding onto the grid; fixed engine:
    // the integer-backed path throws in rational(double). Same observable.
    using R          = inside<{{0, 4}, per<256>}, round_nearest>;
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    ASSERT_THROW((void)(R{nan}), beman::inside::inside_error);
    ASSERT_THROW((void)(R{inf}), beman::inside::inside_error);
    ASSERT_THROW((void)(R{-inf}), beman::inside::inside_error);

    // Non-finite input is reported as errc::not_finite (distinct from the
    // overflow used for finite-but-out-of-interval values), both engines.
    try {
        R{nan};
        FAIL() << "expected throw";
    } catch (const beman::inside::inside_error& e) {
        ASSERT_EQ(e.Code, errc::not_finite);
    }
}

// Full-domain inverse trig (improvement #2): atan beyond |x| ≤ 1 via
// reciprocal reduction; atan2 beyond the unit square via max-magnitude
// normalization. Engine-neutral (both engines accept the same programs).
// atan / atan2 accept magnitudes beyond 1
TEST(StorageTest, atan_atan2_accept_magnitudes_beyond_1) {
    using wide_t = inside<{{-16, 16}, per<16384>}, round_nearest>;

    auto val = [](auto b) { return static_cast<double>(rational{b}); };

    // Determinism: pin the exact grid output (a multiple of 1/16384, exactly
    // representable in double). These values are bit-identical on both math engines
    // and every platform; the comments give the true atan they round from.
    ASSERT_EQ(val(math::atan(wide_t{2})), 0x1.1b7p+0);                // ~1.1071488
    ASSERT_EQ(val(math::atan(wide_t{-3})), -0x1.3fcp+0);              // ~-1.2490234
    ASSERT_EQ(val(math::atan(wide_t{16})), 0x1.8224p+0);              // ~1.5083618
    ASSERT_EQ((val(math::atan(wide_t{rational{1, 2}}))), 0x1.dacp-2); // ~0.4636230

    ASSERT_EQ((val(math::atan2(wide_t{3}, wide_t{1}))), 0x1.3fcp+0);   // ~1.2490234
    ASSERT_EQ((val(math::atan2(wide_t{1}, wide_t{-5}))), 0x1.78dcp+1); // ~2.9442139
    ASSERT_EQ((val(math::atan2(wide_t{-7}, wide_t{2}))), -0x1.4aep+0); // ~-1.2924805
}

// Per-operation policy overrides (`with_*`, `on_*`, `policy(ec)`) route
// through the assignment engine and store the endpoint, not a notch count.
TEST(StorageTest, per_operation_policies_work_on_dyadic_bounds) {
    using R = inside<{{1, 4}, per<256>}, round_nearest>; // Lower != 0

    R a{2.0};
    a.policy<clamp>() = 9.5;
    ASSERT_TRUE(static_cast<double>(rational{a}) == 4.0);
    a.policy<clamp>() = 0.25;
    ASSERT_TRUE(static_cast<double>(rational{a}) == 1.0);

    R   b{2.0};
    int fired                                 = 0;
    b.on_clamp([&](auto&, auto) { ++fired; }) = 0.0;
    ASSERT_TRUE(static_cast<double>(rational{b}) == 1.0);
    ASSERT_EQ(fired, 1);

    // In-range per-operation store snaps onto the grid.
    R c{1.0};
    c.policy<round_toward_zero>() = 2.5;
    ASSERT_TRUE(static_cast<double>(rational{c}) == 2.5);

    // errc mode: out-of-range reports, value unchanged.
    beman::inside::errc ec{};
    R                   d{2.0};
    d.policy<checked>(ec) = 9.5;
    ASSERT_TRUE(ec != errc{});
    ASSERT_TRUE(static_cast<double>(rational{d}) == 2.0);

    // inside rhs through the per-operation clamp.
    using S = inside<{0, 100}>;
    R e{2.0};
    e.policy<clamp>() = S{50};
    ASSERT_TRUE(static_cast<double>(rational{e}) == 4.0);

    // wrap override.
    using W = inside<{{0, 359}, 1}, round_nearest>;
    W w{0.0};
    w.policy<wrap>() = 370.0;
    ASSERT_TRUE(static_cast<double>(rational{w}) == 10.0);
}

// Checked arithmetic on notched grids returns a plain inside: the result grid
// holds every result. Continuous (Notch == 0) grids store arbitrary
// rationals, so nothing is provable and they keep the wrapper.
TEST(StorageTest, notched_arithmetic_returns_a_plain_inside) {
    using E = inside<{{0, 10}, per<4>}, checked | round_nearest>;
    E a{rational{3, 4}}, b{rational{5, 4}};

    auto s = a + b;
    static_assert(!detail::is_expected_v<decltype(s)>);
    ASSERT_EQ(rational{s}, 2);

    auto d = a - b;
    static_assert(!detail::is_expected_v<decltype(d)>);
    ASSERT_EQ(rational{d}, (rational{-1, 2}));

    auto p = a * b;
    static_assert(!detail::is_expected_v<decltype(p)>);
    ASSERT_EQ(rational{p}, (rational{15, 16}));

    // Continuous grids bound their denominators: add and mul are plain too,
    // in a fraction sized from the bounds.
    using C = inside<{{0, 10}, max_denominator{std::int64_t{1} << 20}}, checked>;
    static_assert(!detail::is_expected_v<decltype(C{} + C{})>);
    static_assert(!detail::is_expected_v<decltype(C{} * C{})>);
    const C x = C::from_raw(rational{1, std::int64_t{1} << 20});
    ASSERT_EQ(rational{x * x}, (rational{1, std::int64_t{1} << 40}));
}

// Smaller-threads batch: wide trig envelope, pown, clamp saturation.
// sin/cos/tan accept radians up to 2^20
TEST(StorageTest, sin_cos_tan_accept_radians_up_to_2_20) {
    using wide_t     = inside<{{-(std::int64_t{1} << 20), std::int64_t{1} << 20}, per<1024>}, round_nearest>;
    auto         val = [](auto b) { return static_cast<double>(rational{b}); };
    const double tol = 2.0 / 1024; // grid tolerance (notch ≈ 9.8e-4)

    const double xs[] = {100000.0, -551496.5, 1048576.0, -1048576.0, 3.0};
    for (double x : xs) {
        ASSERT_TRUE(std::fabs(val(math::sin(wide_t{x})) - std::sin(x)) < tol);
        ASSERT_TRUE(std::fabs(val(math::cos(wide_t{x})) - std::cos(x)) < tol);
    }
}

// pown<E> - exact compile-time integer powers on any inside
TEST(StorageTest, pown_e_exact_compile_time_integer_powers_on_any_inside) {
    using s8 = inside<{-10, 10}>;
    ASSERT_TRUE(math::pown<3>(s8{-2}) == -8);
    ASSERT_TRUE(math::pown<0>(s8{7}) == 1);
    ASSERT_TRUE(math::pown<1>(s8{5}) == 5);
    ASSERT_TRUE(math::pown<5>(s8{3}) == 243);

    // Result grid widens corner-correctly: (-10..10)^3 covers ±1000.
    using cube_t = decltype(math::pown<3>(s8{}));
    static_assert(lower_of<cube_t> <= -1000);
    static_assert(upper_of<cube_t> >= 1000);

    // Exact on fractional grids.
    using q = inside<{{0, 2}, per<4>}, round_nearest>;
    ASSERT_EQ((rational{math::pown<2>(q{rational{3, 4}})}), (rational{9, 16}));
}

// tan saturates instead of erroring when Out carries clamp
TEST(StorageTest, tan_saturates_instead_of_erroring_when_out_carries_clamp) {
    // Explicit-Out spelling is the impl form (`tan<T>(x)` would bind T as the
    // INPUT of the auto form).
    using in_t  = inside<{{rational{-3, 2}, rational{3, 2}}, per<16384>}, round_nearest>; // one branch
    using sat_t = inside<{{-1, 1}, per<16384>}, round_nearest | clamp>;
    using err_t = inside<{{-1, 1}, per<16384>}, round_nearest>;

    // tan(1.2) ≈ 2.57 — beyond [-1, 1].
    const auto sat = math::tan_into<sat_t>(in_t{1.2});
    ASSERT_TRUE(static_cast<double>(rational{sat}) == 1.0); // clamped to Upper

    // Without clamp the store into Out reports it, like any store.
    try {
        (void)math::tan_into<err_t>(in_t{1.2});
        FAIL() << "no error reported";
    } catch (const inside_error& e) {
        ASSERT_EQ(e.Code, errc::overflow);
    }
}
} // namespace storage_tests

// The default value: a default-constructed or value-initialized inside is 0
// when its grid holds 0, else Lower — a value of the grid on every storage.
// A failed error-code construction keeps the default value.
namespace default_value_tests {
using namespace beman::inside;

namespace {
// The value of T{} and of a default-initialized T, both in a constant
// expression (reading an uninitialized raw there would not compile).
template <class T>
constexpr rational value_init() {
    const T t{};
    return rational{t};
}
template <class T>
constexpr rational default_init() {
    T t;
    return rational{t};
}
template <class T>
constexpr bool defaults_to(const rational& v) {
    return value_init<T>() == v && default_init<T>() == v;
}
} // namespace

TEST(DefaultValueTest, zero_when_the_grid_holds_it_else_lower) {
    static_assert(defaults_to<inside<{0, 100}>>(0));                                 // index from 0
    static_assert(defaults_to<inside<{-100, 100}>>(0));                              // signed value
    static_assert(defaults_to<inside<{5, 100}>>(5));                                 // value raw, no 0
    static_assert(defaults_to<inside<{200, 300}>>(200));                             // index, no 0
    static_assert(defaults_to<inside<{-10, -5}>>(-10));                              // below 0
    static_assert(defaults_to<inside<{{-1, 1}, per<16384>}, round_nearest>>(0));     // Q1.14: not Lower
    static_assert(defaults_to<inside<{{0.5, 10.5}, 1}>>(rational{1u, 2}));           // unanchored
    static_assert(defaults_to<inside<{{-1, 1}, max_denominator{8}}>>(0));            // continuous
    static_assert(defaults_to<inside<{{1, 2}, max_denominator{100}}>>(1));           // continuous, no 0
    static_assert(defaults_to<inside<grid{7}>>(7));                                  // point
    static_assert(defaults_to<inside<{0, 100}, clamp | round_nearest | unsafe>>(0)); // any policy
    static_assert(value_init<inside<{{1, 2}, max_denominator{100}}>>().Den != 0);
}

TEST(DefaultValueTest, wide_storage) {
    // a wide index (more than 2^64 slots)
    static_assert(inside<{0, 0x1p100}>{} == 0_ins);
    static_assert(inside<{0x1p100, 0x1p101}>{} == just<0x1p100>);
    static_assert(inside<{-0x1p101, -0x1p100}>{} == -just<0x1p101>);
    // a fraction raw (a continuous grid past the 64-bit rational)
    using F0 = inside<{{-0x1p70, 0x1p70}, max_denominator{3}}>;
    using F1 = inside<{{1, 0x1p70}, max_denominator{3}}>;
    static_assert(detail::fraction_storage<F0> && detail::fraction_storage<F1>);
    static_assert(F0{} == 0_ins && F1{} == 1_ins);
}

TEST(DefaultValueTest, runtime_matches_compile_time) {
    inside<{5, 100}>                             a;
    inside<{-10, -5}>                            b{};
    inside<{{-1, 1}, per<16384>}, round_nearest> c;
    EXPECT_EQ(rational{a}, 5);
    EXPECT_EQ(rational{b}, -10);
    EXPECT_EQ(rational{c}, 0);
}

TEST(DefaultValueTest, failed_error_code_construction_keeps_the_default) {
    constexpr auto store = [] {
        errc             ec{};
        inside<{5, 100}> x(200, make_policy(ec));
        return std::pair{ec, rational{x}};
    };
    static_assert(store().first == errc::overflow && store().second == 5);
    errc             ec{};
    inside<{5, 100}> x(200, make_policy(ec));
    EXPECT_EQ(ec, errc::overflow);
    EXPECT_EQ(rational{x}, 5);
}

TEST(DefaultValueTest, no_inside_is_trivially_default_constructible) {
    static_assert(!std::is_trivially_default_constructible_v<inside<{5, 100}>>);
    static_assert(!std::is_trivially_default_constructible_v<inside<{5, 100}, unsafe>>);
    static_assert(!std::is_trivially_default_constructible_v<inside<{{1, 2}, max_denominator{100}}>>);
    // a cursor starts at Lower whatever T's flags
    static_assert(rational{cursor<inside<{5, 100}, unsafe>>{}} == 5);
}

// rational is 0 when default-constructed, and == compares values: a
// hand-built unreduced fraction equals its reduced form.
TEST(DefaultValueTest, rational_is_zero_and_compares_values) {
    static_assert(rational{} == 0 && rational{}.Den == 1);
    constexpr auto zero = [] {
        rational r;
        return r;
    }();
    static_assert(zero == 0);
    constexpr auto two_halves = [] {
        rational r;
        r.Num = 2;
        r.Den = 2;
        return r;
    }();
    static_assert(two_halves == rational{1} && !(two_halves != rational{1}) && two_halves <=> rational{1} == 0);
    static_assert(rational{1, 3} != rational{1, 2} && rational{-1, 2} == rational{1, -2});
}

TEST(DefaultValueTest, quantity) {
    using namespace beman::inside::units;
    static_assert(quantity<si::metre, inside<{5, 100}>>{}.numerical_value() == 5_ins);
    static_assert(quantity<si::metre, inside<{-5, 100}>>{}.numerical_value() == 0_ins);
}
} // namespace default_value_tests

// formats
namespace formats_tests {
using namespace beman::inside;
using namespace beman::inside::detail;

//---------------------------------------------------------------------------
// The whole point: each predefined type lands on its native byte width.
//---------------------------------------------------------------------------
// counter saturates; wrap_counter wraps
TEST(FormatsTest, counter_saturates_ring_counter_wraps) {
    static_assert(std::is_same_v<clamp_counter<5>, inside<{0, 5}, clamp>>);
    static_assert(std::is_same_v<wrap_counter<5>, inside<{0, 5}, wrap>>);

    clamp_counter<5> c{4};
    ++c;
    ++c;
    ++c; // 4 -> 5 (saturates, never throws)
    ASSERT_EQ(c, 5);
    --c;
    --c;
    --c;
    --c;
    --c;
    --c; // floors at 0
    ASSERT_EQ(c, 0);

    wrap_counter<5> r{5};
    ++r; // 5 -> 0 (wraps)
    ASSERT_EQ(r, 0);
}

// formats map to native byte widths
TEST(FormatsTest, formats_map_to_native_byte_widths) {
    // Native integers
    static_assert(sizeof(uint8) == 1);
    static_assert(sizeof(uint16) == 2);
    static_assert(sizeof(uint32) == 4);
    static_assert(sizeof(int8) == 1);
    static_assert(sizeof(int16) == 2);
    static_assert(sizeof(int32) == 4);
    static_assert(sizeof(int64) == 8);

    // Unsigned normalized
    static_assert(sizeof(unorm8) == 1);
    static_assert(sizeof(unorm16) == 2);
    static_assert(sizeof(unorm32) == 4);

    // Q-format fixed-point
    static_assert(sizeof(q4_4) == 1);
    static_assert(sizeof(q8_8) == 2);
    static_assert(sizeof(q16_16) == 4);
}

//---------------------------------------------------------------------------
// Fallible results stay out of storage: std::expected appears only as a
// return value, so the stored types keep their native width.
//---------------------------------------------------------------------------
// formats: fallible results are expected, storage stays native
TEST(FormatsTest, formats_fallible_results_are_expected_storage_stays_native) {
    using q = decltype(uint8{200} / uint8{0});
    static_assert(is_expected_v<q>);
    static_assert(sizeof(q) > sizeof(q::value_type)); // why it never becomes storage

    auto r = uint8{200} / uint8{0};
    ASSERT_FALSE(r.has_value());
    ASSERT_EQ(r.error(), errc::division_by_zero);
}

//---------------------------------------------------------------------------
// Round-trips and endpoint reachability.
//---------------------------------------------------------------------------
// formats round-trip representative values
TEST(FormatsTest, formats_round_trip_representative_values) {
    // Native integers — the full native range is usable.
    ASSERT_EQ(uint8{255}, 255);
    ASSERT_EQ(uint8{0}, 0);
    ASSERT_EQ(int16{-32768}, -32768);
    ASSERT_EQ(int16{32767}, 32767);

    // UNORM — both endpoints exact, plus a representable interior point.
    ASSERT_EQ(unorm8{0.0_r}, 0);
    ASSERT_EQ(unorm8{1.0_r}, 1);
    ASSERT_EQ(unorm8{0.2_r}, 0.2_r); // 51/255 == 1/5
    ASSERT_EQ(unorm16{1.0_r}, 1);

    // Q-format — fractional values on the grid.
    ASSERT_EQ(q8_8{42.5}, 42.5_r);
    ASSERT_EQ(q4_4{3.25}, 3.25_r);
    ASSERT_EQ(q16_16{1000.125}, 1000.125_r);
    // Every bit pattern: the top value is 2^n − 1 notches.
    ASSERT_EQ(q4_4{15.9375}, 15.9375_r);
    ASSERT_EQ(q8_8{255.99609375}, rational(65535u, 256u));
    ASSERT_EQ(q16_16{65535.5}, 65535.5_r);
    ASSERT_FALSE(q4_4::try_make(16).has_value());
    static_assert(grid_of<q4_4>.step_count() == 255 && grid_of<q8_8>.step_count() == 65535);
}

//---------------------------------------------------------------------------
// Extremes of the wider types round-trip through the int64 value path.
//---------------------------------------------------------------------------
// formats: wide-type extremes round-trip
TEST(FormatsTest, formats_wide_type_extremes_round_trip) {
    ASSERT_EQ(uint16{65535}, 65535);
    ASSERT_EQ(uint32{4294967295}, 4294967295);
    ASSERT_EQ(int8{-128}, -128);
    ASSERT_EQ(int8{127}, 127);
    ASSERT_EQ(int32{-2147483648LL}, -2147483648LL);
    ASSERT_EQ(int32{2147483647}, 2147483647);
    ASSERT_EQ(int64{-9223372036854775807LL}, -9223372036854775807LL);
    ASSERT_EQ(int64{9223372036854775807LL}, 9223372036854775807LL);
    ASSERT_EQ(unorm32{1.0_r}, 1);
    ASSERT_EQ(unorm32{0.0_r}, 0);
}

//---------------------------------------------------------------------------
// Values past the native range are out of range under the default `checked`
// policy.
//---------------------------------------------------------------------------
// formats reject values past the native range
TEST(FormatsTest, formats_reject_values_past_the_native_range) {
    ASSERT_ANY_THROW((void)([] {
        int   v = 256;
        uint8 x{v};
        (void)x;
    }()));
    ASSERT_ANY_THROW((void)([] {
        int   v = -32769;
        int16 x{v};
        (void)x;
    }()));
    ASSERT_NO_THROW((void)([] {
        int   v = 255;
        uint8 x{v};
        (void)x;
    }()));
    ASSERT_NO_THROW((void)([] {
        int   v = -32768;
        int16 x{v};
        (void)x;
    }()));
}
} // namespace formats_tests

// Every member of the library's class templates either compiles or is
// constrained away. An explicit instantiation compiles each member whose
// constraints hold, as some compilers do when a program reflects on the class
// (units_parity.test.cpp): a member whose body is ill-formed for some
// arguments (a static_assert body, a missing operation) fails here.
using namespace beman::inside;
using namespace beman::inside::units;

namespace {
// A cursor's own policy: flags_of drops the cursor marker.
template <class>
struct policy_of;
template <grid G, policy_flag P>
struct policy_of<inside<G, P>> {
    static constexpr policy_flag value = P;
};
using cur = cursor<inside<{0, 60}, round_floor>, 5>;
using pt  = decltype(point<si::degree_Celsius>(inside<{-50, 50}>{0}));
template <class T>
concept steps = requires(T x) { ++x; };
} // namespace

// Insides: an integer grid, a Q format with a rounding and clamp, a bounded
// continuous grid, a grid past 64 bits, a point, a cursor.
template class beman::inside::inside<grid{0, 100}>;
template class beman::inside::inside<grid{{0, 1}, per<4>}, round_nearest | clamp>;
template class beman::inside::inside<grid{{-1, 1}, max_denominator{8}}, wrap>;
template class beman::inside::inside<grid{{0, 0x1p70}, per<4>}>;
template class beman::inside::inside<grid{7}>;
template class beman::inside::inside<grid_of<cur>, policy_of<cur>::value>;

// Quantities and a point.
template class beman::inside::units::quantity<si::metre, inside<{0, 100}>>;
template class beman::inside::units::quantity<percent, inside<{{0, 100}, per<4>}, round_nearest>>;
template class beman::inside::units::quantity<si::second, cur>;
template class beman::inside::units::quantity_point<pt::reference, pt::origin, pt::rep>;

TEST(ExplicitInstantiationTest, every_member_compiles_or_is_constrained) {
    // The instantiations above are the test; a continuous grid has no ++.
    static_assert(!steps<inside<{{-1, 1}, max_denominator{8}}>>);
    static_assert(steps<inside<{0, 100}>>);
    SUCCEED();
}

// Pinned defects: each case failed before its fix.
namespace pinned_tests {
using namespace beman::inside;
using namespace beman::inside::detail;

template <class T>
concept has_epsilon = requires { std::numeric_limits<T>::epsilon(); };
template <class T>
concept has_round_error = requires { std::numeric_limits<T>::round_error(); };

// A Q-format grid far from zero: Lower·2^frac passes int64, so the integer
// codec does not apply (it overflowed in decode; the type did not compile).
TEST(StorageTest, q_format_grid_far_from_zero) {
    static constexpr std::int64_t base = std::int64_t{1} << 48;
    using Q                            = inside<{{base, base + 1}, per<65536>}>;
    static_assert([] {
        Q x{base};
        Q y{rational{base * 2 + 1, 2}};
        return rational(x) == rational{base} && rational(y) == rational{base * 2 + 1, 2} &&
               y.to<std::int64_t, round_toward_zero>() == base && y.to<std::int64_t>().error() == errc::rounding_error;
    }());
}

// An index grid reaching 2^63: Raw·Notch passes 64 bits on the way to a
// value that fits (the decode dereferenced a failed expected), the store's
// (v − Lower)/Notch did too, and the range check read 2^63 as INT64_MIN.
TEST(StorageTest, index_grid_reaching_2_pow_63) {
    constexpr std::int64_t i64_min = INT64_MIN;
    static_assert([] {
        constexpr rational lo{i64_min}, hi{std::uint64_t{1} << 63};
        using G = inside<{{lo, hi}, 2}>;
        G x{hi};
        G y{lo};
        return rational(x) == hi && rational(y) == lo && x == hi && x.to<double>() == 0x1p63;
    }());
}

// A slot count past 2^63 over a negative base: the value index overflowed
// int64 on the way (UB; constant evaluation rejects it), though the index fits.
TEST(StorageTest, value_index_past_an_int64_raw) {
    using small  = inside<{-7, 5}>;
    using wide_q = inside<{{-(1ll << 40), 1ll << 40}, per<(1ll << 20)>}, clamp>;
    static_assert([] {
        wide_q c{rational{1ll << 39}};
        c *= small{5}; // 5·2^39: the product's raw passes 2^63
        return c == wide_q{rational{1ll << 40}};
    }());
}

// A deduced output limit k·Notch with the index k past 2^63 (π, √2 on a
// 2^-62 notch) wrapped through int64: atan2's grid came out as [0.86, −0.86]
// and did not compile.
TEST(StorageTest, deduced_limit_past_int64_index) {
    using fine = inside<{{0, 1}, per<(1ll << 62)>}, round_nearest>;
    using A    = decltype(math::atan2(fine{}, fine{}));
    static_assert(lower_of<A> < -3 && upper_of<A> > 3);
    using H = decltype(math::hypot(fine{}, fine{}));
    static_assert(upper_of<H> > rational(141, 100) && upper_of<H> < rational(142, 100));
    EXPECT_EQ(math::atan2(fine{rational{1, 2}}, fine{rational{1, 2}}), math::atan2(fine{1}, fine{1}));
    EXPECT_GT(math::hypot(fine{1}, fine{1}), rational(141, 100));
}

// epsilon of a lattice that spans 0 but misses it: none, 0 is no value.
TEST(StorageTest, numeric_limits_epsilon_off_zero) {
    static_assert(!has_epsilon<inside<{{-0.5, 0.5}, 1}>>);
    static_assert(!has_round_error<inside<{{rational{-7, 3}, rational{5, 3}}, rational{2, 3}}>>);
}

// raw·notch passed int64 in to_value although the value fits.
TEST(StorageTest, to_value_on_a_coarse_int64_grid) {
    using I64             = std::int64_t;
    constexpr I64 I64_MAX = std::numeric_limits<I64>::max();
    constexpr I64 I64_MIN = std::numeric_limits<I64>::min();
    using A               = inside<grid{{I64_MIN, I64_MAX - 1}, 2}>;
    static_assert([] {
        constexpr A x = I64_MAX - 1;
        return x.as<I64>() == I64_MAX - 1 && x.to<I64>() == I64_MAX - 1;
    }());
    A y = I64_MIN;
    y   = I64_MAX - 1;
    EXPECT_EQ(y.as<I64>(), I64_MAX - 1);
}

//---------------------------------------------------------------------------
// A point grid stores nothing: its value is the type.
//---------------------------------------------------------------------------
TEST(StorageTest, point_grid_stores_nothing) {
    using P5 = inside<grid{5}>;
    static_assert(point_storage<P5> && sizeof(P5) == 1);
    static_assert(P5{5} == 5);
    using M7 = inside<grid{-7}>;
    static_assert(M7{-7} == -7);
    static_assert(P5{5} + inside<{0, 10}>{3} == 8);

    inside<{0, 10}> t{3};
    t += P5{5};
    EXPECT_EQ(t, 8);
}

TEST(StorageTest, grid_past_int64_constructs_compares_and_converts) {
    constexpr std::uint64_t u64_max = std::numeric_limits<std::uint64_t>::max();
    using U64                       = inside<{{0, rational{u64_max}}, 1}>;
    using Small                     = inside<{0, 10}>;
    static_assert(U64{5} == 5 && U64{5} < 6 && U64{5} > 4); // signed source
    static_assert(U64{u64_max} > 5 && U64{u64_max} == u64_max && U64{u64_max} > Small{3});
    static_assert(U64{1ull << 63} < U64{u64_max});
    static_assert(U64{Small{7}} == 7);
    static_assert(U64{u64_max}.to<unsigned long long>() == u64_max);
    static_assert(U64{3}.to<int>().value() == 3);
    static_assert(U64{u64_max}.to<long long>().error() == errc::overflow);
    static_assert(Small{U64{u64_max}, clamp} == 10);

    constexpr auto narrowed = [] {
        errc  ec{};
        Small s{1};
        s.policy(ec) = U64{u64_max};
        return std::pair{s == 1, ec};
    }();
    static_assert(narrowed.first && narrowed.second == errc::overflow);

    U64 big{u64_max};
    EXPECT_GT(big, Small{10});
    EXPECT_EQ(big, u64_max);
}

TEST(StorageTest, grid_past_int64_increments_and_negates) {
    constexpr std::uint64_t u64_max = std::numeric_limits<std::uint64_t>::max();
    using U64                       = inside<{{0, rational{u64_max}}, 1}>;
    using U64W                      = inside<{{0, rational{u64_max}}, 1}, wrap>;
    using Small                     = inside<{0, 10}>;
    constexpr auto inc              = [] {
        U64 w{u64_max - 1};
        ++w;
        return w == u64_max;
    }();
    static_assert(inc);
    constexpr auto wrap_up = [] {
        U64W w{u64_max};
        ++w;
        return w == 0;
    }();
    constexpr auto wrap_down = [] {
        U64W w{0};
        --w;
        return w == u64_max;
    }();
    constexpr auto wrap_sub = [] {
        U64W w{u64_max};
        w -= Small{10};
        return w == u64_max - 10;
    }();
    constexpr auto wrap_neg = [] {
        U64W w{5};
        w = inside<{-20, 20}>{-1};
        return w == u64_max;
    }();
    static_assert(wrap_up && wrap_down && wrap_sub && wrap_neg);

    using N = U64::negated_type;             // {−(2^64−1), 0}
    static_assert(sizeof(N::raw_type) == 8); // was int8: trunc(Lower) wrapped
    static_assert(-U64{5} == -5 && -U64{u64_max} == -rational{u64_max});
    static_assert(-(-U64{u64_max}) == U64{u64_max});

    static_assert(U64{u64_max} - U64{u64_max - 3} == 3); // result grid spans 2^65
    static_assert(U64{u64_max} * just<1> == u64_max);

    U64 x{u64_max};
    EXPECT_THROW(++x, inside_error);
    EXPECT_EQ(x, u64_max);
}

// inside<{x,x}> singleton round-trips its value
TEST(StorageTest, a_point_round_trips_its_value) {
    using point_pos = inside<{5, 5}>;
    point_pos a{5};
    ASSERT_EQ(a, 5);
    ASSERT_EQ(a.as<std::int64_t>(), 5);

    using point_neg = inside<{-3, -3}>;
    point_neg b{-3};
    ASSERT_EQ(b, -3);
    ASSERT_EQ(b.as<std::int64_t>(), -3);

    // Float assignment to single-value rational-storage grid (notch=0)
    using point_fp = inside<{2.5_r}>;
    point_fp c     = 2.5;
    ASSERT_EQ(c, 2.5_r);
}

// default-constructed inside is well-formed
TEST(StorageTest, default_constructed_inside_is_well_formed) {
    inside<> b;
    (void)b;
    SUCCEED() << "default ctor compiles";
}

// numeric_limits epsilon / round_error report 0 for exact types
TEST(StorageTest, numeric_limits_epsilon_and_round_error_of_exact_types) {
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

} // namespace pinned_tests

// Arms no other test reaches, found by the reachability audit (gcov and an
// `if constexpr` probe over the suite).
namespace reached_arms_tests {
using namespace beman::inside;

// A point's index is 0; its numerator and denominator are its own.
TEST(StorageArmTest, index_and_parts_of_a_point) {
    const inside<{5, 5}> p;
    EXPECT_TRUE(p.index() == 0 && p.numerator() == 5 && p.denominator() == 1);
}

// x++ returns the old value.
TEST(StorageArmTest, postfix_increment) {
    volatile int    three = 3;
    inside<{0, 10}> x{static_cast<int>(three)};
    const auto      y = x++;
    EXPECT_TRUE(x == 4 && y == 3);
}
} // namespace reached_arms_tests

namespace reached_arms_tests {
using namespace beman::inside;

// A continuous grid has no binary digits to count.
TEST(StorageArmTest, digits_of_a_continuous_grid) {
    static_assert(std::numeric_limits<inside<{{0, 1}, max_denominator{10}}>>::digits == 0);
}
} // namespace reached_arms_tests

namespace reached_arms_tests {
using namespace beman::inside;

// A notch-0 point's numerator is its value.
TEST(StorageArmTest, numerator_of_a_notch_zero_point) { EXPECT_TRUE((inside<grid{5}>{}.numerator() == 5)); }
} // namespace reached_arms_tests
