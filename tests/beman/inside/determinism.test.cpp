// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Determinism golden pins for the transcendental functions.
//
// The library advertises bit-exact reproducibility. Accuracy-vs-std tolerances do
// NOT verify that — they would pass a sub-tolerance cross-platform drift. These
// tests instead pin each function's EXACT output (`==`) at representative and
// corner inputs, across a VARIETY of grids (notch resolution, interval, storage),
// so the multi-arch CI (x64 + ARM64 × GCC/Clang/AppleClang, both engines)
// locks the values bit-for-bit.
//
// Values were captured from the library itself; comments give the approximate true
// result. Grid snapping makes many of them exact (e.g. log10(1000)=3, hypot(3,4)=5).
// The double and CORDIC engines agree on every pin here.

#include <beman/inside/inside.hpp>
#include <beman/inside/cmath.hpp>

#include <gtest/gtest.h>

using namespace beman::inside;
using namespace beman::inside::detail;

// rational{expr} == rational{N, D}  (sign carried in D, as the library encodes it)
#define EXACT(expr, N, D)     ASSERT_EQ(rational{(expr)}, (rational{N, D}))
#define EXACT_OK(expr, N, D)  do { auto _r = (expr); ASSERT_TRUE(_r.has_value()); \
                                   ASSERT_EQ(rational{*_r}, (rational{N, D})); } while (0)
#define EXACT_ERR(expr)       do { auto _r = (expr); ASSERT_FALSE(_r.has_value()); \
                                   ASSERT_EQ(_r.error(), errc::domain_error); } while (0)

// These pins are the DEFAULT engine's values (double / CORDIC, which agree here). The float engine is a third value set, so under
// BEMAN_INSIDE_MATH_FLOAT the unqualified math::fn produces different snapped values — its
// determinism is pinned separately in test_math_engines.cpp.
#ifndef BEMAN_INSIDE_MATH_FLOAT

// determinism: asin / acos across grids and domain corners
TEST(DeterminismTest, determinism_asin_acos_across_grids_and_domain_corners)
{
  using A1 = inside<{{-1, 1}, notch<1, 65536>}, round_nearest | real>;
  using A2 = inside<{{-1, 1}, notch<1, 4096>},  round_nearest | real>;

  EXACT(math::asin(A1{-1}),            3217, -2048);   // -pi/2
  EXACT(math::acos(A1{-1}),          205887, 65536);   //  pi
  EXACT(math::asin(A2{-1}),            3217, -2048);
  EXACT(math::acos(A2{-1}),            3217,  1024);
  EXACT(math::asin(A1{rational{-1,2}}), 34315, -65536);
  EXACT(math::acos(A1{rational{-1,2}}), 68629,  32768);
  EXACT(math::asin(A2{rational{-1,2}}),  2145,  -4096);
  EXACT(math::acos(A2{rational{-1,2}}),  8579,   4096);
  EXACT(math::asin(A1{0}),                0,      1);   //  0
  EXACT(math::acos(A1{0}),             3217,   2048);   //  pi/2
  EXACT(math::asin(A2{0}),                0,      1);
  EXACT(math::acos(A2{0}),             3217,   2048);
  EXACT(math::asin(A1{rational{1,2}}), 34315,  65536);
  EXACT(math::acos(A1{rational{1,2}}), 68629,  65536);
  EXACT(math::asin(A2{rational{1,2}}),  2145,   4096);
  EXACT(math::acos(A2{rational{1,2}}),  4289,   4096);
  EXACT(math::asin(A1{1}),             3217,   2048);   //  pi/2
  EXACT(math::acos(A1{1}),                0,      1);   //  0
  EXACT(math::asin(A2{1}),             3217,   2048);
  EXACT(math::acos(A2{1}),                0,      1);
  EXACT(math::asin(A1{rational{65535,65536}}), 51291, 32768);  // near +1
  EXACT(math::acos(A1{rational{65535,65536}}),   181, 32768);
  EXACT(math::asin(A2{rational{65535,65536}}),  3217,  2048);  // snaps to 1
  EXACT(math::acos(A2{rational{65535,65536}}),     0,     1);
}

