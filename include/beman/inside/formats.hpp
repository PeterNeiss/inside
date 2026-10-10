// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_FORMATS_HPP
#define BEMAN_INSIDE_FORMATS_HPP

#include <beman/inside/inside.hpp>

//---------------------------------------------------------------------------
// formats — predefined `inside` aliases mapping to hardware byte widths, so you
// can write `beman::inside::byte` / `beman::inside::unorm16` / `beman::inside::q8_8` directly.
//
// The bare `u8`/`i16`/… names are storage-policy flags (policy_flag.hpp); the
// native-width *types* below use width words instead: `byte`/`word`/`dword`
// (unsigned 8/16/32/64: `qword`) and `sbyte`/`sword`/`sdword`/`sqword` (signed 8/16/32/64).
//
// Each alias uses the full range of its native storage type: `byte` is
// [0, 255] in a uint8. Q-format types keep power-of-two notches.
//
// These default to `checked`; for wraparound/saturation declare your own (e.g.
// `inside<{0,254}, wrap>`).
//---------------------------------------------------------------------------
namespace beman::inside {
//-------------------------------------------------------------------------
// Native integer widths — value storage (Raw == value), `checked`.
// Full native range.
//-------------------------------------------------------------------------
using byte  = inside<{0, 255}>;        // uint8
using word  = inside<{0, 65535}>;      // uint16
using dword = inside<{0, 4294967295}>; // uint32
// qword reaches past int64, so it has no implicit `operator imax`; read it
// with `to<std::uint64_t>()`. A difference of qwords spans 2^65 values and
// gets a wide-integer index; a sum's upper bound passes the 64-bit grid
// numbers, so it needs C++26 big grids.
using qword = inside<{0, 18446744073709551615u}>; // uint64

using sbyte  = inside<{-128, 127}>;               // int8
using sword  = inside<{-32768, 32767}>;           // int16
using sdword = inside<{-2147483648, 2147483647}>; // int32
// sqword stays symmetric: the internal value path is `imax`, and -2^63
// has no negation in int64.
using sqword = inside<{-9223372036854775807, 9223372036854775807}>; // int64

//-------------------------------------------------------------------------
// Unsigned normalized (UNORM) — [0, 1] at N-bit resolution, `round_nearest`.
// The notch denominator is the type max, so the index 0..max fills the native
// width; both endpoints (0 and 1) are exactly representable.
//-------------------------------------------------------------------------
using unorm8  = inside<{{0, 1}, per<255>}, round_nearest>;        // uint8
using unorm16 = inside<{{0, 1}, per<65535>}, round_nearest>;      // uint16
using unorm32 = inside<{{0, 1}, per<4294967295>}, round_nearest>; // uint32

//-------------------------------------------------------------------------
// Q-format fixed-point — unsigned integer.fraction, power-of-two notch,
// full natural range. `round_nearest`.
//-------------------------------------------------------------------------
using q4_4   = inside<{{0, 15}, per<16>}, round_nearest>;       // uint8
using q8_8   = inside<{{0, 255}, per<256>}, round_nearest>;     // uint16
using q16_16 = inside<{{0, 65535}, per<65536>}, round_nearest>; // uint32

//-------------------------------------------------------------------------
// Counters — a counter is an inside over [0, Max] whose overflow policy says
// what `++` does at the ceiling (the boundary behavior is in the type).
//-------------------------------------------------------------------------
// Saturating counter: `++` caps at Max (never throws or wraps) — the honest
// "count up to a ceiling / ≥Max" tally. `--` saturates at 0.
template <umax Max>
using counter = inside<{0, Max}, clamp>;
// Modular / ring counter: `++` wraps Max → 0 (sequence numbers, epochs).
template <umax Max>
using ring_counter = inside<{0, Max}, wrap>;

} // namespace beman::inside

#endif // BEMAN_INSIDE_FORMATS_HPP
