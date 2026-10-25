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

#define CERTIFY(fn, Out, In) r.unary<Out, In>(#fn, [](In x) { return am::fn##_into<Out>(x); })

void pairs(certify::runner& r) {
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
}
#undef CERTIFY
} // namespace

int main(int argc, char** argv) { return certify::main(argc, argv, pairs); }
