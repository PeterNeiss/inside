// Compile-time correctness suite. Every assertion here is `STATIC_REQUIRE`
// (or `static_assert`), so a regression in grid arithmetic, storage selection,
// trait predicates, or policy machinery fails the build rather than waiting
// for runtime test execution.
//
// Library quirks to respect:
//   - `rational::inv(0)` and division-by-zero `throw` under
//     `is_constant_evaluated()` and hard-fail the build — they cannot appear
//     in a constant expression.
//   - The unhandled-`checked` path (out-of-range value, no clamp/wrap/
//     sentinel) still aborts constant evaluation via the
//     `is_constant_evaluated()` guard inside `policy::report` (clearer
//     than the prior unconditional throw).

#include "bound/bound.hpp"
#include "bound/numeric_limits.hpp"
#include "bound/predicates.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <limits>
#include <type_traits>

using namespace bnd;

//---------------------------------------------------------------------------
// rational
//---------------------------------------------------------------------------
TEST_CASE("constexpr: rational identities", "[constexpr][rational]")
{
  // construction + canonicalisation
  STATIC_REQUIRE(rational{6u, 8} == rational{3u, 4});
  STATIC_REQUIRE(rational{0u, 7} == rational{0u, 1});
  STATIC_REQUIRE(rational{-1, 2} == rational{1, -2});
  STATIC_REQUIRE(rational{-1, -2} == rational{1, 2});

  // sign + abs
  STATIC_REQUIRE(rational{1, -2}.sign() == -1);
  STATIC_REQUIRE((0_r).sign() == 0);
  STATIC_REQUIRE(rational{3u, 4}.sign() == 1);
  STATIC_REQUIRE(abs(rational{3, -4}) == rational{3u, 4});

  // unary minus
  STATIC_REQUIRE(-rational{3u, 4} == rational{3, -4});
}

TEST_CASE("constexpr: rational arithmetic", "[constexpr][rational]")
{
  // +/-/*//  return slim::optional<rational>; the * deref is the canonical
  // form used elsewhere in the codebase (mirrors `2_r/3` literal pattern).
  STATIC_REQUIRE(*(rational{3u, 2} + rational{1u, 5}) == rational{17u, 10});
  STATIC_REQUIRE(*(rational{3u, 4} - rational{1u, 4}) == rational{1u, 2});
  STATIC_REQUIRE(*(rational{2u, 3} * rational{3u, 4}) == rational{1u, 2});
  STATIC_REQUIRE(*(rational{1u, 2} / rational{1u, 4}) == rational{2u, 1});

  // gcd is also optional-returning
  STATIC_REQUIRE(*gcd(rational{2u, 3}, rational{1u, 6}) == rational{1u, 6});

  // Div-by-zero is runtime-only — at compile time `rational::inv(0)` throws
  // under `is_constant_evaluated()`, which hard-fails the build.
}

TEST_CASE("constexpr: rational rounding helpers", "[constexpr][rational]")
{
  // 7/2 = 3.5
  STATIC_REQUIRE(rational{7u, 2}.trunc() == 3);
  STATIC_REQUIRE(rational{7u, 2}.floor() == 3);
  STATIC_REQUIRE(rational{7u, 2}.round() == 4);

  // -7/2 = -3.5 — floor steps further toward -inf, round goes half-away-from-zero
  STATIC_REQUIRE(rational{7, -2}.trunc() == -3);
  STATIC_REQUIRE(rational{7, -2}.floor() == -4);
  STATIC_REQUIRE(rational{7, -2}.round() == -4);
}

TEST_CASE("constexpr: rational inv and divides_evenly", "[constexpr][rational]")
{
  STATIC_REQUIRE(*rational::inv(rational{3u, 4}) == rational{4u, 3});
  STATIC_REQUIRE(*rational::inv(rational{2u, 7}) == rational{7u, 2});
  // `rational::inv(0_r)` is runtime-only — see file header.

  STATIC_REQUIRE(divides_evenly(6_r, 2_r));
  STATIC_REQUIRE_FALSE(divides_evenly(7_r, 2_r));
  STATIC_REQUIRE(divides_evenly(0_r, 2_r));
}

