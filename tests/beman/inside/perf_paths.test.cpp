// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Regression coverage for the perf-driven changes:
//   - Case 1: `needs_runtime_domain_check` gates the runtime range branch
//             in assignment::assign. Verify behaviour is preserved under
//             every policy and action that should still trigger it.
//   - Case 3: native_div_qformat fast path + Q-format operator rational()
//             fast path. Verify numerical exactness vs the rational route.

#include <beman/inside/inside.hpp>
#include <beman/inside/detail/rational.hpp>

#include <gtest/gtest.h>
#include <stdexcept>

using namespace beman::inside;
using namespace beman::inside::detail;

//---------------------------------------------------------------------------
// Case 1 — runtime range branch elision under `unsafe`
//---------------------------------------------------------------------------
// unsafe + no action: out-of-range int rhs stores without throwing
TEST(PerfPathsTest, unsafe_plus_no_action_out_of_range_int_rhs_stores_without_throwing)
{
  using L = inside<{0, 100}, unsafe>;
  // The += must not throw, even with an out-of-range value. Storage holds
  // the raw value as-is (UB on read, but that's the `unsafe` contract).
  L b{50};
  // A delta inside with Lower==0 keeps the raw-add fast path (a singleton `200_ins`
  // would widen to a disjoint grid that only clamp/wrap can absorb).
  ASSERT_NO_THROW((void)((b += inside<{0, 200}>{200})));
  // No assertion on the value — `unsafe` doesn't promise meaningful behaviour
  // for out-of-range writes, only that they don't throw.
}

// checked + no action: range check still fires
TEST(PerfPathsTest, checked_plus_no_action_range_check_still_fires)
{
  using L = inside<{0, 100}, checked>;
  L b{50};
  ASSERT_THROW((void)((b += inside<{0, 200}>{200})), beman::inside::inside_error);
}

// clamp policy: range check still fires
TEST(PerfPathsTest, clamp_policy_range_check_still_fires)
{
  using L = inside<{0, 100}, clamp>;
  L b{50};
  b += inside<{0, 200}>{200};
  ASSERT_EQ(b, 100);
}

// wrap policy: range check still fires
TEST(PerfPathsTest, wrap_policy_range_check_still_fires)
{
  using L = inside<{0, 100}, wrap>;
  L b{50};
  b += inside<{0, 200}>{200};
  ASSERT_EQ(b, 48);   // (50 + 200 - 0) mod 101 + 0 = 250 mod 101 = 48
}

//---------------------------------------------------------------------------
// Case 3 — Q-format fast path correctness
//---------------------------------------------------------------------------
// Q-format division: native_div_qformat matches rational arithmetic
TEST(PerfPathsTest, q_format_division_native_div_qformat_matches_rational_arithmetic)
{
  using fp = inside<{{0, 255}, 0x1p-8_r}>;   // Q8.8; the `truncated` call policy
                                            // supplies snap for the native
                                            // path, without unsafe's ignore_zero

  // Spot checks against expected Q-format integer-truncation values.
  auto q1 = div(fp{200}, fp{8}, truncated);
  ASSERT_TRUE(q1.has_value());
  ASSERT_EQ(*q1, 25);

  auto q2 = div(fp{255}, fp{1}, truncated);
  ASSERT_TRUE(q2.has_value());
  ASSERT_EQ(*q2, 255);

  // Non-integer quotient: 200 / 3 ≈ 66.6667. Q-format multiplies before
  // dividing — (51200 * 256) / 768 = 17066 (= floor(66.6667 * 256)) — same
  // as native `(a << 8) / b`. NOT 66 * 256 = 16896 (that would be
  // truncate-then-scale, which loses fractional precision).
  auto q3 = div(fp{200}, fp{3}, truncated);
  ASSERT_TRUE(q3.has_value());
  ASSERT_EQ((*q3).raw(), 17066);

  // Divide by zero produces errc::division_by_zero.
  auto q4 = div(fp{1}, fp{0}, truncated);
  ASSERT_FALSE(q4.has_value());
  ASSERT_EQ(q4.error(), errc::division_by_zero);
}

// Q-format division: result type is Q-format (same notch as L)
TEST(PerfPathsTest, q_format_division_result_type_is_q_format_same_notch_as_l)
{
  using fp = inside<{{0, 255}, 0x1p-8_r}, unsafe>;
  auto q = div(fp{200}, fp{8}, truncated);
  using R = std::remove_cvref_t<decltype(*q)>;
  static_assert(Notch<R> == Notch<fp>);   // same Q-format, not rational-raw
  static_assert(!(rational_raw<R>));
}

//---------------------------------------------------------------------------
// Case 3 — operator rational() fast path correctness
//---------------------------------------------------------------------------
// operator rational() round-trips through Q-format fast path
TEST(PerfPathsTest, operator_rational_round_trips_through_q_format_fast_path)
{
  using fp = inside<{{0, 255}, 0x1p-8_r}, unsafe>;

  // Bit-for-bit exactness over a sampling of values.
  for (int v : {0, 1, 50, 127, 254, 255})
  {
    fp b{v};
    rational r = b;
    ASSERT_EQ(r, rational{static_cast<unsigned>(v)});
    // round-trip back to fp.value
    ASSERT_EQ(b, v);
  }
}

// operator rational() handles fractional Q-format value
TEST(PerfPathsTest, operator_rational_handles_fractional_q_format_value)
{
  using fp = inside<{{0, 255}, 0x1p-8_r}, unsafe>;
  // Raw=128 → value 128/256 = 0.5.
  auto b = fp::from_raw(128);
  rational r = b;
  ASSERT_EQ(r, 0.5_r);
}
