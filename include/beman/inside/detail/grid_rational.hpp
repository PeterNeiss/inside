// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_DETAIL_GRID_RATIONAL_HPP
#define BEMAN_INSIDE_DETAIL_GRID_RATIONAL_HPP

#include <beman/inside/detail/rational.hpp>

//---------------------------------------------------------------------------
// grid_rational — the number type of a grid's limits and notch (the NTTP
// substrate of `interval` and `grid`).
//
// BEMAN_INSIDE_BIG_GRIDS selects it. Under C++26 static reflection
// (std::define_static_array) a grid number has no size limit: its limbs are
// interned in static storage, so equal values stay the same template argument.
// Without reflection (C++23, older compilers) it is the 64-bit runtime
// `rational`, and grids keep today's limits. Define the macro to 0 to force the
// 64-bit grids on a C++26 compiler.
//
// The two modes give `grid` and `interval` different layouts, so each mode
// lives in its own inline namespace: linking a C++23 TU against a C++26 one is
// a link error, not a silent ODR violation.
//---------------------------------------------------------------------------
#if !defined(BEMAN_INSIDE_BIG_GRIDS)
#  if defined(__cpp_impl_reflection) && __has_include(<meta>)
#    define BEMAN_INSIDE_BIG_GRIDS 1
#  else
#    define BEMAN_INSIDE_BIG_GRIDS 0
#  endif
#endif

#if BEMAN_INSIDE_BIG_GRIDS
#  define BEMAN_INSIDE_GRID_ABI big_grids_v1
#else
#  define BEMAN_INSIDE_GRID_ABI small_grids_v1
#endif

namespace beman::inside::detail
{
  // Phase 0: both modes still use the 64-bit rational; big_rational follows.
  using grid_rational = rational;
}

#endif