//---------------------------------------------------------------------------
// interval
//---------------------------------------------------------------------------
TEST_CASE("constexpr: interval predicates", "[constexpr][interval]")
{
  constexpr interval a{0, 10};
  constexpr interval b{5, 15};
  constexpr interval c{20, 30};
  constexpr interval inner{2, 8};

  STATIC_REQUIRE(a.includes(5));
  STATIC_REQUIRE(a.includes(0));
  STATIC_REQUIRE(a.includes(10));
  STATIC_REQUIRE_FALSE(a.includes(11));

  STATIC_REQUIRE(a.includes(inner));
  STATIC_REQUIRE_FALSE(inner.includes(a));

  STATIC_REQUIRE(a.overlaps(b));
  STATIC_REQUIRE_FALSE(a.excludes(b));
  STATIC_REQUIRE(a.excludes(c));
  STATIC_REQUIRE_FALSE(a.overlaps(c));
}

TEST_CASE("constexpr: interval arithmetic", "[constexpr][interval]")
{
  constexpr interval a{0, 10};
  constexpr interval b{0, 5};

  STATIC_REQUIRE(*(a + b) == interval{0, 15});
  STATIC_REQUIRE(*(a - b) == interval{-5, 10});
  STATIC_REQUIRE(*(a * b) == interval{0, 50});

  // division by an interval that straddles zero returns nullopt
  constexpr interval zero_crossing{-1, 1};
  STATIC_REQUIRE_FALSE((a / zero_crossing).has_value());

  // unary minus flips and swaps
  STATIC_REQUIRE(-a == interval{-10, 0});
}

TEST_CASE("constexpr: interval divides_evenly", "[constexpr][interval]")
{
  constexpr interval grid_iv{0, 10};
  STATIC_REQUIRE(grid_iv.divides_evenly(2_r));
  STATIC_REQUIRE(grid_iv.divides_evenly(rational{1u, 2}));   // 10 / 0.5 = 20
  STATIC_REQUIRE_FALSE(grid_iv.divides_evenly(3_r));
}

//---------------------------------------------------------------------------
// grid
//---------------------------------------------------------------------------
TEST_CASE("constexpr: grid arithmetic produces expected result grids",
          "[constexpr][grid]")
{
  constexpr grid g_a{{0, 10}, 1};
  constexpr grid g_b{{0,  5}, 1};

  constexpr auto sum  = g_a + g_b;
  STATIC_REQUIRE(sum.has_value());
  STATIC_REQUIRE(sum->Interval == interval{0, 15});
  STATIC_REQUIRE(sum->Notch == 1);

  constexpr auto prod = g_a * g_b;
  STATIC_REQUIRE(prod.has_value());
  STATIC_REQUIRE(prod->Interval == interval{0, 50});

  // div by a zero-only divisor grid is nullopt
  constexpr grid g_zero{{0, 0}, 0};
  STATIC_REQUIRE_FALSE((g_a / g_zero).has_value());
}

TEST_CASE("constexpr: grid notch alignment via gcd", "[constexpr][grid]")
{
  // (notch 1) + (notch 0.5) → gcd = 0.5
  constexpr grid coarse{{0, 10}, 1};
  constexpr grid fine{{0, 5}, rational{1u, 2}};
  constexpr auto r = coarse + fine;
  STATIC_REQUIRE(r.has_value());
  STATIC_REQUIRE(r->Notch == rational{1u, 2});
}

