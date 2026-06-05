//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDformatsHPP
#define BNDformatsHPP

#include "bound/bound.hpp"

//---------------------------------------------------------------------------
// formats — predefined `bound` types that map to hardware byte widths.
//
// Opt-in convenience aliases so you can write `bnd::u8` / `bnd::unorm16` /
// `bnd::q8_8` instead of spelling the grid and policy by hand.
//
// THE RESERVED-TOP TRADEOFF
// The library reserves each storage type's extreme value as a sentinel slot
// (so `slim::optional<bound>` is zero-overhead — no separate bool). Storage is
// therefore chosen with a strict `<` margin, and a *full*-width range like
// `{0, 255}` would promote to the next-larger type (uint16). To keep these
// aliases at their native byte width, each range/resolution stops one short of
// the sentinel: `u8` is `[0, 254]`, not `[0, 255]`. The payoff is native
// storage size AND a still-working zero-overhead `slim::optional<u8>`.
//
// Q-format types already have headroom below the type max, so they keep their
// full natural range and use power-of-two notches (maximally fast).
//
// Behavior is composable: these default to the library's `checked` policy
// (out-of-range throws / reports). For register-style wraparound or saturation,
// declare your own variant, e.g. `bound<{0,254}, wrap>`.
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

} // namespace bnd

#endif // BNDformatsHPP
