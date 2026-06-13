// Bit-exact contract tests for the bnd::math INTEGER (CORDIC) engine.
//
// Every assertion here pins a specific integer output for a specific integer
// input — the unconditional bit-exact contract of the fixed engine. These run
// under `-DBOUND_MATH_FIXED=ON` (the integer engine); the default build uses the
// double engine, covered by test_cmath_double.cpp.

#include "bound/cmath.hpp"
#include "bound/bound.hpp"
#include "bound/print.hpp"

#include <catch2/catch_test_macros.hpp>

#include <iostream>

#ifdef BND_MATH_FIXED

using namespace bnd;
using namespace bnd::detail;

namespace
{
  // Q.14 signed amplitude in [-1, 1]; same shape as audio_mixer's sample_t.
  using sample_t = bound<{{-1, 1}, notch<1, 16384>}, round_nearest | clamp | real>;

  // Helper: build a Q.16 turn-phase from a raw uint16 slot index.
  // `turns_t<N>` lives in `math::detail::` — the public sin/cos accept
  // radians; the tests below pin the underlying turn-input cores via
  // `math::detail::*_turn_impl` directly.
  constexpr math::detail::turns_t<16> phase_from(unsigned raw)
  {
    using P = math::detail::turns_t<16>;
    return P::from_raw(static_cast<typename P::raw_type>(raw));
  }

  // Thin auto-form wrappers around the internal turn-input workers,
  // matching the call-site shape the tests had before the radians
  // redesign. Each picks the same auto-deduced output type
  // `detail::sin_auto_t<In>` the public auto-form used to produce.
  template <boundable In>
  constexpr auto sin_turn(In phase) noexcept
  { return math::detail::sin_turn_impl<math::detail::sin_auto_t<In>>(phase); }

  template <boundable In>
  constexpr auto cos_turn(In phase) noexcept
  { return math::detail::cos_turn_impl<math::detail::cos_auto_t<In>>(phase); }

  template <boundable In>
  constexpr auto tan_turn(In phase) noexcept
  { return math::detail::tan_turn_impl<math::detail::tan_auto_t<In>>(phase); }

  // Helper: compute f(phase) and return Q.14 amplitude as a signed int.
  // sample_t uses notch-offset storage (Raw = (value − Lower) · 16384 =
  // (value + 1) · 16384), so the underlying Raw is unsigned. The Q.14
  // amplitude is just Raw minus that offset — storage-shape-independent
  // and the natural unit for a [−1, 1] Q.14 grid.
  constexpr int sin_q14(unsigned phase_raw)
  {
    return static_cast<int>(sample_t{sin_turn(phase_from(phase_raw))}.raw())
         - 16384;
  }

  constexpr int cos_q14(unsigned phase_raw)
  {
    return static_cast<int>(sample_t{cos_turn(phase_from(phase_raw))}.raw())
         - 16384;
  }

  // --- grid-native circle<M> degree angle + amp<K> amplitude ---------------
  // The public reference-output path: `math::sin(circle<M>, amp<K>&)`. amp<K>
  // stores raw = (value + 1)·K, so `raw - K` is the signed amplitude in units
  // of 1/K (e.g. K = 16384 → Q.14, directly comparable to sin_q14 above).
  template <std::uint64_t M, std::uint64_t K = 16384>
  constexpr int circ_sin_qk(int deg)
  {
    math::circle<M> a = deg;
    math::amp<K>    y;
    math::sin(a, y);
    return static_cast<int>(y.raw()) - static_cast<int>(K);
  }

  template <std::uint64_t M, std::uint64_t K = 16384>
  constexpr int circ_cos_qk(int deg)
  {
    math::circle<M> a = deg;
    math::amp<K>    y;
    math::cos(a, y);
    return static_cast<int>(y.raw()) - static_cast<int>(K);
  }
}

//---------------------------------------------------------------------------
// Exact-by-symmetry: range reduction sends these phases to x_q30 = 0,
// so the polynomial evaluator returns exactly 0 with no rounding error.
//---------------------------------------------------------------------------
TEST_CASE("bnd::math::sin: exact at multiples of π", "[cmath][sin][constexpr]")
{
  static_assert(sin_q14(0)     == 0);   // sin(0)     = 0
  static_assert(sin_q14(32768) == 0);   // sin(π)     = 0
  // Q.16 turns can't represent 2π (== 0) as a non-zero raw; 65536 wraps.
}

//---------------------------------------------------------------------------
// Quadrant boundaries: sin(±π/2) should saturate the Q.14 amplitude grid
// exactly (raw = ±16384). The Taylor truncation + kRadPerSlotQ30 rounding
// error is below 1 Q.14 ULP, so the result rounds back to the boundary.
//---------------------------------------------------------------------------
TEST_CASE("bnd::math::sin: quadrant peaks", "[cmath][sin][constexpr]")
{
  static_assert(sin_q14(16384) ==  16384);   //  sin(π/2)
  static_assert(sin_q14(49152) == -16384);   //  sin(3π/2)
}

//---------------------------------------------------------------------------
// Bit-exact sweep at known-significant phases. Values pinned to the exact
// Q.14 raw the constexpr evaluator produces. Same int on every platform.
//---------------------------------------------------------------------------
TEST_CASE("bnd::math::sin: bit-exact sweep", "[cmath][sin][constexpr]")
{
  // Phase 1/12 turn = π/6 rad. sin(π/6) = 0.5 → expected raw ≈ 8192.
  static_assert(sin_q14(65536/12) == 8192);

  // Phase 1/8 turn = π/4 rad. sin(π/4) = √2/2 ≈ 0.7071068 → raw ≈ 11585.
  static_assert(sin_q14(65536/8)  == 11585);

  // Phase 1/6 turn ≈ π/3 rad. Integer-truncated phase is 10922/65536, which
  // is *just below* exact 1/6, so the result rounds to 14188 not 14189.
  static_assert(sin_q14(65536/6)  == 14189);   // double-rounding shift

  // Phase 5/12 turn = 5π/6 rad (second quadrant). sin(5π/6) = sin(π/6) = 0.5.
  // Exercises the upper-reflect path (raw > 0x4000 in the quadrant reducer);
  // the integer-truncated 5·5461/65536 lands one slot below ideal → 8193.
  static_assert(sin_q14(5 * 65536/12) == 8193);

  // Mirror across π: sin(π + θ) = -sin(θ). Pin the sign-flip path.
  static_assert(sin_q14(32768 + 65536/8) == -11585);
  // sin(π+π/3) = -0.86603 → Q.14 -14188.96 → round-half-away-from-zero = -14189
  // (the cross-grid store into the Q.14 grid now rounds the value, not the
  // non-negative offset, so negatives round away from zero like division).
  static_assert(sin_q14(32768 + 65536/6) == -14189);

  // Mirror across 2π via raw wrap: phase 65535 ≈ 2π - δ → sin ≈ -δ.
  // -δ ≈ -9.59e-5 → Q.14 -1.571 → round-half-away-from-zero = -2.
  static_assert(sin_q14(65535) == -2);
}

//---------------------------------------------------------------------------
// Grid-native periodic trig: circle<M> degree angle → amp<K> amplitude.
// Degrees have an integer period (360), so the wrap is exact and the path is
// a table lookup — no radians conversion. Pins the bit-exact amplitude the
// table produces; identical on every platform.
//---------------------------------------------------------------------------
TEST_CASE("bnd::math::sin(circle): cardinal degrees", "[cmath][sin][circle][constexpr]")
{
  static_assert(circ_sin_qk<360>(0)   ==      0);   // sin(0°)   =  0
  static_assert(circ_sin_qk<360>(90)  ==  16384);   // sin(90°)  =  1
  static_assert(circ_sin_qk<360>(180) ==      0);   // sin(180°) =  0
  static_assert(circ_sin_qk<360>(270) == -16384);   // sin(270°) = -1
  static_assert(circ_sin_qk<360>(30)  ==   8192);   // sin(30°)  =  0.5
  static_assert(circ_sin_qk<360>(120) ==  14189);   // sin(120°) = sin(60°)
  static_assert(circ_sin_qk<360>(210) ==  -8192);   // sin(210°) = -0.5 (sign flip)
  static_assert(circ_sin_qk<360>(45)  ==  11585);   // √2/2; matches the turn path

  // Power-of-two M is the optimal path: 90° == slot 64 of circle<256>,
  // an exact quarter-turn — reflection is a bitmask, result is exactly 1.
  static_assert(circ_sin_qk<256>(90)  ==  16384);
}

TEST_CASE("bnd::math::cos(circle): cardinal degrees", "[cmath][cos][circle][constexpr]")
{
  static_assert(circ_cos_qk<360>(0)   ==  16384);   // cos(0°)   =  1
  static_assert(circ_cos_qk<360>(90)  ==      0);   // cos(90°)  =  0
  static_assert(circ_cos_qk<360>(120) ==  -8192);   // cos(120°) = -0.5
  static_assert(circ_cos_qk<360>(180) == -16384);   // cos(180°) = -1
  static_assert(circ_cos_qk<360>(270) ==      0);   // cos(270°) =  0
}

