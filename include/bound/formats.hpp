//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDformatsHPP
#define BNDformatsHPP

#include "bound/bound.hpp"

//---------------------------------------------------------------------------
// formats — predefined `bound` aliases mapping to hardware byte widths, so you
// can write `bnd::u8` / `bnd::unorm16` / `bnd::q8_8` directly.
//
// Reserved-top tradeoff: each storage type's extreme value is a sentinel slot
// (zero-overhead slim::optional<bound>), chosen with a strict `<` margin — so a
// full-width range like {0,255} would promote to uint16. To stay at native
// width these aliases stop one short: `u8` is [0, 254]. Q-format types already
// have headroom, so they keep full range with power-of-two notches.
//
// These default to `checked`; for wraparound/saturation declare your own (e.g.
// `bound<{0,254}, wrap>`).
//---------------------------------------------------------------------------
namespace bnd
{
  //-------------------------------------------------------------------------
  // Native integer widths — direct storage (Raw == value), `checked`.
  // Range is the native width minus the one reserved sentinel value.
  //-------------------------------------------------------------------------
  using u8  = bound<{0, 254}>;                          // uint8
  using u16 = bound<{0, 65534}>;                        // uint16
  using u32 = bound<{0, 4294967294}>;                   // uint32
  // u64 is intentionally absent: the library's internal value path is `imax`
  // (int64) — `to_value` returns `imax` — so unsigned values above 2^63-1
  // cannot round-trip through arithmetic/compare. Use `i64` or a hand-rolled
  // grid if you need 64-bit storage.

  using i8  = bound<{-127, 127}>;                        // int8
  using i16 = bound<{-32767, 32767}>;                    // int16
  using i32 = bound<{-2147483647, 2147483647}>;          // int32
  using i64 = bound<{-9223372036854775807, 9223372036854775807}>; // int64

  //-------------------------------------------------------------------------
  // Unsigned normalized (UNORM) — [0, 1] at N-bit resolution, `round_nearest`.
  // The notch denominator is one short of the type max so the index fits the
  // native width; both endpoints (0 and 1) are exactly representable.
  //-------------------------------------------------------------------------
  using unorm8  = bound<{{0, 1}, notch<1, 254>},        round_nearest>; // uint8
  using unorm16 = bound<{{0, 1}, notch<1, 65534>},      round_nearest>; // uint16
  using unorm32 = bound<{{0, 1}, notch<1, 4294967294>}, round_nearest>; // uint32

  //-------------------------------------------------------------------------
  // Q-format fixed-point — unsigned integer.fraction, power-of-two notch,
  // full natural range (already fits with headroom). `round_nearest`.
  //-------------------------------------------------------------------------
  using q4_4   = bound<{{0, 15},    notch<1, 16>},    round_nearest>; // uint8
  using q8_8   = bound<{{0, 255},   notch<1, 256>},   round_nearest>; // uint16
  using q16_16 = bound<{{0, 65535}, notch<1, 65536>}, round_nearest>; // uint32

  //-------------------------------------------------------------------------
  // Counters — a counter is a bound over [0, Max] whose overflow policy says
  // what `++` does at the ceiling (the boundary behavior is in the type).
  //-------------------------------------------------------------------------
  // Saturating counter: `++` caps at Max (never throws or wraps) — the honest
  // "count up to a ceiling / ≥Max" tally. `--` saturates at 0.
  template <umax Max> using counter      = bound<{0, Max}, clamp>;
  // Modular / ring counter: `++` wraps Max → 0 (sequence numbers, epochs).
  template <umax Max> using ring_counter = bound<{0, Max}, wrap>;

} // namespace bnd

#endif // BNDformatsHPP
