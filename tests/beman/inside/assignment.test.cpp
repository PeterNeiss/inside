// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Stores into an inside: from every source kind (integers, doubles,
// rationals, other insides), try_make, the casts and predicates, wrap
// arithmetic, rounding of negative values, and the compound assignments.

#include <beman/inside/inside.hpp>
#include <beman/inside/inside/casts.hpp>
#include <beman/inside/inside/detail/numbers.hpp>
#include <beman/inside/inside/detail/rational.hpp>
#include <beman/inside/inside/formats.hpp>
#include <beman/inside/inside/grid.hpp>
#include <beman/inside/inside/io.hpp>
#include <beman/inside/inside/predicates.hpp>
#include <beman/inside/math.hpp>

#include <gtest/gtest.h>

#include <cfloat>
#include <cmath>
#include <compare>
#include <cstdint>
#include <expected>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_set>
#include <utility>

#include "support.hpp"

// inside_assign
namespace inside_assign_tests {
using namespace beman::inside;
using namespace beman::inside::detail;

// conversion between bounds with compatible grids
TEST(InsideAssignTest, conversion_between_bounds_with_compatible_grids) {
    using f30t40 = inside<{{30, 40}, 2}>;
    using f20t50 = inside<{interval{20, 50}, 1}>;
    f30t40 smaller{34};
    f20t50 bigger;

    bigger = smaller;
    ASSERT_EQ(bigger, 34);

    // notch 2 -> notch 1 is always compatible (every step lands on integer)
    bigger = 39;
}

// rounding requires opt-in / policy<round_toward_zero>() rounds (truncates toward zero on positive)
TEST(InsideAssignTest, rounding_requires_opt_in__policy_snap_rounds_truncates_toward_zero_on_positive) {
    using f30t40 = inside<{{30, 40}, 2}>;
    using f20t50 = inside<{interval{20, 50}, 1}>;
    [[maybe_unused]] f20t50 bigger{39};
    [[maybe_unused]] f30t40 smaller;

    {
        SCOPED_TRACE("policy<round_toward_zero>() rounds (truncates toward zero on positive)");
        smaller.policy<round_toward_zero>() = bigger;
        ASSERT_EQ(smaller, 38);
    }
}

// rounding requires opt-in / policy<round_toward_zero>() is the alias
TEST(InsideAssignTest, rounding_requires_opt_in__with_snap_is_the_alias) {
    using f30t40 = inside<{{30, 40}, 2}>;
    using f20t50 = inside<{interval{20, 50}, 1}>;
    [[maybe_unused]] f20t50 bigger{39};
    [[maybe_unused]] f30t40 smaller;

    {
        SCOPED_TRACE("policy<round_toward_zero>() is the alias");
        bigger                              = 30;
        smaller.policy<round_toward_zero>() = bigger;
        ASSERT_EQ(smaller, 30);
    }
}

// rounding requires opt-in / policy<round_nearest>() rounds half up to grid
TEST(InsideAssignTest, rounding_requires_opt_in__with_snap_round_nearest_rounds_half_up_to_grid) {
    using f30t40 = inside<{{30, 40}, 2}>;
    using f20t50 = inside<{interval{20, 50}, 1}>;
    [[maybe_unused]] f20t50 bigger{39};
    [[maybe_unused]] f30t40 smaller;

    {
        SCOPED_TRACE("policy<round_nearest>() rounds half up to grid");
        using celsius = inside<{{-40, 60}, 0.5}, round_nearest>;
        celsius room  = 21.4;
        ASSERT_EQ(room, 21.5_r);

        celsius exact = 21.5;
        ASSERT_EQ(exact, 21.5_r);

        celsius truncated = 21.2;
        ASSERT_EQ(truncated, 21);

        using half = inside<{{0, 10}, 0.5}>;
        half h;
        h.policy<round_nearest>() = 3.3;
        ASSERT_EQ(h, 3.5_r);

        h.policy<round_nearest>() = 3.2;
        ASSERT_EQ(h, 3);
    }
}

// rounding requires opt-in / type-level snap
TEST(InsideAssignTest, rounding_requires_opt_in__type_level_snap) {
    using f30t40 = inside<{{30, 40}, 2}>;
    using f20t50 = inside<{interval{20, 50}, 1}>;
    [[maybe_unused]] f20t50 bigger{39};
    [[maybe_unused]] f30t40 smaller;

    {
        SCOPED_TRACE("type-level snap");
        using n2_round = inside<{{0, 10}, 2}, round_toward_zero>;
        using n1       = inside<{{0, 10}, 1}>;
        n2_round c;
        c = n1{3};
        ASSERT_EQ(c, 2);
    }
}

// rounding requires opt-in / type-level round_nearest
TEST(InsideAssignTest, rounding_requires_opt_in__type_level_round_nearest) {
    using f30t40 = inside<{{30, 40}, 2}>;
    using f20t50 = inside<{interval{20, 50}, 1}>;
    [[maybe_unused]] f20t50 bigger{39};
    [[maybe_unused]] f30t40 smaller;

    {
        SCOPED_TRACE("type-level round_nearest");
        // Same cross-grid, off-notch assignment as `type-level snap`, but the
        // coarse grid rounds to the nearest slot instead of truncating toward zero
        // (the `num * num` finer→coarser case). Grid points are 0,4,8,12,16,20.
        using n4_round = inside<{{0, 20}, 4}, round_nearest>;
        using n1       = inside<{{0, 20}, 1}>;
        n4_round c;

        c = n1{3};
        ASSERT_EQ(c, 4); // nearer 4 than 0 (snap would give 0)
        c = n1{5};
        ASSERT_EQ(c, 4); // nearer 4 than 8
        c = n1{1};
        ASSERT_EQ(c, 0); // nearer 0 than 4

        // Exact ties round up (half away from zero), per `round_nearest`.
        c = n1{2};
        ASSERT_EQ(c, 4); // 0|4 tie → 4
        c = n1{6};
        ASSERT_EQ(c, 8); // 4|8 tie → 8

        c = n1{8};
        ASSERT_EQ(c, 8); // already on a notch
    }
}

// rounding requires opt-in / compatible notches require no opt-in
TEST(InsideAssignTest, rounding_requires_opt_in__compatible_notches_require_no_opt_in) {
    using f30t40 = inside<{{30, 40}, 2}>;
    using f20t50 = inside<{interval{20, 50}, 1}>;
    [[maybe_unused]] f20t50 bigger{39};
    [[maybe_unused]] f30t40 smaller;

    {
        SCOPED_TRACE("compatible notches require no opt-in");
        using n1 = inside<{{0, 10}, 1}>;
        using n2 = inside<{{0, 10}, 2}>;
        n1 a;
        a = n2{6};
        ASSERT_EQ(a, 6);
    }
}

// rounding requires opt-in / wide compatible interval needs no opt-in
TEST(InsideAssignTest, rounding_requires_opt_in__wide_compatible_interval_needs_no_opt_in) {
    using f30t40 = inside<{{30, 40}, 2}>;
    using f20t50 = inside<{interval{20, 50}, 1}>;
    [[maybe_unused]] f20t50 bigger{39};
    [[maybe_unused]] f30t40 smaller;

    {
        SCOPED_TRACE("wide compatible interval needs no opt-in");
        using n2   = inside<{{0, 10}, 2}>;
        using wide = inside<{{0, 20}, 2}>;
        n2 b;
        b = wide{6};
        ASSERT_EQ(b, 6);
    }
}

// policy<clamp> / policy<wrap> per-operation
TEST(InsideAssignTest, with_clamp_with_wrap_per_operation) {
    using u100 = inside<{0, 100}>;
    u100 x{50};

    x.policy<clamp>() = 150;
    ASSERT_EQ(x, 100);

    x.policy<wrap>() = 103;
    ASSERT_EQ(x, 2);
}

// clamp during insidable assignment
TEST(InsideAssignTest, clamp_during_insidable_assignment) {
    using wide   = inside<{0, 200}>;
    using narrow = inside<{0, 100}, clamp>;
    wide   w{150};
    narrow n{0};
    n = w;
    ASSERT_EQ(n, 100);
}

// unsafe relaxes domain and round checks
TEST(InsideAssignTest, unsafe_relaxes_domain_and_round_checks) {
    // Notch-incompatible assignment compiles under unsafe.
    using src = inside<{{0, 100}, 2}, unsafe>;
    using dst = inside<{{0, 100}, 1}, unsafe>;
    src s{50};
    dst d{0};
    d = s;
    ASSERT_EQ(d, 50);

    // Native int division path engages
    using u100u = inside<{0, 100}, unsafe>;
    u100u a{51}, b{8};
    auto  q = a / b;
    static_assert(!(std::is_same_v<typename decltype(q)::value_type::raw_type, rational>));
    ASSERT_EQ(*q, 6);

    // Under unsafe the div-by-zero check is skipped entirely:
    // binary `a / 0` is undefined behavior, consistent with the compound
    // `/= zero-inside` no-op below. UB is not testable at runtime, so no assertion.

    // Compound /= by a zero inside: unsafe skips the zero check -> silent no-op.
    u100u y{50};
    y /= u100u{0};
    ASSERT_EQ(y, 50);

    // Out-of-range silent overwrite (no domain check)
    ASSERT_NO_THROW((void)(([&] {
        u100u x{50};
        x = 200;
        (void)x;
    }())));
}

// disjoint-interval assignment is allowed under wrap/clamp
TEST(InsideAssignTest, disjoint_interval_assignment_is_allowed_under_wrap_clamp) {
    // An inside whose interval is wholly outside the target is rejected for strict
    // policies but allowed under wrap/clamp (they bring any value into range —
    // matching the integral-RHS path, whose unbounded interval always overlaps).
    using src = inside<{25, 34}>;
    static_assert(inside_assignable<inside<{0, 9}, wrap>, src>);
    static_assert(inside_assignable<inside<{0, 9}, clamp>, src>);
    static_assert(!(inside_assignable<inside<{0, 9}>, src>)); // checked: rejected

    inside<{0, 9}, wrap> w{0};
    w = src{27};
    ASSERT_EQ(w, 7); // 27 mod 10
    inside<{0, 9}, clamp> c{0};
    c = src{27};
    ASSERT_EQ(c, 9); // clamped to upper
}

// Every inside is trivially copyable and destructible; its default ctor
// stores the default value (0, else Lower), so none is trivially
// default-constructible.
TEST(InsideAssignTest, trivial_copy_guarantees) {
    static_assert(std::is_trivially_copyable_v<inside<{0, 0}, unsafe>>);
    static_assert(std::is_trivially_copyable_v<inside<{0, 100}>>);
    static_assert(std::is_trivially_copyable_v<inside<{{0, 10}, rational{1u, 2}}, unsafe>>);
    static_assert(std::is_trivially_copyable_v<inside<{{-10, 10}, max_denominator{1000}}, unsafe>>);
    static_assert(std::is_trivially_destructible_v<inside<{0, 100}>>);
    static_assert(!std::is_trivially_default_constructible_v<inside<{0, 100}>>);
    static_assert(!std::is_trivially_default_constructible_v<inside<{0, 100}, unsafe>>);
}

// type-alias smoke checks
TEST(InsideAssignTest, type_alias_smoke_checks) {
    using test0_t = inside<{{1, 3}, 1}>;
    using test4_t = inside<{{0u, std::numeric_limits<umax>::max()}, 1}>;
    using test5_t = inside<{1_r}>;
    static_assert(std::is_same_v<test0_t::raw_type, std::uint8_t>);
    static_assert(std::is_same_v<test4_t::raw_type, std::uint64_t>);
    static_assert(std::is_same_v<test5_t::raw_type, detail::point_slot>); // a point stores nothing
}

// integer rhs into non-integer-interval inside
TEST(InsideAssignTest, integer_rhs_into_non_integer_interval_inside) {
    // Lower/Upper are non-integer: integer-interval fast path is skipped,
    // exercising the rational-aware out-of-range branch in handle_out_of_range.
    using halfgrid = inside<{{0.5_r, 5.5_r}, 0.5_r}, clamp>;

    halfgrid over{100}; // out of range, clamps to 5.5
    ASSERT_EQ(over, 5.5_r);

    halfgrid under{-100}; // clamps to 0.5
    ASSERT_EQ(under, 0.5_r);

    halfgrid in{3}; // 3 lands on a notch (3.0 = 6 notches)
    ASSERT_EQ(in, 3);
}

// checked policy throws on float out-of-range
TEST(InsideAssignTest, checked_policy_throws_on_float_out_of_range) {
    using c10 = inside<{0, 10}, checked>;
    ASSERT_THROW((void)(c10{100.0}), beman::inside::inside_error);
    ASSERT_THROW((void)(c10{-1.0}), beman::inside::inside_error);

    // rational rhs takes the same path
    ASSERT_THROW((void)(c10{20_r}), beman::inside::inside_error);
}

// checked policy throws on rounding error
TEST(InsideAssignTest, checked_policy_throws_on_rounding_error) {
    using coarse = inside<{{0, 10}, 2}, checked>;
    coarse c;

    // 3.0 doesn't land on notch 2 — checks_rounding fires
    ASSERT_THROW((void)((c = 3.0)), beman::inside::inside_error);

    // value on the notch is fine
    ASSERT_NO_THROW((void)((c = 4.0)));
    ASSERT_EQ(c, 4);
}

// snap truncates non-notch float at runtime
TEST(InsideAssignTest, snap_truncates_non_notch_float_at_runtime) {
    using coarse = inside<{{0, 10}, 2}, round_toward_zero>;
    coarse c;
    c = 3.0;
    ASSERT_EQ(c, 2); // truncates toward zero

    c = 7.99;
    ASSERT_EQ(c, 6);
}

// checked inside-to-inside out-of-range throws
TEST(InsideAssignTest, checked_inside_to_inside_out_of_range_throws) {
    using src = inside<{0, 100}>;
    using dst = inside<{0, 50}, checked>;
    src s{75};
    ASSERT_THROW((void)(dst{s}), beman::inside::inside_error);
}

// non-integer-mapping inside-to-inside clamp / range_fail
TEST(InsideAssignTest, non_integer_mapping_inside_to_inside_clamp_domain_fail) {
    // Source has notch 1 to a destination with notch 1/3 — Factor=3, integer.
    // To force the non-integer-mapping path we use a fractional-notch source
    // with a destination that doesn't cleanly align: notch 1/2 -> notch 1/3.
    using src = inside<{{0, 5}, rational{1u, 2}}, round_toward_zero>;
    using dst = inside<{{0, 5}, rational{1u, 3}}, round_toward_zero | clamp>;

    src s{4}; // value 4 -> dst aligns
    dst d{s};
    ASSERT_EQ(d, 4);

    // src[0,5] dst[0,4] — value 5 in src is out of dst, exercises clamp path
    using dst2 = inside<{{0, 4}, rational{1u, 3}}, round_toward_zero | clamp>;
    src  s2{5};
    dst2 d2{s2};
    ASSERT_EQ(d2, 4);
}

// wrap on fractional / notch grids (insidable rhs)
TEST(InsideAssignTest, wrap_on_fractional_notch_grids_insidable_rhs) {
    using namespace beman::inside;
    // {0,1} notch 1/4 — integer interval but fractional notch. Wrap period is
    // (Upper - Lower) + Notch = 1.25; slots {0, .25, .5, .75, 1.0}.
    using dst = inside<{{0, 1}, per<4>}, wrap | round_nearest>;
    using src = inside<{{-2, 2}, per<4>}, round_nearest>;

    ASSERT_EQ((rational{dst{src{rational{5, 4}}}}), rational{0});       // 1.25 -> 0
    ASSERT_EQ((rational{dst{src{rational{6, 4}}}}), (rational{1, 4}));  // 1.50 -> 0.25
    ASSERT_EQ((rational{dst{src{rational{7, 4}}}}), (rational{1, 2}));  // 1.75 -> 0.50
    ASSERT_EQ((rational{dst{src{rational{-1, 4}}}}), rational{1});      // -0.25 -> 1.0
    ASSERT_EQ((rational{dst{src{rational{-2, 4}}}}), (rational{3, 4})); // -0.50 -> 0.75

    // Non-integer interval, notch 1/2: {1/2, 5/2} period = 2 + 1/2 = 2.5.
    using dst2 = inside<{{rational{1, 2}, rational{5, 2}}, per<2>}, wrap | round_nearest>;
    using src2 = inside<{{-4, 4}, per<2>}, round_nearest>;
    ASSERT_EQ(rational{dst2{src2{rational{3}}}}, (rational{1, 2})); // 3.0 -> 0.5

    // Unit-integer grid still uses the fast path and wraps as before.
    using deg  = inside<{0, 359}, wrap>;
    using wide = inside<{-720, 720}>;
    ASSERT_EQ(deg{wide{370}}, 10);
    ASSERT_EQ(deg{wide{-10}}, 350);
}

// A continuous source (an exact quotient) between the target's notches is a
// rounding_error under checked, and rounds under a rounding policy.
TEST(InsideAssignTest, continuous_source_off_the_lattice_is_checked) {
    using any        = inside<{0, 100}>;
    using strict     = inside<{{-1, 1}, per<1000>}>;
    using near       = inside<{{-1, 1}, per<1000>}, round_nearest>;
    const auto third = (any{1} / any{3}).value();
    EXPECT_THROW(strict{third}, inside_error);
    strict s{0};
    EXPECT_THROW(s = third, inside_error);
    EXPECT_EQ(near{third}, (rational{333, 1000}));
    const auto half = (any{1} / any{2}).value();
    s               = half; // on the lattice: exact
    EXPECT_EQ(s, (rational{1, 2}));
}

// Mixed-sign integer sources and scalars compare by value: a uint64_t past
// INT64_MAX is not −1, and a negative inside is below every unsigned.
TEST(InsideAssignTest, mixed_sign_integers_compare_by_value) {
    constexpr std::uint64_t big       = std::numeric_limits<std::uint64_t>::max();
    constexpr std::uint64_t past_imax = std::uint64_t{1} << 63;
    using neg                         = inside<{-10, 10}>;
    EXPECT_THROW((void)(neg{big}), inside_error);
    EXPECT_THROW((void)(neg{past_imax}), inside_error);
    inside<{-10, 10}, clamp> c{std::uint64_t{3}};
    c = big;
    EXPECT_EQ(c, 10);
    c = past_imax;
    EXPECT_EQ(c, 10);
    c = 4u;
    EXPECT_EQ(c, 4);

    const neg m{-1};
    EXPECT_NE(m, big);
    EXPECT_NE(m, 0xFFFFFFFFu);
    EXPECT_TRUE(m < 0u);
    EXPECT_TRUE(m < big);
    EXPECT_EQ(m <=> past_imax, std::strong_ordering::less);
    EXPECT_EQ(neg{7} <=> std::uint64_t{7}, std::strong_ordering::equal);
    EXPECT_EQ(neg{7} <=> 6u, std::strong_ordering::greater);
    EXPECT_EQ(neg{7}, std::uint64_t{7});
    EXPECT_EQ(neg{1}, true);
    EXPECT_EQ(neg{0}, false);
    static_assert(neg{-1} != big && neg{-1} < past_imax);
}
} // namespace inside_assign_tests