// determinism: sinh / cosh / tanh across grids and corners
TEST(DeterminismTest, determinism_sinh_cosh_tanh_across_grids_and_corners)
{
  using H1 = inside<{{-10, 10}, notch<1, 65536>}, round_nearest | real>;
  using H2 = inside<{{-4, 4},   notch<1, 4096>},  round_nearest | real>;

  EXACT(math::sinh(H1{0}), 0, 1);  EXACT(math::cosh(H1{0}), 1, 1);  EXACT(math::tanh(H1{0}), 0, 1);
  EXACT(math::sinh(H1{1}),  38509,  32768);
  EXACT(math::cosh(H1{1}), 101127,  65536);
  EXACT(math::tanh(H1{1}),   6239,   8192);
  EXACT(math::sinh(H1{-1}), 38509, -32768);
  EXACT(math::cosh(H1{-1}),101127,  65536);
  EXACT(math::tanh(H1{-1}),  6239,  -8192);
  EXACT(math::sinh(H1{5}), 2431491,  32768);   // ~74.2
  EXACT(math::cosh(H1{5}), 4863423,  65536);
  EXACT(math::tanh(H1{5}),   32765,  32768);   // ~0.9999
  EXACT(math::sinh(H1{-5}),2431491, -32768);
  EXACT(math::cosh(H1{-5}),4863423,  65536);
  EXACT(math::tanh(H1{-5}),  32765, -32768);

  EXACT(math::sinh(H2{0}), 0, 1);  EXACT(math::cosh(H2{0}), 1, 1);  EXACT(math::tanh(H2{0}), 0, 1);
  EXACT(math::sinh(H2{1}), 2407,  2048);
  EXACT(math::cosh(H2{1}),  395,   256);
  EXACT(math::tanh(H2{1}), 3119,  4096);
  EXACT(math::sinh(H2{-1}),2407, -2048);
  EXACT(math::cosh(H2{-1}), 395,   256);
  EXACT(math::tanh(H2{-1}),3119, -4096);
  // sinh(4)·4096 = 111779.502…: a near-tie the engines now both round up.
  EXACT(math::sinh(H2{4}),  27945, 1024);
  EXACT(math::cosh(H2{4}), 111855, 4096);
  EXACT(math::tanh(H2{4}),   4093, 4096);
}

// determinism: log10 across grids and corners
TEST(DeterminismTest, determinism_log10_across_grids_and_corners)
{
  using L1 = inside<{{1, 1024}, notch<1, 65536>}, round_nearest | real>;
  using L2 = inside<{{1, 256},  notch<1, 4096>},  round_nearest | real>;

  EXACT(math::log10(L1{1}),    0, 1);   // log10(1)   = 0
  EXACT(math::log10(L2{1}),    0, 1);
  EXACT(math::log10(L1{10}),   1, 1);   // log10(10)  = 1
  EXACT(math::log10(L2{10}),   1, 1);
  EXACT(math::log10(L1{100}),  2, 1);   // log10(100) = 2
  EXACT(math::log10(L2{100}),  2, 1);
  EXACT(math::log10(L1{1000}), 3, 1);   // log10(1000)= 3
  EXACT(math::log10(L1{1024}), 197283, 65536);   // ~3.0103, upper edge
  EXACT(math::log10(L2{256}),    1233,   512);    // ~2.4082, upper edge
}

// determinism: hypot across grids and corners
TEST(DeterminismTest, determinism_hypot_across_grids_and_corners)
{
  using P1 = inside<{{-16, 16}, notch<1, 65536>}, round_nearest | real>;
  using P2 = inside<{{-4, 4},   notch<1, 4096>},  round_nearest | real>;

  EXACT(math::hypot(P1{0},   P1{0}),   0, 1);   // origin
  EXACT(math::hypot(P1{3},   P1{4}),   5, 1);   // 3-4-5
  EXACT(math::hypot(P1{5},   P1{0}),   5, 1);   // x-axis
  EXACT(math::hypot(P1{0},   P1{7}),   7, 1);   // y-axis
  EXACT(math::hypot(P1{16},  P1{16}),  741455, 32768);   // ~22.627, max magnitude
  EXACT(math::hypot(P1{-16}, P1{-16}), 741455, 32768);   // sign-symmetric
  EXACT(math::hypot(P2{3},   P2{4}),   5, 1);
  EXACT(math::hypot(P2{rational{1,2}}, P2{rational{1,2}}), 181, 256);  // ~0.7071
}

// determinism: pow across grids and corners
TEST(DeterminismTest, determinism_pow_across_grids_and_corners)
{
  using PB1 = inside<{{1, 16}, notch<1, 65536>}, round_nearest | real>;
  using PE1 = inside<{{-4, 8}, notch<1, 65536>}, round_nearest | real>;
  using PB2 = inside<{{1, 4},  notch<1, 4096>},  round_nearest | real>;
  using PE2 = inside<{{0, 4},  notch<1, 4096>},  round_nearest | real>;

  EXACT_OK(math::pow(PB1{2}, PE1{0}),  1, 1);   // b^0 = 1
  EXACT_OK(math::pow(PB1{2}, PE1{1}),  2, 1);   // b^1 = b
  EXACT_OK(math::pow(PB1{2}, PE1{4}), 16, 1);   // 2^4 = 16
  EXACT_OK(math::pow(PB1{1}, PE1{3}),  1, 1);   // 1^e = 1
  EXACT_OK(math::pow(PB1{2}, PE1{-1}), 1, 2);   // 2^-1 = 0.5
  EXACT_OK(math::pow(PB2{4}, PE2{2}), 16, 1);   // 4^2 = 16 (coarse grid)
  EXACT_OK(math::pow(PB2{3}, PE2{0}),  1, 1);
}

