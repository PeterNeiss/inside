// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//---------------------------------------------------------------------------
// inside.certify — the library's own certified math pairs (certify/
// certify.hpp has the tool). Every input of each pair is computed as a
// run-time call and on the integer path alone; a pair is certified when the
// two agree everywhere and no result is a guess. docs/certified.md is this
// program's table, and the test checks every hash against it:
//
//   ./inside.certify --estimate                the pairs and the time they take
//   ./inside.certify                           certify them (seconds, optimized)
//   ./inside.certify --check docs/certified.md and match the committed hashes
//   ./inside.certify --markdown docs/certified.md   regenerate the page
//
// An optimized build runs every pair, a debug build the pairs up to 4096
// inputs.
//---------------------------------------------------------------------------
#include "certify/certify.hpp"

using namespace ins;
namespace am = ins::math;

namespace {
// Inputs.
using sym4       = id<"[-4..4] 1/262144 round_half_away">;         // 2^21 + 1 inputs
using sym8       = id<"[-8..8] 1/65536 round_half_away">;         // 2^20 + 1
using symdec     = id<"[-4..4] 1/100'000 round_half_away">; // 800 001
using unit_value = id<"[-1..1] 1/524288 round_half_away">;         // 2^20 + 1
using open1      = id<"[-1023/1024..1023/1024] 1/1024 round_half_away">;
using pos        = id<"[1/1024..1024] 1/1024 round_half_away">; // 2^20
using ge1        = id<"[1..1025] 1/1024 round_half_away">;                 // 2^20 + 1
using small      = id<"[-2..2] 1/16 round_half_away">;                     // 65
using side       = id<"[-16..16] 1/32 round_half_away">;                   // 1025
using base       = id<"[1/64..16] 1/64 round_half_away">;       // 1024
using expo       = id<"[-4..4] 1/8 round_half_away">;                      // 65

// Outputs: coarse (a 4-bit notch: start precision 16, cap 64 bits), fine,
// decimal, and one past the double tier (the dd tier: a 2^-52 notch).
using c4f  = id<"[-64..64] 1/16 round_floor|clamp">;
using c4c  = id<"[-64..64] 1/16 round_ceil|clamp">;
using c4n  = id<"[-64..64] 1/16 round_half_away|clamp">;
using f20n = id<"[-64..64] 1/1048576 round_half_away|clamp">;
using f20e = id<"[-64..64] 1/1048576 round_half_even|clamp">;
using d3n  = id<"[-64..64] 1/1000 round_half_away|clamp">;
using d3t  = id<"[-64..64] 1/1000 round_toward_zero|clamp">;
using dd52 = id<key{grid{{-4, 4}, rational{1, std::uint64_t{1} << 52}}, round_half_away | clamp}>;
using big  = id<"[0..1048576] 1/16 round_floor|clamp">;    // exp, cosh of sym8
using wide = id<"[-1048576..1048576] 1/16 round_half_away">; // tan, pow: overflow reported
// tan's inputs: one branch each (an interval holding a pole is a compile
// error), the one around 0 and the next, (π/2, 3π/2).
using branch0 = id<"[-25/16..25/16] 1/262144 round_half_away">; // 819 201
using branch1 = id<"[51/32..75/16] 1/262144 round_half_away">;  // 811 009

// The examples' pairs, spelled as the number grids each math call sees
// (find_grids.py over a debug build lists them; its test fails when an
// example pair has no row here). The alias each grid comes from is named.
namespace ex {
// navigation.cpp
using coord    = id<"[-1..1] 1/16384 round_half_away">;                                       // coord_t
using angle    = id<"[-3217/1024..3217/1024] 1/16384 round_half_away">; // atan2's
using radius   = id<"[0..23171/16384] 1/16384 round_half_away">; // hypot(coord_t, coord_t)
using ratio    = id<"[-1..1] 1/4096 round_half_away">;                      // ratio_t
using asin_r   = id<"[-3217/2048..3217/2048] 1/4096 round_half_away">;
using acos_r   = id<"[0..3217/1024] 1/4096 round_half_away">;
using position = id<"[-1024..1024] 1/256 round_half_away">; // pos_t's number
using fine =
    id<key{grid{{rational{-3294199, 1 << 20}, rational{3294199, 1 << 20}}, per<1 << 20>}, round_half_away}>; // atan2's
using range = id<"[0..46341/32] 1/256 round_half_away">; // math::hypot(pos_t, pos_t)
// audio_math.cpp
using t_tau    = id<"[-4..4] 1/1024 round_half_away">; // seconds_t / tau (keeps the shared policy)
using amp      = id<"[75/4096..894537/16384] 1/16384 round_half_away">; // exp's
using exponent = id<"[0..4] 1/1024 round_half_away">;                                         // exponent_t
using half     = id<"[0..4] 1/2 round_half_away">;      // the half-octave cursor
using mult     = id<"[1..16] 1/16384 round_half_away">; // exp2's
using drive    = id<"[-4..4] 1/2048 round_half_away">;  // drive_t
using tanh_r   = id<"[-2047/2048..2047/2048] 1/2048 round_half_away">;
using sinh_r   = id<"[-27945/1024..27945/1024] 1/2048 round_half_away">;
using cosh_r   = id<"[1..6991/256] 1/2048 round_half_away">;
using octaves  = id<"[1/16..16] 1/1024 round_half_away">; // ratio_t
using log2_r   = id<"[-4..4] 1/1024 round_half_away">;
using vol      = id<"[0..64] 1/256 round_half_away">; // vol_t
using cbrt_r   = id<"[0..4] 1/256 round_half_away">;
// units.cpp
using rad  = id<"[0..7] 1/1'000'000 round_half_away">; // angle
using trig = id<"[-1..1] 1/1'000'000 round_half_away">;
// decibels.cpp, audio_mixer.cpp
using gain    = id<"[1/256..4] 1/65536 round_half_away">;            // decibels gain_t
using gain0   = id<"[0..4] 1/65536 round_half_away">;                           // audio_mixer gain_t
using db20    = id<"[-97/40..5/8] 1/40 round_half_away">; // log10<per<40>>'s
using db_by20 = id<"[-6/5..3/5] 1/40 round_half_away">;   // db_t / 20 dB
// moon_rocket.cpp: sqrt of the exhaust-velocity ratio, onto per<10^9>
using v_ratio = id<"[977'000'000/1'993'002'209..979'500'000/1'993'002'209] 2500/1'993'002'209">;
using fine9 =
    id<"[140'030'741/200'000'000..701'048'929/1'000'000'000] 1/1'000'000'000 round_half_away">;
// huge_angles.cpp, solar_system.cpp: too many inputs to certify
using huge   = id<key{grid{0, (std::int64_t{1} << 62) - 1}}>;      // radians' number
using amp52  = math::unit_amplitude<(std::int64_t{1} << 52)>; // sin's and cos's on per<2^52>
using metres = id<"[-10'000'000'000'000..10'000'000'000'000] 1/1'000'000'000">;
using mm     = id<"[0..14'142'135'623'730'951/1000] 1/1000 round_half_away">; // hypot's
} // namespace ex

#define CERTIFY(fn, Out, In) r.unary<Out, In>(#fn, [](In x) { return am::fn<Out>(x); })
#define CERTIFY2(fn, Out, A, B) r.binary<Out, A, B>(#fn, [](A a, B b) { return am::fn<Out>(a, b); })
#define CERTIFY_POW10(Out, In) r.unary<Out, In>("pow_base<10>", [](In x) { return am::pow_base<10, Out>(x); })

void pairs(certify::runner& r) {
    r.section("## Library pairs\n\n"
              "Grids picked to cover every function, each rounding mode, coarse and fine outputs, decimal\n"
              "notches and the double-double tier.");
    CERTIFY(sin, c4f, sym4);
    CERTIFY(sin, f20n, sym4);
    CERTIFY(sin, dd52, sym4);
    CERTIFY(sin, d3t, symdec);
    CERTIFY(cos, c4c, sym4);
    CERTIFY(cos, d3n, sym4);
    CERTIFY(tan, c4n, branch0);
    CERTIFY(tan, wide, branch1);
    CERTIFY(atan, c4n, sym4);
    CERTIFY(atan, f20e, sym4);

    CERTIFY(exp, big, sym8);
    CERTIFY(exp, f20n, sym4);
    CERTIFY(exp2, c4f, sym4);
    CERTIFY(sinh, f20n, sym4);
    CERTIFY(cosh, big, sym8);
    CERTIFY(tanh, c4c, sym8);
    CERTIFY(tanh, f20n, sym8);

    CERTIFY(log, c4c, pos);
    CERTIFY(log, f20n, pos);
    CERTIFY(log2, c4f, pos);
    CERTIFY(log10, d3n, pos);
    CERTIFY(sqrt, c4n, pos);
    CERTIFY(sqrt, f20e, pos);
    CERTIFY(cbrt, c4f, sym4);
    CERTIFY(cbrt, dd52, sym4);

    CERTIFY(asin, c4f, unit_value);
    CERTIFY(asin, f20n, unit_value);
    CERTIFY(acos, c4c, unit_value);
    CERTIFY(asinh, c4n, sym4);
    CERTIFY(acosh, f20n, ge1);
    CERTIFY(atanh, c4f, open1);

    r.unary<f20n, sym4>("pow_base<10>", [](sym4 x) { return am::pow_base<10, f20n>(x); });

    // Small inputs: these run in a debug build too.
    CERTIFY(sin, c4f, small);
    CERTIFY(exp, c4c, small);
    CERTIFY(atan, f20n, side);

    r.binary<c4n, side, side>("atan2", [](side y, side x) { return am::atan2<c4n>(y, x); });
    r.binary<f20n, side, side>("hypot", [](side x, side y) { return am::hypot<f20n>(x, y); });
    r.binary<wide, base, expo>("pow", [](base b, expo e) { return am::pow<wide>(b, e); });

    using namespace ex;
    constexpr const char* examples =
        "## Pairs the examples use\n\n"
        "Every math call of the [examples](../examples) on a notched grid. The rows marked skipped have\n"
        "more inputs than exhaustion can compute.\n\n";
    r.section(std::string{examples} + "### navigation.cpp");
    CERTIFY(sin, coord, angle);
    CERTIFY(cos, coord, angle);
    CERTIFY(asin, asin_r, ratio);
    CERTIFY(acos, acos_r, ratio);
    CERTIFY2(atan2, angle, coord, coord);
    CERTIFY2(hypot, radius, coord, coord);
    CERTIFY2(atan2, fine, position, position);
    CERTIFY2(hypot, range, position, position);
    r.section("### audio_math.cpp");
    CERTIFY(exp, amp, t_tau);
    CERTIFY(exp2, mult, half);
    CERTIFY(exp2, mult, exponent);
    CERTIFY(tanh, tanh_r, drive);
    CERTIFY(sinh, sinh_r, drive);
    CERTIFY(cosh, cosh_r, drive);
    CERTIFY(log2, log2_r, octaves);
    CERTIFY(cbrt, cbrt_r, vol);
    r.section("### units.cpp");
    CERTIFY(sin, trig, rad);
    CERTIFY(cos, trig, rad);
    r.section("### decibels.cpp");
    CERTIFY(log10, db20, gain);
    CERTIFY_POW10(gain, db_by20);
    r.section("### audio_mixer.cpp");
    CERTIFY_POW10(gain0, db_by20);
    r.section("### moon_rocket.cpp");
    CERTIFY(sqrt, fine9, v_ratio);
    r.section("### huge_angles.cpp");
    CERTIFY(sin, amp52, huge);
    CERTIFY(cos, amp52, huge);
    r.section("### solar_system.cpp");
    CERTIFY2(hypot, mm, metres, metres);
}
#undef CERTIFY_POW10
#undef CERTIFY2
#undef CERTIFY
} // namespace

int main(int argc, char** argv) { return certify::main(argc, argv, pairs); }