//---------------------------------------------------------------------------
// The property radians cannot provide: wrapping is drift-free. A degree
// angle advanced past 360° a thousand times lands on the *identical* slot as
// its in-range equivalent, because one revolution is exactly M notch steps.
//---------------------------------------------------------------------------
TEST_CASE("bnd::math::sin(circle): drift-free wrap", "[cmath][sin][circle][constexpr]")
{
  static_assert([]{
    math::circle<360> b = 30;
    for (int k = 0; k < 1000; ++k) b = b.as<imax>() + 360;   // wrap 1000 times
    return b.raw();
  }() == math::circle<360>{30}.raw());

  // …and the amplitude after all that wrapping equals sin(30°) bit-for-bit.
  static_assert([]{
    math::circle<360> b = 30;
    for (int k = 0; k < 1000; ++k) b = b.as<imax>() + 360;
    math::amp<16384> y; math::sin(b, y);
    return static_cast<int>(y.raw()) - 16384;
  }() == 8192);
}

TEST_CASE("bnd::math::tan(circle): value and pole", "[cmath][tan][circle][constexpr]")
{
  static_assert([]{
    math::circle<360> a = 45; math::amp<16384> y;
    bool ok = math::tan(a, y);
    return ok && (static_cast<int>(y.raw()) - 16384) == 16384;   // tan(45°) = 1
  }());

  // cos(90°) == 0 → pole reported, out left untouched.
  static_assert([]{
    math::circle<360> a = 90; math::amp<16384> y;
    return math::tan(a, y);
  }() == false);
}

//---------------------------------------------------------------------------
// Precision follows the output grid: the grid-scaled CORDIC engine computes
// sin to exactly the amplitude grid's resolution (W derived from the notch),
// not a fixed Q.30 tier. The same angle lands on the nearest representable
// value of a coarse and a fine grid alike.
//---------------------------------------------------------------------------
TEST_CASE("bnd::math::sin(circle): precision follows the grid", "[cmath][sin][circle][constexpr]")
{
  // sin(30°) = 0.5 is exactly representable on every amp<K>; the engine hits it
  // on a tiny grid and a large one.
  static_assert(circ_sin_qk<360,        64>(30) ==        32);   // 0.5 · 64
  static_assert(circ_sin_qk<360,      1024>(30) ==       512);
  static_assert(circ_sin_qk<360,   1048576>(30) ==    524288);   // 0.5 · 2^20
  static_assert(circ_sin_qk<360,        64>(90) ==        64);   // sin90 = 1
  // A non-cardinal angle, pinned bit-exact on a fine grid.
  static_assert(circ_sin_qk<360,     65536>(50) ==     50203);   // sin50 ≈ 0.76604
}

//---------------------------------------------------------------------------
// cos is implemented as sin(phase + 0.25 turn); a separate sweep guards
// against a future refactor that breaks the phase-shift wiring even when
// sin's own vectors still pass.
//---------------------------------------------------------------------------
TEST_CASE("bnd::math::cos: quadrant peaks", "[cmath][cos][constexpr]")
{
  static_assert(cos_q14(0)     ==  16384);   //  cos(0)    =  1
  static_assert(cos_q14(32768) == -16384);   //  cos(π)    = -1
}

TEST_CASE("bnd::math::cos: zero crossings", "[cmath][cos][constexpr]")
{
  static_assert(cos_q14(16384) == 0);        //  cos(π/2)  = 0
  static_assert(cos_q14(49152) == 0);        //  cos(3π/2) = 0
}

TEST_CASE("bnd::math::cos: bit-exact sweep", "[cmath][cos][constexpr]")
{
  // cos(π/4) = √2/2 ≈ 0.7071068 → 11585.
  static_assert(cos_q14(65536/8)  == 11585);

  // cos(π/3) = 0.5. The phase raw 10922 is just below 1/6 turn; shifted by
  // 0.25, sin(27306) reflects to sin(5462) which is just above π/6, so the
  // algorithm gives 8193 (one slot above 0.5).
  static_assert(cos_q14(65536/6)  == 8193);

  // cos(π/6) = √3/2 ≈ 0.8660254 → 14189. The cos sweep picks up a different
  // truncation than sin (the phase shift moves which side of 1/12 turn the
  // reflected raw lands on), so this one rounds up where sin rounded down.
  static_assert(cos_q14(65536/12) == 14189);

  // Mirror across π: cos(π + θ) = -cos(θ). Pin the sign-flip path.
  static_assert(cos_q14(32768 + 65536/8) == -11585);
  static_assert(cos_q14(32768 + 65536/6) ==  -8193);

  // Mirror across 2π: cos(2π - δ) ≈ cos(δ) ≈ 1.
  static_assert(cos_q14(65535) == 16384);
}

//---------------------------------------------------------------------------
// Runtime probes: emit the full sweep as `raw → out` lines. Useful when
// adding new vectors or auditing a tweak; not load-bearing for the bit
// contract (the static_asserts above are).
//---------------------------------------------------------------------------
TEST_CASE("bnd::math::sin: probe (informational)", "[cmath][sin][.probe]")
{
  for (unsigned i = 0; i <= 65535; i += 4096)
    std::cout << "sin_q14(" << i << ") = " << sin_q14(i) << "\n";
}

TEST_CASE("bnd::math::cos: probe (informational)", "[cmath][cos][.probe]")
{
  for (unsigned i = 0; i <= 65535; i += 4096)
    std::cout << "cos_q14(" << i << ") = " << cos_q14(i) << "\n";
}

//---------------------------------------------------------------------------
// sqrt: Newton-Raphson family. Input is a non-negative Q.16 magnitude in
// [0, 4]; output is a Q.14 magnitude in [0, 2]. Both have Lower = 0, so
// Raw == value · grid_resolution with no offset to subtract.
//---------------------------------------------------------------------------
namespace
{
  using sqrt_in_t  = bound<{{0, 4}, notch<1, 65536>}, round_nearest | real>;
  using sqrt_out_t = bound<{{0, 2}, notch<1, 16384>}, round_nearest | real>;

  constexpr sqrt_in_t sqrt_input(unsigned raw_q16)
  {
    return sqrt_in_t::from_raw(static_cast<typename sqrt_in_t::raw_type>(raw_q16));
  }

  constexpr int sqrt_q14(unsigned input_raw_q16)
  {
    return static_cast<int>(sqrt_out_t{math::sqrt(sqrt_input(input_raw_q16))}.raw());
  }
}

TEST_CASE("bnd::math::sqrt: exact corners", "[cmath][sqrt][constexpr]")
{
  static_assert(sqrt_q14(0)      == 0);          // sqrt(0)    = 0
  static_assert(sqrt_q14(16384)  == 8192);       // sqrt(0.25) = 0.5
  static_assert(sqrt_q14(65536)  == 16384);      // sqrt(1)    = 1
  static_assert(sqrt_q14(262144) == 32768);      // sqrt(4)    = 2
}

TEST_CASE("bnd::math::sqrt: irrationals", "[cmath][sqrt][constexpr]")
{
  // sqrt(0.5) = √2/2 ≈ 0.7071068 → Q.14 ≈ 11585.
  static_assert(sqrt_q14(32768) == 11585);

  // sqrt(2) = √2 ≈ 1.4142136 → Q.14 ≈ 23170.
  static_assert(sqrt_q14(131072) == 23171);   // double-rounding shift

  // sqrt(3) = √3 ≈ 1.7320508 → Q.14 ≈ 28378.
  static_assert(sqrt_q14(196608) == 28378);

  // sqrt(0.0625) = 0.25 → Q.14 = 4096.
  static_assert(sqrt_q14(4096) == 4096);

  // Tiny input: sqrt(1/65536) = 1/256 → Q.14 = 64.
  static_assert(sqrt_q14(1) == 64);
}

TEST_CASE("bnd::math::sqrt: probe (informational)", "[cmath][sqrt][.probe]")
{
  for (unsigned i = 0; i <= 262144u; i += 16384)
    std::cout << "sqrt_q14(" << i << ") = " << sqrt_q14(i) << "\n";
}

TEST_CASE("bnd::math::sqrt: mixed-sign input returns expected", "[cmath][sqrt][mixed_sign]")
{
  using signed_in = bound<{{-1, 1}, notch<1, 65536>}, round_nearest | real>;

  // Non-negative runtime value → value with the usual Q.30 result.
  signed_in pos{0.25_r};
  auto r_pos = math::sqrt(pos);
  REQUIRE(r_pos.has_value());
  REQUIRE(*r_pos == 0.5_r);

  // Zero → value, result is 0.
  signed_in zero{0};
  auto r_zero = math::sqrt(zero);
  REQUIRE(r_zero.has_value());
  REQUIRE(*r_zero == 0);

  // Negative runtime value → unexpected(domain_error).
  signed_in neg{-0.5_r};
  auto r_neg = math::sqrt(neg);
  REQUIRE_FALSE(r_neg.has_value());
  REQUIRE(r_neg.error() == errc::domain_error);

  // The non-negative overload (Lower == 0) returns bound directly; the
  // mixed-sign overload returns expected (not optional). Disjoint by `requires`.
  using nonneg_in = bound<{{0, 1}, notch<1, 65536>}, round_nearest | real>;
  nonneg_in v{0.25_r};
  STATIC_REQUIRE_FALSE(is_slim_optional_v<decltype(math::sqrt(v))>);
  STATIC_REQUIRE_FALSE(is_slim_optional_v<decltype(math::sqrt(pos))>);
  REQUIRE(math::sqrt(v) == 0.5_r);
}