//---------------------------------------------------------------------------
// storage selection
//---------------------------------------------------------------------------
TEST_CASE("constexpr: storage_min picks the smallest fitting raw",
          "[constexpr][storage]")
{
  // smallest_uint_for reserves the type's max as the slim::optional sentinel,
  // so a grid hitting UINT8_MAX exactly promotes to uint16_t.
  STATIC_REQUIRE(std::is_same_v<raw_t<bound<{0,   100}>>, std::uint8_t>);
  STATIC_REQUIRE(std::is_same_v<raw_t<bound<{0,   254}>>, std::uint8_t>);
  STATIC_REQUIRE(std::is_same_v<raw_t<bound<{0,   255}>>, std::uint16_t>);
  STATIC_REQUIRE(std::is_same_v<raw_t<bound<{0, 65534}>>, std::uint16_t>);
  STATIC_REQUIRE(std::is_same_v<raw_t<bound<{0, 65535}>>, std::uint32_t>);

  // signed-direct: lower < 0 + notch 1 → signed int that fits the range.
  // INT8_MIN is reserved for the sentinel, so {-128, 127} promotes to int16_t.
  STATIC_REQUIRE(std::is_same_v<raw_t<bound<{-40,   85}>>, std::int8_t>);
  STATIC_REQUIRE(std::is_same_v<raw_t<bound<{-127, 127}>>, std::int8_t>);
  STATIC_REQUIRE(std::is_same_v<raw_t<bound<{-128, 127}>>, std::int16_t>);

  // notch 0 → rational raw
  STATIC_REQUIRE(std::is_same_v<raw_t<bound<{{-10, 10}, 0}>>, rational>);

  // fractional notch with unsigned offset (signed lower forced into offset
  // encoding because notch != 1).
  STATIC_REQUIRE(std::is_same_v<raw_t<bound<{{-5, 5}, rational{1u, 2}}>>,
                                std::uint8_t>);
}

TEST_CASE("constexpr: IsDirectStorage / IsRawRational / IsNotchStorage",
          "[constexpr][storage]")
{
  // Direct storage: Raw == value. Three legal shapes:
  //   - rational raw (notch 0)
  //   - notch 1 + lower 0
  //   - notch 1 + signed raw
  STATIC_REQUIRE(IsDirectStorage<bound<{0,   100}>>);
  STATIC_REQUIRE(IsDirectStorage<bound<{-40,  85}>>);
  STATIC_REQUIRE(IsDirectStorage<bound<{{-10, 10}, 0}>>);

  // Offset encoding: notch 1 with non-zero unsigned lower OR fractional notch
  STATIC_REQUIRE_FALSE(IsDirectStorage<bound<{5, 100}>>);
  STATIC_REQUIRE_FALSE(IsDirectStorage<bound<{{0, 5}, rational{1u, 2}}>>);
  STATIC_REQUIRE(IsNotchStorage<bound<{5, 100}>>);

  STATIC_REQUIRE(IsRawRational<bound<{{-10, 10}, 0}>>);
  STATIC_REQUIRE_FALSE(IsRawRational<bound<{0, 100}>>);
}

//---------------------------------------------------------------------------
// bound arithmetic
//---------------------------------------------------------------------------
TEST_CASE("constexpr: bound +/-/* on signed-direct grids",
          "[constexpr][bound][arithmetic]")
{
  using s = bound<{-100, 100}>;
  constexpr s a{30}, b{20};
  STATIC_REQUIRE(a + b == 50);
  STATIC_REQUIRE(a - b == 10);
  STATIC_REQUIRE(b - a == -10);
  STATIC_REQUIRE(a * b == 600);
  STATIC_REQUIRE(-a == -30);
}

TEST_CASE("constexpr: bound +/-/* on offset-encoded grids",
          "[constexpr][bound][arithmetic]")
{
  using o = bound<{10, 50}>;                 // offset encoding (uint8 raw)
  STATIC_REQUIRE_FALSE(IsDirectStorage<o>);

  constexpr o a{15}, b{40};
  STATIC_REQUIRE(a + b == 55);
  STATIC_REQUIRE(b - a == 25);
}

TEST_CASE("constexpr: bound +/-/* on fractional-notch grids",
          "[constexpr][bound][arithmetic]")
{
  using f = bound<{{0, 10}, rational{1u, 2}}>;     // notch 1/2
  constexpr f a{rational{3u, 2}}, b{rational{5u, 2}};
  STATIC_REQUIRE(a + b == 4);
  STATIC_REQUIRE(b - a == 1);
  STATIC_REQUIRE(a * b == rational{15u, 4});
}

