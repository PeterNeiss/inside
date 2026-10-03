// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include <beman/inside/inside.hpp>
#include <beman/inside/numeric_limits.hpp>
#include <beman/inside/predicates.hpp>

#include <gtest/gtest.h>

#include <unordered_set>
#include <limits>

using namespace beman::inside;
using namespace beman::inside::detail;

//---------------------------------------------------------------------------
// std::numeric_limits<inside>
//---------------------------------------------------------------------------
// numeric_limits reports grid bounds
TEST(CastsTest, numeric_limits_reports_grid_bounds)
{
  using pct = inside<{0, 100}>;
  using nl  = std::numeric_limits<pct>;

  static_assert(nl::is_specialized);
  static_assert(nl::is_bounded);
  static_assert(nl::is_integer);
  static_assert(nl::is_exact);
  static_assert(!(nl::is_signed));
  static_assert(!(nl::is_modulo));

  static_assert(nl::min()    == pct{0});
  static_assert(nl::max()    == pct{100});
  static_assert(nl::lowest() == pct{0});
}

// numeric_limits handles signed and wrapping bounds
TEST(CastsTest, numeric_limits_handles_signed_and_wrapping_bounds)
{
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
TEST(CastsTest, hash_specialization_works_with_unordered_set)
{
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
// will_conversion_overflow
TEST(CastsTest, will_conversion_overflow)
{
  using pct = inside<{0, 100}>;

  static_assert(!(will_conversion_overflow<pct>(50)));
  static_assert(will_conversion_overflow<pct>(150));
  static_assert(will_conversion_overflow<pct>(-1));
  static_assert(!(will_conversion_overflow<pct>(0)));
  static_assert(!(will_conversion_overflow<pct>(100)));
}

// will_conversion_trunc detects non-notch values
TEST(CastsTest, will_conversion_trunc_detects_non_notch_values)
{
  using coarse = inside<{{0, 10}, 2}>;          // notch 2

  static_assert(!(will_conversion_trunc<coarse>(0)));
  static_assert(!(will_conversion_trunc<coarse>(4)));
  static_assert(will_conversion_trunc<coarse>(3));     // doesn't land on 2-notch
  static_assert(!(will_conversion_trunc<coarse>(11))); // out of range, not truncation
}

// is_conversion_lossy combines both
TEST(CastsTest, is_conversion_lossy_combines_both)
{
  using coarse = inside<{{0, 10}, 2}>;

  static_assert(!(is_conversion_lossy<coarse>(4)));
  static_assert(is_conversion_lossy<coarse>(3));   // truncation
  static_assert(is_conversion_lossy<coarse>(20));  // overflow
}

//---------------------------------------------------------------------------
// clamp_cast / checked_cast / unchecked_cast
//---------------------------------------------------------------------------
// clamp_cast clamps to boundary
TEST(CastsTest, clamp_cast_clamps_to_boundary)
{
  using pct = inside<{0, 100}>;

  ASSERT_TRUE(clamp_cast<pct>(150) == pct{100});
  ASSERT_TRUE(clamp_cast<pct>(-5)  == pct{0});
  ASSERT_TRUE(clamp_cast<pct>(42)  == pct{42});
}

// checked_cast throws on out-of-range
TEST(CastsTest, checked_cast_throws_on_out_of_range)
{
  using pct = inside<{0, 100}>;

  static_assert(checked_cast<pct>(42) == pct{42});
  ASSERT_THROW((void)(checked_cast<pct>(150)), beman::inside::inside_error);
  ASSERT_THROW((void)(checked_cast<pct>(-1)), beman::inside::inside_error);
}

// checked_cast throws on truncation
TEST(CastsTest, checked_cast_throws_on_truncation)
{
  using coarse = inside<{{0, 10}, 2}>;

  static_assert(checked_cast<coarse>(4) == coarse{4});
  ASSERT_THROW((void)(checked_cast<coarse>(3)), beman::inside::inside_error);
}

// unchecked_cast bypasses runtime checks
TEST(CastsTest, unchecked_cast_bypasses_runtime_checks)
{
  using pct = inside<{0, 100}>;

  // In-range value: same result as checked_cast.
  static_assert(unchecked_cast<pct>(42) == pct{42});
  static_assert(unchecked_cast<pct>(0)  == pct{0});
}

//---------------------------------------------------------------------------
// _ins literal
//---------------------------------------------------------------------------
// _ins literal produces just<N>
TEST(CastsTest, ins_literal_produces_just_n)
{
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
TEST(CastsTest, add_all_mul_all_fold_variadically)
{
  using v = inside<{0, 100}>;
  constexpr v a{10}, b{20}, c{30}, d{40};
  static_assert(add_all(a, b, c, d) == 100);

  constexpr v p{2}, q{3}, r{5};
  static_assert(mul_all(p, q, r) == 30);
}

//---------------------------------------------------------------------------
// rounding modes (with_snap<round_floor> / with_snap<round_ceil> / with_snap<round_half_even>)
//---------------------------------------------------------------------------
// Rounding modes apply when rhs is f64-valued (float, double, rational).
// Integer rhs takes the truncation fast path which is *intentionally*
// rounding-policy-agnostic — see assignment.hpp:store(integral).
// with_snap<round_floor> rounds toward -inf for double rhs
TEST(CastsTest, with_snap_round_floor_rounds_toward_inf_for_double_rhs)
{
  using coarse = inside<{{0, 10}, 2}>;
  coarse c{0};

  c.with_snap<round_floor>() = 3.0;
  ASSERT_EQ(c, 2);

  c.with_snap<round_floor>() = 4.0;
  ASSERT_EQ(c, 4);

  c.with_snap<round_floor>() = 5.0;
  ASSERT_EQ(c, 4);
}

// with_snap<round_ceil> rounds toward +inf for double rhs
TEST(CastsTest, with_snap_round_ceil_rounds_toward_plus_inf_for_double_rhs)
{
  using coarse = inside<{{0, 10}, 2}>;
  coarse c{0};

  c.with_snap<round_ceil>() = 3.0;
  ASSERT_EQ(c, 4);

  c.with_snap<round_ceil>() = 4.0;
  ASSERT_EQ(c, 4);

  c.with_snap<round_ceil>() = 5.0;
  ASSERT_EQ(c, 6);
}

// with_snap<round_half_even> applies banker's rounding
TEST(CastsTest, with_snap_round_half_even_applies_banker_s_rounding)
{
  using coarse = inside<{{0, 10}, 2}>;
  coarse c{0};

  // 1.0 is the half-way point between notch 0 and notch 2 → even wins (0).
  c.with_snap<round_half_even>() = 1.0;
  ASSERT_EQ(c, 0);

  // 3.0 is half-way between 2 and 4 → even wins (4).
  c.with_snap<round_half_even>() = 3.0;
  ASSERT_EQ(c, 4);

  // 5.0 is halfway between 4 and 6 → even wins (4).
  c.with_snap<round_half_even>() = 5.0;
  ASSERT_EQ(c, 4);

  // 7.0 is halfway between 6 and 8 → even wins (8).
  c.with_snap<round_half_even>() = 7.0;
  ASSERT_EQ(c, 8);
}

//---------------------------------------------------------------------------
// clamp_floor / clamp_ceil / clamp_round
//---------------------------------------------------------------------------
// clamp_floor / clamp_ceil / clamp_round compose clamp + round
TEST(CastsTest, clamp_floor_clamp_ceil_clamp_round_compose_clamp_plus_round)
{
  using coarse = inside<{{0, 10}, 2}>;

  // In-range, off-notch: round per mode.
  ASSERT_TRUE(clamp_floor<coarse>(3.0) == coarse{2});
  ASSERT_TRUE(clamp_ceil <coarse>(3.0) == coarse{4});
  ASSERT_TRUE(clamp_round<coarse>(3.0) == coarse{4});

  // Out-of-range: clamp to boundary.
  ASSERT_TRUE(clamp_floor<coarse>(15.0) == coarse{10});
  ASSERT_TRUE(clamp_ceil <coarse>(15.0) == coarse{10});
  ASSERT_TRUE(clamp_round<coarse>(15.0) == coarse{10});

  ASSERT_TRUE(clamp_floor<coarse>(-3.0) == coarse{0});
}

//---------------------------------------------------------------------------
// clamp_* accept a notch-incompatible *inside* source. The one-shot rounding
// policy widens the value+policy constructor's assignable check, so a finer
// grid rounds onto the target (not just arithmetic sources).
//---------------------------------------------------------------------------
// clamp_floor / clamp_ceil / clamp_round accept an inside source
TEST(CastsTest, clamp_floor_clamp_ceil_clamp_round_accept_an_inside_source)
{
  using small = inside<{{0, 10}, notch<1, 10>}, clamp>;   // 1/10 grid
  small a  = 2.5;                                          // exact on the 1/10 grid
  auto  sq = a * a;                                        // exact 6.25 on the 1/100 grid

  ASSERT_TRUE(clamp_floor<small>(sq) == 6.2_r);   // toward −∞
  ASSERT_TRUE(clamp_ceil <small>(sq) == 6.3_r);   // toward +∞
  ASSERT_TRUE(clamp_round<small>(sq) == 6.3_r);   // 6.25 tie → half away → 6.3

  small four = 4;                                          // 16 is out of range
  ASSERT_TRUE(clamp_round<small>(four * four) == 10);          // clamp to boundary

  ASSERT_TRUE(clamp_round<small>(150.0) == 10);                // arithmetic source: no regression
}
