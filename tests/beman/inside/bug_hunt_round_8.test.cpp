// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Defects from the eighth bug-hunt round. The static_asserts did not compile
// before the fixes; the runtime cases compared or stored wrongly, reported an
// error a clamp should have absorbed, or terminated.

#include <beman/inside/inside.hpp>
#include <beman/inside/formats.hpp>

#include <gtest/gtest.h>

#include <cfloat>
#include <cstdint>
#include <limits>

using namespace beman::inside;

namespace {
using I64 = std::int64_t;

// A long double with bits past the double's (x87 and IEEE quad; not where
// long double is a double, as on Apple arm64). A preprocessor test:
// an if constexpr outside a template still evaluates its static_asserts.
#define WIDE_LONG_DOUBLE (LDBL_MANT_DIG > DBL_MANT_DIG)

template <class T>
T launder(T v) {
    volatile T x = v;
    return x;
}
} // namespace

// rational ⋈ double went through rational{d}, which raises at |d| ≥ 2^64:
// every rational lies strictly inside ±2^64, so the sign decides.
TEST(BugHuntRound8, rational_against_a_huge_double) {
    static_assert(rational{3} < 1e20 && rational{-3} > -1e20 && rational{3} != 1e20);
    EXPECT_TRUE(rational{3} < launder(1e20));
    EXPECT_TRUE(rational{-3} > launder(-0x1p64));
    EXPECT_FALSE(rational{3} == launder(1e300));
}

// rational ⋈ long double, rational{long double}: the long double was cast to
// a double first (1e-30L compared equal to 0, 1 + 2^-60 equal to 1).
TEST(BugHuntRound8, rational_and_long_double) {
#if WIDE_LONG_DOUBLE
    static_assert(rational{0} != 1e-30L && rational{0} < 1e-30L);
    static_assert(rational{1} < 1.0L + 0x1p-63L && !(rational{1} == 1.0L + 0x1p-63L));
    static_assert(rational{1.0L + 0x1p-60L} == (rational{1} + rational{1, I64{1} << 60}).value());
    static_assert(rational{3} < 1e4000L && rational{3} > -1e4000L);
    EXPECT_TRUE(rational{1} < launder(1.0L + 0x1p-63L));
    EXPECT_TRUE((rational{1, 3} < launder(1.0L / 3))); // 1/3 rounds up in 64 bits
    EXPECT_EQ(rational{launder(0.5L + 0x1p-60L)}, (rational{1, 2} + rational{1, I64{1} << 60}).value());
#endif
}

// inside ⋈ long double read the long double as a double.
TEST(BugHuntRound8, inside_against_long_double) {
#if WIDE_LONG_DOUBLE
    using Q = inside<{{-100, 100}, per<4>}>;
    using C = inside<{{-1, 2}, max_denominator{I64{1} << 62}}>;
    static_assert(Q{1} != 1.0L + 0x1p-60L && Q{1} < 1.0L + 0x1p-60L && Q{1} == 1.0L);
    EXPECT_TRUE(Q{1} < launder(1.0L + 0x1p-60L));
    EXPECT_FALSE(C{1} == launder(1.0L + 0x1p-63L));
    EXPECT_TRUE(C{1} > launder(1.0L - 0x1p-64L));
    EXPECT_TRUE(Q{100} < launder(1e4000L));
#endif
}

// includes(interval, long double) and grid::representable(long double)
// dropped the bits past the double's.
TEST(BugHuntRound8, interval_and_grid_take_long_double_exactly) {
#if WIDE_LONG_DOUBLE
    constexpr grid G{{-100, 100}, per<4>};
    static_assert(!G.representable(1.0L + 0x1p-60L) && G.representable(1.0L));
    static_assert(!includes(interval{-100, 1}, 1.0L + 0x1p-60L) && !includes(interval{-1, 0}, 0x1p-70L));
    constexpr grid C{{-1, 2}, max_denominator{I64{1} << 62}};
    static_assert(!C.representable(1.0L + 0x1p-63L)); // finer than the bound
    static_assert(C.representable(1.0L + 0x1p-60L));
    EXPECT_FALSE(G.representable(launder(1.0L + 0x1p-60L)));
    EXPECT_FALSE(includes(interval{-1, 0}, launder(0x1p-70L)));
#endif
}

