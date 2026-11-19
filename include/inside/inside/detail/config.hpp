// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INSIDE_INSIDE_DETAIL_CONFIG_HPP
#define INSIDE_INSIDE_DETAIL_CONFIG_HPP

// The build options and the platform-dependent macros in one place:
// the other headers test these, never a platform macro (__SIZEOF_INT128__, __FMA__, …) themselves,
// and every header includes this one first.
//   build options  INSIDE_MATH_NO_FP, INSIDE_MATH_INSTRUMENT
//   features       INSIDE_HAS_EXCEPTIONS, _HAS_INT128, _HAS_FMA
// The library is C++26 with static reflection (GCC 16, -freflection),
// written in the GNU dialect: [[gnu::cold]], __builtin_fabs, __builtin_*_overflow.

#include <version>

#if !defined(__cpp_impl_reflection) || !__has_include(<meta>)
  #error "ins needs C++26 static reflection: compile with g++-16 -std=c++26 -freflection"
#endif

// INSIDE_MATH_NO_FP: no hardware floating point anywhere.
// The math engine's double and dd tiers compile out; its integer path computes every result,
// and results do not change.
// Define it to force the FP-free build; it is on by itself on freestanding targets (__STDC_HOSTED__ == 0).
// The API and grid deduction are unchanged.
#if !defined(INSIDE_MATH_NO_FP)
  #if defined(__STDC_HOSTED__) && __STDC_HOSTED__ == 0
    #define INSIDE_MATH_NO_FP
  #endif
#endif

// INSIDE_MATH_INSTRUMENT: 1 adds math::driver (math/detail/adaptive.hpp),
// run-time counts of the math engine's integer path, its deep steps and its guesses
// (results still undecided at the cap), and a switch that sends every call to the integer path.
// The certify tool (tests/certify) is built on it. 0, the default, compiles it all out.
#if !defined(INSIDE_MATH_INSTRUMENT)
  #define INSIDE_MATH_INSTRUMENT 0
#endif

// -ffast-math is not supported.
// The results are exact or correctly rounded, which rests on IEEE arithmetic as written:
// double sources are screened for infinities and NaN,
// and the math engine's error bounds and error-free sums count every rounding in program order.
// Fast-math assumes no NaN or infinity and reassociates, which can change results silently,
// so a build that announces it stops here.
#if defined(__FAST_MATH__) || defined(__ASSOCIATIVE_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__)
  #error \
    "ins does not support -ffast-math, -fassociative-math or -ffinite-math-only: its results rely on IEEE floating point as written"
#endif

// Nor is excess precision: the x87 unit (32-bit x86 without SSE2) evaluates double expressions at 80 bits
// and rounds them when it spills, so the same source rounds differently by register pressure.
#if defined(__FLT_EVAL_METHOD__) && __FLT_EVAL_METHOD__ != 0
  #error \
    "ins needs floating point evaluated in its own type (FLT_EVAL_METHOD 0): on 32-bit x86 compile for SSE2 (-msse2 -mfpmath=sse)"
#endif

#if defined(__cpp_exceptions)
  #define INSIDE_HAS_EXCEPTIONS 1
#else
  #define INSIDE_HAS_EXCEPTIONS 0
#endif

// unsigned __int128: the 64×64 → 128 products and divisions in one step;
// else (32-bit targets) a portable 32-bit split.
#if defined(__SIZEOF_INT128__)
  #define INSIDE_HAS_INT128 1
#else
  #define INSIDE_HAS_INT128 0
#endif

// A hardware fused multiply-add the compiler targets (math/detail/fp.hpp).
#if defined(__FMA__) || defined(__ARM_FEATURE_FMA)
  #define INSIDE_HAS_FMA 1
#else
  #define INSIDE_HAS_FMA 0
#endif

#endif // INSIDE_INSIDE_DETAIL_CONFIG_HPP