TEST_CASE("constexpr: division returns slim::optional", "[constexpr][bound][div]")
{
  using v = bound<{1, 255}>;
  constexpr v a{102};
  constexpr v b{16};
  constexpr auto q = a / b;
  STATIC_REQUIRE(q.has_value());
  STATIC_REQUIRE(*q == *(51_r / 8));

  // ignore_round selects native integer division — result has integer raw
  using vi = bound<{0, 100}, ignore_round>;
  constexpr vi p{51}, r{8};
  constexpr auto qi = p / r;
  STATIC_REQUIRE_FALSE(std::is_same_v<typename decltype(qi)::value_type::raw_type,
                                       rational>);
  STATIC_REQUIRE(*qi == 6);
}

TEST_CASE("constexpr: modulo under ignore_round", "[constexpr][bound][mod]")
{
  using v = bound<{0, 100}, ignore_round>;
  constexpr v a{17}, b{5};
  constexpr auto m = a % b;
  STATIC_REQUIRE(m.has_value());
  STATIC_REQUIRE(*m == 2);
}

TEST_CASE("constexpr: add_all / mul_all folds", "[constexpr][bound][fold]")
{
  using v = bound<{0, 100}>;
  constexpr v a{10}, b{20}, c{30}, d{40};
  STATIC_REQUIRE(add_all(a, b, c, d) == 100);

  constexpr v p{2}, q{3}, r{5};
  STATIC_REQUIRE(mul_all(p, q, r) == 30);
}

//---------------------------------------------------------------------------
// casts
//---------------------------------------------------------------------------
TEST_CASE("constexpr: unchecked_cast preserves in-range values",
          "[constexpr][bound][cast]")
{
  using pct = bound<{0, 100}>;
  STATIC_REQUIRE(unchecked_cast<pct>(42) == 42);
  STATIC_REQUIRE(unchecked_cast<pct>(0)  ==  0);
  STATIC_REQUIRE(unchecked_cast<pct>(100) == 100);
}

TEST_CASE("constexpr: checked_cast with in-range values",
          "[constexpr][bound][cast]")
{
  using pct = bound<{0, 100}>;
  STATIC_REQUIRE(checked_cast<pct>( 42) ==  42);
  STATIC_REQUIRE(checked_cast<pct>(100) == 100);
  STATIC_REQUIRE(checked_cast<pct>(  0) ==   0);
}

TEST_CASE("constexpr: clamp_cast clamps out-of-range",
          "[constexpr][bound][cast]")
{
  using pct = bound<{0, 100}>;
  STATIC_REQUIRE(clamp_cast<pct>(150) == 100);
  STATIC_REQUIRE(clamp_cast<pct>(-5)  ==   0);
  STATIC_REQUIRE(clamp_cast<pct>( 42) ==  42);
}

TEST_CASE("constexpr: wrap_cast wraps modulo the grid",
          "[constexpr][bound][cast]")
{
  using angle = bound<{0, 359}>;
  STATIC_REQUIRE(wrap_cast<angle>(370) ==  10);
  STATIC_REQUIRE(wrap_cast<angle>(-10) == 350);
  STATIC_REQUIRE(wrap_cast<angle>(180) == 180);
}

TEST_CASE("constexpr: add_all_into / mul_all_into clip to target grid",
          "[constexpr][bound][fold]")
{
  using bus = bound<{-100, 100}, clamp>;
  using ch  = bound<{-50, 50}>;
  constexpr ch a{30}, b{40}, c{45};

  // Naive widened sum would be 115; add_all_into clips to bus's interval.
  STATIC_REQUIRE(add_all_into<bus>(a, b, c) == 100);
  STATIC_REQUIRE(add_all_into<bus>(ch{10}, ch{-5}, ch{3}) == 8);

  using small = bound<{0, 50}, clamp>;
  using v = bound<{0, 10}>;
  STATIC_REQUIRE(mul_all_into<small>(v{4}, v{5}) == 20);
  STATIC_REQUIRE(mul_all_into<small>(v{6}, v{10}) == 50);  // 60 clamped
}