//---------------------------------------------------------------------------
// Decimal-display tests: bound in, bound out, no raw extraction. The
// comparisons run at the bound-value level; Catch2 stringifies failures
// via `bound/print.hpp`'s `operator<<`, so any mismatch reports e.g.
// `1 == 0.99993896484375` instead of `16384 == 16383`. The companion
// probes also print every value as decimal for at-a-glance auditing.
//---------------------------------------------------------------------------
TEST_CASE("bnd::math::sin: bound-typed equality (decimal display)",
          "[cmath][sin][decimal]")
{
  // Exact-on-the-grid cases: the Taylor truncation and rad-per-slot
  // rounding sum to less than 1 Q.14 ULP at these phases, so the result
  // lands exactly on 0 / ±1.
  REQUIRE(sample_t{sin_turn(phase_from(0))}     == sample_t{ 0});
  REQUIRE(sample_t{sin_turn(phase_from(16384))} == sample_t{ 1});
  REQUIRE(sample_t{sin_turn(phase_from(32768))} == sample_t{ 0});
  REQUIRE(sample_t{sin_turn(phase_from(49152))} == sample_t{-1});

  // Off-grid cases: pin against the exact rational the Q.14 grid rounds to.
  // 11585/16384 = 0.70709228515625 — sin(π/4) snapped to the sample grid.
  REQUIRE(sample_t{sin_turn(phase_from(8192))}
          == sample_t{rational{11585, 16384}});
  REQUIRE(sample_t{sin_turn(phase_from(24576))}
          == sample_t{rational{11585, 16384}});
}

TEST_CASE("bnd::math::cos: bound-typed equality (decimal display)",
          "[cmath][cos][decimal]")
{
  REQUIRE(sample_t{cos_turn(phase_from(0))}     == sample_t{ 1});
  REQUIRE(sample_t{cos_turn(phase_from(16384))} == sample_t{ 0});
  REQUIRE(sample_t{cos_turn(phase_from(32768))} == sample_t{-1});
  REQUIRE(sample_t{cos_turn(phase_from(49152))} == sample_t{ 0});

  // cos(π/4) = sin(π/4) = √2/2 → 11585/16384.
  REQUIRE(sample_t{cos_turn(phase_from(8192))}
          == sample_t{rational{11585, 16384}});
}

TEST_CASE("bnd::math::sqrt: bound-typed equality (decimal display)",
          "[cmath][sqrt][decimal]")
{
  // Exact-on-the-grid cases.
  REQUIRE(sqrt_out_t{math::sqrt(sqrt_input(0))}      == sqrt_out_t{0});
  REQUIRE(sqrt_out_t{math::sqrt(sqrt_input(16384))}  == sqrt_out_t{0.5_r});
  REQUIRE(sqrt_out_t{math::sqrt(sqrt_input(65536))}  == sqrt_out_t{1});
  REQUIRE(sqrt_out_t{math::sqrt(sqrt_input(262144))} == sqrt_out_t{2});

  // Off-grid: sqrt(2) ≈ 1.4142136 → double-rounded to 23171/16384 ≈ 1.4142.
  REQUIRE(sqrt_out_t{math::sqrt(sqrt_input(131072))}
          == sqrt_out_t{rational{23171, 16384}});
  // sqrt(0.5) ≈ 0.7071068 → 11585/16384.
  REQUIRE(sqrt_out_t{math::sqrt(sqrt_input(32768))}
          == sqrt_out_t{rational{11585, 16384}});
}

//---------------------------------------------------------------------------
// exp2: Taylor poly on [0, 1) plus integer-power-of-2 shift. Output bound
// must cover the range; below it picks [0, 16] at Q.14 (262145 slots).
//---------------------------------------------------------------------------
namespace
{
  using exp2_in_t  = bound<{{-4, 4}, notch<1, 16384>}, round_nearest | real>;
  using exp2_out_t = bound<{{0, 16}, notch<1, 16384>}, round_nearest | real>;

  constexpr exp2_in_t exp2_input(int raw_offset)
  {
    // exp2_in_t has Lower = -4, notch 1/16384 → Raw = (value + 4) · 16384.
    return exp2_in_t::from_raw(static_cast<typename exp2_in_t::raw_type>(raw_offset));
  }

  // Build an exp2 input from a rational value (cleaner than computing the
  // offset-encoded Raw by hand).
  constexpr exp2_in_t exp2_from(rational r) { return exp2_in_t{r}; }

  constexpr int exp2_q14(rational x)
  {
    // Out has Lower = 0 → Raw == value · 16384, no offset to subtract.
    return static_cast<int>(exp2_out_t{math::exp2(exp2_from(x))}.raw());
  }
}

TEST_CASE("bnd::math::exp2: integer powers", "[cmath][exp2][constexpr]")
{
  static_assert(exp2_q14(rational{ 0})              == 16384);  // 2^0  = 1
  static_assert(exp2_q14(rational{ 1})              == 32768);  // 2^1  = 2
  static_assert(exp2_q14(rational{ 2})              == 65536);  // 2^2  = 4
  static_assert(exp2_q14(rational{ 3})              == 131072); // 2^3  = 8
  static_assert(exp2_q14(rational{ 4})              == 262144); // 2^4  = 16 (boundary)
  static_assert(exp2_q14(-1_r)              == 8192);   // 2^-1 = 1/2
  static_assert(exp2_q14(-2_r)              == 4096);   // 2^-2 = 1/4
  static_assert(exp2_q14(-3_r)              == 2048);   // 2^-3 = 1/8
  static_assert(exp2_q14(-4_r)              == 1024);   // 2^-4 = 1/16
}

TEST_CASE("bnd::math::exp2: half-integer (irrational results)",
          "[cmath][exp2][constexpr]")
{
  // 2^(1/2) = √2 ≈ 1.41421356 → Q.14 ≈ 23170.
  static_assert(exp2_q14(0.5_r) == 23170);
  // 2^(-1/2) = 1/√2 ≈ 0.70710678 → Q.14 ≈ 11585.
  static_assert(exp2_q14(-0.5_r) == 11585);
  // 2^(3/2) = 2√2 ≈ 2.82842712 → Q.14 ≈ 46341.
  static_assert(exp2_q14(1.5_r) == 46341);
  // 2^(1/4) ≈ 1.18920711 → Q.14 ≈ 19484.
  static_assert(exp2_q14(0.25_r) == 19484);
}

TEST_CASE("bnd::math::exp2: decimal display", "[cmath][exp2][decimal]")
{
  REQUIRE(exp2_out_t{math::exp2(exp2_from(rational{ 0}))} == exp2_out_t{1});
  REQUIRE(exp2_out_t{math::exp2(exp2_from(rational{ 1}))} == exp2_out_t{2});
  REQUIRE(exp2_out_t{math::exp2(exp2_from(-1_r))} == exp2_out_t{0.5_r});
  REQUIRE(exp2_out_t{math::exp2(exp2_from(-2_r))} == exp2_out_t{0.25_r});
}

TEST_CASE("bnd::math::exp2: probe (informational)", "[cmath][exp2][.probe]")
{
  for (int n = -8; n <= 8; ++n) {
    rational xv{n, 2};
    std::cout << "    exp2(" << xv << ") = "
              << exp2_out_t{math::exp2(exp2_from(xv))} << "\n";
  }
}

//---------------------------------------------------------------------------
// log2: atanh-based reduction. Input is positive, output is signed.
//---------------------------------------------------------------------------
namespace
{
  using log2_in_t  = bound<{{0x1p-8_r, 256}, notch<1, 16384>}, round_nearest | real>;
  using log2_out_t = bound<{{-8, 8}, notch<1, 16384>}, round_nearest | real>;

  constexpr log2_in_t log2_from(rational r) { return log2_in_t{r}; }

  constexpr int log2_q14(rational x)
  {
    // Out has Lower = -8 → Raw = (value + 8) · 16384. Subtract the offset.
    return static_cast<int>(log2_out_t{math::log2(log2_from(x))}.raw())
         - 8 * 16384;
  }
}

TEST_CASE("bnd::math::log2: integer powers", "[cmath][log2][constexpr]")
{
  static_assert(log2_q14(1_r)       == 0);
  static_assert(log2_q14(2_r)       == 16384);    // log2(2) = 1
  static_assert(log2_q14(4_r)       == 32768);    // log2(4) = 2
  static_assert(log2_q14(8_r)       == 49152);    // log2(8) = 3
  static_assert(log2_q14(16_r)      == 65536);    // log2(16) = 4
  static_assert(log2_q14(0.5_r)   == -16384);   // log2(1/2) = -1
  static_assert(log2_q14(0.25_r)   == -32768);
  static_assert(log2_q14(0.125_r)   == -49152);
  static_assert(log2_q14(0.0625_r)   == -65536);
}

TEST_CASE("bnd::math::log2: irrational results", "[cmath][log2][constexpr]")
{
  // log2(√2) = 0.5 → Q.14 = 8192.
  static_assert(log2_q14(rational{23170, 16384}) == 8192);

  // log2(3) ≈ 1.5849625 → Q.14 ≈ 25968.
  static_assert(log2_q14(3_r) == 25968);

  // log2(1.5) ≈ 0.5849625 → Q.14 ≈ 9584.
  static_assert(log2_q14(1.5_r) == 9584);

  // log2(10) ≈ 3.3219281 → Q.14 ≈ 54426.
  static_assert(log2_q14(10_r) == 54426);
}