// Fallible construction is `B::try_make(value)` → std::expected<B, errc>: a
// value the policy cannot bring into the grid is an error value, not a throw.
// (The former `inside(value, errc&)` constructor is gone; assigning with an error
// code stays `b.policy(ec) = value`.)
namespace try_make_tests {
using namespace beman::inside;
using namespace beman::inside::detail;

// out of range → overflow
TEST(TryMakeTest, out_of_range_is_overflow) {
    auto r = inside<{0, 100}>::try_make(150);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), errc::overflow);
}

// in range → the value
TEST(TryMakeTest, in_range_yields_the_value) {
    auto r = inside<{0, 100}>::try_make(42);
    ASSERT_TRUE(r.has_value());
    EXPECT_TRUE(static_cast<rational>(*r) == 42);
}

// clamp / wrap bring the value in: no error
TEST(TryMakeTest, clamp_and_wrap_are_not_errors) {
    auto c = inside<{0, 100}, clamp>::try_make(150);
    ASSERT_TRUE(c.has_value());
    EXPECT_TRUE(static_cast<rational>(*c) == 100);
    auto w = inside<{0, 9}, wrap>::try_make(13);
    ASSERT_TRUE(w.has_value());
    EXPECT_TRUE(static_cast<rational>(*w) == 3); // 13 mod 10
}

// try_make reports what policy(ec) assignment reports on the same input
TEST(TryMakeTest, matches_per_op_policy_ec_on_the_same_input) {
    errc             ec_op{};
    inside<{0, 100}> via_op{0};
    via_op.policy(ec_op) = 200;
    using pct            = inside<{0, 100}>;
    EXPECT_EQ(pct::try_make(200).error(), ec_op);
}

// a double source goes through the same store as the constructors
TEST(TryMakeTest, double_source_reports_like_the_constructor) {
    using F = inside<{{0, 1}, per<4>}, round_nearest>;
    EXPECT_EQ(F::try_make(1.2).error(), errc::overflow); // rounds to 1.25: outside
    ASSERT_TRUE(F::try_make(1.1).has_value());           // rounds to 1.0
    EXPECT_EQ(rational{*F::try_make(1.1)}, rational{1});
}

// error values work in constant evaluation (error-code mode records, no abort)
TEST(TryMakeTest, errors_are_values_at_compile_time) {
    using pct = inside<{0, 100}>;
    static_assert(pct::try_make(150).error() == errc::overflow);
    static_assert(inside<{{0, 10}, 2}>::try_make(3).error() == errc::rounding_error);
    static_assert(pct::try_make(42).has_value());
}
} // namespace try_make_tests

