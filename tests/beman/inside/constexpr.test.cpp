// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Compile-time correctness suite. Every assertion here is a `static_assert`,
// so a regression in grid arithmetic, storage selection, trait predicates, or
// policy machinery fails the build rather than waiting for runtime test
// execution.
//
// Library quirks to respect:
//   - `rational::inv(0)` and division-by-zero `throw` under
//     `is_constant_evaluated()` and hard-fail the build — they cannot appear
//     in a constant expression.
//   - The unhandled-`checked` path (out-of-range value, no clamp/wrap/
//     sentinel) still aborts constant evaluation via the
//     `is_constant_evaluated()` guard inside `policy::report` (clearer
//     than the prior unconditional throw).

#include <beman/inside/inside.hpp>
#include <beman/inside/numeric_limits.hpp>
#include <beman/inside/predicates.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <type_traits>

using namespace beman::inside;
using namespace beman::inside::detail;

//---------------------------------------------------------------------------
// rational
//---------------------------------------------------------------------------
// constexpr: rational identities
TEST(ConstexprTest, constexpr_rational_identities)
{
  // construction + canonicalisation
  static_assert(rational{6u, 8} == rational{3u, 4});
  static_assert(rational{0u, 7} == rational{0u, 1});
  static_assert(rational{-1, 2} == rational{1, -2});
  static_assert(rational{-1, -2} == rational{1, 2});

  // sign + abs
  static_assert(sign(rational{1, -2}) == -1);
  static_assert(sign((0_r)) == 0);
  static_assert(sign(rational{3u, 4}) == 1);
  static_assert(abs(rational{3, -4}) == rational{3u, 4});

  // unary minus
  static_assert(-rational{3u, 4} == rational{3, -4});
}

// constexpr: rational arithmetic
TEST(ConstexprTest, constexpr_rational_arithmetic)
{
  // +/-/*//  return slim::optional<rational>; the * deref is the canonical
  // form used elsewhere in the codebase (mirrors `2_r/3` literal pattern).
  static_assert(*(rational{3u, 2} + rational{1u, 5}) == rational{17u, 10});
  static_assert(*(rational{3u, 4} - rational{1u, 4}) == rational{1u, 2});
  static_assert(*(rational{2u, 3} * rational{3u, 4}) == rational{1u, 2});
  static_assert(*(rational{1u, 2} / rational{1u, 4}) == rational{2u, 1});

  // gcd is also optional-returning
  static_assert(*gcd(rational{2u, 3}, rational{1u, 6}) == rational{1u, 6});

  // Div-by-zero is runtime-only — at compile time `rational::inv(0)` throws
  // under `is_constant_evaluated()`, which hard-fails the build.
}

// constexpr: rational rounding helpers
TEST(ConstexprTest, constexpr_rational_rounding_helpers)
{
  // 7/2 = 3.5
  static_assert(trunc(rational{7u, 2}) == 3);
  static_assert(floor(rational{7u, 2}) == 3);
  static_assert(round(rational{7u, 2}) == 4);

  // -7/2 = -3.5 — floor steps further toward -inf, round goes half-away-from-zero
  static_assert(trunc(rational{7, -2}) == -3);
  static_assert(floor(rational{7, -2}) == -4);
  static_assert(round(rational{7, -2}) == -4);
}

// constexpr: rational inv and divides_evenly
TEST(ConstexprTest, constexpr_rational_inv_and_divides_evenly)
{
  static_assert(*rational::inv(rational{3u, 4}) == rational{4u, 3});
  static_assert(*rational::inv(rational{2u, 7}) == rational{7u, 2});
  // `rational::inv(0_r)` is runtime-only — see file header.

  static_assert(divides_evenly(6_r, 2_r));
  static_assert(!(divides_evenly(7_r, 2_r)));
  static_assert(divides_evenly(0_r, 2_r));
}

//---------------------------------------------------------------------------
// interval
//---------------------------------------------------------------------------
// constexpr: interval predicates
TEST(ConstexprTest, constexpr_interval_predicates)
{
  constexpr interval a{0, 10};
  constexpr interval b{5, 15};
  constexpr interval c{20, 30};
  constexpr interval inner{2, 8};

  static_assert(includes(a, 5));
  static_assert(includes(a, 0));
  static_assert(includes(a, 10));
  static_assert(!(includes(a, 11)));

  static_assert(includes(a, inner));
  static_assert(!(includes(inner, a)));

  static_assert(overlaps(a, b));
  static_assert(!(excludes(a, b)));
  static_assert(excludes(a, c));
  static_assert(!(overlaps(a, c)));
}