TEST_CASE("bnd::math::log2: decimal display", "[cmath][log2][decimal]")
{
  REQUIRE(log2_out_t{math::log2(log2_from(1_r))} == log2_out_t{0});
  REQUIRE(log2_out_t{math::log2(log2_from(2_r))} == log2_out_t{1});
  REQUIRE(log2_out_t{math::log2(log2_from(4_r))} == log2_out_t{2});
  REQUIRE(log2_out_t{math::log2(log2_from(0.5_r))} == log2_out_t{-1});
}

TEST_CASE("bnd::math::log2: exp2 round-trip", "[cmath][log2][exp2]")
{
  // log2(exp2(x)) should round-trip to x. The intermediate Q.14 of exp2's
  // output loses precision below 1/16384, so the round-trip is exact only
  // for inputs that land on the exp2 output grid.
  for (int n = -4; n <= 4; ++n) {
    auto e = exp2_out_t{math::exp2(exp2_from(rational{n}))};
    auto l = log2_out_t{math::log2(log2_in_t{e})};
    REQUIRE(l == log2_out_t{n});
  }
}

TEST_CASE("bnd::math::log2: probe (informational)", "[cmath][log2][.probe]")
{
  for (int n = -4; n <= 4; ++n) {
    rational xv = (n >= 0) ? rational{imax{1} << n} : rational{1, imax{1} << -n};
    std::cout << "    log2(" << xv << ") = "
              << log2_out_t{math::log2(log2_from(xv))} << "\n";
  }
  // Off-power-of-2 inputs:
  for (int n = 1; n <= 10; ++n)
    std::cout << "    log2(" << n << ") = "
              << log2_out_t{math::log2(log2_from(rational{n}))} << "\n";
}

//---------------------------------------------------------------------------
// exp / log / pow_base — wrappers built on exp2 / log2.
//---------------------------------------------------------------------------
namespace
{
  using exp_in_t   = bound<{{-10, 10}, notch<1, 16384>}, round_nearest | real>;
  using exp_out_t  = bound<{{0, 32768}, notch<1, 256>}, round_nearest | real>;
  using log_in_t   = bound<{{0x1p-8_r, 256}, notch<1, 256>}, round_nearest | real>;
  using log_out_t  = bound<{{-8, 8}, notch<1, 16384>}, round_nearest | real>;
  using pow10_in_t = bound<{{-9, 9}, notch<1, 16384>}, round_nearest | real>;
  using pow10_out_t = bound<{{0, 65536}, notch<1, 256>}, round_nearest | real>;
}

TEST_CASE("bnd::math::exp: integer points", "[cmath][exp][constexpr]")
{
  // exp(0) = 1 exactly.
  static_assert(exp_out_t{math::exp(exp_in_t{0})} == exp_out_t{1});
  // exp(1) = e ≈ 2.71828. Pinned to the algorithm's Q.8 output (256 notches/unit).
  // Q.8 of e: round(2.71828 · 256) = 696.
  REQUIRE(exp_out_t{math::exp(exp_in_t{1})} == exp_out_t{rational{696, 256}});
  // exp(-1) = 1/e ≈ 0.36788 → Q.8 = round(0.36788 · 256) = 94.
  REQUIRE(exp_out_t{math::exp(exp_in_t{-1})} == exp_out_t{rational{94, 256}});
  // exp(ln(2)) should be 2 (rounded to grid).
  REQUIRE(exp_out_t{math::exp(exp_in_t{0.693_r})} == exp_out_t{2});
}

TEST_CASE("bnd::math::log: integer points", "[cmath][log][constexpr]")
{
  // log(1) = 0 exactly.
  static_assert(log_out_t{math::log(log_in_t{1})} == log_out_t{0});
  // log(e) ≈ 1. log_in_t snaps 2.718 to nearest grid point (696/256 =
  // 2.71875), and log(2.71875) ≈ 1.000183 → Q.14 = 16387.
  REQUIRE(log_out_t{math::log(log_in_t{2.718_r})}
          == log_out_t{rational{16384, 16384}});
  // log(2) ≈ 0.69315 → after double rounding (Q.8 auto → Q.14 log_out_t)
  // it lands on 177/256 = 11328/16384.
  REQUIRE(log_out_t{math::log(log_in_t{2})}
          == log_out_t{rational{11328, 16384}});
}

TEST_CASE("bnd::math::pow_base<10>: integer powers",
          "[cmath][pow_base][constexpr]")
{
  // 10^0 = 1, 10^1 = 10, 10^2 = 100, 10^3 = 1000, …
  // pow10_out_t has notch 1/256, so values pin exactly when on the grid.
  static_assert(pow10_out_t{math::pow_base<10>(pow10_in_t{0})}
                == pow10_out_t{1});
  REQUIRE(pow10_out_t{math::pow_base<10>(pow10_in_t{1})}
          == pow10_out_t{10});
  REQUIRE(pow10_out_t{math::pow_base<10>(pow10_in_t{2})}
          == pow10_out_t{100});
  REQUIRE(pow10_out_t{math::pow_base<10>(pow10_in_t{3})}
          == pow10_out_t{1000});
  REQUIRE(pow10_out_t{math::pow_base<10>(pow10_in_t{4})}
          == pow10_out_t{10000});
  REQUIRE(pow10_out_t{math::pow_base<10>(pow10_in_t{-1})}
          == pow10_out_t{0.1_r});
  REQUIRE(pow10_out_t{math::pow_base<10>(pow10_in_t{-2})}
          == pow10_out_t{0.01_r});
}

TEST_CASE("bnd::math::pow_base<10>: db_to_linear endpoints",
          "[cmath][pow_base][audio]")
{
  // The audio-mixer dB use case: 10^(dB/20) for dB ∈ [-24, 12].
  // 10^(0/20) = 1, 10^(20/20) = 10, 10^(-20/20) = 0.1.
  REQUIRE(pow10_out_t{math::pow_base<10>(pow10_in_t{0_r})}
          == pow10_out_t{1});
  REQUIRE(pow10_out_t{math::pow_base<10>(pow10_in_t{1_r})}
          == pow10_out_t{10});
  // 10^(6/20) = 10^0.3 ≈ 1.9953 (the "+6 dB doubles" approximation).
  // Algorithm rounds to 511/256 = 1.99609 (~0.04% above ideal).
  REQUIRE(pow10_out_t{math::pow_base<10>(pow10_in_t{0.3_r})}
          == pow10_out_t{rational{511, 256}});
}

TEST_CASE("bnd::math: exp/log/pow_base decimal probe",
          "[cmath][exp][log][pow_base][.probe]")
{
  std::cout << "\n  exp sweep:\n";
  for (int n = -3; n <= 3; ++n)
    std::cout << "    exp(" << n << ") = "
              << exp_out_t{math::exp(exp_in_t{n})} << "\n";

  std::cout << "\n  log sweep:\n";
  for (int n : {1, 2, 3, 5, 10, 100, 1000})
    std::cout << "    log(" << n << ") = "
              << log_out_t{math::log(log_in_t{n})} << "\n";

  std::cout << "\n  pow_base<10> sweep (db_to_linear shape):\n";
  for (int db : {-24, -12, -6, -3, 0, 3, 6, 12})
    std::cout << "    10^(" << db << "/20) = "
              << pow10_out_t{math::pow_base<10>(
                   pow10_in_t{rational{db, 20}})} << "\n";
}

//---------------------------------------------------------------------------
// atan2: CORDIC family. Inputs are signed bounds in [-1, 1]; output is an
// angle in radians covering [-π, π].
//---------------------------------------------------------------------------
namespace
{
  using atan2_in_t  = bound<{{-1, 1}, notch<1, 16384>}, round_nearest | real>;
  // [-4, 4] is the smallest integer interval containing [-π, π] that divides
  // evenly by the notch (π is irrational, so the exact endpoints can't be a
  // grid bound against a rational notch).
  using atan2_out_t = bound<{{-4, 4}, notch<1, 16384>}, round_nearest | real>;

  constexpr rational atan2_rad(rational y, rational x)
  { return rational{atan2_out_t{math::atan2(atan2_in_t{y}, atan2_in_t{x})}}; }

  // Radian references derived from the library's own π.
  constexpr rational kPi      = math::detail::pi_r;
  constexpr rational kHalfPi  = rational::mul_unchecked(kPi, rational{1, 2});
  constexpr rational kQrtPi   = rational::mul_unchecked(kPi, rational{1, 4});
  constexpr rational k3QrtPi  = rational::mul_unchecked(kPi, rational{3, 4});
  // A handful of Q.14 notches absorbs CORDIC + quantization drift.
  constexpr rational kTol{8, 16384};
  constexpr bool near_rad(rational got, rational want)
  { return bnd::detail::abs((got - want).value()) <= kTol; }
}

TEST_CASE("bnd::math::atan2: axis cases", "[cmath][atan2][constexpr]")
{
  static_assert(near_rad(atan2_rad(0_r,  1_r),  rational{0}));   // atan2(0, 1) = 0
  static_assert(near_rad(atan2_rad(1_r,  0_r),  kHalfPi));       // atan2(1, 0) = +π/2
  static_assert(near_rad(atan2_rad(0_r, -1_r),  kPi));           // atan2(0,-1) = +π
  static_assert(near_rad(atan2_rad(-1_r, 0_r), -kHalfPi));       // atan2(-1,0) = -π/2
  static_assert(near_rad(atan2_rad(0_r,  0_r),  rational{0}));   // convention
}