// determinism: sqrt across grids, corners, and the error path
TEST(DeterminismTest, determinism_sqrt_across_grids_corners_and_the_error_path)
{
  using S1   = inside<{{0, 4},   notch<1, 65536>}, round_nearest | real>;
  using S2   = inside<{{0, 256}, notch<1, 256>},   round_nearest | real>;
  using Smix = inside<{{-4, 9},  notch<1, 65536>}, round_nearest | real>;

  EXACT(math::sqrt(S1{0}), 0, 1);
  EXACT(math::sqrt(S1{1}), 1, 1);
  EXACT(math::sqrt(S1{4}), 2, 1);
  EXACT(math::sqrt(S2{9}), 3, 1);
  EXACT(math::sqrt(S2{256}), 16, 1);
  EXACT_OK(math::sqrt(Smix{9}), 3, 1);   // mixed-sign grid → expected
  EXACT_ERR(math::sqrt(Smix{-1}));       // determinism of the error path
}

// determinism: cbrt across grids and corners
TEST(DeterminismTest, determinism_cbrt_across_grids_and_corners)
{
  using C1 = inside<{{-16, 16}, notch<1, 65536>}, round_nearest | real>;
  using C2 = inside<{{-8, 8},   notch<1, 1024>},  round_nearest | real>;

  EXACT(math::cbrt(C1{0}),  0,  1);  EXACT(math::cbrt(C2{0}),  0,  1);
  EXACT(math::cbrt(C1{1}),  1,  1);  EXACT(math::cbrt(C2{1}),  1,  1);
  EXACT(math::cbrt(C1{-1}), 1, -1);  EXACT(math::cbrt(C2{-1}), 1, -1);  // -1
  EXACT(math::cbrt(C1{8}),  2,  1);  EXACT(math::cbrt(C2{8}),  2,  1);  //  2
  EXACT(math::cbrt(C1{-8}), 2, -1);  EXACT(math::cbrt(C2{-8}), 2, -1);  // -2
}

// determinism: atan / atan2 corners across quadrants and axes
TEST(DeterminismTest, determinism_atan_atan2_corners_across_quadrants_and_axes)
{
  using AT1 = inside<{{-16, 16}, notch<1, 16384>}, round_nearest | real>;
  using AT2 = inside<{{-1, 1},   notch<1, 65536>}, round_nearest | real>;

  EXACT(math::atan(AT1{0}),     0,     1);
  EXACT(math::atan(AT1{1}),  3217,  4096);   //  pi/4
  EXACT(math::atan(AT1{-1}), 3217, -4096);   // -pi/4
  EXACT(math::atan(AT1{16}),  24713, 16384);  // ~1.5083
  EXACT(math::atan(AT1{-16}), 24713,-16384);
  EXACT(math::atan(AT2{rational{1,2}}), 15193, 32768);  // ~0.4636

  EXACT(math::atan2(AT1{3},  AT1{1}),  1279, 1024);   // Q1
  EXACT(math::atan2(AT1{1},  AT1{-5}), 24119, 8192);  // Q2
  EXACT(math::atan2(AT1{-7}, AT1{-2}), 3787, -2048);  // Q3
  EXACT(math::atan2(AT1{-7}, AT1{2}),  2647, -2048);  // Q4
  EXACT(math::atan2(AT1{0},  AT1{4}),     0,    1);   // +x axis
  EXACT(math::atan2(AT1{4},  AT1{0}),  3217, 2048);   // +y axis (pi/2)
}

// determinism: sin / cos / tan radian corners
TEST(DeterminismTest, determinism_sin_cos_tan_radian_corners)
{
  using RAD = inside<{{-8, 8}, notch<1, 16384>}, round_nearest | real>;

  EXACT(math::sin(RAD{0}), 0, 1);
  EXACT(math::cos(RAD{0}), 1, 1);
  EXACT(math::sin(RAD{1}), 13787, 16384);   // ~0.8415
  EXACT(math::cos(RAD{1}),  2213,  4096);    // ~0.5403
  EXACT_OK(math::tan(RAD{0}),     0,     1);
  EXACT_OK(math::tan(RAD{1}),  25517, 16384);  // ~1.5574
}

#endif // !BEMAN_INSIDE_MATH_FLOAT
