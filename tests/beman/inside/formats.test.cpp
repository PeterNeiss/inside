// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include <gtest/gtest.h>

#include <beman/inside/formats.hpp>

using namespace beman::inside;
using namespace beman::inside::detail;

//---------------------------------------------------------------------------
// The whole point: each predefined type lands on its native byte width.
//---------------------------------------------------------------------------
// counter saturates; ring_counter wraps
TEST(FormatsTest, counter_saturates_ring_counter_wraps)
{
  static_assert(std::is_same_v<counter<5>,      inside<{0, 5}, clamp>>);
  static_assert(std::is_same_v<ring_counter<5>, inside<{0, 5}, wrap>>);

  counter<5> c{4};
  ++c; ++c; ++c;                 // 4 -> 5 (saturates, never throws)
  ASSERT_EQ(c, 5);
  --c; --c; --c; --c; --c; --c;  // floors at 0
  ASSERT_EQ(c, 0);

  ring_counter<5> r{5};
  ++r;                           // 5 -> 0 (wraps)
  ASSERT_EQ(r, 0);
}

// formats map to native byte widths
TEST(FormatsTest, formats_map_to_native_byte_widths)
{
  // Native integers
  static_assert(sizeof(byte)  == 1);
  static_assert(sizeof(word) == 2);
  static_assert(sizeof(dword) == 4);
  static_assert(sizeof(sbyte)  == 1);
  static_assert(sizeof(sword) == 2);
  static_assert(sizeof(sdword) == 4);
  static_assert(sizeof(sqword) == 8);

  // Unsigned normalized
  static_assert(sizeof(unorm8)  == 1);
  static_assert(sizeof(unorm16) == 2);
  static_assert(sizeof(unorm32) == 4);

  // Q-format fixed-point
  static_assert(sizeof(q4_4)   == 1);
  static_assert(sizeof(q8_8)   == 2);
  static_assert(sizeof(q16_16) == 4);
}

//---------------------------------------------------------------------------
// Fallible results stay out of storage: std::expected appears only as a
// return value, so the stored types keep their native width.
//---------------------------------------------------------------------------
// formats: fallible results are expected, storage stays native
TEST(FormatsTest, formats_fallible_results_are_expected_storage_stays_native)
{
  using q = decltype(byte{200} / byte{0});
  static_assert(is_expected_v<q>);
  static_assert(sizeof(q) > sizeof(q::value_type));   // why it never becomes storage

  auto r = byte{200} / byte{0};
  ASSERT_FALSE(r.has_value());
  ASSERT_EQ(r.error(), errc::division_by_zero);
}

//---------------------------------------------------------------------------
// Round-trips and endpoint reachability.
//---------------------------------------------------------------------------
// formats round-trip representative values
TEST(FormatsTest, formats_round_trip_representative_values)
{
  // Native integers — the full native range is usable.
  ASSERT_EQ(byte{255}, 255);
  ASSERT_EQ(byte{0}, 0);
  ASSERT_EQ(sword{-32768}, -32768);
  ASSERT_EQ(sword{32767}, 32767);

  // UNORM — both endpoints exact, plus a representable interior point.
  ASSERT_EQ(unorm8{0.0_r}, 0);
  ASSERT_EQ(unorm8{1.0_r}, 1);
  ASSERT_EQ(unorm8{0.2_r}, 0.2_r);     // 51/255 == 1/5
  ASSERT_EQ(unorm16{1.0_r}, 1);

  // Q-format — fractional values on the grid.
  ASSERT_EQ(q8_8{42.5}, 42.5_r);
  ASSERT_EQ(q4_4{3.25}, 3.25_r);
  ASSERT_EQ(q16_16{1000.125}, 1000.125_r);
}

//---------------------------------------------------------------------------
// Extremes of the wider types round-trip through the imax value path.
//---------------------------------------------------------------------------
// formats: wide-type extremes round-trip
TEST(FormatsTest, formats_wide_type_extremes_round_trip)
{
  ASSERT_EQ(word{65535}, 65535);
  ASSERT_EQ(dword{4294967295}, 4294967295);
  ASSERT_EQ(sbyte{-128}, -128);
  ASSERT_EQ(sbyte{127}, 127);
  ASSERT_EQ(sdword{-2147483648LL}, -2147483648LL);
  ASSERT_EQ(sdword{2147483647}, 2147483647);
  ASSERT_EQ(sqword{-9223372036854775807LL}, -9223372036854775807LL);
  ASSERT_EQ(sqword{9223372036854775807LL}, 9223372036854775807LL);
  ASSERT_EQ(unorm32{1.0_r}, 1);
  ASSERT_EQ(unorm32{0.0_r}, 0);
}

//---------------------------------------------------------------------------
// Values past the native range are out of range under the default `checked`
// policy.
//---------------------------------------------------------------------------
// formats reject values past the native range
TEST(FormatsTest, formats_reject_values_past_the_native_range)
{
  ASSERT_ANY_THROW((void)([]{ int v = 256; byte x{v}; (void)x; }()));
  ASSERT_ANY_THROW((void)([]{ int v = -32769; sword x{v}; (void)x; }()));
  ASSERT_NO_THROW((void)([]{ int v = 255; byte x{v}; (void)x; }()));
  ASSERT_NO_THROW((void)([]{ int v = -32768; sword x{v}; (void)x; }()));
}