TEST_CASE("bnd::math::atan2: diagonal cases", "[cmath][atan2][constexpr]")
{
  static_assert(near_rad(atan2_rad(rational{1}, rational{1}),  kQrtPi));   // +π/4
  static_assert(near_rad(atan2_rad(-1_r, rational{1}),        -kQrtPi));   // -π/4
  static_assert(near_rad(atan2_rad(rational{1}, -1_r),         k3QrtPi));  // +3π/4
  static_assert(near_rad(atan2_rad(-1_r, -1_r),              -k3QrtPi));   // -3π/4
}

TEST_CASE("bnd::math::atan2: decimal display", "[cmath][atan2][decimal]")
{
  REQUIRE(near_rad(atan2_rad(0_r,  1_r), rational{0}));
  REQUIRE(near_rad(atan2_rad(1_r,  0_r), kHalfPi));
  REQUIRE(near_rad(atan2_rad(-1_r, 0_r), -kHalfPi));
  REQUIRE(near_rad(atan2_rad(1_r,  1_r), kQrtPi));
}

TEST_CASE("bnd::math::atan2: sin/cos round-trip",
          "[cmath][atan2][sin][cos]")
{
  // For any phase φ, atan2(sin φ, cos φ) should round-trip back to φ
  // (modulo Q.14 grid rounding from the trip through sample_t). The
  // sweep walks 16 evenly-spaced Q.14 turn slots; each slot is converted
  // to a radians angle (turn × 2π) before feeding the public sin/cos.
  // Angle bound uses round integer endpoints that divide evenly by the
  // notch (the grid validator requires `(Upper - Lower) / Notch` be
  // integer). ±8 rad comfortably covers ±2π for sweep tests.
  using angle_t = bound<{{-8, 8}, notch<1, 16384>}, round_nearest | real>;
  const rational two_pi_r = math::detail::two_pi_r;
  for (unsigned i = 0; i < 16; ++i) {
    unsigned phase_raw = i * 1024;  // 0..15/16 turn in Q.14 raw
    rational turn_r{static_cast<imax>(phase_raw), 16384};
    rational ang_r{rational::mul_unchecked(turn_r, two_pi_r)};  // 0..2π
    angle_t  phase{ang_r};
    auto s = atan2_in_t{math::sin(phase)};
    auto c = atan2_in_t{math::cos(phase)};
    rational recovered = rational{atan2_out_t{math::atan2(s, c)}};  // (-π, π] rad

    // Expected angle wrapped into (-π, π] to match atan2's range.
    rational want = (ang_r > kPi) ? (ang_r - two_pi_r).value() : ang_r;

    // Diff, wrapped across the ±π seam (φ = ±π may land on either side).
    rational diff = (recovered - want).value();
    if (diff >  kPi) diff = (diff - two_pi_r).value();
    if (diff < -kPi) diff = (diff + two_pi_r).value();

    // sin/cos quantize to 1/16384 before atan2 sees them; allow a few notches.
    REQUIRE(bnd::detail::abs(diff) <= rational{16, 16384});
  }
}

TEST_CASE("bnd::math::atan2: probe (informational)",
          "[cmath][atan2][.probe]")
{
  std::cout << "\n  atan2 axis cases:\n";
  std::cout << "    atan2( 0,  1) = "
            << math::atan2(atan2_in_t{ 0}, atan2_in_t{ 1}) << "\n";
  std::cout << "    atan2( 1,  0) = "
            << math::atan2(atan2_in_t{ 1}, atan2_in_t{ 0}) << "\n";
  std::cout << "    atan2( 0, -1) = "
            << math::atan2(atan2_in_t{ 0}, atan2_in_t{-1}) << "\n";
  std::cout << "    atan2(-1,  0) = "
            << math::atan2(atan2_in_t{-1}, atan2_in_t{ 0}) << "\n";

  std::cout << "\n  atan2 unit-circle sweep (cos, sin):\n";
  for (int i = 0; i < 8; ++i) {
    // 8 evenly-spaced angles around the circle. Use exact ratios where possible.
    rational y, x;
    switch (i) {
      case 0: y = rational{ 0}; x = rational{ 1}; break;  //   0°
      case 1: y = rational{ 1}; x = rational{ 1}; break;  //  45°
      case 2: y = rational{ 1}; x = rational{ 0}; break;  //  90°
      case 3: y = rational{ 1}; x = -1_r; break;  // 135°
      case 4: y = rational{ 0}; x = -1_r; break;  // 180°
      case 5: y = -1_r; x = -1_r; break;  // 225°
      case 6: y = -1_r; x = rational{ 0}; break;  // 270°
      case 7: y = -1_r; x = rational{ 1}; break;  // 315°
    }
    std::cout << "    atan2(" << y << ", " << x << ") = "
              << math::atan2(atan2_in_t{y}, atan2_in_t{x}) << "\n";
  }
}

//---------------------------------------------------------------------------
// tan: sin/cos with a pole guard. First function in the library that
// returns slim::expected<Out, errc> — the pattern for any function whose
// output depends on a runtime domain check.
//---------------------------------------------------------------------------
namespace
{
  // Output covers [-10, 10]: tan in this range corresponds to phases within
  // ~5.7° of the equator (well inside any quadrant). Narrower outputs would
  // trip the overflow branch at fewer "near-pole" inputs.
  using tan_out_t = bound<{{-10, 10}, notch<1, 1024>}, round_nearest | real>;

  constexpr math::detail::turns_t<16> tan_phase_from(unsigned raw)
  {
    using P = math::detail::turns_t<16>;
    return P::from_raw(static_cast<typename P::raw_type>(raw));
  }

  // Helper: extract the Q.10 amplitude from the auto-deduced tan result
  // by constructing tan_out_t (Lower=-10, notch 1/1024).
  constexpr int tan_q10(unsigned phase_raw)
  {
    auto r = tan_turn(tan_phase_from(phase_raw));
    if (!r) return -999999;
    return static_cast<int>(tan_out_t{r.value()}.raw()) - 10 * 1024;
  }
}

TEST_CASE("bnd::math::tan: exact-zero phases", "[cmath][tan][constexpr]")
{
  static_assert(tan_q10(0)     == 0);          // tan(0)  = 0
  static_assert(tan_q10(32768) == 0);          // tan(π)  = 0
}

TEST_CASE("bnd::math::tan: ±π/4 = ±1", "[cmath][tan][constexpr]")
{
  // tan(π/4) = 1 → Q.10 = 1024.
  static_assert(tan_q10(8192)  ==  1024);
  // tan(3π/4) = -1 → Q.10 = -1024.
  static_assert(tan_q10(24576) == -1024);
  // tan(5π/4) = 1.
  static_assert(tan_q10(40960) ==  1024);
  // tan(7π/4) = -1.
  static_assert(tan_q10(57344) == -1024);
}

TEST_CASE("bnd::math::tan: pole returns division_by_zero",
          "[cmath][tan][error]")
{
  // tan(π/2): raw 16384 lands exactly on a pole.
  auto r = tan_turn(tan_phase_from(16384));
  REQUIRE_FALSE(r.has_value());
  REQUIRE(r.error() == errc::division_by_zero);

  // tan(3π/2): raw 49152 also on a pole.
  auto r2 = tan_turn(tan_phase_from(49152));
  REQUIRE_FALSE(r2.has_value());
  REQUIRE(r2.error() == errc::division_by_zero);
}

TEST_CASE("bnd::math::tan: near-pole returns overflow",
          "[cmath][tan][error]")
{
  // One Q.16 phase slot off the π/2 pole. tan jumps to a huge value.
  // tan(π/2 - δ) ≈ 1/δ, δ ≈ 9.6e-5 rad → tan ≈ 1.04e4, exceeding the
  // auto-deduced [−1024, 1024] envelope.
  auto r = tan_turn(tan_phase_from(16383));
  REQUIRE_FALSE(r.has_value());
  REQUIRE(r.error() == errc::overflow);
}

TEST_CASE("bnd::math::tan: decimal display", "[cmath][tan][decimal]")
{
  REQUIRE(tan_out_t{tan_turn(tan_phase_from(0)).value()}     == tan_out_t{0});
  REQUIRE(tan_out_t{tan_turn(tan_phase_from(8192)).value()}  == tan_out_t{1});
  REQUIRE(tan_out_t{tan_turn(tan_phase_from(24576)).value()} == tan_out_t{-1});
}

TEST_CASE("bnd::math::tan: probe (informational)", "[cmath][tan][.probe]")
{
  std::cout << "\n  tan sweep:\n";
  for (unsigned i = 0; i <= 65536u; i += 4096) {
    auto r = tan_turn(tan_phase_from(i % 65536));
    std::cout << "    tan(" << i << "/65536 turn) = ";
    if (r.has_value()) std::cout << r.value();
    else if (r.error() == errc::division_by_zero) std::cout << "[pole]";
    else                                          std::cout << "[overflow]";
    std::cout << "\n";
  }
}

//---------------------------------------------------------------------------
// Algebraic tier — abs/floor/ceil/round/trunc/fmod. Exact, no polynomials.
//---------------------------------------------------------------------------
namespace
{
  using algeb_in_t  = bound<{{-8, 8}, notch<1, 16384>}, round_nearest | real>;
  using algeb_abs_t = bound<{{0, 8}, notch<1, 16384>}, round_nearest | real>;
  using algeb_int_t = bound<{{-8, 8}, notch<1>}, round_nearest | real>;
}