// casts
namespace casts_tests {
using namespace beman::inside;
using namespace beman::inside::detail;

//---------------------------------------------------------------------------
// std::numeric_limits<inside>
//---------------------------------------------------------------------------
// numeric_limits reports grid bounds
TEST(CastsTest, numeric_limits_reports_grid_bounds) {
    using pct = inside<{0, 100}>;
    using nl  = std::numeric_limits<pct>;

    static_assert(nl::is_specialized);
    static_assert(nl::is_bounded);
    static_assert(nl::is_integer);
    static_assert(nl::is_exact);
    static_assert(!(nl::is_signed));
    static_assert(!(nl::is_modulo));

    static_assert(nl::min() == pct{0});
    static_assert(nl::max() == pct{100});
    static_assert(nl::lowest() == pct{0});
}

// numeric_limits handles signed and wrapping bounds
TEST(CastsTest, numeric_limits_handles_signed_and_wrapping_bounds) {
    using temp = inside<{-40, 60}>;
    static_assert(std::numeric_limits<temp>::is_signed);
    static_assert(std::numeric_limits<temp>::lowest() == temp{-40});

    using ang = inside<{0, 359}, wrap>;
    static_assert(std::numeric_limits<ang>::is_modulo);
}

//---------------------------------------------------------------------------
// std::hash<inside>
//---------------------------------------------------------------------------
// hash specialization works with unordered_set
TEST(CastsTest, hash_specialization_works_with_unordered_set) {
    using idx = inside<{0, 9}>;
    std::unordered_set<idx> s;
    s.insert(idx{3});
    s.insert(idx{7});
    s.insert(idx{3});

    ASSERT_EQ(s.size(), 2);
    ASSERT_TRUE(s.contains(idx{3}));
    ASSERT_FALSE(s.contains(idx{5}));
}

//---------------------------------------------------------------------------
// predicates
//---------------------------------------------------------------------------
// conversion_overflows
TEST(CastsTest, conversion_overflows) {
    using pct = inside<{0, 100}>;

    static_assert(!(conversion_overflows<pct>(50)));
    static_assert(conversion_overflows<pct>(150));
    static_assert(conversion_overflows<pct>(-1));
    static_assert(!(conversion_overflows<pct>(0)));
    static_assert(!(conversion_overflows<pct>(100)));
}

// conversion_rounds detects non-notch values
TEST(CastsTest, conversion_rounds_detects_non_notch_values) {
    using coarse = inside<{{0, 10}, 2}>; // notch 2

    static_assert(!(conversion_rounds<coarse>(0)));
    static_assert(!(conversion_rounds<coarse>(4)));
    static_assert(conversion_rounds<coarse>(3));     // doesn't land on 2-notch
    static_assert(!(conversion_rounds<coarse>(11))); // out of range, not truncation
}

// conversion_loses combines both
TEST(CastsTest, conversion_is_lossy_combines_both) {
    using coarse = inside<{{0, 10}, 2}>;

    static_assert(!(conversion_loses<coarse>(4)));
    static_assert(conversion_loses<coarse>(3));  // truncation
    static_assert(conversion_loses<coarse>(20)); // overflow
}

//---------------------------------------------------------------------------
// the clamp constructor / checked_cast / the unsafe constructor
//---------------------------------------------------------------------------
// the clamp constructor clamps to boundary
TEST(CastsTest, clamp_cast_clamps_to_boundary) {
    using pct = inside<{0, 100}>;

    ASSERT_TRUE((pct{150, clamp} == pct{100}));
    ASSERT_TRUE((pct{-5, clamp} == pct{0}));
    ASSERT_TRUE((pct{42, clamp} == pct{42}));
}

// checked_cast throws on out-of-range
TEST(CastsTest, checked_cast_throws_on_out_of_range) {
    using pct = inside<{0, 100}>;

    static_assert(checked_cast<pct>(42) == pct{42});
    ASSERT_THROW((void)(checked_cast<pct>(150)), beman::inside::inside_error);
    ASSERT_THROW((void)(checked_cast<pct>(-1)), beman::inside::inside_error);
}

// checked_cast throws on truncation
TEST(CastsTest, checked_cast_throws_on_truncation) {
    using coarse = inside<{{0, 10}, 2}>;

    static_assert(checked_cast<coarse>(4) == coarse{4});
    ASSERT_THROW((void)(checked_cast<coarse>(3)), beman::inside::inside_error);
}

// the unsafe constructor bypasses runtime checks
TEST(CastsTest, unchecked_cast_bypasses_runtime_checks) {
    using pct = inside<{0, 100}>;

    // In-range value: same result as checked_cast.
    static_assert(pct{42, unsafe} == pct{42});
    static_assert(pct{0, unsafe} == pct{0});
}

//---------------------------------------------------------------------------
// _ins literal
//---------------------------------------------------------------------------
// _ins literal produces just<N>
TEST(CastsTest, ins_literal_produces_just_n) {
    constexpr auto five = 5_ins;
    static_assert(lower_of<decltype(five)> == 5);
    static_assert(upper_of<decltype(five)> == 5);
    static_assert(five == 5);

    // Composes with inside arithmetic — grid widens through addition.
    using pct = inside<{0, 100}>;
    static_assert(10_ins + pct{40} == 50);
}

//---------------------------------------------------------------------------
// add_all / mul_all
//---------------------------------------------------------------------------
// add_all / mul_all fold variadically
TEST(CastsTest, add_all_mul_all_fold_variadically) {
    using v = inside<{0, 100}>;
    constexpr v a{10}, b{20}, c{30}, d{40};
    static_assert(add_all(a, b, c, d) == 100);

    constexpr v p{2}, q{3}, r{5};
    static_assert(mul_all(p, q, r) == 30);
}

//---------------------------------------------------------------------------
// rounding modes (policy<round_floor> / policy<round_ceil> / policy<round_half_even>)
//---------------------------------------------------------------------------
// Rounding modes apply when rhs is fractional (float, double, rational).
// Integer rhs takes the truncation fast path which is *intentionally*
// rounding-policy-agnostic — see assignment.hpp:store(integral).
// policy<round_floor> rounds toward -inf for double rhs
TEST(CastsTest, with_snap_round_floor_rounds_toward_inf_for_double_rhs) {
    using coarse = inside<{{0, 10}, 2}>;
    coarse c{0};

    c.policy<round_floor>() = 3.0;
    ASSERT_EQ(c, 2);

    c.policy<round_floor>() = 4.0;
    ASSERT_EQ(c, 4);

    c.policy<round_floor>() = 5.0;
    ASSERT_EQ(c, 4);
}

// policy<round_ceil> rounds toward +inf for double rhs
TEST(CastsTest, with_snap_round_ceil_rounds_toward_plus_inf_for_double_rhs) {
    using coarse = inside<{{0, 10}, 2}>;
    coarse c{0};

    c.policy<round_ceil>() = 3.0;
    ASSERT_EQ(c, 4);

    c.policy<round_ceil>() = 4.0;
    ASSERT_EQ(c, 4);

    c.policy<round_ceil>() = 5.0;
    ASSERT_EQ(c, 6);
}

// policy<round_half_even> applies banker's rounding
TEST(CastsTest, with_snap_round_half_even_applies_banker_s_rounding) {
    using coarse = inside<{{0, 10}, 2}>;
    coarse c{0};

    // 1.0 is the half-way point between notch 0 and notch 2 → even wins (0).
    c.policy<round_half_even>() = 1.0;
    ASSERT_EQ(c, 0);

    // 3.0 is half-way between 2 and 4 → even wins (4).
    c.policy<round_half_even>() = 3.0;
    ASSERT_EQ(c, 4);

    // 5.0 is halfway between 4 and 6 → even wins (4).
    c.policy<round_half_even>() = 5.0;
    ASSERT_EQ(c, 4);

    // 7.0 is halfway between 6 and 8 → even wins (8).
    c.policy<round_half_even>() = 7.0;
    ASSERT_EQ(c, 8);
}

//---------------------------------------------------------------------------
// the clamp constructor with a rounding mode
//---------------------------------------------------------------------------
// the clamp constructor with a rounding mode compose clamp + round
TEST(CastsTest, clamp_cast_with_rounding_compose_clamp_plus_round) {
    using coarse = inside<{{0, 10}, 2}>;

    // In-range, off-notch: round per mode.
    ASSERT_TRUE((coarse{3.0, clamp | round_floor} == coarse{2}));
    ASSERT_TRUE((coarse{3.0, clamp | round_ceil} == coarse{4}));
    ASSERT_TRUE((coarse{3.0, clamp | round_nearest} == coarse{4}));

    // Out-of-range: clamp to boundary.
    ASSERT_TRUE((coarse{15.0, clamp | round_floor} == coarse{10}));
    ASSERT_TRUE((coarse{15.0, clamp | round_ceil} == coarse{10}));
    ASSERT_TRUE((coarse{15.0, clamp | round_nearest} == coarse{10}));

    ASSERT_TRUE((coarse{-3.0, clamp | round_floor} == coarse{0}));
}

//---------------------------------------------------------------------------
// clamp_* accept a notch-incompatible *inside* source. The one-shot rounding
// policy widens the value+policy constructor's assignable check, so a finer
// grid rounds onto the target (not just arithmetic sources).
//---------------------------------------------------------------------------
// the clamp constructor with a rounding mode accept an inside source
TEST(CastsTest, clamp_cast_with_rounding_accept_an_inside_source) {
    using small = inside<{{0, 10}, per<10>}, clamp>; // 1/10 grid
    small a     = 2.5;                               // exact on the 1/10 grid
    auto  sq    = a * a;                             // exact 6.25 on the 1/100 grid

    ASSERT_TRUE((small{sq, clamp | round_floor} == 6.2_r));   // toward −∞
    ASSERT_TRUE((small{sq, clamp | round_ceil} == 6.3_r));    // toward +∞
    ASSERT_TRUE((small{sq, clamp | round_nearest} == 6.3_r)); // 6.25 tie → half away → 6.3

    small four = 4;                                                 // 16 is out of range
    ASSERT_TRUE((small{four * four, clamp | round_nearest} == 10)); // clamp to boundary

    ASSERT_TRUE((small{150.0, clamp | round_nearest} == 10)); // arithmetic source: no regression
}
} // namespace casts_tests