TEST_CASE("constexpr: clamp_floor / clamp_ceil / clamp_round",
          "[constexpr][bound][cast][round]")
{
  using coarse = bound<{{0, 10}, 2}>;
  STATIC_REQUIRE(clamp_floor<coarse>( 3.0) == 2);
  STATIC_REQUIRE(clamp_ceil <coarse>( 3.0) == 4);
  STATIC_REQUIRE(clamp_round<coarse>( 3.0) == 4);

  // out of range clamps to boundary
  STATIC_REQUIRE(clamp_floor<coarse>(15.0) == 10);
  STATIC_REQUIRE(clamp_floor<coarse>(-3.0) ==  0);
}

TEST_CASE("constexpr: conversion predicates", "[constexpr][bound][predicates]")
{
  using pct = bound<{0, 100}>;
  STATIC_REQUIRE_FALSE(will_conversion_overflow<pct>(  50));
  STATIC_REQUIRE      (will_conversion_overflow<pct>( 150));
  STATIC_REQUIRE      (will_conversion_overflow<pct>(  -1));
  STATIC_REQUIRE_FALSE(will_conversion_overflow<pct>(   0));

  using coarse = bound<{{0, 10}, 2}>;
  STATIC_REQUIRE_FALSE(will_conversion_truncate<coarse>(4));
  STATIC_REQUIRE      (will_conversion_truncate<coarse>(3));
  STATIC_REQUIRE_FALSE(will_conversion_truncate<coarse>(11));

  STATIC_REQUIRE_FALSE(is_conversion_lossy<coarse>(4));
  STATIC_REQUIRE      (is_conversion_lossy<coarse>(3));
  STATIC_REQUIRE      (is_conversion_lossy<coarse>(20));
}

//---------------------------------------------------------------------------
// numeric_limits
//---------------------------------------------------------------------------
TEST_CASE("constexpr: numeric_limits<bound>", "[constexpr][numeric_limits]")
{
  using pct = bound<{0, 100}>;
  using nl  = std::numeric_limits<pct>;
  STATIC_REQUIRE(nl::is_specialized);
  STATIC_REQUIRE(nl::is_bounded);
  STATIC_REQUIRE(nl::is_integer);
  STATIC_REQUIRE(nl::is_exact);
  STATIC_REQUIRE_FALSE(nl::is_signed);
  STATIC_REQUIRE_FALSE(nl::is_modulo);
  STATIC_REQUIRE(nl::min()    == pct{0});
  STATIC_REQUIRE(nl::max()    == pct{100});
  STATIC_REQUIRE(nl::lowest() == pct{0});

  // signed grid → is_signed
  using temp = bound<{-40, 60}>;
  STATIC_REQUIRE(std::numeric_limits<temp>::is_signed);

  // wrap policy → is_modulo
  using ang = bound<{0, 359}, wrap>;
  STATIC_REQUIRE(std::numeric_limits<ang>::is_modulo);
}

//---------------------------------------------------------------------------
// bound_range
//---------------------------------------------------------------------------
TEST_CASE("constexpr: bound_range iterates the full grid", "[constexpr][range]")
{
  // 0 + 1 + ... + 9 = 45
  constexpr imax sum = [] {
    imax s = 0;
    for (auto i : bound_range<{0, 9}>{})
      s += static_cast<imax>(i);
    return s;
  }();
  STATIC_REQUIRE(sum == 45);

  // wrap-start: visits every element exactly once, starting mid-range
  constexpr imax sum_from_5 = [] {
    imax s = 0;
    for (auto i : bound_range<{0, 9}>{bound<{0, 9}>{5}})
      s += static_cast<imax>(i);
    return s;
  }();
  STATIC_REQUIRE(sum_from_5 == 45);
}