TEST_CASE("bnd::math::abs", "[cmath][algebraic][constexpr]")
{
  static_assert(algeb_abs_t{math::abs(algeb_in_t{rational{ 5, 2}})}
                == algeb_abs_t{2.5_r});
  static_assert(algeb_abs_t{math::abs(algeb_in_t{-2.5_r})}
                == algeb_abs_t{2.5_r});
  static_assert(algeb_abs_t{math::abs(algeb_in_t{0})}
                == algeb_abs_t{0});
}

TEST_CASE("bnd::math::floor / ceil / round / trunc",
          "[cmath][algebraic][constexpr]")
{
  // floor: rounds toward -∞.
  static_assert(algeb_int_t{math::floor(algeb_in_t{rational{ 17, 10}})}
                == algeb_int_t{ 1});   // floor(1.7) = 1
  static_assert(algeb_int_t{math::floor(algeb_in_t{rational{-17, 10}})}
                == algeb_int_t{-2});   // floor(-1.7) = -2

  // ceil: rounds toward +∞.
  static_assert(algeb_int_t{math::ceil(algeb_in_t{rational{ 13, 10}})}
                == algeb_int_t{ 2});   // ceil(1.3) = 2
  static_assert(algeb_int_t{math::ceil(algeb_in_t{rational{-13, 10}})}
                == algeb_int_t{-1});   // ceil(-1.3) = -1

  // round: half-away-from-zero.
  static_assert(algeb_int_t{math::round(algeb_in_t{rational{ 3, 2}})}
                == algeb_int_t{ 2});   // round(1.5) = 2
  static_assert(algeb_int_t{math::round(algeb_in_t{-1.5_r})}
                == algeb_int_t{-2});   // round(-1.5) = -2
  static_assert(algeb_int_t{math::round(algeb_in_t{rational{ 13, 10}})}
                == algeb_int_t{ 1});   // round(1.3) = 1

  // trunc: toward zero. Distinct from floor for negatives.
  static_assert(algeb_int_t{math::trunc(algeb_in_t{rational{ 17, 10}})}
                == algeb_int_t{ 1});   // trunc(1.7) = 1
  static_assert(algeb_int_t{math::trunc(algeb_in_t{rational{-17, 10}})}
                == algeb_int_t{-1});   // trunc(-1.7) = -1
}

TEST_CASE("bnd::math::fmod", "[cmath][algebraic][constexpr]")
{
  // Positive dividend, positive divisor.
  static_assert(algeb_in_t{math::fmod(algeb_in_t{7_r},
                                       algeb_in_t{3_r})}
                == algeb_in_t{1});

  // Negative dividend keeps its sign (trunc-division convention).
  static_assert(algeb_in_t{math::fmod(algeb_in_t{-7_r},
                                       algeb_in_t{3_r})}
                == algeb_in_t{-1});

  // Non-integer dividend: fmod(5.5, 2) = 1.5.
  static_assert(algeb_in_t{math::fmod(algeb_in_t{5.5_r},
                                       algeb_in_t{2_r})}
                == algeb_in_t{1.5_r});

  // Divisor doesn't divide evenly: fmod(7, 2.5) = 2.
  static_assert(algeb_in_t{math::fmod(algeb_in_t{7_r},
                                       algeb_in_t{2.5_r})}
                == algeb_in_t{2});
}

TEST_CASE("bnd::math: algebraic tier decimal probe",
          "[cmath][algebraic][.probe]")
{
  std::cout << "\n  algebraic tier:\n";
  std::cout << "    abs(-2.5)        = "
            << algeb_abs_t{math::abs(algeb_in_t{-2.5_r})} << "\n";
  std::cout << "    floor(1.7)       = "
            << algeb_int_t{math::floor(algeb_in_t{1.7_r})} << "\n";
  std::cout << "    ceil(-1.3)       = "
            << algeb_int_t{math::ceil(algeb_in_t{-1.3_r})} << "\n";
  std::cout << "    round(1.5)       = "
            << algeb_int_t{math::round(algeb_in_t{1.5_r})} << "\n";
  std::cout << "    trunc(-1.7)      = "
            << algeb_int_t{math::trunc(algeb_in_t{-1.7_r})} << "\n";
  std::cout << "    fmod(7, 3)       = "
            << algeb_in_t{math::fmod(algeb_in_t{7_r},
                                      algeb_in_t{3_r})} << "\n";
  std::cout << "    fmod(5.5, 2)     = "
            << algeb_in_t{math::fmod(algeb_in_t{5.5_r},
                                      algeb_in_t{2_r})} << "\n";
}

//---------------------------------------------------------------------------
// Auto-deduction phase 1 — algebraic tier. Validates that the auto-form
// overload (`math::abs(x)` etc.) produces a bound whose value equals what
// the explicit form would produce on the deduced type.
//---------------------------------------------------------------------------
TEST_CASE("bnd::math::abs: auto-deduced output", "[cmath][algebraic][auto]")
{
  // Input is signed [-8, 8]; auto output range is [0, 8] with same notch.
  constexpr auto y_neg = math::abs(algeb_in_t{-2.5_r});
  constexpr auto y_pos = math::abs(algeb_in_t{rational{ 5, 2}});
  static_assert(y_neg == y_pos);
  // Result still compares equal to a sample_t-shape rational value.
  REQUIRE(y_neg == 2.5_r);

  // Deduced output type covers [0, 8] with notch 1/16384.
  using deduced = decltype(math::abs(algeb_in_t{0}));
  static_assert(Lower<deduced> == 0);
  static_assert(Upper<deduced> == 8);
  static_assert(Notch<deduced> == bnd::notch<1, 16384>);
}

TEST_CASE("bnd::math::floor / ceil / round / trunc: auto-deduced output",
          "[cmath][algebraic][auto]")
{
  // floor: [-8, 8] input → [-8, 8] integer output, notch<1>.
  constexpr auto f_pos = math::floor(algeb_in_t{1.7_r});
  static_assert(f_pos == 1);
  using floor_deduced = decltype(math::floor(algeb_in_t{0}));
  static_assert(Notch<floor_deduced> == bnd::notch<1>);

  // ceil
  constexpr auto c_neg = math::ceil(algeb_in_t{-1.3_r});
  static_assert(c_neg == -1);

  // round
  constexpr auto r_pos = math::round(algeb_in_t{1.5_r});
  static_assert(r_pos == 2);

  // trunc
  constexpr auto t_neg = math::trunc(algeb_in_t{-1.7_r});
  static_assert(t_neg == -1);
}

TEST_CASE("bnd::math: explicit and auto forms produce identical values",
          "[cmath][algebraic][auto]")
{
  // With explicit forms removed (auto is the only public surface), there's
  // no separate "explicit vs auto" comparison to make. Spot-check the auto
  // form values directly.
  constexpr algeb_in_t x{rational{-7, 3}};   // ≈ -2.333

  // abs's auto type uses Notch<In> = 1/16384, so 7/3 snaps to nearest grid
  // point: round(7/3 · 16384) = 38229 → 38229/16384.
  REQUIRE(rational{math::abs(x)}   == rational{38229, 16384});
  REQUIRE(rational{math::floor(x)} == -3);
  REQUIRE(rational{math::ceil(x)}  == -2);
  REQUIRE(rational{math::round(x)} == -2);
  REQUIRE(rational{math::trunc(x)} == -2);
}

TEST_CASE("bnd::math: auto-form composition", "[cmath][algebraic][auto]")
{
  // The composition floor(abs(x)) auto-deduces an integer-storage result
  // from a Q.14 signed input. No explicit type spelled anywhere.
  constexpr algeb_in_t x{rational{-17, 10}};
  constexpr auto r = math::floor(math::abs(x));
  static_assert(r == 1);   // floor(|−1.7|) = floor(1.7) = 1
}

//---------------------------------------------------------------------------
// Auto-deduction phase 2 — monotonic transcendentals (sqrt, exp, exp2,
// log, log2, pow_base). Endpoints computed at compile time via the Q.30
// cores; deduced output range covers the true mathematical result.
//---------------------------------------------------------------------------
TEST_CASE("bnd::math::sqrt: auto-deduced output", "[cmath][sqrt][auto]")
{
  // Input [0, 4] with notch 1/65536. Auto output range: [0, ceil_notch(2)] = [0, 2].
  using deduced = decltype(math::sqrt(sqrt_in_t{0}));
  static_assert(Lower<deduced> == 0);
  static_assert(Upper<deduced> == 2);
  static_assert(Notch<deduced> == bnd::notch<1, 65536>);

  // Spot checks: integer perfect squares land exactly on the grid.
  REQUIRE(rational{math::sqrt(sqrt_input(65536))}  == 1); // √1 = 1
  REQUIRE(rational{math::sqrt(sqrt_input(262144))} == 2); // √4 = 2
  REQUIRE(rational{math::sqrt(sqrt_input(16384))}
          == 0.5_r);                                       // √0.25 = 0.5
}