// A fine double on a grid whose v − Lower passes one limb: the
// lattice test formed that difference as a big grid number, which exists
// only at compile time (std::terminate from the noexcept representable).
TEST(BugHuntRound8, representable_fine_double_at_runtime) {
    constexpr grid G{{-100, 100}, per<4>};
    EXPECT_FALSE(G.representable(launder(0x1.0000000000001p-10)));
    EXPECT_TRUE(G.representable(launder(0.25)));
}

// The predicates of a grid past 64 bits at runtime: comparisons and the
// lattice test formed big values (std::terminate).
TEST(BugHuntRound8, big_grid_predicates_at_runtime) {
    constexpr grid G{{0, 1267650600228229401496703205376_g}, 1}; // 2^100
    constexpr grid H{{1267650600228229401496703205376_g, 1267650600228229401496703205386_g}, 2};
    EXPECT_TRUE(G.representable(launder(5.0)));
    EXPECT_FALSE(G.representable(launder(2.5)));
    EXPECT_TRUE(G.representable(launder(0x1p100)));
    EXPECT_FALSE(G.representable(launder(0x1p101)));
    EXPECT_TRUE(H.representable(launder(0x1p100)));
    EXPECT_FALSE(H.representable(launder(5.0)));
    EXPECT_TRUE(includes(G.Interval, launder(1e30)));
    EXPECT_FALSE(includes(G.Interval, launder(-1.0)));
    EXPECT_TRUE(includes(G.Interval, launder(7)));
    EXPECT_FALSE(includes(H.Interval, launder(7)));
}

// A point source into a grid whose raw map is neither integer nor a 64-bit
// multiply-add (a 2^-62 notch, the full uint64 range) did not compile.
TEST(BugHuntRound8, point_into_a_fine_or_full_width_grid) {
    using F = inside<{{-1, 1}, per<(I64{1} << 62)>}, round_ceil>;
    const F f{just<rational{1, 2}>};
    EXPECT_EQ(f, rational(1, 2));
    const uint64 q{5_ins};
    EXPECT_EQ(q.to<std::uint64_t>(), 5u);
}

// An on_error action ranked above the type's clamp / wrap policy: the store
// reported overflow (and kept the old value) instead of saturating or folding.
TEST(BugHuntRound8, on_error_does_not_preempt_clamp_or_wrap) {
    errc code{};
    auto record = [&](auto&, errc c, const char*) { code = c; };

    inside<{0, 100}, clamp> c{0};
    c.on_error(record) = 150;
    EXPECT_EQ(code, errc{});
    EXPECT_EQ(c, 100);

    inside<{0, 59}, wrap> w{0};
    w.on_error(record) = 61;
    EXPECT_EQ(code, errc{});
    EXPECT_EQ(w, 1);

    inside<{{rational{1, 2}, rational{21, 2}}, 1}, round_half_even | clamp> h{rational{1, 2}};
    h.on_error(record) = 1e30;
    EXPECT_EQ(code, errc{});
    EXPECT_EQ(h, rational(21, 2));

    inside<{0, 100}> u{7}; // no clamp: on_error still replaces the failure
    u.on_error(record) = 150;
    EXPECT_EQ(code, errc::overflow);
    EXPECT_EQ(u, 7);
}

// to<long double>() went through a double: 1 + 2^-60 came back as 1. It is
// now the long double nearest the exact value, rounded once.
TEST(BugHuntRound8, to_long_double_is_rounded_once) {
#if WIDE_LONG_DOUBLE
    using F = inside<{{-2, 2}, per<(I64{1} << 62)>}>;
    using T = inside<{{-1, 1}, per<3>}>;
    static_assert(F{rational{1} + rational{1, I64{1} << 60}}.to<long double>().value() == 1.0L + 0x1p-60L);
    static_assert(T{rational{1, 3}}.to<long double>().value() == 1.0L / 3);
    const F f{rational{-1} - rational{1, I64{1} << 62}};
    EXPECT_EQ(f.to<long double>().value(), -1.0L - 0x1p-62L);
    using Q = inside<{{-1000000, 1000000}, per<1000000007>}>;
    const Q q{rational{1, 1000000007}};
    EXPECT_EQ(q.to<long double>().value(), 1.0L / 1000000007);
    using S = inside<{{-9223372036854775807, 9223372036854775807}, 1}>;
    EXPECT_EQ(S{launder(I64{9223372036854775807})}.to<long double>().value(), 9223372036854775807.0L);
#endif
}
