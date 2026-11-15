// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_INSIDE_FORMATS_HPP
#define BEMAN_INSIDE_INSIDE_FORMATS_HPP

#include <beman/inside/inside/detail/config.hpp>

#include <beman/inside/inside.hpp>

//---------------------------------------------------------------------------
// formats — predefined `inside` aliases mapping to hardware byte widths, so you
// can write `beman::inside::uint8` / `beman::inside::unorm16` / `beman::inside::uq8_8` directly.
//
// `uint8` … `uint64` and `int8` … `int64` are named after the std::uintN_t /
// std::intN_t they are stored in, and each uses that type's full range:
// `uint8` is [0, 255] in a std::uint8_t. Q-format types keep power-of-two
// notches.
//
// The integer widths are `checked`; unorm and the Q formats round to nearest.
// For wraparound or saturation declare your own (`inside<{0, 255}, wrap>`),
// or use the counters clamp_counter<Max> and wrap_counter<Max> below.
//---------------------------------------------------------------------------
// In their own namespace, so the names (int8, uint64, …) come in on request:
//   using namespace beman::inside::formats;
namespace beman::inside::formats {
//-------------------------------------------------------------------------
// Native integer widths — value storage (Raw == value), `checked`.
// Full native range.
//-------------------------------------------------------------------------
using uint8  = inside<{0, 255}>;
using uint16 = inside<{0, 65535}>;
using uint32 = inside<{0, 4294967295}>;
// uint64 reaches past int64: read it with `to<std::uint64_t>()`. A difference of two uint64s spans 2^65 values and
// gets a wide-integer index.
using uint64 = inside<{0, 18446744073709551615u}>;

using int8  = inside<{-128, 127}>;
using int16 = inside<{-32768, 32767}>;
using int32 = inside<{-2147483648, 2147483647}>;
// int64 is std::int64_t's whole range: its negation's type reaches 2^63,
// past int64, and takes a wider raw (exact, as every result type).
using int64 = inside<{-9223372036854775807 - 1, 9223372036854775807}>;

//-------------------------------------------------------------------------
// Unsigned normalized (UNORM) — [0, 1] at N-bit resolution, `round_half_away`.
// The notch denominator is the type max, so the index 0..max fills the native
// width; both endpoints (0 and 1) are exactly representable.
//-------------------------------------------------------------------------
using unorm8  = inside<{{0, 1}, per<255>}, round_half_away>;        // uint8
using unorm16 = inside<{{0, 1}, per<65535>}, round_half_away>;      // uint16
using unorm32 = inside<{{0, 1}, per<4294967295>}, round_half_away>; // uint32

//-------------------------------------------------------------------------
// Q-format fixed-point — unsigned integer.fraction, power-of-two notch,
// full natural range. `round_half_away`.
//-------------------------------------------------------------------------
using uq4_4   = inside<{{0, just<255, 16>}, per<16>}, round_half_away>;              // uint8: [0, 15.9375]
using uq8_8   = inside<{{0, just<65535, 256>}, per<256>}, round_half_away>;          // uint16
using uq16_16 = inside<{{0, just<4294967295, 65536>}, per<65536>}, round_half_away>; // uint32

//-------------------------------------------------------------------------
// Counters — a counter is an inside over [0, Max] whose overflow policy says
// what `++` does at the ceiling (the boundary behavior is in the type).
//-------------------------------------------------------------------------
// Saturating counter: `++` caps at Max (never throws or wraps) — the honest
// "count up to a ceiling / ≥Max" tally. `--` saturates at 0.
template <auto Max> // a builtin or a grid number of any size
using clamp_counter = inside<{0, Max}, clamp>;
// Wrapping counter: `++` wraps Max → 0 (sequence numbers, epochs).
template <auto Max>
using wrap_counter = inside<{0, Max}, wrap>;

} // namespace beman::inside::formats

#endif // BEMAN_INSIDE_INSIDE_FORMATS_HPP
