//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#include <catch2/catch_test_macros.hpp>

#include "bound/formats.hpp"

using namespace bnd;
using namespace bnd::detail;

//---------------------------------------------------------------------------
// The whole point: each predefined type lands on its native byte width.
//---------------------------------------------------------------------------
TEST_CASE("counter saturates; ring_counter wraps", "[formats][counter]")
{
  STATIC_REQUIRE(std::is_same_v<counter<5>,      bound<{0, 5}, clamp>>);
  STATIC_REQUIRE(std::is_same_v<ring_counter<5>, bound<{0, 5}, wrap>>);

  counter<5> c{4};
  ++c; ++c; ++c;                 // 4 -> 5 (saturates, never throws)
  REQUIRE(c == 5);
  --c; --c; --c; --c; --c; --c;  // floors at 0
  REQUIRE(c == 0);

  ring_counter<5> r{5};
  ++r;                           // 5 -> 0 (wraps)
  REQUIRE(r == 0);
}

TEST_CASE("formats map to native byte widths", "[formats][storage]")
{
  // Native integers
  STATIC_REQUIRE(sizeof(byte)  == 1);
  STATIC_REQUIRE(sizeof(word) == 2);
  STATIC_REQUIRE(sizeof(dword) == 4);
  STATIC_REQUIRE(sizeof(sbyte)  == 1);
  STATIC_REQUIRE(sizeof(sword) == 2);
  STATIC_REQUIRE(sizeof(sdword) == 4);
  STATIC_REQUIRE(sizeof(sqword) == 8);

  // Unsigned normalized
  STATIC_REQUIRE(sizeof(unorm8)  == 1);
  STATIC_REQUIRE(sizeof(unorm16) == 2);
  STATIC_REQUIRE(sizeof(unorm32) == 4);

  // Q-format fixed-point
  STATIC_REQUIRE(sizeof(q4_4)   == 1);
  STATIC_REQUIRE(sizeof(q8_8)   == 2);
  STATIC_REQUIRE(sizeof(q16_16) == 4);
}

//---------------------------------------------------------------------------
// Keeping the sentinel slot means slim::optional stays zero-overhead.
//---------------------------------------------------------------------------
TEST_CASE("formats keep zero-overhead optional", "[formats][optional]")
{
  STATIC_REQUIRE(sizeof(slim::optional<byte>)      == sizeof(byte));
  STATIC_REQUIRE(sizeof(slim::optional<sword>)     == sizeof(sword));
  STATIC_REQUIRE(sizeof(slim::optional<unorm8>)  == sizeof(unorm8));
  STATIC_REQUIRE(sizeof(slim::optional<unorm16>) == sizeof(unorm16));
  STATIC_REQUIRE(sizeof(slim::optional<q8_8>)    == sizeof(q8_8));

  slim::optional<byte> o = byte{200};
  REQUIRE(o.has_value());
  REQUIRE(*o == 200);
  o = slim::nullopt;
  REQUIRE_FALSE(o.has_value());
}

//---------------------------------------------------------------------------
// Round-trips and endpoint reachability.
//---------------------------------------------------------------------------
TEST_CASE("formats round-trip representative values", "[formats][values]")
{
  // Native integers — top usable value is one below the native max.
  REQUIRE(byte{254}  == 254);
  REQUIRE(byte{0}    == 0);
  REQUIRE(sword{-32767} == -32767);
  REQUIRE(sword{32767}  == 32767);

  // UNORM — both endpoints exact, plus a representable interior point.
  REQUIRE(unorm8{0.0_r} == 0);
  REQUIRE(unorm8{1.0_r} == 1);
  REQUIRE(unorm8{0.5_r} == 0.5_r);     // 127/254 == 1/2
  REQUIRE(unorm16{1.0_r} == 1);

  // Q-format — fractional values on the grid.
  REQUIRE(q8_8{42.5}   == 42.5_r);
  REQUIRE(q4_4{3.25}   == 3.25_r);
  REQUIRE(q16_16{1000.125} == 1000.125_r);
}

//---------------------------------------------------------------------------
// Extremes of the wider types round-trip through the imax value path.
//---------------------------------------------------------------------------
TEST_CASE("formats: wide-type extremes round-trip", "[formats][values]")
{
  REQUIRE(word{65534} == 65534);
  REQUIRE(dword{4294967294} == 4294967294);
  REQUIRE(sbyte{-127} == -127);
  REQUIRE(sdword{-2147483647} == -2147483647);
  REQUIRE(sdword{2147483647}  == 2147483647);
  REQUIRE(sqword{-9223372036854775807LL} == -9223372036854775807LL);
  REQUIRE(sqword{9223372036854775807LL}  == 9223372036854775807LL);
  REQUIRE(unorm32{1.0_r} == 1);
  REQUIRE(unorm32{0.0_r} == 0);
}

//---------------------------------------------------------------------------
// The reserved value is out of range under the default `checked` policy.
//---------------------------------------------------------------------------
TEST_CASE("formats reject the reserved top value", "[formats][checked]")
{
  REQUIRE_THROWS([]{ byte x{255}; (void)x; }());      // 255 is the reserved slot
  REQUIRE_THROWS([]{ sword x{-32768}; (void)x; }());  // INT16_MIN is reserved
  REQUIRE_NOTHROW([]{ byte x{254}; (void)x; }());
}