TEST_CASE("constexpr: bound_range on fractional notch grid", "[constexpr][range]")
{
  // {-1, 1} with notch 1/2: visits -1, -0.5, 0, 0.5, 1 (5 slots).
  // Sum: -1 + -0.5 + 0 + 0.5 + 1 = 0 (so we count instead).
  constexpr int count = [] {
    int n = 0;
    for (auto v : bound_range<{{-1, 1}, notch<1, 2>}>{}) { (void)v; ++n; }
    return n;
  }();
  STATIC_REQUIRE(count == 5);

  // Sum the doubled values (-1 + -0.5 + 0 + 0.5 + 1) * 2 = 0
  constexpr imax twice_sum = [] {
    imax s = 0;
    for (auto v : bound_range<{{-1, 1}, notch<1, 2>}>{})
      s += static_cast<imax>(2 * v.to<double>().value());
    return s;
  }();
  STATIC_REQUIRE(twice_sum == 0);
}

//---------------------------------------------------------------------------
// policy machinery
//---------------------------------------------------------------------------
TEST_CASE("constexpr: implied_flags / merged_implied_flags",
          "[constexpr][policy]")
{
  auto noop = [](auto&, auto) {};
  auto noerr = [](auto&, errc) {};

  using clamp_tag    = on_clamp_t<decltype(noop)>;
  using overflow_tag = on_overflow_t<decltype(noerr)>;
  using wrap_tag     = on_wrap_t<decltype(noop)>;

  STATIC_REQUIRE(implied_flags<clamp_tag>    == clamp);
  STATIC_REQUIRE(implied_flags<wrap_tag>     == wrap);
  STATIC_REQUIRE(implied_flags<overflow_tag> == checked);

  // OR-merge across the pack
  STATIC_REQUIRE(merged_implied_flags<clamp_tag, overflow_tag>
                 == (clamp | checked));
}

TEST_CASE("constexpr: IsPolicy / UsesErrorRef",
          "[constexpr][policy]")
{
  STATIC_REQUIRE(IsPolicy<policy<checked>>);
  STATIC_REQUIRE(IsPolicy<policy<none, error_ref>>);
  STATIC_REQUIRE_FALSE(IsPolicy<int>);

  STATIC_REQUIRE_FALSE(UsesErrorRef<policy<checked>>);
  STATIC_REQUIRE(UsesErrorRef<policy<checked, error_ref>>);
}

//---------------------------------------------------------------------------
// just / _b literal
//---------------------------------------------------------------------------
TEST_CASE("constexpr: just<N> and _b literal", "[constexpr][literal]")
{
  STATIC_REQUIRE(just<1>  == 1);
  STATIC_REQUIRE(just<42> == 42);

  constexpr auto five = 5_b;
  STATIC_REQUIRE(Lower<decltype(five)> == 5);
  STATIC_REQUIRE(Upper<decltype(five)> == 5);
  STATIC_REQUIRE(five == 5);

  // Composes with bound — grid widens via add
  using pct = bound<{0, 100}>;
  STATIC_REQUIRE(10_b + pct{40} == 50);
}

//---------------------------------------------------------------------------
// lift — additional coverage beyond test_lift.cpp
//---------------------------------------------------------------------------
TEST_CASE("constexpr: lift over multiple slim::optional args", "[constexpr][lift]")
{
  constexpr auto plus = [](int a, int b) { return a + b; };

  constexpr slim::optional<int> a{2}, b{3};
  STATIC_REQUIRE(*lift(plus, a, b) == 5);

  // one empty → nullopt
  constexpr slim::optional<int> empty{slim::nullopt};
  STATIC_REQUIRE_FALSE(lift(plus, a, empty).has_value());

  // three-arg fold over a mix of values and optionals
  constexpr auto sum3 = [](int x, int y, int z) { return x + y + z; };
  STATIC_REQUIRE(*lift(sum3, a, 4, b) == 9);
}

//---------------------------------------------------------------------------
// slim::optional<bound> size invariant
//---------------------------------------------------------------------------
TEST_CASE("constexpr: optional<bound> is the same size as bound",
          "[constexpr][optional][size]")
{
  // slim::optional<bound> uses a sentinel value rather than a bool flag.
  STATIC_REQUIRE(sizeof(slim::optional<bound<{0, 100}>>)
                 == sizeof(bound<{0, 100}>));
  STATIC_REQUIRE(sizeof(slim::optional<bound<{-40, 85}>>)
                 == sizeof(bound<{-40, 85}>));
}
