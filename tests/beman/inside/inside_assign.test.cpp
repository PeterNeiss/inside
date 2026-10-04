// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

#include <gtest/gtest.h>

#include <type_traits>

using namespace beman::inside;
using namespace beman::inside::detail;

// conversion between bounds with compatible grids
TEST(InsideAssignTest, conversion_between_bounds_with_compatible_grids)
{
  using f30t40 = inside<{{30, 40}, 2}>;
  using f20t50 = inside<{interval{20, 50}, 1}>;
  f30t40 smaller{34};
  f20t50 bigger;

  bigger = smaller;
  ASSERT_EQ(bigger, 34);

  // notch 2 -> notch 1 is always compatible (every step lands on integer)
  bigger = 39;
}

// rounding requires opt-in / policy<snap>() rounds (truncates toward zero on positive)
TEST(InsideAssignTest, rounding_requires_opt_in__policy_snap_rounds_truncates_toward_zero_on_positive)
{
  using f30t40 = inside<{{30, 40}, 2}>;
  using f20t50 = inside<{interval{20, 50}, 1}>;
  [[maybe_unused]] f20t50 bigger{39};
  [[maybe_unused]] f30t40 smaller;

  {
    SCOPED_TRACE("policy<snap>() rounds (truncates toward zero on positive)");
    smaller.policy<snap>() = bigger;
    ASSERT_EQ(smaller, 38);
  }

}

// rounding requires opt-in / with_snap() is the alias
TEST(InsideAssignTest, rounding_requires_opt_in__with_snap_is_the_alias)
{
  using f30t40 = inside<{{30, 40}, 2}>;
  using f20t50 = inside<{interval{20, 50}, 1}>;
  [[maybe_unused]] f20t50 bigger{39};
  [[maybe_unused]] f30t40 smaller;

  {
    SCOPED_TRACE("with_snap() is the alias");
    bigger = 30;
    smaller.with_snap() = bigger;
    ASSERT_EQ(smaller, 30);
  }

}

// rounding requires opt-in / with_snap<round_nearest>() rounds half up to grid
TEST(InsideAssignTest, rounding_requires_opt_in__with_snap_round_nearest_rounds_half_up_to_grid)
{
  using f30t40 = inside<{{30, 40}, 2}>;
  using f20t50 = inside<{interval{20, 50}, 1}>;
  [[maybe_unused]] f20t50 bigger{39};
  [[maybe_unused]] f30t40 smaller;

  {
    SCOPED_TRACE("with_snap<round_nearest>() rounds half up to grid");
    using celsius = inside<{{-40, 60}, 0.5}, round_nearest>;
    celsius room = 21.4;
    ASSERT_EQ(room, 21.5_r);

    celsius exact = 21.5;
    ASSERT_EQ(exact, 21.5_r);

    celsius truncated = 21.2;
    ASSERT_EQ(truncated, 21);

    using half = inside<{{0, 10}, 0.5}>;
    half h;
    h.with_snap<round_nearest>() = 3.3;
    ASSERT_EQ(h, 3.5_r);

    h.with_snap<round_nearest>() = 3.2;
    ASSERT_EQ(h, 3);
  }

}

// rounding requires opt-in / type-level snap
TEST(InsideAssignTest, rounding_requires_opt_in__type_level_snap)
{
  using f30t40 = inside<{{30, 40}, 2}>;
  using f20t50 = inside<{interval{20, 50}, 1}>;
  [[maybe_unused]] f20t50 bigger{39};
  [[maybe_unused]] f30t40 smaller;

  {
    SCOPED_TRACE("type-level snap");
    using n2_round = inside<{{0, 10}, 2}, snap>;
    using n1       = inside<{{0, 10}, 1}>;
    n2_round c;
    c = n1{3};
    ASSERT_EQ(c, 2);
  }

}

// rounding requires opt-in / type-level round_nearest
TEST(InsideAssignTest, rounding_requires_opt_in__type_level_round_nearest)
{
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

    c = n1{3};  ASSERT_EQ(c, 4);   // nearer 4 than 0 (snap would give 0)
    c = n1{5};  ASSERT_EQ(c, 4);   // nearer 4 than 8
    c = n1{1};  ASSERT_EQ(c, 0);   // nearer 0 than 4

    // Exact ties round up (half away from zero), per `round_nearest`.
    c = n1{2};  ASSERT_EQ(c, 4);   // 0|4 tie → 4
    c = n1{6};  ASSERT_EQ(c, 8);   // 4|8 tie → 8

    c = n1{8};  ASSERT_EQ(c, 8);   // already on a notch
  }

}