// wrap policy must reduce out-of-range integers without signed overflow.
//
// Regression: apply_wrap computed `range = upper - lower + 1` and
// `shifted = rhs - lower` in imax, which overflowed (UB) when rhs was a large
// integral and Lower was a large-magnitude opposite sign (e.g. assigning
// INT64_MAX to a wrap grid with a negative Lower), and when the grid spanned
// more than imax under unsigned storage. The reduction now runs in umax.
//
// The oracle is an independent __int128 modular wrap, so values are checked, not
// pinned to magic constants.
namespace wrap_overflow_tests {
using namespace beman::inside;
using namespace beman::inside::detail;

namespace {
// Reference: reduce v into [lo, hi] by wrapping (range = hi - lo + 1).
long long wrap_ref(long long v, long long lo, long long hi) {
#if defined(__SIZEOF_INT128__)
    __int128 range = static_cast<__int128>(hi) - static_cast<__int128>(lo) + 1;
    __int128 r     = (((static_cast<__int128>(v) - static_cast<__int128>(lo)) % range) + range) % range;
    return static_cast<long long>(r + lo);
#else
    // u64 modular reduction (no __int128 needed). `v - lo` can exceed 64 signed bits
    // at the extremes, so reduce in unsigned (two's-complement) space.
    unsigned long long urange = static_cast<unsigned long long>(hi) - static_cast<unsigned long long>(lo) + 1ull;
    unsigned long long off;
    if (v >= lo)
        off = (static_cast<unsigned long long>(v) - static_cast<unsigned long long>(lo)) % urange;
    else {
        unsigned long long d = static_cast<unsigned long long>(lo) - static_cast<unsigned long long>(v);
        unsigned long long m = d % urange;
        off                  = m ? urange - m : 0ull;
    }
    return static_cast<long long>(static_cast<unsigned long long>(lo) + off);
#endif
}
} // namespace

// wrap reduces extreme integers with no signed overflow
TEST(WrapOverflowTest, wrap_reduces_extreme_integers_with_no_signed_overflow) {
    using W                = inside<{-5000000000LL, 5000000000LL}, wrap>; // span 1e10 > would-be int32
    constexpr long long lo = -5000000000LL, hi = 5000000000LL;

    for (long long v : {hi + 1,
                        lo - 1,
                        0LL,
                        7777777777LL,
                        -7777777777LL,
                        std::numeric_limits<long long>::max(),
                        std::numeric_limits<long long>::min() + 1,
                        std::numeric_limits<long long>::min()}) {
        W               b = v;
        const long long got =
            static_cast<long long>(static_cast<rational>(b).Num) * (static_cast<rational>(b).Den < 0 ? -1 : 1);
        SCOPED_TRACE(::testing::Message() << "v=" << v);
        ASSERT_EQ(got, (wrap_ref(v, lo, hi)));
        ASSERT_TRUE(got >= lo);
        ASSERT_TRUE(got <= hi);
    }
}

// wrap on a grid spanning more than imax
TEST(WrapOverflowTest, wrap_on_a_grid_spanning_more_than_imax) {
    // Unsigned-storage grid whose span exceeds INT64_MAX.
    using Big                       = inside<{0, 12000000000000000000ULL}, wrap>; // span ~1.2e19 > imax
    constexpr unsigned long long hi = 12000000000000000000ULL;

    // A value above the top wraps to near the bottom; one below 0 wraps to the top.
    Big a = -1; // -1 -> hi
    ASSERT_TRUE((static_cast<rational>(a) == rational{hi, 1}));
    Big b = 0;
    ASSERT_TRUE(static_cast<rational>(b) == 0);
}

// wrap is identity on a grid spanning the whole u64 range
TEST(WrapOverflowTest, wrap_is_identity_on_a_grid_spanning_the_whole_u64_range) {
    // span == 2^64-1, so `urange = upper - lower + 1` wraps to 0 in umax: every
    // u64 bit pattern is already in range, so the wrap is the identity and the
    // value is stored verbatim. Exercises the urange==0 fast path in apply_wrap.
    // Identity is a raw round-trip: lower is 0, so the stored raw is exactly the
    // input reinterpreted as u64 (a negative input keeps its two's-complement bits
    // — it is not reducible, since the whole u64 range is in-grid).
    using All = inside<{0, 18446744073709551615ULL}, wrap>;
    static_assert(std::is_same_v<All::raw_type, std::uint64_t>);
    for (long long v :
         {0LL, 123456789LL, -1LL, std::numeric_limits<long long>::max(), std::numeric_limits<long long>::min()}) {
        All b = v;
        SCOPED_TRACE(::testing::Message() << "v=" << v);
        ASSERT_TRUE(b.raw() == static_cast<std::uint64_t>(v)); // identity, no reduction
    }
}

// wrap still correct on a small symmetric grid
TEST(WrapOverflowTest, wrap_still_correct_on_a_small_symmetric_grid) {
    using S = inside<{-3, 3}, wrap>;                     // range 7
    ASSERT_TRUE(static_cast<rational>(S{0} = 4) == -3);  // 4  -> -3
    ASSERT_TRUE(static_cast<rational>(S{0} = -4) == 3);  // -4 ->  3
    ASSERT_TRUE(static_cast<rational>(S{0} = 10) == 3);  // 10 ->  3
    ASSERT_TRUE(static_cast<rational>(S{0} = -3) == -3); // on-edge unchanged
}
} // namespace wrap_overflow_tests

// Assignment/conversion rounding of negative values must match the division
// path (detail::div_rounded) and the documented policy semantics.
//
// Regression: round_quotient rounded the NON-NEGATIVE grid offset
// (rhs - Lower)/Notch instead of the signed value, so on grids spanning
// negative values:
//   * round_nearest behaved as round-half-UP, not half-away-from-zero — and
//     disagreed with division (assigning -2.5 gave -2 while x/y == -2.5 gave -3);
//   * round_half_even broke ties to an even OFFSET INDEX, giving an odd VALUE
//     whenever Lower was odd (Lower=-9: -2.5 -> -3 instead of -2);
//   * bare snap truncated toward -inf instead of toward zero.
// The fix rounds in value space, so assignment and division now agree.
namespace round_negative_ties_tests {
using namespace beman::inside;
using namespace beman::inside::detail;

// round_nearest assignment is half-away-from-zero on negatives
TEST(RoundNegativeTiesTest, round_nearest_assignment_is_half_away_from_zero_on_negatives) {
    using N = inside<{-100, 100}, round_nearest>;
    ASSERT_TRUE((static_cast<rational>(N(rational{5, 2})) == 3));   //  2.5 ->  3
    ASSERT_TRUE((static_cast<rational>(N(rational{-5, 2})) == -3)); // -2.5 -> -3 (was -2)
    ASSERT_TRUE((static_cast<rational>(N(rational{3, 2})) == 2));   //  1.5 ->  2
    ASSERT_TRUE((static_cast<rational>(N(rational{-3, 2})) == -2)); // -1.5 -> -2 (was -1)
    ASSERT_TRUE((static_cast<rational>(N(rational{-1, 2})) == -1)); // -0.5 -> -1 (was  0)
}

// assignment rounding agrees with division on the same value
TEST(RoundNegativeTiesTest, assignment_rounding_agrees_with_division_on_the_same_value) {
    // -5/2 == -2.5 ; assigning -2.5 and dividing to -2.5 must store the same point.
    using N  = inside<{-100, 100}, round_nearest>;
    using Nd = inside<{1, 10}, round_nearest>;
    ASSERT_TRUE((static_cast<rational>(N{-5} / Nd{2}) == static_cast<rational>(N(rational{-5, 2}))));
    ASSERT_TRUE((static_cast<rational>(N{-7} / Nd{2}) == static_cast<rational>(N(rational{-7, 2}))));
    ASSERT_TRUE((static_cast<rational>(N{7} / Nd{2}) == static_cast<rational>(N(rational{7, 2}))));
}

// round_half_even assignment ties to even VALUE regardless of Lower parity
TEST(RoundNegativeTiesTest, round_half_even_assignment_ties_to_even_value_regardless_of_lower_parity) {
    using He = inside<{-10, 10}, round_half_even>; // Lower even
    using Ho = inside<{-9, 9}, round_half_even>;   // Lower odd
    // -2.5 -> -2 (even) in BOTH; the historical bug gave -3 for the odd-Lower grid.
    ASSERT_TRUE((static_cast<rational>(He(rational{-5, 2})) == -2));
    ASSERT_TRUE((static_cast<rational>(Ho(rational{-5, 2})) == -2));
    // a few more ties, both signs / both grids.
    ASSERT_TRUE((static_cast<rational>(He(rational{5, 2})) == 2)); //  2.5 ->  2
    ASSERT_TRUE((static_cast<rational>(He(rational{7, 2})) == 4)); //  3.5 ->  4
    ASSERT_TRUE((static_cast<rational>(Ho(rational{5, 2})) == 2));
    ASSERT_TRUE((static_cast<rational>(Ho(rational{-7, 2})) == -4)); // -3.5 -> -4
}

// bare snap assignment truncates toward zero on negatives
TEST(RoundNegativeTiesTest, bare_snap_assignment_truncates_toward_zero_on_negatives) {
    using S = inside<{{-100, 100}, per<4>}, round_toward_zero>;
    // 1/8-off values truncate toward zero (not toward -inf).
    ASSERT_TRUE((static_cast<rational>(S(rational{7, 8})) == rational{3, 4}));   //  0.875 ->  0.75
    ASSERT_TRUE((static_cast<rational>(S(rational{-7, 8})) == rational{-3, 4})); // -0.875 -> -0.75 (was -1)
    ASSERT_TRUE((static_cast<rational>(S(rational{-1, 8})) == 0));               // -0.125 ->  0    (was -0.25)
}

// round_floor / round_ceil assignment stay direction-correct
TEST(RoundNegativeTiesTest, round_floor_round_ceil_assignment_stay_direction_correct) {
    using F = inside<{-100, 100}, round_floor>;
    using C = inside<{-100, 100}, round_ceil>;
    ASSERT_TRUE((static_cast<rational>(F(rational{-3, 2})) == -2)); // floor(-1.5)
    ASSERT_TRUE((static_cast<rational>(F(rational{3, 2})) == 1));   // floor( 1.5)
    ASSERT_TRUE((static_cast<rational>(C(rational{-3, 2})) == -1)); // ceil(-1.5)
    ASSERT_TRUE((static_cast<rational>(C(rational{3, 2})) == 2));   // ceil( 1.5)
}

// Cross-grid inside->inside conversion has its own rounding path (assignment.hpp
// store()). It used to special-case only round_nearest (offset half-up) and let
// round_ceil / round_half_even fall through to truncation; it now routes through
// the shared round_quotient so every mode rounds in value space.
// cross-grid conversion rounds every mode in value space
TEST(RoundNegativeTiesTest, cross_grid_conversion_rounds_every_mode_in_value_space) {
    using Src = inside<{{-10, 10}, per<2>}>; // half-steps, spans negatives
    using Dn  = inside<{-10, 10}, round_nearest>;
    using Df  = inside<{-10, 10}, round_floor>;
    using Dc  = inside<{-10, 10}, round_ceil>;
    using De  = inside<{-10, 10}, round_half_even>;

    Src s{rational{-5, 2}};                          // -2.5, on the source grid
    ASSERT_TRUE(static_cast<rational>(Dn{s}) == -3); // half away (was -2)
    ASSERT_TRUE(static_cast<rational>(Df{s}) == -3); // floor
    ASSERT_TRUE(static_cast<rational>(Dc{s}) == -2); // ceil  (was -3: truncated)
    ASSERT_TRUE(static_cast<rational>(De{s}) == -2); // tie -> even (was -3)

    Src s2{rational{-7, 2}};                          // -3.5
    ASSERT_TRUE(static_cast<rational>(De{s2}) == -4); // tie -> even
    ASSERT_TRUE(static_cast<rational>(Dc{s2}) == -3); // ceil

    Src p{rational{5, 2}}; // +2.5 (positives unaffected)
    ASSERT_TRUE(static_cast<rational>(Dn{p}) == 3);
    ASSERT_TRUE(static_cast<rational>(De{p}) == 2);
}
} // namespace round_negative_ties_tests