// constexpr: interval arithmetic
TEST(ConstexprTest, constexpr_interval_arithmetic)
{
  constexpr interval a{0, 10};
  constexpr interval b{0, 5};

  static_assert(*(a + b) == interval{0, 15});
  static_assert(*(a - b) == interval{-5, 10});
  static_assert(*(a * b) == interval{0, 50});

  // division by an interval that straddles zero returns nullopt
  constexpr interval zero_crossing{-1, 1};
  static_assert(!((a / zero_crossing).has_value()));

  // unary minus flips and swaps
  static_assert(-a == interval{-10, 0});
}

// constexpr: interval divides_evenly
TEST(ConstexprTest, constexpr_interval_divides_evenly)
{
  constexpr interval grid_iv{0, 10};
  static_assert(grid_iv.divides_evenly(2_r));
  static_assert(grid_iv.divides_evenly(rational{1u, 2}));   // 10 / 0.5 = 20
  static_assert(!(grid_iv.divides_evenly(3_r)));
}

//---------------------------------------------------------------------------
// grid
//---------------------------------------------------------------------------
// constexpr: grid arithmetic produces expected result grids
TEST(ConstexprTest, constexpr_grid_arithmetic_produces_expected_result_grids)
{
  constexpr grid g_a{{0, 10}, 1};
  constexpr grid g_b{{0,  5}, 1};

  constexpr auto sum  = g_a + g_b;
  static_assert(sum.has_value());
  static_assert(sum->Interval == interval{0, 15});
  static_assert(sum->Notch == 1);

  constexpr auto prod = g_a * g_b;
  static_assert(prod.has_value());
  static_assert(prod->Interval == interval{0, 50});

  // div by a zero-only divisor grid is nullopt
  constexpr grid g_zero{{0, 0}, 0};
  static_assert(!((g_a / g_zero).has_value()));
}

// constexpr: grid notch alignment via gcd
TEST(ConstexprTest, constexpr_grid_notch_alignment_via_gcd)
{
  // (notch 1) + (notch 0.5) → gcd = 0.5
  constexpr grid coarse{{0, 10}, 1};
  constexpr grid fine{{0, 5}, rational{1u, 2}};
  constexpr auto r = coarse + fine;
  static_assert(r.has_value());
  static_assert(r->Notch == rational{1u, 2});
}

//---------------------------------------------------------------------------
// storage selection
//---------------------------------------------------------------------------
// constexpr: storage_min picks the smallest fitting raw
TEST(ConstexprTest, constexpr_storage_min_picks_the_smallest_fitting_raw)
{
  // smallest_uint_for reserves the type's max as the slim::optional sentinel,
  // so a grid hitting UINT8_MAX exactly promotes to uint16_t.
  static_assert(std::is_same_v<raw_t<inside<{0,   100}>>, std::uint8_t>);
  static_assert(std::is_same_v<raw_t<inside<{0,   254}>>, std::uint8_t>);
  static_assert(std::is_same_v<raw_t<inside<{0,   255}>>, std::uint16_t>);
  static_assert(std::is_same_v<raw_t<inside<{0, 65534}>>, std::uint16_t>);
  static_assert(std::is_same_v<raw_t<inside<{0, 65535}>>, std::uint32_t>);

  // signed-direct: lower < 0 + notch 1 → signed int that fits the range.
  // INT8_MIN is reserved for the sentinel, so {-128, 127} promotes to int16_t.
  static_assert(std::is_same_v<raw_t<inside<{-40,   85}>>, std::int8_t>);
  static_assert(std::is_same_v<raw_t<inside<{-127, 127}>>, std::int8_t>);
  static_assert(std::is_same_v<raw_t<inside<{-128, 127}>>, std::int16_t>);

  // notch 0 → rational raw
  static_assert(std::is_same_v<raw_t<inside<{{-10, 10}, 0}>>, rational>);

  // fractional notch with unsigned offset (signed lower forced into offset
  // encoding because notch != 1).
  static_assert(std::is_same_v<raw_t<inside<{{-5, 5}, rational{1u, 2}}>>,
                                std::uint8_t>);
}

