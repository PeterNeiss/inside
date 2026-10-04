// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_DETAIL_GRID_RATIONAL_HPP
#define BEMAN_INSIDE_DETAIL_GRID_RATIONAL_HPP

#include <beman/inside/detail/rational.hpp>
#include <beman/inside/detail/wide_int.hpp>

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

#include <beman/inside/detail/big_rational.hpp>   // empty without big grids

#if BEMAN_INSIDE_BIG_GRIDS
#  define BEMAN_INSIDE_GRID_ABI big_grids_v1
#else
#  define BEMAN_INSIDE_GRID_ABI small_grids_v1
#endif

//---------------------------------------------------------------------------
// The grid-number vocabulary, the same in both modes:
//   grid_rational          the type of a grid's limits and notch
//   grid_wide              exact integers for grid computations (slot counts,
//                          value indices): wide enough for every grid
//   wide_numerator(r) /    a grid number's signed numerator and positive
//   wide_denominator(r)    denominator as grid_wide (also for a rational)
//   grid_divides_evenly    a / n is an integer (true for n == 0)
//   grid_same_lattice      (a − b) / n is an integer (true for n == 0)
//   grid_gcd               gcd of two fractions; may fail only with 64-bit
//                          grid numbers (returns expected there)
//   fits_rational(r) /     whether, and as which, 64-bit rational a grid
//   to_rational(r)         number serves the 64-bit-only paths
//---------------------------------------------------------------------------
namespace beman::inside::detail
{
#if BEMAN_INSIDE_BIG_GRIDS
  using grid_rational = big_rational;
  using grid_wide     = big_int;

  constexpr grid_wide wide_numerator(big_rational const& r) { return r.Num; }
  constexpr grid_wide wide_denominator(big_rational const& r) { return r.Den; }
  constexpr grid_wide wide_numerator(rational const& r)
  {
    const grid_wide n{r.Numerator};
    return r.Denominator < 0 ? -n : n;
  }
  constexpr grid_wide wide_denominator(rational const& r) { return grid_wide{abs_den(r.Denominator)}; }

  // (Convention, as divides_evenly: everything divides 0 evenly.)
  constexpr bool grid_divides_evenly(big_rational const& a, big_rational const& n)
  { return n == 0 || (a / n).is_integer(); }

  // (a − b) / n is an integer (true for n == 0): b and a share n's lattice.
  constexpr bool grid_same_lattice(big_rational const& a, big_rational const& b, big_rational const& n)
  { return grid_divides_evenly(a - b, n); }

  // The 64-bit rational of a grid number, for 64-bit-only paths.
  constexpr bool fits_rational(big_rational const& r) { return r.fits_rational(); }
  constexpr rational to_rational(big_rational const& r) { return r; }
  constexpr big_rational grid_gcd(big_rational const& a, big_rational const& b) { return gcd(a, b); }
#else
  using grid_rational = rational;
  // A product of three 64-bit magnitudes plus a sign.
  using grid_wide     = wide_sint<4>;

  constexpr grid_wide wide_numerator(rational const& r) noexcept
  {
    const grid_wide n{r.Numerator};
    return r.Denominator < 0 ? -n : n;
  }
  constexpr grid_wide wide_denominator(rational const& r) noexcept
  { return grid_wide{abs_den(r.Denominator)}; }

  constexpr bool grid_divides_evenly(rational const& a, rational const& n) { return divides_evenly(a, n); }

  // (A difference past the rational range has no lattice offset to test:
  // compare both ends' residues instead.)
  constexpr bool grid_same_lattice(rational const& a, rational const& b, rational const& n)
  {
    if (n == 0) return true;
    if (const auto d = try_sub(a, b)) return divides_evenly(*d, n);
    return divides_evenly(a, n) && divides_evenly(b, n);
  }

  constexpr bool fits_rational(rational const&) { return true; }
  constexpr rational to_rational(rational const& r) { return r; }
  constexpr std::expected<rational, errc> grid_gcd(rational const& a, rational const& b) { return gcd(a, b); }
#endif
}

#endif