TEST_CASE("bnd::math::exp2: auto-deduced output", "[cmath][exp2][auto]")
{
  using deduced = decltype(math::exp2(exp2_in_t{0}));
  // True range [1/16, 16]; deduced bound rounds outward to input's notch.
  static_assert(Lower<deduced> <= 0.0625_r);
  static_assert(Upper<deduced> >= 16);

  REQUIRE(rational{math::exp2(exp2_from(rational{ 0}))} == 1);
  REQUIRE(rational{math::exp2(exp2_from(rational{ 1}))} == 2);
  REQUIRE(rational{math::exp2(exp2_from(-1_r))} == 0.5_r);
}

TEST_CASE("bnd::math::log2: auto-deduced output", "[cmath][log2][auto]")
{
  using deduced = decltype(math::log2(log2_in_t{1}));
  static_assert(Lower<deduced> <= -8);
  static_assert(Upper<deduced> >= 8);

  REQUIRE(rational{math::log2(log2_from(1_r))} == 0);
  REQUIRE(rational{math::log2(log2_from(2_r))} == 1);
  REQUIRE(rational{math::log2(log2_from(0.5_r))} == -1);
}

TEST_CASE("bnd::math::exp / log: auto-deduced output",
          "[cmath][exp][log][auto]")
{
  // Identity points are exact on the algorithm's output.
  REQUIRE(rational{math::exp(exp_in_t{0})} == 1);
  REQUIRE(rational{math::log(log_in_t{1})} == 0);
}

TEST_CASE("bnd::math::pow_base<10>: auto-deduced output",
          "[cmath][pow_base][auto]")
{
  // Integer powers land exactly on the grid.
  REQUIRE(rational{math::pow_base<10>(pow10_in_t{0})} == 1);
  REQUIRE(rational{math::pow_base<10>(pow10_in_t{1})} == 10);
  REQUIRE(rational{math::pow_base<10>(pow10_in_t{2})} == 100);
}

TEST_CASE("bnd::math: phase-2 composition", "[cmath][auto]")
{
  // sqrt(exp2(x)) — both auto-deduced, no explicit type spelled.
  // exp2(2) = 4 → sqrt(4) = 2. Auto type chain works.
  auto e = math::exp2(exp2_in_t{2});
  auto s = math::sqrt(decltype(sqrt_in_t{0}){rational{e}});
  REQUIRE(rational{s} == 2);
}

//---------------------------------------------------------------------------
// Auto-deduction phase 3 — trig + atan2. Output ranges are hardcoded full
// ranges; notch inherits from input.
//---------------------------------------------------------------------------
TEST_CASE("bnd::math::sin / cos: auto-deduced output",
          "[cmath][sin][cos][auto]")
{
  // Public sin/cos take radians; the auto-deduced output is [-1, 1]
  // with `Notch<In>` inherited and `round_nearest` added to policy.
  // Angle bound uses round integer endpoints that divide evenly by the
  // notch (the grid validator requires `(Upper - Lower) / Notch` be
  // integer). ±8 rad comfortably covers ±2π for sweep tests.
  using angle_t = bound<{{-8, 8}, notch<1, 16384>}, round_nearest | real>;
  using deduced = decltype(math::sin(angle_t{0}));
  static_assert(Lower<deduced> == -1);
  static_assert(Upper<deduced> == rational{ 1});
  static_assert(Notch<deduced> == bnd::notch<1, 16384>);

  // Spot checks at exact radian angles. `math::pi` / `math::two_pi`
  // come from the public constants in `bound/cmath.hpp`. `div_unchecked`
  // is the unchecked rational division — the constants are small, so
  // overflow is impossible at compile time.
  constexpr rational half_pi = rational::div_unchecked(math::pi, 2_r);
  REQUIRE(rational{math::sin(angle_t{0})}             == 0);
  REQUIRE(rational{math::sin(angle_t{ half_pi})}      == 1);
  REQUIRE(rational{math::sin(angle_t{-half_pi})}      == -1);
  REQUIRE(rational{math::cos(angle_t{0})}             == 1);
  REQUIRE(rational{math::cos(angle_t{math::pi})}      == -1);
}

//---------------------------------------------------------------------------
// Radians identity points — sin/cos/tan at 0, ±π/2, π, 2π. The public
// trig path takes radians (the std::sin equivalent); these assertions
// pin the radians → Q.30 turn → polynomial chain end-to-end against
// well-known values. Off-grid radian inputs (e.g. `rational{1, 1}` = 1
// rad) also lock the conversion bit-pattern so a future refactor that
// changes the radian path is forced to update the expected ULP.
//---------------------------------------------------------------------------
TEST_CASE("bnd::math::sin: radians identity points",
          "[cmath][sin][radians]")
{
  using angle_t = bound<{{-8, 8}, notch<1, 16384>}, round_nearest | real>;
  using amp_t   = bound<{{-1, 1}, notch<1, 16384>}, round_nearest | real>;

  constexpr rational half_pi = rational::div_unchecked(math::pi, 2_r);

  // Exact values on the Q.14 grid (within ±1 ULP of mathematical truth).
  REQUIRE(amp_t{math::sin(angle_t{0})}            == amp_t{0});
  REQUIRE(amp_t{math::sin(angle_t{ half_pi})}     == amp_t{1});
  REQUIRE(amp_t{math::sin(angle_t{-half_pi})}     == amp_t{-1});

  // sin(π) and sin(2π) are 0 mathematically; the radian → Q.30 turn
  // conversion picks up at most a couple of Q.30 ULPs, which then round
  // to 0 on the Q.14 grid.
  REQUIRE(amp_t{math::sin(angle_t{math::pi})}     == amp_t{0});
  REQUIRE(amp_t{math::sin(angle_t{math::two_pi})} == amp_t{0});

  // Off-grid: 1 rad. sin(1) ≈ 0.8414709848. The radians → Q.30 turn →
  // Q.30 sin chain rounds to 13787/16384 (one Q.14 ULP below the
  // mathematical 13788). Pinned to the algorithm's actual output.
  REQUIRE(amp_t{math::sin(angle_t{1_r})}
          == amp_t{rational{13787, 16384}});
}

TEST_CASE("bnd::math::cos: radians identity points",
          "[cmath][cos][radians]")
{
  using angle_t = bound<{{-8, 8}, notch<1, 16384>}, round_nearest | real>;
  using amp_t   = bound<{{-1, 1}, notch<1, 16384>}, round_nearest | real>;

  constexpr rational half_pi = rational::div_unchecked(math::pi, 2_r);

  REQUIRE(amp_t{math::cos(angle_t{0})}            == amp_t{ 1});
  REQUIRE(amp_t{math::cos(angle_t{ half_pi})}     == amp_t{ 0});
  REQUIRE(amp_t{math::cos(angle_t{-half_pi})}     == amp_t{ 0});
  REQUIRE(amp_t{math::cos(angle_t{math::pi})}     == amp_t{-1});
  REQUIRE(amp_t{math::cos(angle_t{math::two_pi})} == amp_t{ 1});

  // cos(1) ≈ 0.5403023059 → Q.14 = 8852/16384.
  REQUIRE(amp_t{math::cos(angle_t{1_r})}
          == amp_t{rational{8852, 16384}});
}

TEST_CASE("bnd::math::tan: radians identity points",
          "[cmath][tan][radians]")
{
  using angle_t = bound<{{-8, 8}, notch<1, 16384>}, round_nearest | real>;
  using out_t   = bound<{{-10, 10}, notch<1, 1024>}, round_nearest | real>;

  constexpr rational pi_over_4 = rational::div_unchecked(math::pi, 4_r);

  // tan(0) = 0, tan(±π/4) = ±1.
  auto r0 = math::tan(angle_t{0});
  REQUIRE(r0.has_value());
  REQUIRE(out_t{r0.value()} == out_t{0});

  auto rp = math::tan(angle_t{ pi_over_4});
  REQUIRE(rp.has_value());
  REQUIRE(out_t{rp.value()} == out_t{1});

  auto rn = math::tan(angle_t{-pi_over_4});
  REQUIRE(rn.has_value());
  REQUIRE(out_t{rn.value()} == out_t{-1});
}

TEST_CASE("bnd::math::atan2: auto-deduced output", "[cmath][atan2][auto]")
{
  using deduced = decltype(math::atan2(atan2_in_t{0}, atan2_in_t{0}));
  // Output covers [-π, π] in radians, rounded outward to the inherited notch.
  static_assert(Lower<deduced> <= -kPi);
  static_assert(Upper<deduced> >=  kPi);
  static_assert(Notch<deduced> == bnd::notch<1, 16384>);

  // Axis cases match the explicit form's behavior (radians).
  REQUIRE(near_rad(rational{math::atan2(atan2_in_t{0},  atan2_in_t{ 1})}, rational{0}));
  REQUIRE(near_rad(rational{math::atan2(atan2_in_t{1},  atan2_in_t{ 0})}, kHalfPi));
  REQUIRE(near_rad(rational{math::atan2(atan2_in_t{0},  atan2_in_t{-1})}, kPi));
  REQUIRE(near_rad(rational{math::atan2(atan2_in_t{-1}, atan2_in_t{ 0})}, -kHalfPi));
  REQUIRE(near_rad(rational{math::atan2(atan2_in_t{1},  atan2_in_t{ 1})}, kQrtPi));
}