// constexpr: storage-kind classification
TEST(ConstexprTest, constexpr_storage_kind_classification)
{
  // The disjoint storage encodings, deduced from the grid.
  // value_raw: Raw == value as a plain int (notch 1 + lower 0, or signed raw).
  static_assert(value_raw<inside<{0,   100}>>);
  static_assert(value_raw<inside<{-40,  85}>>);
  // rational_raw: notch 0 — Raw is the value as a rational.
  static_assert(rational_raw<inside<{{-10, 10}, 0}>>);
  // index_raw: notch 1 with non-zero unsigned lower, OR fractional notch — Raw
  // is a 0-based notch index.
  static_assert(index_raw<inside<{5, 100}>>);
  static_assert(index_raw<inside<{{0, 5}, rational{1u, 2}}>>);
}

//---------------------------------------------------------------------------
// inside arithmetic
//---------------------------------------------------------------------------
// constexpr: inside +/-/* on signed-direct grids
TEST(ConstexprTest, constexpr_inside_plus_on_signed_direct_grids)
{
  using s = inside<{-100, 100}>;
  constexpr s a{30}, b{20};
  static_assert(a + b == 50);
  static_assert(a - b == 10);
  static_assert(b - a == -10);
  static_assert(a * b == 600);
  static_assert(-a == -30);
}

// constexpr: inside +/-/* on offset-encoded grids
TEST(ConstexprTest, constexpr_inside_plus_on_offset_encoded_grids)
{
  using o = inside<{10, 50}>;                 // offset encoding (uint8 raw)
  static_assert(index_raw<o>);

  constexpr o a{15}, b{40};
  static_assert(a + b == 55);
  static_assert(b - a == 25);
}

// constexpr: inside +/-/* on fractional-notch grids
TEST(ConstexprTest, constexpr_inside_plus_on_fractional_notch_grids)
{
  using f = inside<{{0, 10}, rational{1u, 2}}>;     // notch 1/2
  constexpr f a{rational{3u, 2}}, b{rational{5u, 2}};
  static_assert(a + b == 4);
  static_assert(b - a == 1);
  static_assert(a * b == rational{15u, 4});
}

// constexpr: division returns slim::optional
TEST(ConstexprTest, constexpr_division_returns_slim_optional)
{
  using v = inside<{1, 255}>;
  constexpr v a{102};
  constexpr v b{16};
  constexpr auto q = a / b;
  static_assert(q.has_value());
  static_assert(*q == *(51_r / 8));

  // snap selects native integer division — result has integer raw
  using vi = inside<{0, 100}, snap>;
  constexpr vi p{51}, r{8};
  constexpr auto qi = p / r;
  static_assert(!(std::is_same_v<typename decltype(qi)::value_type::raw_type,
                                       rational>));
  static_assert(*qi == 6);
}

// constexpr: modulo under snap
TEST(ConstexprTest, constexpr_modulo_under_snap)
{
  using v = inside<{0, 100}, snap>;
  constexpr v a{17}, b{5};
  constexpr auto m = a % b;
  static_assert(m.has_value());
  static_assert(*m == 2);
}

// constexpr: add_all / mul_all folds
TEST(ConstexprTest, constexpr_add_all_mul_all_folds)
{
  using v = inside<{0, 100}>;
  constexpr v a{10}, b{20}, c{30}, d{40};
  static_assert(add_all(a, b, c, d) == 100);

  constexpr v p{2}, q{3}, r{5};
  static_assert(mul_all(p, q, r) == 30);
}

//---------------------------------------------------------------------------
// casts
//---------------------------------------------------------------------------
// constexpr: unchecked_cast preserves in-range values
TEST(ConstexprTest, constexpr_unchecked_cast_preserves_in_range_values)
{
  using pct = inside<{0, 100}>;
  static_assert(unchecked_cast<pct>(42) == 42);
  static_assert(unchecked_cast<pct>(0)  ==  0);
  static_assert(unchecked_cast<pct>(100) == 100);
}

// constexpr: checked_cast with in-range values
TEST(ConstexprTest, constexpr_checked_cast_with_in_range_values)
{
  using pct = inside<{0, 100}>;
  static_assert(checked_cast<pct>( 42) ==  42);
  static_assert(checked_cast<pct>(100) == 100);
  static_assert(checked_cast<pct>(  0) ==   0);
}

// constexpr: clamp_cast clamps out-of-range
TEST(ConstexprTest, constexpr_clamp_cast_clamps_out_of_range)
{
  using pct = inside<{0, 100}>;
  static_assert(clamp_cast<pct>(150) == 100);
  static_assert(clamp_cast<pct>(-5)  ==   0);
  static_assert(clamp_cast<pct>( 42) ==  42);
}

