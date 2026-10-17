// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_DETAIL_CONFIG_HPP
#define BEMAN_INSIDE_DETAIL_CONFIG_HPP

//---------------------------------------------------------------------------
// config — every compiler- and platform-dependent macro in one place. The
// other headers test these, never a compiler macro (_MSC_VER, __GNUC__,
// __SIZEOF_INT128__, …) themselves. Every header includes this one first.
//
//   Compiler            BEMAN_INSIDE_MSVC
//   Build options       BEMAN_INSIDE_MATH_NO_FP, BEMAN_INSIDE_BIG_GRIDS
//   Features            BEMAN_INSIDE_HAS_EXCEPTIONS, _HAS_INT128,
//                       _HAS_UMUL128, _HAS_OVERFLOW_BUILTINS, _HAS_FMA,
//                       _HAS_VALUED_DIAG,
//                       _HAS_STD_FORMAT
//   Attributes          BEMAN_INSIDE_COLD, _NOINLINE, _ALWAYS_INLINE,
//                       _NO_UNIQUE_ADDRESS, BEMAN_INSIDE_TRAP()
//   Deleted overloads   BEMAN_INSIDE_DELETE("reason"), _HAS_DELETE_REASON,
//                       _DELETE_BEGIN / _END
//   Floating point      BEMAN_INSIDE_FABS(x), _NEARBYINT(x), _INF(),
//                       _ASSOC_BARRIER(x)
//   Warnings            BEMAN_INSIDE_WARNINGS_PUSH / _POP
//   Preprocessor        BEMAN_INSIDE_EXPAND(x)
//---------------------------------------------------------------------------

#include <version> // the library feature-test macros (__cpp_lib_format)

//---------------------------------------------------------------------------
// Compiler
//---------------------------------------------------------------------------
// Microsoft's compiler (not clang-cl, which behaves as Clang), for its
// workarounds.
#if defined(_MSC_VER) && !defined(__clang__)
    #define BEMAN_INSIDE_MSVC 1
#else
    #define BEMAN_INSIDE_MSVC 0
#endif

// Deducing `this` (P0847) folds the lvalue/rvalue overload pairs.
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ < 14
    #error "beman.inside requires GCC 14 or newer (deducing this)"
#endif

//---------------------------------------------------------------------------
// Build options
//---------------------------------------------------------------------------
// BEMAN_INSIDE_MATH_NO_FP — no hardware floating point anywhere: the math
// engine's double and dd tiers compile out (its integer path computes every
// result; results do not change). Define it to force the FP-free build; it is
// auto-enabled on freestanding targets (__STDC_HOSTED__ == 0). Public API and
// grid deduction are unchanged.
#if !defined(BEMAN_INSIDE_MATH_NO_FP)
    #if defined(__STDC_HOSTED__) && __STDC_HOSTED__ == 0
        #define BEMAN_INSIDE_MATH_NO_FP
    #endif
#endif

// -ffast-math is not supported. The library's results are exact or correctly
// rounded, and that rests on IEEE arithmetic as written: double sources are
// screened for infinities and NaN, and the math engine's error bounds and
// error-free sums count every rounding in program order. Fast-math lets the
// compiler assume no NaN or infinity and reassociate, which can change
// results silently, so a build that announces it stops here.
#if defined(__FAST_MATH__) || defined(__ASSOCIATIVE_MATH__) || \
    (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__) || defined(_M_FP_FAST)
    #error \
        "beman::inside does not support -ffast-math, -fassociative-math, -ffinite-math-only or /fp:fast: its results rely on IEEE floating point as written"
#endif

// BEMAN_INSIDE_BIG_GRIDS — grid numbers of any size (C++26 static
// reflection), else 64-bit grids. Define it to 0 to force the 64-bit grids on
// a C++26 compiler (grid_rational.hpp).
#if !defined(BEMAN_INSIDE_BIG_GRIDS)
    #if defined(__cpp_impl_reflection) && __has_include(<meta>)
        #define BEMAN_INSIDE_BIG_GRIDS 1
    #else
        #define BEMAN_INSIDE_BIG_GRIDS 0
    #endif