// rounding requires opt-in / compatible notches require no opt-in
TEST(InsideAssignTest, rounding_requires_opt_in__compatible_notches_require_no_opt_in)
{
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
TEST(InsideAssignTest, rounding_requires_opt_in__wide_compatible_interval_needs_no_opt_in)
{
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

// with_clamp / with_wrap per-operation
TEST(InsideAssignTest, with_clamp_with_wrap_per_operation)
{
  using u100 = inside<{0, 100}>;
  u100 x{50};

  x.with_clamp() = 150;
  ASSERT_EQ(x, 100);

  x.with_wrap() = 103;
  ASSERT_EQ(x, 2);
}

// clamp during insidable assignment
TEST(InsideAssignTest, clamp_during_insidable_assignment)
{
  using wide   = inside<{0, 200}>;
  using narrow = inside<{0, 100}, clamp>;
  wide w{150};
  narrow n{0};
  n = w;
  ASSERT_EQ(n, 100);
}

// unsafe relaxes domain and round checks
TEST(InsideAssignTest, unsafe_relaxes_domain_and_round_checks)
{
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
  auto q = a / b;
  static_assert(!(std::is_same_v<typename decltype(q)::value_type::raw_type, rational>));
  ASSERT_EQ(*q, 6);

  // Under unsafe (ignore_zero) the div-by-zero check is skipped entirely:
  // binary `a / 0` is undefined behavior, consistent with the compound
  // `/= zero-inside` no-op below. UB is not testable at runtime, so no assertion.

  // Compound /= by a zero inside: unsafe implies ignore_zero -> silent no-op.
  u100u y{50};
  y /= u100u{0};
  ASSERT_EQ(y, 50);

  // Out-of-range silent overwrite (no domain check)
  ASSERT_NO_THROW((void)(([&] { u100u x{50}; x = 200; (void)x; }())));
}

// disjoint-interval assignment is allowed under wrap/clamp
TEST(InsideAssignTest, disjoint_interval_assignment_is_allowed_under_wrap_clamp)
{
  // An inside whose interval is wholly outside the target is rejected for strict
  // policies but allowed under wrap/clamp (they bring any value into range —
  // matching the integral-RHS path, whose unbounded interval always overlaps).
  using src = inside<{25, 34}>;
  static_assert(inside_assignable<inside<{0, 9}, wrap>,  src>);
  static_assert(inside_assignable<inside<{0, 9}, clamp>, src>);
  static_assert(!(inside_assignable<inside<{0, 9}>,        src>));   // checked: rejected

  inside<{0, 9}, wrap> w{0};
  w = src{27};
  ASSERT_EQ(w, 7);                    // 27 mod 10
  inside<{0, 9}, clamp> c{0};
  c = src{27};
  ASSERT_EQ(c, 9);                    // clamped to upper
}

// trivial-type guarantees
TEST(InsideAssignTest, trivial_type_guarantees)
{
  static_assert(std::is_trivial_v<inside<{0, 0},      unsafe>>);
  static_assert(std::is_trivial_v<inside<{0, 100},    unsafe>>);
  static_assert(std::is_trivial_v<inside<{-100, 100}, unsafe>>);
  static_assert(std::is_trivial_v<inside<{{0, 10}, rational{1u, 2}}, unsafe>>);
  static_assert(std::is_trivial_v<inside<{{-10, 10}, 0}, unsafe>>);
  static_assert(std::is_trivial_v<inside<{0, 100}, clamp>>);
  static_assert(std::is_trivial_v<inside<{0, 100}, wrap>>);

  static_assert(std::is_trivially_copyable_v<inside<{0, 100}>>);
  static_assert(std::is_trivially_destructible_v<inside<{0, 100}>>);
  // The default ctor is `= default` for every policy (no zero-fill), so even a
  // `checked` inside is trivially default-constructible — and fully trivial.
  static_assert(std::is_trivially_default_constructible_v<inside<{0, 100}>>);
  static_assert(std::is_trivial_v<inside<{0, 100}>>);
}

// type-alias smoke checks
TEST(InsideAssignTest, type_alias_smoke_checks)
{
  using test0_t = inside<{{1,3}, 1}>;
  using test4_t = inside<{{0u, std::numeric_limits<umax>::max()}, 1}>;
  using test5_t = inside<{1_r}>;
  static_assert(std::is_same_v<test0_t::raw_type, std::uint8_t>);
  static_assert(std::is_same_v<test4_t::raw_type, std::uint64_t>);
  static_assert(std::is_same_v<test5_t::raw_type, detail::point_slot>);   // a point stores nothing
}

// integer rhs into non-integer-interval inside
TEST(InsideAssignTest, integer_rhs_into_non_integer_interval_inside)
{
  // Lower/Upper are non-integer: integer-interval fast path is skipped,
  // exercising the rational-aware out-of-range branch in handle_out_of_range.
  using halfgrid = inside<{{0.5_r, 5.5_r}, 0.5_r}, clamp>;

  halfgrid over{100};                 // out of range, clamps to 5.5
  ASSERT_EQ(over, 5.5_r);

  halfgrid under{-100};               // clamps to 0.5
  ASSERT_EQ(under, 0.5_r);

  halfgrid in{3};                     // 3 lands on a notch (3.0 = 6 notches)
  ASSERT_EQ(in, 3);
}

// checked policy throws on float out-of-range
TEST(InsideAssignTest, checked_policy_throws_on_float_out_of_range)
{
  using c10 = inside<{0, 10}, checked>;
  ASSERT_THROW((void)(c10{100.0}), beman::inside::inside_error);
  ASSERT_THROW((void)(c10{-1.0}), beman::inside::inside_error);

  // rational rhs takes the same path
  ASSERT_THROW((void)(c10{20_r}), beman::inside::inside_error);
}

// checked policy throws on rounding error
TEST(InsideAssignTest, checked_policy_throws_on_rounding_error)
{
  using coarse = inside<{{0, 10}, 2}, checked>;
  coarse c;

  // 3.0 doesn't land on notch 2 — round_check fires
  ASSERT_THROW((void)((c = 3.0)), beman::inside::inside_error);

  // value on the notch is fine
  ASSERT_NO_THROW((void)((c = 4.0)));
  ASSERT_EQ(c, 4);
}

// snap truncates non-notch float at runtime
TEST(InsideAssignTest, snap_truncates_non_notch_float_at_runtime)
{
  using coarse = inside<{{0, 10}, 2}, snap>;
  coarse c;
  c = 3.0;
  ASSERT_EQ(c, 2);    // truncates toward zero

  c = 7.99;
  ASSERT_EQ(c, 6);
}

// checked inside-to-inside out-of-range throws
TEST(InsideAssignTest, checked_inside_to_inside_out_of_range_throws)
{
  using src = inside<{0, 100}>;
  using dst = inside<{0, 50}, checked>;
  src s{75};
  ASSERT_THROW((void)(dst{s}), beman::inside::inside_error);
}

// non-integer-mapping inside-to-inside clamp / range_fail
TEST(InsideAssignTest, non_integer_mapping_inside_to_inside_clamp_domain_fail)
{
  // Source has notch 1 to a destination with notch 1/3 — Factor=3, integer.
  // To force the non-integer-mapping path we use a fractional-notch source
  // with a destination that doesn't cleanly align: notch 1/2 -> notch 1/3.
  using src = inside<{{0, 5}, rational{1u, 2}}, snap>;
  using dst = inside<{{0, 5}, rational{1u, 3}}, snap | clamp>;

  src s{4};   // value 4 -> dst aligns
  dst d{s};
  ASSERT_EQ(d, 4);

  // src[0,5] dst[0,4] — value 5 in src is out of dst, exercises clamp path
  using dst2 = inside<{{0, 4}, rational{1u, 3}}, snap | clamp>;
  src s2{5};
  dst2 d2{s2};
  ASSERT_EQ(d2, 4);
}

// wrap on fractional / notch grids (insidable rhs)
TEST(InsideAssignTest, wrap_on_fractional_notch_grids_insidable_rhs)
{
  using namespace beman::inside;
  // {0,1} notch 1/4 — integer interval but fractional notch. Wrap period is
  // (Upper - Lower) + Notch = 1.25; slots {0, .25, .5, .75, 1.0}.
  using dst = inside<{{0, 1}, per<4>}, wrap | round_nearest>;
  using src = inside<{{-2, 2}, per<4>}, round_nearest>;

  ASSERT_EQ((rational{dst{src{rational{5, 4}}}}), rational{0});       // 1.25 -> 0
  ASSERT_EQ((rational{dst{src{rational{6, 4}}}}), (rational{1, 4}));    // 1.50 -> 0.25
  ASSERT_EQ((rational{dst{src{rational{7, 4}}}}), (rational{1, 2}));    // 1.75 -> 0.50
  ASSERT_EQ((rational{dst{src{rational{-1, 4}}}}), rational{1});       // -0.25 -> 1.0
  ASSERT_EQ((rational{dst{src{rational{-2, 4}}}}), (rational{3, 4}));    // -0.50 -> 0.75

  // Non-integer interval, notch 1/2: {1/2, 5/2} period = 2 + 1/2 = 2.5.
  using dst2 = inside<{{rational{1, 2}, rational{5, 2}}, per<2>}, wrap | round_nearest>;
  using src2 = inside<{{-4, 4}, per<2>}, round_nearest>;
  ASSERT_EQ(rational{dst2{src2{rational{3}}}}, (rational{1, 2}));      // 3.0 -> 0.5

  // Unit-integer grid still uses the fast path and wraps as before.
  using deg = inside<{0, 359}, wrap>;
  using wide = inside<{-720, 720}>;
  ASSERT_EQ(deg{wide{370}}, 10);
  ASSERT_EQ(deg{wide{-10}}, 350);
}