TEST_CASE("bnd::math: full auto-form chain", "[cmath][auto]")
{
  // Compose three auto-deduced calls in a single expression. No explicit
  // bound types spelled in the chain itself; the type system carries the
  // precision and range information end-to-end. Angle bound is the only
  // type spelled — sin/cos/atan2 each auto-deduce their result shape.
  // Angle bound uses round integer endpoints that divide evenly by the
  // notch (the grid validator requires `(Upper - Lower) / Notch` be
  // integer). ±8 rad comfortably covers ±2π for sweep tests.
  using angle_t = bound<{{-8, 8}, notch<1, 16384>}, round_nearest | real>;
  constexpr rational pi_over_4 = rational::div_unchecked(math::pi, 4_r);
  angle_t p{pi_over_4};              // π/4 → expect sin = cos = √2/2

  auto s = math::sin(p);             // ≈ √2/2
  auto c = math::cos(p);             // ≈ √2/2
  auto angle = math::atan2(s, c);    // back to ≈ π/4 radians

  // Should round-trip to π/4 within the grid + CORDIC tolerance.
  REQUIRE(near_rad(rational{angle}, kQrtPi));
}

TEST_CASE("bnd::math reductions on a bound",
          "[cmath][reduce]")
{
  using fxd = bound<{{-3.5_r, 3.5_r}, notch<1, 4>}>;
  fxd x{-1.75_r};
  fxd y{2.25_r};

  // Auto-deduced output grid (integer-valued, notch 1).
  REQUIRE(math::floor(x) == -2);
  REQUIRE(math::ceil (x) == -1);
  REQUIRE(math::round(x) == -2);
  REQUIRE(math::trunc(x) == -1);
  REQUIRE(math::abs  (x) == 1.75_r);

  REQUIRE(math::floor(y) ==  2);
  REQUIRE(math::ceil (y) ==  3);
  REQUIRE(math::round(y) ==  2);
  REQUIRE(math::trunc(y) ==  2);

  // Explicit Out form picks a caller-chosen grid.
  using out = bound<{-5, 5}>;
  REQUIRE(math::floor_impl<out>(x) == -2);
  REQUIRE(math::ceil_impl <out>(y) ==  3);
}

TEST_CASE("bnd::math: decimal probe at the bound level",
          "[cmath][decimal][.probe]")
{
  std::cout << "\n  sin sweep (bound output, decimal):\n";
  for (unsigned i = 0; i <= 65536u; i += 8192)
    std::cout << "    sin(" << i << "/65536 turn) = "
              << sample_t{sin_turn(phase_from(i % 65536))} << "\n";

  std::cout << "\n  cos sweep (bound output, decimal):\n";
  for (unsigned i = 0; i <= 65536u; i += 8192)
    std::cout << "    cos(" << i << "/65536 turn) = "
              << sample_t{cos_turn(phase_from(i % 65536))} << "\n";

  std::cout << "\n  sqrt sweep (bound output, decimal):\n";
  for (unsigned i = 0; i <= 262144u; i += 32768)
    std::cout << "    sqrt(" << sqrt_input(i) << ") = "
              << sqrt_out_t{math::sqrt(sqrt_input(i))} << "\n";
}

//---------------------------------------------------------------------------
// Extended transcendentals (#3): inverse trig (radians), hyperbolic, log10,
// pow, cbrt, hypot. Transcendental outputs are irrational, so most checks use
// a small rational tolerance; algebraically-exact cases are pinned by
// `static_assert` to hold the bit-exact contract.
//---------------------------------------------------------------------------
namespace
{
  using inv_in2_t = bound<{{-1, 1}, notch<1, 1048576>}, round_nearest | real>;
  using hyp_in_t  = bound<{{-10, 10}, notch<1, 65536>}, round_nearest | real>;  // sinh/cosh/tanh envelope
  using cbrt_in_t = bound<{{-16, 16}, notch<1, 65536>}, round_nearest | real>;  // cbrt (wider envelope)
  using pos_in2_t = bound<{{1, 1024}, notch<1, 65536>}, round_nearest | real>;

  // True references from the library's own π.
  constexpr rational kPi_x     = math::detail::pi_r;
  constexpr rational kHalfPi_x = rational::mul_unchecked(kPi_x, rational{1, 2});

  constexpr rational approx_tol{1, 4096};   // ~2.4e-4, covers every grid here
  bool approx(rational got, rational want)
  { return bnd::detail::abs((got - want).value()) <= approx_tol; }
}

TEST_CASE("bnd::math::atan: known values (radians)", "[cmath][atan]")
{
  REQUIRE(approx(rational{math::atan(inv_in2_t{rational{1, 2}})}, rational{4636476, 10000000}));
  REQUIRE(approx(rational{math::atan(inv_in2_t{1})},  rational::mul_unchecked(kPi_x, rational{1, 4})));
  REQUIRE(approx(rational{math::atan(inv_in2_t{0})},  rational{0}));
  REQUIRE(approx(rational{math::atan(inv_in2_t{-1})}, rational::mul_unchecked(kPi_x, rational{-1, 4})));
}

TEST_CASE("bnd::math::asin / acos: known values (radians)", "[cmath][asin][acos]")
{
  REQUIRE(approx(rational{math::asin(inv_in2_t{0})},  rational{0}));
  REQUIRE(approx(rational{math::asin(inv_in2_t{1})},  kHalfPi_x));
  REQUIRE(approx(rational{math::asin(inv_in2_t{-1})}, -kHalfPi_x));
  REQUIRE(approx(rational{math::asin(inv_in2_t{rational{1, 2}})}, rational::mul_unchecked(kPi_x, rational{1, 6})));

  REQUIRE(approx(rational{math::acos(inv_in2_t{1})},  rational{0}));
  REQUIRE(approx(rational{math::acos(inv_in2_t{-1})}, kPi_x));
  REQUIRE(approx(rational{math::acos(inv_in2_t{0})},  kHalfPi_x));
}

TEST_CASE("bnd::math::sinh / cosh / tanh: known values", "[cmath][sinh][cosh][tanh]")
{
  // sinh(0)=0, cosh(0)=1, tanh(0)=0 — exact.
  REQUIRE(rational{math::sinh(hyp_in_t{0})} == 0);
  REQUIRE(rational{math::cosh(hyp_in_t{0})} == 1);
  REQUIRE(rational{math::tanh(hyp_in_t{0})} == 0);
  // sinh(1) ≈ 1.175201, cosh(1) ≈ 1.543081, tanh(1) ≈ 0.761594
  REQUIRE(approx(rational{math::sinh(hyp_in_t{1})}, rational{11752012, 10000000}));
  REQUIRE(approx(rational{math::cosh(hyp_in_t{1})}, rational{15430806, 10000000}));
  REQUIRE(approx(rational{math::tanh(hyp_in_t{1})}, rational{7615942, 10000000}));
  // cosh is even.
  REQUIRE(approx(rational{math::cosh(hyp_in_t{-2})}, rational{math::cosh(hyp_in_t{2})}));
}

TEST_CASE("bnd::math::log10: known values", "[cmath][log10]")
{
  REQUIRE(approx(rational{math::log10(pos_in2_t{1})},   rational{0}));
  REQUIRE(approx(rational{math::log10(pos_in2_t{10})},  rational{1}));
  REQUIRE(approx(rational{math::log10(pos_in2_t{100})}, rational{2}));
  REQUIRE(approx(rational{math::log10(pos_in2_t{2})},   rational{30103, 100000}));
}

TEST_CASE("bnd::math::cbrt: exact and signed", "[cmath][cbrt][constexpr]")
{
  // Perfect cubes of powers of two are exact through the Q.30 log/exp cores.
  static_assert(rational{math::cbrt(cbrt_in_t{8})}  == 2);
  static_assert(rational{math::cbrt(cbrt_in_t{-8})} == -2);
  static_assert(rational{math::cbrt(cbrt_in_t{0})}  == 0);
  static_assert(rational{math::cbrt(cbrt_in_t{1})}  == 1);
  REQUIRE(approx(rational{math::cbrt(cbrt_in_t{2})}, rational{12599210, 10000000}));
}

TEST_CASE("bnd::math::hypot: Pythagorean", "[cmath][hypot]")
{
  using h_t = bound<{{-16, 16}, notch<1, 65536>}, round_nearest | real>;
  REQUIRE(approx(rational{math::hypot(h_t{3}, h_t{4})},  rational{5}));
  REQUIRE(approx(rational{math::hypot(h_t{5}, h_t{12})}, rational{13}));
  REQUIRE(approx(rational{math::hypot(h_t{0}, h_t{0})},  rational{0}));
  REQUIRE(approx(rational{math::hypot(h_t{-3}, h_t{4})}, rational{5}));
}

TEST_CASE("bnd::math::pow: base^exp with expected", "[cmath][pow]")
{
  using b_t = bound<{{1, 16}, notch<1, 65536>}, round_nearest | real>;
  using e_t = bound<{{-8, 16}, notch<1, 65536>}, round_nearest | real>;
  auto p1 = math::pow(b_t{2}, e_t{10});      // 1024
  REQUIRE(p1.has_value());
  REQUIRE(approx(rational{*p1}, rational{1024}));
  auto p2 = math::pow(b_t{2}, e_t{0});       // 1
  REQUIRE(p2.has_value());
  REQUIRE(approx(rational{*p2}, rational{1}));
  auto p3 = math::pow(b_t{9}, e_t{rational{1, 2}});  // 3
  REQUIRE(p3.has_value());
  REQUIRE(approx(rational{*p3}, rational{3}));
}

#endif // BND_MATH_FIXED