#endif

//---------------------------------------------------------------------------
// Features
//---------------------------------------------------------------------------
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
    #define BEMAN_INSIDE_HAS_EXCEPTIONS 1
#else
    #define BEMAN_INSIDE_HAS_EXCEPTIONS 0
#endif

// unsigned __int128: the 64×64 → 128 products and divisions in one step; else
// a portable 32-bit split.
#if defined(__SIZEOF_INT128__)
    #define BEMAN_INSIDE_HAS_INT128 1
#else
    #define BEMAN_INSIDE_HAS_INT128 0
#endif

// MSVC on x64 has no __int128, but the _umul128 / _udiv128 intrinsics do the
// same at run time (not in constant expressions).
#if !BEMAN_INSIDE_HAS_INT128 && defined(_MSC_VER) && defined(_M_X64) && !defined(_M_ARM64EC) && !defined(__clang__)
    #define BEMAN_INSIDE_HAS_UMUL128 1
    #include <intrin.h>
#else
    #define BEMAN_INSIDE_HAS_UMUL128 0
#endif

// __builtin_{add,sub,mul}_overflow (overflow.hpp); else portable checks.
#if defined(__GNUC__) || defined(__clang__)
    #define BEMAN_INSIDE_HAS_OVERFLOW_BUILTINS 1
#else
    #define BEMAN_INSIDE_HAS_OVERFLOW_BUILTINS 0
#endif

// A hardware fused multiply-add the compiler targets (math_fp.hpp).
#if defined(__FMA__) || defined(__ARM_FEATURE_FMA)
    #define BEMAN_INSIDE_HAS_FMA 1
#else
    #define BEMAN_INSIDE_HAS_FMA 0
#endif

// A static_assert message with values in it (P2741, C++26; diag_text.hpp).
// Clang accepts it in C++23 too, with a warning, so it waits for C++26 mode.
#if defined(__cpp_static_assert) && __cpp_static_assert >= 202306L && __cplusplus > 202302L
    #define BEMAN_INSIDE_HAS_VALUED_DIAG 1
#else
    #define BEMAN_INSIDE_HAS_VALUED_DIAG 0
#endif

// std::format and std::formatter specializations (io.hpp, units.hpp).
#if defined(__cpp_lib_format)
    #define BEMAN_INSIDE_HAS_STD_FORMAT 1
#else
    #define BEMAN_INSIDE_HAS_STD_FORMAT 0
#endif

// One more expansion pass: MSVC's traditional preprocessor (without
// /Zc:preprocessor) passes a forwarded __VA_ARGS__ on as one argument.
#define BEMAN_INSIDE_EXPAND(x) x

//---------------------------------------------------------------------------
// Attributes. Error and throw paths are cold and out of line, so the
// optimizer keeps them out of the hot path. MSVC ignores the standard
// [[no_unique_address]] in favour of its own spelling: an empty raw (a point
// grid) or an empty action then takes no space.
//---------------------------------------------------------------------------
#if defined(__GNUC__) || defined(__clang__)
    #define BEMAN_INSIDE_COLD [[gnu::cold]]
    #define BEMAN_INSIDE_NOINLINE [[gnu::noinline]]
    #define BEMAN_INSIDE_ALWAYS_INLINE [[gnu::always_inline]]
    #define BEMAN_INSIDE_TRAP() __builtin_trap()
#elif BEMAN_INSIDE_MSVC
    #define BEMAN_INSIDE_COLD
    #define BEMAN_INSIDE_NOINLINE __declspec(noinline)
    #define BEMAN_INSIDE_ALWAYS_INLINE
    #define BEMAN_INSIDE_TRAP() __debugbreak()
#else
    #include <cstdlib>
    #define BEMAN_INSIDE_COLD
    #define BEMAN_INSIDE_NOINLINE
    #define BEMAN_INSIDE_ALWAYS_INLINE
    #define BEMAN_INSIDE_TRAP() ::abort()
#endif

#if BEMAN_INSIDE_MSVC
    #define BEMAN_INSIDE_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#else
    #define BEMAN_INSIDE_NO_UNIQUE_ADDRESS [[no_unique_address]]
