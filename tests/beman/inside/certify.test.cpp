// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//---------------------------------------------------------------------------
// beman.inside.certify — the library's own certified math pairs (certify/
// certify.hpp has the tool). Every input of each pair is computed as a
// run-time call and on the integer path alone; a pair is certified when the
// two agree everywhere and no result is a guess. docs/certified.md is this
// program's table, and the test checks every hash against it:
//
//   ./beman.inside.certify --estimate                the pairs and the time they take
//   ./beman.inside.certify                           certify them (seconds, optimized)
//   ./beman.inside.certify --check docs/certified.md and match the committed hashes
//   ./beman.inside.certify --markdown docs/certified.md   regenerate the page
//
// An optimized build runs every pair, a debug build the pairs up to 4096
// inputs.
//---------------------------------------------------------------------------
#include "certify/certify.hpp"

using namespace beman::inside;
namespace am = beman::inside::math;

namespace {
// Inputs.
using sym4   = inside<{{-4, 4}, per<1 << 18>}, round_nearest>;         // 2^21 + 1 inputs
using sym8   = inside<{{-8, 8}, per<1 << 16>}, round_nearest>;         // 2^20 + 1
using symdec = inside<{{-4, 4}, rational{1, 100'000}}, round_nearest>; // 800 001
using unit   = inside<{{-1, 1}, per<1 << 19>}, round_nearest>;         // 2^20 + 1
using open1  = inside<{{rational{-1023, 1024}, rational{1023, 1024}}, per<1024>}, round_nearest>;
using pos    = inside<{{rational{1, 1024}, 1024}, per<1024>}, round_nearest>; // 2^20
using ge1    = inside<{{1, 1025}, per<1024>}, round_nearest>;                 // 2^20 + 1
using small  = inside<{{-2, 2}, per<16>}, round_nearest>;                     // 65
using side   = inside<{{-16, 16}, per<32>}, round_nearest>;                   // 1025
using base   = inside<{{rational{1, 64}, 16}, per<64>}, round_nearest>;       // 1024
using expo   = inside<{{-4, 4}, per<8>}, round_nearest>;                      // 65

// Outputs: coarse (a 4-bit notch: start precision 16, cap 64 bits), fine,
// decimal, and one past the double tier (the dd tier: a 2^-52 notch).
using c4f  = inside<{{-64, 64}, per<16>}, round_floor | clamp>;
using c4c  = inside<{{-64, 64}, per<16>}, round_ceil | clamp>;
using c4n  = inside<{{-64, 64}, per<16>}, round_nearest | clamp>;
using f20n = inside<{{-64, 64}, per<1 << 20>}, round_nearest | clamp>;
using f20e = inside<{{-64, 64}, per<1 << 20>}, round_half_even | clamp>;
using d3n  = inside<{{-64, 64}, rational{1, 1000}}, round_nearest | clamp>;
using d3t  = inside<{{-64, 64}, rational{1, 1000}}, round_toward_zero | clamp>;
using dd52 = inside<{{-4, 4}, rational{1, umax{1} << 52}}, round_nearest | clamp>;
using big  = inside<{{0, 1 << 20}, per<16>}, round_floor | clamp>;  // exp, cosh of sym8
using wide = inside<{{-1 << 20, 1 << 20}, per<16>}, round_nearest>; // tan, pow: overflow reported

// The examples' pairs, spelled as the number grids each math call sees
// (find_grids.py over a debug build lists them; its test fails when an
// example pair has no row here). The alias each grid comes from is named.
namespace ex {
// navigation.cpp
using coord    = inside<{{-1, 1}, per<16384>}, round_nearest>;                     // coord_t
using angle    = inside<{{-4, 4}, per<16384>}, round_nearest>;                     // angle_t
using radius   = inside<{{0, rational{23171, 16384}}, per<16384>}, round_nearest>; // hypot(coord_t, coord_t)
using ratio    = inside<{{-1, 1}, per<4096>}, round_nearest>;                      // ratio_t
using asin_r   = inside<{{rational{-3217, 2048}, rational{3217, 2048}}, per<4096>}, round_nearest>;
using acos_r   = inside<{{0, rational{3217, 1024}}, per<4096>}, round_nearest>;
using position = inside<{{-1024, 1024}, per<256>}, round_nearest>;            // pos_t's number
using fine     = inside<{{-4, 4}, per<1 << 20>}, round_nearest>;              // fine_t
using range    = inside<{{0, rational{46341, 32}}, per<256>}, round_nearest>; // units::hypot(pos_t, pos_t)
// audio_math.cpp
using t_tau    = inside<{{-4, 4}, per<1024>}>;                  // seconds_t / tau
using amp      = inside<{{0, 100}, per<16384>}, round_nearest>; // amp_t
using exponent = inside<{{0, 4}, per<1024>}, round_nearest>;    // exponent_t
using half     = inside<{{0, 4}, per<2>}, round_nearest>;       // the half-octave cursor
using mult     = inside<{{0, 16}, per<16384>}, round_nearest>;  // mult_t
using drive    = inside<{{-4, 4}, per<2048>}, round_nearest>;   // drive_t
using tanh_r   = inside<{{rational{-2047, 2048}, rational{2047, 2048}}, per<2048>}, round_nearest>;
using sinh_r   = inside<{{rational{-27945, 1024}, rational{27945, 1024}}, per<2048>}, round_nearest>;
using cosh_r   = inside<{{1, rational{6991, 256}}, per<2048>}, round_nearest>;
using octaves  = inside<{{rational{1, 16}, 16}, per<1024>}, round_nearest>; // ratio_t
using log2_r   = inside<{{-4, 4}, per<1024>}, round_nearest>;
using vol      = inside<{{0, 64}, per<256>}, round_nearest>; // vol_t
using cbrt_r   = inside<{{0, 4}, per<256>}, round_nearest>;
// units.cpp
using rad  = inside<{{0, 7}, rational{1, 1'000'000}}, round_nearest>; // angle
using trig = inside<{{-1, 1}, rational{1, 1'000'000}}, round_nearest>;
// decibels.cpp, audio_mixer.cpp
using gain    = inside<{{rational{1, 256}, 4}, per<65536>}, round_nearest>; // decibels gain_t
using gain0   = inside<{{0, 4}, per<65536>}, round_nearest>;                // audio_mixer gain_t
using db20    = inside<{{rational{-5, 2}, 1}, per<40>}, round_nearest>;     // db_div20_t
using db_by20 = inside<{{rational{-6, 5}, rational{3, 5}}, per<40>}>;       // db_t / 20 dB
// moon_rocket.cpp: sqrt of the exhaust-velocity ratio, onto fine<10^9>
using v_ratio = inside<{{rational{977'000'000, 1'993'002'209}, rational{979'500'000, 1'993'002'209}},
                        rational{2500, 1'993'002'209}}>;
using fine9   = inside<{{0, 200}, rational{1, 1'000'000'000}}, round_nearest>;
// huge_angles.cpp, solar_system.cpp: too many inputs to certify
using huge   = inside<{0, (std::int64_t{1} << 62) - 1}>; // radians' number
using amp52  = math::unit_amplitude<(std::int64_t{1} << 52)>;
using metres = inside<{{-10'000'000'000'000, 10'000'000'000'000}, rational{1, 1'000'000'000}}>;
using mm     = inside<{{0, 100'000'000'000'000}, per<1000>}, round_nearest>;
} // namespace ex

#define CERTIFY(fn, Out, In) r.unary<Out, In>(#fn, [](In x) { return am::fn##_into<Out>(x); })
#define CERTIFY2(fn, Out, A, B) r.binary<Out, A, B>(#fn, [](A a, B b) { return am::fn##_into<Out>(a, b); })
#define CERTIFY_POW10(Out, In) r.unary<Out, In>("pow_base<10>", [](In x) { return am::pow_base_into<Out, 10>(x); })

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
    CERTIFY(tan, c4n, sym4);
    CERTIFY(tan, wide, sym8);
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
    CERTIFY(sqrt, c4f, sym4); // a grid that crosses 0: errc below it
    CERTIFY(cbrt, c4f, sym4);
    CERTIFY(cbrt, dd52, sym4);

    CERTIFY(asin, c4f, unit);
    CERTIFY(asin, f20n, unit);
    CERTIFY(acos, c4c, unit);
    CERTIFY(asinh, c4n, sym4);
    CERTIFY(acosh, f20n, ge1);
    CERTIFY(atanh, c4f, open1);

    r.unary<f20n, sym4>("pow_base<10>", [](sym4 x) { return am::pow_base_into<f20n, 10>(x); });

    // Small inputs: these run in a debug build too.
    CERTIFY(sin, c4f, small);
    CERTIFY(exp, c4c, small);
    CERTIFY(atan, f20n, side);

    r.binary<c4n, side, side>("atan2", [](side y, side x) { return am::atan2_into<c4n>(y, x); });
    r.binary<f20n, side, side>("hypot", [](side x, side y) { return am::hypot_into<f20n>(x, y); });
    r.binary<wide, base, expo>("pow", [](base b, expo e) { return am::pow_into<wide>(b, e); });

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
