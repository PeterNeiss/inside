// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
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
// Keeping the sentinel slot means slim::optional stays zero-overhead.
//---------------------------------------------------------------------------
// formats keep zero-overhead optional
TEST(FormatsTest, formats_keep_zero_overhead_optional)
{
  static_assert(sizeof(slim::optional<byte>)      == sizeof(byte));
  static_assert(sizeof(slim::optional<sword>)     == sizeof(sword));
  static_assert(sizeof(slim::optional<unorm8>)  == sizeof(unorm8));
  static_assert(sizeof(slim::optional<unorm16>) == sizeof(unorm16));
  static_assert(sizeof(slim::optional<q8_8>)    == sizeof(q8_8));

  slim::optional<byte> o = byte{200};
  ASSERT_TRUE(o.has_value());
  ASSERT_EQ(*o, 200);
  o = slim::nullopt;
  ASSERT_FALSE(o.has_value());
}

//---------------------------------------------------------------------------
// Round-trips and endpoint reachability.
//---------------------------------------------------------------------------
// formats round-trip representative values
TEST(FormatsTest, formats_round_trip_representative_values)
{
  // Native integers — top usable value is one below the native max.
  ASSERT_EQ(byte{254}, 254);
  ASSERT_EQ(byte{0}, 0);
  ASSERT_EQ(sword{-32767}, -32767);
  ASSERT_EQ(sword{32767}, 32767);

  // UNORM — both endpoints exact, plus a representable interior point.
  ASSERT_EQ(unorm8{0.0_r}, 0);
  ASSERT_EQ(unorm8{1.0_r}, 1);
  ASSERT_EQ(unorm8{0.5_r}, 0.5_r);     // 127/254 == 1/2
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
  ASSERT_EQ(word{65534}, 65534);
  ASSERT_EQ(dword{4294967294}, 4294967294);
  ASSERT_EQ(sbyte{-127}, -127);
  ASSERT_EQ(sdword{-2147483647}, -2147483647);
  ASSERT_EQ(sdword{2147483647}, 2147483647);
  ASSERT_EQ(sqword{-9223372036854775807LL}, -9223372036854775807LL);
  ASSERT_EQ(sqword{9223372036854775807LL}, 9223372036854775807LL);
  ASSERT_EQ(unorm32{1.0_r}, 1);
  ASSERT_EQ(unorm32{0.0_r}, 0);
}

//---------------------------------------------------------------------------
// The reserved value is out of range under the default `checked` policy.
//---------------------------------------------------------------------------
// formats reject the reserved top value
TEST(FormatsTest, formats_reject_the_reserved_top_value)
{
  ASSERT_ANY_THROW((void)([]{ byte x{255}; (void)x; }()));      // 255 is the reserved slot
  ASSERT_ANY_THROW((void)([]{ sword x{-32768}; (void)x; }()));  // INT16_MIN is reserved
  ASSERT_NO_THROW((void)([]{ byte x{254}; (void)x; }()));
}