// constexpr: wrap_cast wraps modulo the grid
TEST(ConstexprTest, constexpr_wrap_cast_wraps_modulo_the_grid)
{
  using angle = inside<{0, 359}>;
  static_assert(wrap_cast<angle>(370) ==  10);
  static_assert(wrap_cast<angle>(-10) == 350);
  static_assert(wrap_cast<angle>(180) == 180);
}

// constexpr: add_all_into / mul_all_into clip to target grid
TEST(ConstexprTest, constexpr_add_all_into_mul_all_into_clip_to_target_grid)
{
  using bus = inside<{-100, 100}, clamp>;
  using ch  = inside<{-50, 50}>;
  constexpr ch a{30}, b{40}, c{45};

  // Naive widened sum would be 115; add_all_into clips to bus's interval.
  static_assert(add_all_into<bus>(a, b, c) == 100);
  static_assert(add_all_into<bus>(ch{10}, ch{-5}, ch{3}) == 8);

  using small = inside<{0, 50}, clamp>;
  using v = inside<{0, 10}>;
  static_assert(mul_all_into<small>(v{4}, v{5}) == 20);
  static_assert(mul_all_into<small>(v{6}, v{10}) == 50);  // 60 clamped
}

// constexpr: clamp_floor / clamp_ceil / clamp_round
TEST(ConstexprTest, constexpr_clamp_floor_clamp_ceil_clamp_round)
{
  using coarse = inside<{{0, 10}, 2}>;
  static_assert(clamp_floor<coarse>( 3.0) == 2);
  static_assert(clamp_ceil <coarse>( 3.0) == 4);
  static_assert(clamp_round<coarse>( 3.0) == 4);

  // out of range clamps to boundary
  static_assert(clamp_floor<coarse>(15.0) == 10);
  static_assert(clamp_floor<coarse>(-3.0) ==  0);
}

// constexpr: conversion predicates
TEST(ConstexprTest, constexpr_conversion_predicates)
{
  using pct = inside<{0, 100}>;
  static_assert(!(will_conversion_overflow<pct>(  50)));
  static_assert(will_conversion_overflow<pct>( 150));
  static_assert(will_conversion_overflow<pct>(  -1));
  static_assert(!(will_conversion_overflow<pct>(   0)));

  using coarse = inside<{{0, 10}, 2}>;
  static_assert(!(will_conversion_trunc<coarse>(4)));
  static_assert(will_conversion_trunc<coarse>(3));
  static_assert(!(will_conversion_trunc<coarse>(11)));

  static_assert(!(is_conversion_lossy<coarse>(4)));
  static_assert(is_conversion_lossy<coarse>(3));
  static_assert(is_conversion_lossy<coarse>(20));
}

//---------------------------------------------------------------------------
// numeric_limits
//---------------------------------------------------------------------------
// constexpr: numeric_limits<inside>
TEST(ConstexprTest, constexpr_numeric_limits_inside)
{
  using pct = inside<{0, 100}>;
  using nl  = std::numeric_limits<pct>;
  static_assert(nl::is_specialized);
  static_assert(nl::is_bounded);
  static_assert(nl::is_integer);
  static_assert(nl::is_exact);
  static_assert(!(nl::is_signed));
  static_assert(!(nl::is_modulo));
  static_assert(nl::min()    == pct{0});
  static_assert(nl::max()    == pct{100});
  static_assert(nl::lowest() == pct{0});

  // signed grid → is_signed
  using temp = inside<{-40, 60}>;
  static_assert(std::numeric_limits<temp>::is_signed);

  // wrap policy → is_modulo
  using ang = inside<{0, 359}, wrap>;
  static_assert(std::numeric_limits<ang>::is_modulo);
}

//---------------------------------------------------------------------------
// inside_range
//---------------------------------------------------------------------------
// constexpr: inside_range iterates the full grid
TEST(ConstexprTest, constexpr_inside_range_iterates_the_full_grid)
{
  // 0 + 1 + ... + 9 = 45
  constexpr imax sum = [] {
    imax s = 0;
    for (auto i : inside_range<{0, 9}>{})
      s += imax{i};
    return s;
  }();
  static_assert(sum == 45);

  // wrap-start: visits every element exactly once, starting mid-range
  constexpr imax sum_from_5 = [] {
    imax s = 0;
    for (auto i : inside_range<{0, 9}>{inside<{0, 9}>{5}})
      s += imax{i};
    return s;
  }();
  static_assert(sum_from_5 == 45);
}