// compound_assign
namespace compound_assign_tests {
using namespace beman::inside;
using namespace beman::inside::detail;

// Raw int/float/double are no longer arithmetic-mutation operands: `b += 1`,
// `b *= 2.0` are ill-formed (guidance static_assert in arithmetic.hpp). The only
// non-inside operand a compound assign accepts is a `rational`. As with the binary
// operators, the guidance overloads are SFINAE-transparent (the static_assert
// fires only on a call), so the ill-formedness can't be probed with
// `requires` — only the sanctioned spellings are positively testable.
// compound assignment: sanctioned RHS compiles
TEST(CompoundAssignTest, compound_assignment_sanctioned_rhs_compiles) {
    using u100 = inside<{0, 100}>;
    static_assert(requires(u100 b) { b += 1_ins; });       // an inside
    static_assert(requires(u100 b) { b += rational{1}; }); // a rational
}

// compound assignment: insidable RHS
TEST(CompoundAssignTest, compound_assignment_insidable_rhs) {
    using u100 = inside<{0, 100}>;
    static_assert([] {
        u100 a{50}, d{20};
        a -= d;
        return a.as<imax>();
    }() == 30);

    using ui = inside<{0, 100}, round_toward_zero>;
    static_assert([] {
        ui d{50}, two{2};
        d *= two;
        return d.as<imax>();
    }() == 100);
    static_assert([] {
        ui e{60}, three{3};
        e /= three;
        return e.as<imax>();
    }() == 20);
    static_assert([] {
        ui f{17}, five{5};
        f %= five;
        return f.as<imax>();
    }() == 2);
}

// compound /= and %= by a zero inside report by default
TEST(CompoundAssignTest, compound_and_by_a_zero_inside_report_by_default) {
    using u100 = inside<{0, 100}>;
    u100 a{50};
    ASSERT_THROW((void)(([&] { a /= u100{0}; }())), beman::inside::inside_error);

    using ui = inside<{0, 100}, round_toward_zero>;
    ui s{50};
    ASSERT_THROW((void)(([&] { s %= ui{0}; }())), beman::inside::inside_error);
}

// compound assignment: rational RHS
TEST(CompoundAssignTest, compound_assignment_rational_rhs) {
    // round_nearest required so the inside's rational assignment path is available.
    using rn = inside<{{0, 100}, per<100>}, round_nearest>;

    rn a{0.5_r};
    a += 0.25_r; // 0.50 + 0.25 = 0.75
    ASSERT_EQ(a, rn{0.75_r});

    a -= 0.25_r; // 0.75 − 0.25 = 0.50
    ASSERT_EQ(a, rn{0.5_r});

    a *= 4_r; // 0.50 × 4 = 2
    ASSERT_EQ(a, rn{2_r});

    a /= 2_r; // 2 / 2 = 1
    ASSERT_EQ(a, rn{1_r});
}

// compound assignment: fractional RHS via rational
TEST(CompoundAssignTest, compound_assignment_fractional_rhs_via_rational) {
    using rn = inside<{{-100, 100}, per<16>}, round_nearest>;

    rn a{1.0}; // construction from a double is unchanged
    a += 2.5_r;
    ASSERT_EQ(a, rn{3.5});

    a -= 1.5_r;
    ASSERT_EQ(a, rn{2.0});

    a *= 1.5_r;
    ASSERT_EQ(a, rn{3.0});

    a /= 2_r;
    ASSERT_EQ(a, rn{1.5});
}

// compound /= 0_r (rational zero) reports error
TEST(CompoundAssignTest, compound_0_r_rational_zero_reports_error) {
    using rn = inside<{{0, 100}, per<100>}, round_nearest>;
    rn a{0.5_r};
    ASSERT_THROW((void)(([&] { a /= 0_r; }())), beman::inside::inside_error);
}

// increment / decrement
TEST(CompoundAssignTest, increment_decrement) {
    using u10 = inside<{0, 10}>;
    // The ++/-- happens inside each lambda body, fully sequenced before the lambda
    // returns; the `== N` compares the lambda's *result*, not a mutated operand.
    static_assert([] {
        u10 a{5};
        ++a;
        return a.as<imax>();
    }() == 6);
    static_assert([] {
        u10 a{5};
        a++;
        return a.as<imax>();
    }() == 6);
    static_assert([] {
        u10 a{5};
        --a;
        return a.as<imax>();
    }() == 4);
    static_assert([] {
        u10 a{5};
        a--;
        return a.as<imax>();
    }() == 4);

    // post-inc/dec returns the old value
    static_assert([] {
        u10  a{5};
        auto r = a++;
        return r.as<imax>();
    }() == 5);
    static_assert([] {
        u10  a{5};
        auto r = a--;
        return r.as<imax>();
    }() == 5);
}

//---------------------------------------------------------------------------
// operator-= raw fast path (perf): with equal notches `-=` subtracts raws
// directly (index rhs debits a compile-time Lower/Notch bias) instead of
// delegating to `+= (-rhs)`. These cases pin value equality with the binary
// route and the policy tail at both raw edges. [perf-paths]
//---------------------------------------------------------------------------
// compound -=: raw fast path agrees with binary subtraction
TEST(CompoundAssignTest, compound_raw_fast_path_agrees_with_binary_subtraction) {
    // Q8.8 — index storage both sides, bias 0.
    using q88 = inside<{{0, 255}, per<256>}, round_toward_zero>;
    for (int whole : {0, 1, 100, 255})
        for (int sub : {0, 1, 55, 100}) {
            if (whole - sub < 0)
                continue;
            q88 lhs{whole};
            lhs -= q88{sub};
            ASSERT_EQ(rational{lhs}, rational{whole - sub});
        }

    // fractional raws on the same grid
    q88 frac_lhs{rational{771, 256}};    // 3 + 3/256
    frac_lhs -= q88{rational{515, 256}}; // 2 + 3/256
    ASSERT_EQ(rational{frac_lhs}, rational{1});

    // offset index grid (negative Lower): bias = Lower/Notch = -8.
    using off = inside<{{-2, 2}, per<4>}, round_toward_zero>;
    off offset_lhs{rational{3, 4}};
    offset_lhs -= off{rational{-1, 2}}; // 3/4 − (−1/2) = 5/4
    ASSERT_EQ(rational{offset_lhs}, (rational{5, 4}));

    // mixed storages on the same unit notch: [1000,2000] stores an unsigned
    // index (Lower-relative raw), [-3000,3000] stores the value directly.
    using hi   = inside<{1000, 2000}, round_toward_zero>;
    using lo   = inside<{0, 500}, round_toward_zero>;
    using wide = inside<{-3000, 3000}, round_toward_zero>;
    hi index_lhs{1500}; // index-raw lhs, value-raw rhs
    index_lhs -= lo{300};
    ASSERT_EQ(rational{index_lhs}, rational{1200});
    wide value_lhs{1500};  // value-raw lhs, index-raw rhs:
    value_lhs -= hi{1200}; // bias = lower_of<R>/Notch = 1000
    ASSERT_EQ(rational{value_lhs}, rational{300});
}

// compound -=: policy tail at the raw edges
TEST(CompoundAssignTest, compound_policy_tail_at_the_raw_edges) {
    using q_clamp = inside<{{0, 255}, per<256>}, clamp | round_toward_zero>;
    q_clamp c{1};
    c -= q_clamp{100}; // 1 − 100 < 0 -> clamps to Lower
    ASSERT_EQ(rational{c}, rational{0});

    using q_wrap = inside<{{0, 3}, per<4>}, wrap | round_toward_zero>;
    q_wrap w{0};
    w -= q_wrap{rational{1, 4}}; // 0 − 1/4 wraps to Upper − ... = 3
    ASSERT_EQ(rational{w}, rational{3});

    using q_checked = inside<{{0, 255}, per<256>}, checked>;
    q_checked reported{1};
    ASSERT_THROW((void)(([&] { reported -= q_checked{100}; }())), beman::inside::inside_error);

    // in-range edge stays exact under every policy
    q_clamp exact_edge{255};
    exact_edge -= q_clamp{255};
    ASSERT_EQ(rational{exact_edge}, rational{0});
}

// compound -=: non-fast storages still route through += (-rhs)
TEST(CompoundAssignTest, compound_non_fast_storages_still_route_through_plus_rhs) {
    // rational raw falls back and stays exact
    using ex = inside<{{0, 4}, per<3>}, round_nearest>;
    ex exact_lhs{rational{7, 3}};
    exact_lhs -= ex{rational{2, 3}};
    ASSERT_EQ(rational{exact_lhs}, (rational{5, 3}));

    // a signed dyadic grid
    using rl = inside<{{-4, 4}, per<256>}, round_nearest>;
    rl dy_lhs{rational{3, 2}};
    dy_lhs -= rl{rational{1, 4}};
    ASSERT_EQ(rational{dy_lhs}, (rational{5, 4}));

    // cross-notch operands take the binary route
    using tenths   = inside<{{0, 10}, per<10>}, round_nearest>;
    using quarters = inside<{{0, 10}, per<4>}, round_nearest>;
    tenths cross{rational{5, 2}};      // 2.5 on the 1/10 grid
    cross -= quarters{rational{1, 2}}; // 2.0 — exact on both grids
    ASSERT_EQ(rational{cross}, rational{2});
}
} // namespace compound_assign_tests

// x += r and x -= r with a rational r: on a Q-format grid (notch 1/K) a
// whole number of notches adds to the raw directly, without the exact sum.
// That count needs no division where K is a power of two. Whatever path a
// value takes, the result is the exact sum rational{x} ± r stored by x's
// policy: the same value, or the same error. Checked here for every value
// of small grids (K a power of two, 10, 12), every policy kind, and
// denominators that do and do not divide K, including unreduced ones.
namespace compound_rational_tests {
using namespace beman::inside;
using namespace beman::inside::detail;

namespace {
template <class T, class Fn>
std::expected<T, errc> outcome(Fn&& f) {
    try {
        return f();
    } catch (const inside_error& e) {
        return std::unexpected{e.Code};
    }
}

template <class T>
T added(T y, rational r) {
    y += r;
    return y;
}
template <class T>
T subtracted(T y, rational r) {
    y -= r;
    return y;
}

template <class T>
void expect_compound_matches_exact_sum() {
    const imax dens[] = {1, 2, 3, 4, 5, 8, 10, 12, 16, 20, 24, 64, 256, 512, 1000};
    for (const T x : values<T>)
        for (const imax d : dens)
            for (imax n = -3 * d; n <= 3 * d; n += (d > 24 ? d / 8 : 1)) {
                const rational r{n, d};
                ASSERT_EQ(outcome<T>([&] { return added(x, r); }), outcome<T>([&] { return T{*(rational{x} + r)}; }))
                    << "raw " << x.raw() << " + " << n << "/" << d;
                ASSERT_EQ(outcome<T>([&] { return subtracted(x, r); }),
                          outcome<T>([&] { return T{*(rational{x} - r)}; }))
                    << "raw " << x.raw() << " - " << n << "/" << d;
            }
    // A hand-built rational that is not in lowest terms: 2/512 is 1/256.
    for (const T x : values<T>)
        ASSERT_EQ(outcome<T>([&] { return added(x, rational_from_parts(2, 512)); }),
                  outcome<T>([&] { return T{*(rational{x} + rational{1, 256})}; }))
            << "raw " << x.raw();
}

template <grid G>
void expect_all_policies() {
    expect_compound_matches_exact_sum<inside<G>>();
    expect_compound_matches_exact_sum<inside<G, clamp>>();
    expect_compound_matches_exact_sum<inside<G, wrap>>();
    expect_compound_matches_exact_sum<inside<G, round_nearest>>();
    expect_compound_matches_exact_sum<inside<G, clamp | round_floor>>();
}
} // namespace

TEST(CompoundRationalTest, binary_q_format) {
    BEMAN_INSIDE_RELEASE_ONLY();
    expect_all_policies<grid{{-2, 2}, per<16>}>();
}
TEST(CompoundRationalTest, binary_q_format_unsigned) {
    BEMAN_INSIDE_RELEASE_ONLY();
    expect_all_policies<grid{{0, 0.25_r}, per<256>}>();
}
TEST(CompoundRationalTest, decimal_q_format) {
    BEMAN_INSIDE_RELEASE_ONLY();
    expect_all_policies<grid{{-3, 3}, per<10>}>();
}
TEST(CompoundRationalTest, duodecimal_q_format) {
    BEMAN_INSIDE_RELEASE_ONLY();
    expect_all_policies<grid{{0, 5}, per<12>}>();
}
} // namespace compound_rational_tests