#endif

//---------------------------------------------------------------------------
// Deleted overloads with a reason: `= BEMAN_INSIDE_DELETE("…")`. They take an
// operation's failing case, so `requires { a + b; }` is false and the error
// names the reason (= delete("…"), C++26, accepted as an extension in C++23
// by GCC 15+ and Clang 19+; older compilers get a plain `= delete`).
// Declarations using it sit between BEGIN and END, which silence the
// extension warning.
//---------------------------------------------------------------------------
#if defined(__cpp_deleted_function) || (defined(__clang__) && __clang_major__ >= 19) || \
    (defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 15)
    #define BEMAN_INSIDE_HAS_DELETE_REASON 1
    #define BEMAN_INSIDE_DELETE(reason) delete (reason)
#else
    #define BEMAN_INSIDE_HAS_DELETE_REASON 0
    #define BEMAN_INSIDE_DELETE(reason) delete
#endif
#if (defined(__GNUC__) || defined(__clang__)) && !defined(__cpp_deleted_function)
    #define BEMAN_INSIDE_DELETE_BEGIN \
        _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wc++26-extensions\"")
    #define BEMAN_INSIDE_DELETE_END _Pragma("GCC diagnostic pop")
#else
    #define BEMAN_INSIDE_DELETE_BEGIN
    #define BEMAN_INSIDE_DELETE_END
#endif

//---------------------------------------------------------------------------
// Floating point for the double kernels: |x|, round-to-nearest-even and +inf
// are the GCC/Clang builtins (constexpr, no errno, no call, no <cmath>); MSVC
// takes |x| by its sign (-0 + 0 is +0) and <cmath>. ASSOC_BARRIER keeps an
// operand of an error-free sum out of an fma the compiler may contract
// (-ffp-contract=fast); without the builtin it is the value itself.
//---------------------------------------------------------------------------
#if defined(__GNUC__) || defined(__clang__)
    #define BEMAN_INSIDE_FABS(x) __builtin_fabs(x)
    #define BEMAN_INSIDE_NEARBYINT(x) __builtin_nearbyint(x)
    #define BEMAN_INSIDE_INF() __builtin_inf()
#else
    #if !defined(BEMAN_INSIDE_MATH_NO_FP)
        #include <cmath>
    #endif
    #include <limits>
    #define BEMAN_INSIDE_FABS(x) ((x) < 0.0 ? -(x) : (x) + 0.0)
    #define BEMAN_INSIDE_NEARBYINT(x) std::nearbyint(x)
    #define BEMAN_INSIDE_INF() std::numeric_limits<double>::infinity()
#endif

#if defined(__has_builtin)
    #if __has_builtin(__builtin_assoc_barrier)
        #define BEMAN_INSIDE_ASSOC_BARRIER(x) __builtin_assoc_barrier(x)
    #elif __has_builtin(__arithmetic_fence)
        #define BEMAN_INSIDE_ASSOC_BARRIER(x) __arithmetic_fence(x)
    #endif
#endif
#if !defined(BEMAN_INSIDE_ASSOC_BARRIER)
    #define BEMAN_INSIDE_ASSOC_BARRIER(x) (x)
#endif

//---------------------------------------------------------------------------
// Warnings a library header switches off for its own code on MSVC: C4702
// (unreachable code: the branches an if constexpr leaves behind), C4268 (a
// const object of a class with a defaulted constructor is zeroed) and C4459
// (a local named like one of the user's globals, `base`, `one`). Each
// header wraps its body in PUSH / POP; templates instantiated from it keep
// that state, and user code does not.
//---------------------------------------------------------------------------
#if defined(_MSC_VER)
    #define BEMAN_INSIDE_WARNINGS_PUSH __pragma(warning(push)) __pragma(warning(disable : 4702 4268 4459))
    #define BEMAN_INSIDE_WARNINGS_POP __pragma(warning(pop))
#else
    #define BEMAN_INSIDE_WARNINGS_PUSH
    #define BEMAN_INSIDE_WARNINGS_POP
#endif

#endif // BEMAN_INSIDE_DETAIL_CONFIG_HPP