// constexpr: inside_range on fractional notch grid
TEST(ConstexprTest, constexpr_inside_range_on_fractional_notch_grid)
{
  // {-1, 1} with notch 1/2: visits -1, -0.5, 0, 0.5, 1 (5 slots).
  // Sum: -1 + -0.5 + 0 + 0.5 + 1 = 0 (so we count instead).
  constexpr int count = [] {
    int n = 0;
    for (auto v : inside_range<{{-1, 1}, notch<1, 2>}>{}) { (void)v; ++n; }
    return n;
  }();
  static_assert(count == 5);

  // Sum the doubled values (-1 + -0.5 + 0 + 0.5 + 1) * 2 = 0
  constexpr imax twice_sum = [] {
    imax s = 0;
    for (auto v : inside_range<{{-1, 1}, notch<1, 2>}>{})
      s += static_cast<imax>(2 * v.to<double>().value());
    return s;
  }();
  static_assert(twice_sum == 0);
}

//---------------------------------------------------------------------------
// policy machinery
//---------------------------------------------------------------------------
// constexpr: implied_flags / merged_implied_flags
TEST(ConstexprTest, constexpr_implied_flags_merged_implied_flags)
{
  auto noop = [](auto&, auto) {};
  auto noerr = [](auto&, errc) {};

  using clamp_tag    = on_clamp_t<decltype(noop)>;
  using overflow_tag = on_overflow_t<decltype(noerr)>;
  using wrap_tag     = on_wrap_t<decltype(noop)>;

  static_assert(implied_flags<clamp_tag>    == clamp);
  static_assert(implied_flags<wrap_tag>     == wrap);
  static_assert(implied_flags<overflow_tag> == checked);

  // OR-merge across the pack
  static_assert(merged_implied_flags<clamp_tag, overflow_tag>
                 == (clamp | checked));
}

// constexpr: IsPolicy / UsesErrorRef
TEST(ConstexprTest, constexpr_ispolicy_useserrorref)
{
  static_assert(IsPolicy<policy<checked>>);
  static_assert(IsPolicy<policy<none, error_ref>>);
  static_assert(!(IsPolicy<int>));

  static_assert(!(UsesErrorRef<policy<checked>>));
  static_assert(UsesErrorRef<policy<checked, error_ref>>);
}

//---------------------------------------------------------------------------
// just / _ins literal
//---------------------------------------------------------------------------
// constexpr: just<N> and _ins literal
TEST(ConstexprTest, constexpr_just_n_and_ins_literal)
{
  static_assert(just<1>  == 1);
  static_assert(just<42> == 42);

  constexpr auto five = 5_ins;
  static_assert(Lower<decltype(five)> == 5);
  static_assert(Upper<decltype(five)> == 5);
  static_assert(five == 5);

  // Composes with inside — grid widens via add
  using pct = inside<{0, 100}>;
  static_assert(10_ins + pct{40} == 50);
}

//---------------------------------------------------------------------------
// lift — additional coverage beyond test_lift.cpp
//---------------------------------------------------------------------------
// constexpr: lift over multiple slim::optional args
TEST(ConstexprTest, constexpr_lift_over_multiple_slim_optional_args)
{
  constexpr auto plus = [](int a, int b) { return a + b; };

  constexpr slim::optional<int> a{2}, b{3};
  static_assert(*lift(plus, a, b) == 5);

  // one empty → nullopt
  constexpr slim::optional<int> empty{slim::nullopt};
  static_assert(!(lift(plus, a, empty).has_value()));

  // three-arg fold over a mix of values and optionals
  constexpr auto sum3 = [](int x, int y, int z) { return x + y + z; };
  static_assert(*lift(sum3, a, 4, b) == 9);
}

//---------------------------------------------------------------------------
// slim::optional<inside> size invariant
//---------------------------------------------------------------------------
// constexpr: optional<inside> is the same size as inside
TEST(ConstexprTest, constexpr_optional_inside_is_the_same_size_as_inside)
{
  // slim::optional<inside> uses a sentinel value rather than a bool flag.
  static_assert(sizeof(slim::optional<inside<{0, 100}>>)
                 == sizeof(inside<{0, 100}>));
  static_assert(sizeof(slim::optional<inside<{-40, 85}>>)
                 == sizeof(inside<{-40, 85}>));
}