// Pinned defects: each case failed before its fix.
namespace pinned_tests {
using namespace beman::inside;
using namespace beman::inside::detail;

namespace fpk = beman::inside::math::detail::fp;
namespace d {
// The full kernels' values.
double fp_cbrt(double x) {
    double b;
    return fpk::pow_k<fpk::full_bits>::cbrt(x, b);
}
double fp_atan2(double y, double x) {
    double b;
    return fpk::atan_k<fpk::full_bits>::atan2(y, x, b);
}
} // namespace d

// Integer raw mapping into a full-width int64 target: the old map_raw added
// trunc(Offset) (up to 2^63) before raw_lo<L>, overflowing on the way.
TEST(AssignmentTest, raw_map_into_a_full_int64_grid) {
    constexpr std::int64_t i64_min = INT64_MIN;
    constexpr std::int64_t i64_max = INT64_MAX;
    using full                     = inside<{i64_min, i64_max}>;
    static_assert([] {
        int64 a = inside<{0, 100}>{5};
        full  b = inside<{0, 100}>{7};
        full  c = inside<{i64_min, 0}>{i64_min};
        return a == 5 && b == 7 && c == i64_min;
    }());
    static_assert([] {
        inside<{-10, 10}, clamp> lo = full{i64_min};
        inside<{-10, 10}, clamp> hi = full{i64_max};
        return lo == -10 && hi == 10;
    }());
}

// Scalar stores into a full-width int64 grid: the Q-format guard took the
// ceiling of a 2^63 bound in imax, and the range check negated −2^63.
TEST(AssignmentTest, scalar_store_into_a_full_int64_grid) {
    constexpr std::int64_t i64_min = INT64_MIN;
    constexpr std::int64_t i64_max = INT64_MAX;
    using full                     = inside<{i64_min, i64_max}>;
    static_assert([] {
        int64 a{2.0};
        int64 b{rational{-7}};
        full  c{rational{-3}};
        c += rational{1};
        full d{-9.0e18};
        return a == 2 && b == -7 && c == -2 && d == -9'000'000'000'000'000'000;
    }());
}

// A double finer than 2^-62 lost its low bits (truncated toward zero) on the
// way to rational: ceil stored 0, a strict grid stored 0 without
// rounding_error, and comparisons saw the truncated value.
TEST(AssignmentTest, a_tiny_double_is_taken_exactly) {
    using cent_ceil = inside<{{0, 1}, per<100>}, round_ceil>;
    static_assert(cent_ceil{1e-20} == rational{1, 100});
    static_assert(inside<{{0, 1}, per<100000>}, round_ceil>{1e-5} == rational{2, 100000}); // 1e-5 is above 10^-5
    static_assert(inside<{{0, 1}, per<100>}, round_floor>{1e-20} == 0);
    using cent = inside<{{0, 1}, per<100>}>;
    EXPECT_FALSE(cent::try_make(1e-20).has_value());
    EXPECT_FALSE((inside<{{0, 1}, per<100>}, round_floor>::try_make(-1e-20).has_value())); // floors to -0.01
    EXPECT_EQ((inside<{{0, 1}, per<100>}, round_ceil>{-1e-20}), 0);
    EXPECT_FALSE((inside<grid{interval{0, 1}, max_denominator{1000}}>::try_make(1e-20).has_value())); // off the grid
    constexpr inside<grid{interval{0, 1}, max_denominator{1000}}> zero{0};
    static_assert(zero < just<1e-20_g> && zero != just<1e-20_g>);
}

// A bounded continuous grid rounds an off-bound value onto its bound under
// a rounding policy (0.1 as a double is not 1/10); strict still rejects.
TEST(AssignmentTest, bounded_continuous_rounds_onto_the_bound) {
    using N = inside<grid{interval{0, 1}, max_denominator{1000}}, round_nearest>;
    static_assert(N{0.1} == rational{1, 10});
    using F2 = inside<grid{interval{0, 1}, max_denominator{2}}, round_floor>;
    using C2 = inside<grid{interval{0, 1}, max_denominator{2}}, round_ceil>;
    static_assert(F2{rational{1, 3}} == 0 && C2{rational{1, 3}} == rational{1, 2});
    static_assert(inside<grid{interval{0, 1}, max_denominator{7}}, round_nearest>{rational{1000, 2001}} ==
                  rational{1, 2});
    static_assert(inside<grid{interval{-1, 1}, max_denominator{3}}, round_floor>{rational{-1, 4}} == rational{-1, 3});
    static_assert(inside<grid{interval{-1, 1}, max_denominator{3}}, round_nearest>{rational{-3, 10}} ==
                  rational{-1, 3});
    EXPECT_FALSE((inside<grid{interval{0, 1}, max_denominator{2}}>::try_make(rational{1, 3}).has_value()));
}

// A full-int64 source into a fine or offset grid: the raw map's Offset
// (Lower_R − Lower_L)/Notch_L passed the 64-bit rational at compile time.
TEST(AssignmentTest, full_int64_into_a_fine_grid) {
    constexpr std::int64_t i64_min = INT64_MIN;
    constexpr std::int64_t i64_max = INT64_MAX;
    using full                     = inside<{i64_min, i64_max}>;
    using quarter                  = inside<{{-2, 3}, per<4>}>;
    using halves                   = inside<{{0.5, 10.5}, 1}>; // Lower off the notch's multiples
    static_assert([] {
        auto a = quarter::try_make(full{2});
        auto b = inside<grid_of<halves>, round_nearest>::try_make(full{3}); // a tie: away from zero
        auto c = quarter::try_make(full{i64_min});
        return *a == 2_ins && *b == 3.5_ins && c.error() == errc::overflow;
    }());
    auto w = inside<grid_of<halves>, wrap | round_floor>::try_make(full{i64_max});
    EXPECT_EQ(to_string(*w), "6.5"); // (2^63 − 1.5) mod 11 from 0.5
}

// A lattice whose values pass the 64-bit rational between limits that fit
// ({{−1.2e19, 1.2e19}, 1.5}): reading such a value threw.
TEST(AssignmentTest, lattice_past_the_64_bit_rational) {
    using neg    = inside<{{-3.5, -0.5}, 1}>; // ditto, below 0
    using thirds = inside<{{-(3ll << 60), 3ll << 60}, 3}>;
    const auto p = neg{rational{-7, 2}} * thirds{(3ll << 60) - 3};
    EXPECT_EQ(to_string(p), "-12105675798371893237.5");
    using clamped = inside<grid_of<neg>, clamp | round_toward_zero>;
    using wrapped = inside<grid_of<neg>, wrap | round_floor>;
    EXPECT_EQ(*clamped::try_make(p), -3.5_ins);
    EXPECT_EQ(to_string(*wrapped::try_make(p)), "-1.5");
}

// A double into a grid whose limit no double holds (INT64_MAX): the range
// test built the double as a grid number, whose runtime ops stop at 64 bits
// under C++26 — 2^-20 needed a 2^72 denominator on the way and threw.
TEST(AssignmentTest, small_double_into_a_full_int64_grid) {
    constexpr std::int64_t i64_min = INT64_MIN;
    constexpr std::int64_t i64_max = INT64_MAX;
    using F                        = inside<{i64_min, i64_max}, round_half_even>;
    EXPECT_EQ(*F::try_make(0x1p-20), 0_ins);
    EXPECT_EQ(*F::try_make(1e-4), 0_ins);
    EXPECT_EQ(*F::try_make(-2.5), -2_ins);
    using C = inside<{i64_min, i64_max}, clamp | round_ceil>;
    EXPECT_EQ(*C::try_make(0x1p-70), 1_ins);
}

// A rational just outside the grid with a denominator near 2^62: rounding
// it onto the lattice threw bad_expected_access (v·64 passed the rational).
TEST(AssignmentTest, fine_rational_rounds_onto_the_grid) {
    using A        = inside<{{0, 1}, per<64>}, round_nearest>;
    const imax d   = (1ll << 62) + 1;
    const auto top = rational{static_cast<umax>(d + 1), d}; // 1 + 1/d
    EXPECT_EQ(*A::try_make(top), 1_ins);
    EXPECT_EQ(*A::try_make(rational{1, -d}), 0_ins);
    EXPECT_FALSE(conversion_overflows<A>(top));
    EXPECT_TRUE(conversion_rounds<A>(top));
}

// += of a rational on a large Q-format grid: the raw add passed imax (UB).
TEST(AssignmentTest, add_rational_on_a_large_q_format_grid) {
    inside<{{0, 1ll << 60}, per<4>}, clamp> c{rational{1ll << 60}};
    c += rational{1ll << 60};
    EXPECT_EQ(to_string(c), "1152921504606846976");
    inside<{{0, 1ll << 60}, per<4>}, wrap> w{rational{1ll << 60}};
    w += rational{1ll << 60};
    EXPECT_EQ(to_string(w), "1152921504606846975.75");
}

// An inside stored into a point grid skipped the range check: the raw map
// sends every raw onto the point's one raw, so 4 and 3.5 both "fit" {3}.
TEST(AssignmentTest, point_grid_checks_the_value) {
    using three  = inside<{3}>;              // a point grid
    using halves = inside<{{0.5, 10.5}, 1}>; // Lower off the notch's multiples
    using small  = inside<{-7, 5}>;
    static_assert(three::try_make(small{4}).error() == errc::overflow);
    static_assert(three::try_make(halves{rational{7, 2}}).error() == errc::overflow);
    static_assert(*three::try_make(small{3}) == 3_ins);
    using clamped = inside<{3}, clamp>;
    static_assert(*clamped::try_make(small{4}) == 3_ins);
    three x{};
    EXPECT_THROW(x += small{3}, inside_error); // 6 is not 3
}

// A source the exact path stores (a fine or wide value) into a point grid
// divided by the point's zero notch inside noexcept code: std::terminate.
// wrap onto a point (no period) terminated the same way since the exact wrap
// fold; it reports overflow, as before.
TEST(AssignmentTest, point_grid_on_the_exact_path) {
    using three = inside<{3}>; // a point grid
    using fine  = inside<{{0, 1}, per<(1ll << 62)>}>;
    three x{};
    EXPECT_THROW((x += fine{rational{1, 2}}), inside_error);
    EXPECT_EQ(three::try_make(fine{rational{1, 2}} + 2_ins).error(), errc::overflow);
    using wrapped = inside<{3}, wrap>;
    EXPECT_EQ(wrapped::try_make(5).error(), errc::overflow);
    EXPECT_EQ(wrapped::try_make(3.5).error(), errc::overflow);
    EXPECT_EQ(*wrapped::try_make(3), 3_ins);
}

// The exact store (mul_into, div_into, wide sums) tested the notch before the
// range: an off-notch value past a limit reported rounding_error, and clamp
// did not clamp it — the builtin stores range-check the value first.
TEST(AssignmentTest, exact_store_checks_range_before_notch) {
    using digit   = inside<{0, 9}>;
    using fourths = inside<{{-2, 3}, per<4>}>;
    const fourths a{rational{-1, 4}}, b{rational{11, 4}}; // −11/16
    using clamped = inside<{0, 9}, clamp>;
    EXPECT_EQ(mul_into<clamped>(a, b), 0_ins);
    try {
        (void)mul_into<digit>(a, b);
        ADD_FAILURE() << "no error";
    } catch (const inside_error& e) {
        EXPECT_EQ(e.Code, errc::overflow);
    }
    try {
        (void)mul_into<digit>(b, b); // 121/16: in range, off the notch
        ADD_FAILURE() << "no error";
    } catch (const inside_error& e) {
        EXPECT_EQ(e.Code, errc::rounding_error);
    }
}

// A rounding policy rounds before the range check (policies.md), as the
// lattices do: 9.5 floors onto 9. A bounded continuous grid checked the
// range first and reported overflow.
TEST(AssignmentTest, bounded_grid_rounds_before_the_range_check) {
    constexpr grid sevenths{interval{0, 1}, max_denominator{7}};
    constexpr grid wholes{interval{0, 9}, max_denominator{1}};
    static_assert(*inside<wholes, round_floor>::try_make(rational{19, 2}) == 9);
    static_assert(*inside<sevenths, round_nearest>::try_make(rational{22, 21}) == 1);
    static_assert(*inside<sevenths, round_ceil>::try_make(rational{-1, 50}) == 0);
    static_assert(inside<sevenths, round_ceil>::try_make(rational{22, 21}).error() == errc::overflow);
    EXPECT_EQ((*inside<wholes, round_floor>::try_make(9.5)), 9);
    inside<sevenths, round_nearest> x{rational{5, 7}};
    x += rational{1, 3}; // 22/21
    EXPECT_EQ(x, 1);
}

// A value past the 64-bit rational (a double finer than 2^-62) lying within
// a bounded grid's limits reported overflow: it rounds onto the bound, or is
// off the grid (rounding_error).
TEST(AssignmentTest, bounded_grid_rounds_a_fine_double) {
    constexpr grid sevenths{interval{0, 1}, max_denominator{7}};
    EXPECT_EQ((*inside<sevenths, round_nearest>::try_make(5.1e-12)), 0);
    EXPECT_EQ((*inside<sevenths, round_ceil>::try_make(5.1e-12)), rational(1, 7));
    EXPECT_EQ(inside<sevenths>::try_make(5.1e-12).error(), errc::rounding_error);
    EXPECT_EQ(inside<sevenths>::try_make(1e-300).error(), errc::rounding_error);
}

// A failed store of a double finer than 2^-62 through errc& or on_error
// replaced the destination with a value-initialised raw (0/0 on a
// continuous grid) instead of leaving it intact.
TEST(AssignmentTest, failed_fine_double_store_keeps_the_value) {
    using C = inside<{{0, 1}, max_denominator{1000}}>;
    C    z{rational{1, 2}};
    errc ec{};
    z.policy(ec) = 1e-30; // finer than the bound: off the grid
    EXPECT_EQ(ec, errc::rounding_error);
    EXPECT_EQ(z, rational(1, 2));
    C    w{rational{1, 2}};
    errc cb{};
    w.on_error([&](auto&, errc c, const char*) { cb = c; }) = 1e-30;
    EXPECT_EQ(cb, errc::rounding_error);
    EXPECT_EQ(w, rational(1, 2));
}

// A double finer than 2^-62 stored into a default-constructed inside copied
// its uninitialized raw (not a constant expression; indeterminate at run time).
TEST(AssignmentTest, fine_double_into_an_uninitialized_inside) {
    constexpr grid sevenths{interval{0, 1}, max_denominator{7}};
    using M          = inside<sevenths, round_nearest>;
    constexpr auto f = [] {
        M x;
        x = 0x1p-70;
        return x;
    };
    static_assert(f() == 0);
    M x;
    x = 0x1p-70;
    EXPECT_EQ(x, 0);
    // A failure still reaches on_error with the destination's value; a clamp
    // policy saturates instead (no failure).
    using U = inside<{{0, 1}, max_denominator{1000}}>;
    U    u{rational{1, 2}};
    errc cb{};
    u.on_error([&](auto& v, errc c, const char*) {
        cb = c;
        EXPECT_EQ(v, rational(1, 2));
    }) = 1e-30;
    EXPECT_EQ(cb, errc::rounding_error);
    EXPECT_EQ(u, rational(1, 2));
    using C = inside<{{1, 2}, max_denominator{1000}}, clamp>;
    C w{rational{3, 2}};
    cb                                                      = {};
    w.on_error([&](auto&, errc c, const char*) { cb = c; }) = 1e-30;
    EXPECT_EQ(cb, errc{});
    EXPECT_EQ(w, 1);
}

// A double into uint64 (and any grid whose upper limit rounds to 2^64 as a
// double) did not compile: the range test built a rational of 2^64.
TEST(AssignmentTest, double_into_a_grid_reaching_2_pow_64) {
    EXPECT_EQ(uint64{3.0}, 3);
    EXPECT_EQ(uint64::try_make(1e19)->to<std::uint64_t>(), 10000000000000000000u);
    EXPECT_EQ(uint64::try_make(0x1p64).error(), errc::overflow);
    EXPECT_EQ(uint64::try_make(-1.0).error(), errc::overflow);
    const inside<{0, UINT64_MAX}, clamp> c{1e30};
    EXPECT_EQ(c.to<std::uint64_t>(), UINT64_MAX);
}

// A long double was narrowed to double: a store lost its low bits silently
// and the predicates disagreed with the range check.
TEST(AssignmentTest, long_double_is_taken_exactly) {
    if constexpr (sizeof(long double) > sizeof(double)) {
        using B             = inside<{0, 10}>;
        const long double x = 1.0L + 0x1p-60L;
        EXPECT_EQ(B::try_make(x).error(), errc::rounding_error);
        EXPECT_TRUE(conversion_rounds<B>(x));
        EXPECT_TRUE(conversion_overflows<B>(10.0L + 0x1p-60L));
        EXPECT_EQ(B::try_make(10.0L + 0x1p-60L).error(), errc::overflow);
        EXPECT_EQ((inside<{0, 100}, round_ceil>{x}), 2);
        EXPECT_EQ((inside<{0, 100}, round_floor>{1.0L - 0x1p-60L}), 0);
        EXPECT_EQ((inside<{0, 100}, wrap>{0x1p60L + 3.0L}), static_cast<int>(((1ull << 60) + 3) % 101));
#if LDBL_MAX_EXP > DBL_MAX_EXP // past the double range (not where long double is a double)
        EXPECT_EQ((inside<{0, 100}, clamp>{1e4000L}), 100);
        EXPECT_EQ((inside<{0, 100}, wrap>::try_make(1e4000L).error()), errc::overflow);
#endif
    }
    EXPECT_EQ((inside<{0, 100}>{2.0L}), 2);
}

// The raw += / -= fast paths added R's raw and the bias before the delta
// was formed, overflowing their work type.
TEST(AssignmentTest, compound_add_raw_delta) {
    using I64             = std::int64_t;
    constexpr I64 I64_MAX = std::numeric_limits<I64>::max();
    constexpr I64 I64_MIN = std::numeric_limits<I64>::min();
    static_assert([] {
        using L = inside<grid{{0, I64_MAX - 1}, 2}, wrap>;
        using R = inside<grid{{I64_MIN, I64_MAX - 1}, 2}>;
        L x     = I64_MAX - 1;
        x += R{I64_MAX - 1};
        return x == I64_MAX - 3;
    }());
    constexpr std::uint64_t TOP = (std::uint64_t{1} << 63) + (std::uint64_t{1} << 62);
    using L2                    = inside<grid{{0, (I64{1} << 62) - 2}, 2}, wrap>;
    using R2                    = inside<grid{{rational{I64_MIN}, rational{TOP}}, 2}>;
    L2 z                        = 0;
    z -= R2{rational{TOP}};
    EXPECT_EQ(z, 0);
}

// A point source into a grid whose raw map is neither integer nor a 64-bit
// multiply-add (a 2^-62 notch, the full uint64 range) did not compile.
TEST(AssignmentTest, point_into_a_fine_or_full_width_grid) {
    using I64 = std::int64_t;
    using F   = inside<{{-1, 1}, per<(I64{1} << 62)>}, round_ceil>;
    const F f{just<rational{1, 2}>};
    EXPECT_EQ(f, rational(1, 2));
    const uint64 q{5_ins};
    EXPECT_EQ(q.to<std::uint64_t>(), 5u);
}

//---------------------------------------------------------------------------
// 2026-07: fp-derived rational store on a snap grid with |Lower| ≫ 1. The
// cold store path forms (rhs − Lower)/Notch exactly; with a full-mantissa
// double source (den 2^54) that once dereferenced an empty result (terminate
// through the noexcept math engines). Now: offsets that fit 64 bits go
// through the rescued rational path, and offsets beyond it go through the
// exact wide index (exact_index) — both land on the correctly rounded slot.
//---------------------------------------------------------------------------
TEST(AssignmentTest, double_on_a_wide_rounding_grid_takes_the_exact_path) {
    using wide = inside<{{-1024, 1024}, per<16384>}, round_nearest>;

    {
        SCOPED_TRACE("negative value: offset fits after the 128-bit add rescue");
        wide slot{};
        slot = rational{umax{9006646171630191}, imax{-18014398509481984}}; // ≈ -0.4999693
        // ·16384 = -8191.49692… → round_nearest → -8191/16384
        ASSERT_EQ(rational{slot}, (rational{umax{8191}, imax{-16384}}));
    }

    {
        SCOPED_TRACE("positive value: exact offset needs > 64 bits → exact wide store");
        wide slot{};
        slot = rational{umax{9006646171630191}, imax{18014398509481984}}; // ≈ +0.4999693
        // (1024 + v)·16384 = 16785407.49692… → round_nearest → slot 16785407
        // → value 8191/16384 (0.49993896…, the nearest grid point)
        ASSERT_EQ(rational{slot}, (rational{umax{8191}, imax{16384}}));
    }

    // (No constexpr section: at constant evaluation the transient rational
    // overflow surfaces as the intentional constexpr_error build diagnostic
    // before the wide fallback can engage — the wide path is runtime-only in
    // practice, though itself constexpr-capable.)

    {
        SCOPED_TRACE("strict policy off-notch in the wide regime → rounding_error");
        using strict = inside<{{-1024, 1024}, per<16384>}>; // checked, no round flag
        strict slot{};
        try {
            slot = rational{umax{9006646171630191}, imax{18014398509481984}};
            FAIL() << "expected the default handler to throw";
        } catch (const inside_error& e) {
            ASSERT_EQ(e.Code, errc::rounding_error);
        }
    }
}

TEST(AssignmentTest, dyadic_source_into_integer_grid_reads_the_value) {
    using RealWide = beman::inside::inside<{{-8, 8}, beman::inside::per<4>}>;
    using Int10    = beman::inside::inside<{0, 10}>;
    using namespace beman::inside;
    const RealWide half = RealWide{2.5};
    EXPECT_EQ(detail::to_value(Int10{half, clamp | round_nearest}), 3);
    Int10 i{0};
    i.policy<round_nearest>() = half;
    EXPECT_EQ(detail::to_value(i), 3);
    Int10 j = (half * just<2>).policy<round_toward_zero>();
    EXPECT_EQ(detail::to_value(j), 5);
}

TEST(AssignmentTest, try_make_reports_on_checked_dyadic_target) {
    using RealWide = beman::inside::inside<{{-8, 8}, beman::inside::per<4>}>;
    using namespace beman::inside;
    using Checked = inside<{{0, 4}, per<4>}, checked>;
    EXPECT_EQ(Checked::try_make(RealWide{7.5}).error(), errc::overflow);
}

//---------------------------------------------------------------------------
// A 64-bit unsigned source above INT64_MAX must not be read as negative.
//---------------------------------------------------------------------------
TEST(AssignmentTest, uint64_source_above_int64_max) {
    static constexpr unsigned long long big = ~0ull;

    constexpr auto checked_ok = [] {
        errc                   ec{};
        inside<{{-10, 10}, 1}> x{3};
        x.policy(ec) = big;
        return std::pair{x.as<int>(), ec};
    }();
    static_assert(checked_ok.first == 3 && checked_ok.second == errc::overflow);

    static_assert(inside<{{0, 10}, 1}>{1ull << 63, clamp}.as<int>() == 10);
    static_assert(inside<{{-10, 10}, 1}>{big, clamp}.as<int>() == 10);

    // (2^64 − 1 − Lower) mod 21 + Lower
    static_assert(inside<{{-10, 10}, 1}>{big, wrap}.as<long>() == static_cast<long>((big % 21 + 10) % 21) - 10);
    static_assert(inside<{{5, 9}, 1}>{big, wrap}.as<long>() == static_cast<long>((big - 5) % 5) + 5);
    static_assert(inside<{{5, 9}, 1}>{-7, wrap}.as<int>() == 8);

    // The on_clamp overshoot is an inside over the source type's limits minus
    // the target's, so it is exact (it saturated at imax before).
    inside<{{-10, 10}, 1}> c{0};
    bool                   exact                                = false;
    c.on_clamp([&](auto&, auto o) { exact = (o == big - 10); }) = big;
    EXPECT_EQ(c.as<int>(), 10);
    EXPECT_TRUE(exact);
    c.on_clamp([&](auto&, auto o) { exact = (o == std::numeric_limits<imax>::max() - 10); }) =
        std::numeric_limits<imax>::max();
    EXPECT_TRUE(exact);
}

//---------------------------------------------------------------------------
// A double of magnitude 2^64 or more has no 64-bit rational form; it used to
// convert to a wrapped value (2^64 → 0) and be stored silently.
//---------------------------------------------------------------------------
TEST(AssignmentTest, huge_double_source) {
    constexpr auto rounded = [](double v) {
        errc                                 ec{};
        inside<{{0, 100}, 1}, round_nearest> x{5};
        x.policy(ec) = v;
        return std::pair{x.as<int>(), ec};
    };
    static_assert(rounded(0x1p64).first == 5 && rounded(0x1p64).second == errc::overflow);
    static_assert(rounded(1e300).second == errc::overflow);
    static_assert(rounded(-1e300).second == errc::overflow);

    constexpr auto wrap_of = [](double v) {
        errc                      ec{};
        inside<{{0, 9}, 1}, wrap> x{5};
        x.policy(ec) = v;
        return std::pair{x.as<long>(), ec};
    };
    // wrap folds such a value exactly: 2^64 mod 10 (it reported overflow
    // before the exact fold).
    static_assert(wrap_of(0x1p64).first == 6 && wrap_of(0x1p64).second == errc{});
    static_assert(wrap_of(0x1p62).first == static_cast<long>((1ull << 62) % 10));

    using third = inside<{{0, rational{1, 3}}, rational{1, 3}}, clamp>;
    static_assert(rational{third{1e300}} == rational{1, 3});
    static_assert(rational{third{-1e300}} == 0);

    constexpr inside<{{0, 10}, per<4>}> x{1};
    static_assert(x < just<18446744073709551616_g> && x > -just<18446744073709551616_g> && x < just<1e300_g> &&
                  !(x == just<1e300_g>));
    static_assert(conversion_overflows<inside<{0, 10}>>(0x1p64));
    static_assert(!conversion_rounds<inside<{0, 10}>>(0x1p64));

    EXPECT_THROW((void)rational{0x1p64}, inside_error);
    EXPECT_EQ(rational{0x1p63}, rational{1ull << 63});
}

// += / -= / ++ on the full int64 range: the raw add must not overflow imax.
TEST(AssignmentTest, full_int64_grid_compound_ops) {
    constexpr umax kUM   = std::numeric_limits<umax>::max();
    constexpr imax kIMin = std::numeric_limits<imax>::min();
    constexpr imax kIMax = std::numeric_limits<imax>::max();
    using I64            = inside<{{kIMin, kIMax}, 1}>;
    using I64W           = inside<{{kIMin, kIMax}, 1}, wrap>;
    using I64C           = inside<{{kIMin, kIMax}, 1}, clamp>;
    constexpr auto a     = [] {
        I64W x{kIMax};
        ++x;
        return x.raw();
    }();
    constexpr auto b = [] {
        I64W x{kIMin};
        --x;
        return x.raw();
    }();
    constexpr auto c = [] {
        I64C x{kIMax};
        ++x;
        return x.raw();
    }();
    constexpr auto d = [] {
        I64W x{kIMax};
        x += I64{5};
        return x.raw();
    }();
    constexpr auto e = [] {
        I64W x{kIMin};
        x -= I64{1};
        return x.raw();
    }();
    static_assert(a == kIMin && b == kIMax && c == kIMax && d == kIMin + 4 && e == kIMax);
    static_assert(I64W{kUM} == -1);

    I64 y{kIMax};
    EXPECT_THROW(++y, inside_error);
    EXPECT_EQ(y, kIMax);
}

// A value-raw source with Lower ≠ 0 mapped onto a coarser notch: the affine
// map read the raw (the value) as a 0-based offset.
TEST(AssignmentTest, value_raw_source_affine_mapping) {
    using R = inside<{-5, 5}>; // int8 value raw
    using L = inside<{{-6, 6}, 3}, round_nearest>;
    static_assert(detail::integer_value_storage<R>);
    static_assert(rational{L{R{-5}}} == -6);
    static_assert(rational{L{R{-4}}} == -3);
    static_assert(rational{L{R{4}}} == 3);
    static_assert(rational{L{R{5}}} == 6);
    using LX = inside<{{-6, 6}, 3}, round_nearest>;
    static_assert(rational{LX{R{-5}}} == -6);

    L l{0};
    l = R{-2};
    EXPECT_EQ(rational{l}, -3);
}

// An exact value stores by its own rounding, not its double's: the double
// nearest to it may sit on a rounding boundary the exact value is not on
// (4096/961 · 2^48 is …523.388, its double …523.5, a tie).
TEST(AssignmentTest, store_rounds_the_exact_value_not_its_double) {
    using f48 = inside<{{-8, 8}, per<(std::uint64_t{1} << 48)>}, round_nearest>;
    const rational want{1199710202504523ull, std::int64_t{1} << 48};
    EXPECT_EQ((rational{f48{rational{4096, 961}}}), want);
    f48 a{0};
    a = rational{4096, 961};
    EXPECT_EQ(rational{a}, want);
    // An inside source whose values are not doubles exactly takes the same route.
    using thirds = inside<{{0, 8}, rational{1, 961}}>;
    a            = thirds{rational{4096, 961}};
    EXPECT_EQ(rational{a}, want);

    // Directed modes: 1 + 2^-60 is above the grid point 1.0, its double is 1.0.
    using up   = inside<{{0, 2}, per<1024>}, round_ceil>;
    using down = inside<{{0, 2}, per<1024>}, round_floor>;
    const rational above{(std::uint64_t{1} << 60) + 1, std::int64_t{1} << 60};
    const rational below{(std::uint64_t{1} << 60) - 1, std::int64_t{1} << 60};
    EXPECT_EQ(rational{up{above}}, (rational{1025, 1024}));
    EXPECT_EQ(rational{down{below}}, (rational{1023, 1024}));
    EXPECT_EQ(rational{up{rational{1}}}, rational{1});
}

//---------------------------------------------------------------------------
// inside::operator/= and operator%= (inside/detail/core.hpp) with an inside rhs
//---------------------------------------------------------------------------
// compound /= and %= with a snap inside rhs (runtime)
TEST(AssignmentTest, compound_and_policy_handle_with_a_rounding_inside) {
    // Integer division/modulo now flow through the insidable path; `snap`
    // gives the same C++ trunc-toward-zero / dividend-signed-remainder semantics
    // the old raw-int compound assigns had.
    using sb = inside<{-100, 100}, round_toward_zero>;

    sb a{20};
    a /= sb{3}; // integer division, truncates toward zero
    ASSERT_EQ(a, 6);

    sb n{-20};
    n /= sb{3};
    ASSERT_EQ(n, -6);

    sb b{20};
    b %= sb{7};
    ASSERT_EQ(b, 6);

    sb m{-20};
    m %= sb{7};
    ASSERT_EQ(m, -6); // C++ remainder keeps the dividend's sign
}

//---------------------------------------------------------------------------
// a double store out of range under a reporting policy (the clamp/wrap arms
// are covered elsewhere; the range_fail arm was not).
//---------------------------------------------------------------------------
// double store out of range: checked policy
TEST(AssignmentTest, double_store_out_of_range_checked_policy) {
    using rbc = inside<{{-1, 1}, per<1024>}, checked>;
    ASSERT_THROW((void)((rbc{5.0})), beman::inside::inside_error); // out of range -> report (throws)
}

//---------------------------------------------------------------------------
// an inside -> inside store (assignment.hpp) on the rational
// (non-integer-mapping) path where the target offset is NEGATIVE, so
// raw_from_offset() is reached through the negated branch.
//---------------------------------------------------------------------------
// cross-grid conversion of a negative off-notch value
TEST(AssignmentTest, cross_grid_conversion_of_a_negative_off_notch_value) {
    // notch 1/2 source -> notch 1/3 target: Factor = 3/2 (non-integer mapping),
    // so the rational store path runs; the negative value drives the
    // negative-denominator branch.
    using src_t = inside<{{-4, 4}, per<2>}>;
    using dst_t = inside<{{-4, 4}, per<3>}, round_toward_zero>;

    src_t s{-1.5};
    dst_t d = s;
    // snap truncates toward ZERO in value space (matching the scalar store
    // path and div_rounded): -1.5 on the 1/3 grid → -4/3. (Was -5/3 when the old
    // path truncated the non-negative offset — i.e. toward -inf in value space.)
    ASSERT_TRUE((static_cast<rational>(d) == rational{4, -3}));

    src_t s2{1.5};
    dst_t d2 = s2; // +1.5 toward zero → 4/3 (unchanged)
    ASSERT_TRUE((static_cast<rational>(d2) == rational{4u, 3}));
}

// round_to_lattice on a fine notch over a wide range: the notch index of
// 2^61 − 1 on a 1/8 grid is 2^64 − 8, past imax. It must round in place, not
// wrap to a negative index (it returned −1 when the index was narrowed to
// imax), and a result past the rational range is errc::overflow.
TEST(AssignmentTest, round_to_lattice_keeps_notch_indices_past_imax) {
    using L       = inside<grid{{0, 1ll << 61}, per<8>}>;
    const auto v  = rational{(1ull << 61) - 1, 1};
    const auto nv = rational{(1ull << 61) - 1, -1};
    ASSERT_TRUE((*try_round_to_lattice<L, call_policy<round_half_even>>(v) == v));
    ASSERT_TRUE((*try_round_to_lattice<L, call_policy<round_floor>>(v) == v));
    ASSERT_TRUE((*try_round_to_lattice<L, call_policy<round_floor>>(nv) == nv));
    ASSERT_TRUE((*try_round_to_lattice<L, call_policy<round_ceil>>(nv) == nv));
    const auto off = rational{(1ull << 63) - 1, 4}; // a lattice point: index 2^64 − 2
    ASSERT_TRUE((*try_round_to_lattice<L, call_policy<round_nearest>>(off) == off));
    // ceil(umax) on the 1/8 lattice is umax itself, but its index 8·umax is
    // past every rational: reported, not wrapped.
    ASSERT_TRUE((try_round_to_lattice<L, call_policy<round_ceil>>(rational{~0ull, 1}).error() == errc::overflow));
}

// The unanchored branch (Lower 1/16 off the 1/8 notch's multiples): small
// values round onto {1/16 + k/8}; a far value whose index passes imax, and
// whose lattice neighbours pass the rational range, reports overflow instead
// of a wrapped index.
TEST(AssignmentTest, round_to_lattice_unanchored_rounds_and_reports_overflow) {
    using U = inside<grid{{rational{1, 16}, rational{17, 16}}, per<8>}>;
    ASSERT_TRUE((*try_round_to_lattice<U, call_policy<round_floor>>(rational{5, 1}) == rational{79, 16}));
    ASSERT_TRUE((*try_round_to_lattice<U, call_policy<round_ceil>>(rational{5, 1}) == rational{81, 16}));
    ASSERT_TRUE((*try_round_to_lattice<U, call_policy<round_nearest>>(rational{5, -1}) == rational{81, -16}));
    ASSERT_TRUE(
        (*try_round_to_lattice<U, call_policy<round_half_even>>(rational{5, 1}) == rational{81, 16})); // index 40
    ASSERT_TRUE(
        (try_round_to_lattice<U, call_policy<round_floor>>(rational{(1ull << 61) - 1, 1}).error() == errc::overflow));
}

} // namespace pinned_tests
