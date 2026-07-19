#include "bound/bound.hpp"
#include "bound/io.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace bnd;
using namespace bnd::detail;

// Raw int/float/double are no longer arithmetic-mutation operands: `b += 1`,
// `b *= 2.0` are ill-formed (guidance static_assert in arithmetic.hpp). The only
// non-bound operand a compound assign accepts is a `rational`. As with the binary
// operators, the guidance overloads are SFINAE-transparent (the static_assert
// fires only on a real call), so the ill-formedness can't be probed with
// `requires` — only the sanctioned spellings are positively testable.
TEST_CASE("compound assignment: sanctioned RHS compiles", "[bound][compound]")
{
  using u100 = bound<{0, 100}>;
  STATIC_REQUIRE( requires(u100 b){ b += 1_b; });            // a bound
  STATIC_REQUIRE( requires(u100 b){ b += rational{1}; });    // a rational
}

TEST_CASE("compound assignment: boundable RHS", "[bound][compound]")
{
  using u100 = bound<{0, 100}>;
  STATIC_REQUIRE([]{ u100 a{50}, d{20}; a -= d; return a.as<imax>(); }() == 30);

  using ui = bound<{0, 100}, snap>;
  STATIC_REQUIRE([]{ ui d{50}, two{2}; d *= two; return d.as<imax>(); }() == 100);
  STATIC_REQUIRE([]{ ui e{60}, three{3}; e /= three; return e.as<imax>(); }() == 20);
  STATIC_REQUIRE([]{ ui f{17}, five{5}; f %= five; return f.as<imax>(); }() == 2);
}

TEST_CASE("compound /= and %= by a zero bound report by default", "[bound][compound]")
{
  using u100 = bound<{0, 100}>;
  u100 a{50};
  REQUIRE_THROWS_AS(([&]{ a /= u100{0}; }()), bnd::bound_error);

  using ui = bound<{0, 100}, snap>;
  ui s{50};
  REQUIRE_THROWS_AS(([&]{ s %= ui{0}; }()), bnd::bound_error);
}

TEST_CASE("compound assignment: rational RHS", "[bound][compound][rational]")
{
  // round_nearest required so the bound's rational assignment path is available.
  using rn = bound<{{0, 100}, notch<1, 100>}, round_nearest>;

  rn a{0.5_r};
  a += 0.25_r;                       // 0.50 + 0.25 = 0.75
  REQUIRE(a == rn{0.75_r});

  a -= 0.25_r;                       // 0.75 − 0.25 = 0.50
  REQUIRE(a == rn{0.5_r});

  a *= 4_r;                          // 0.50 × 4 = 2
  REQUIRE(a == rn{2_r});

  a /= 2_r;                          // 2 / 2 = 1
  REQUIRE(a == rn{1_r});
}

TEST_CASE("compound assignment: fractional RHS via rational", "[bound][compound][rational]")
{
  using rn = bound<{{-100, 100}, notch<1, 16>}, round_nearest>;

  rn a{1.0};                         // construction from a double is unchanged
  a += 2.5_r;
  REQUIRE(a == rn{3.5});

  a -= 1.5_r;
  REQUIRE(a == rn{2.0});

  a *= 1.5_r;
  REQUIRE(a == rn{3.0});

  a /= 2_r;
  REQUIRE(a == rn{1.5});
}

TEST_CASE("compound /= 0_r (rational zero) reports error", "[bound][compound][rational]")
{
  using rn = bound<{{0, 100}, notch<1, 100>}, round_nearest>;
  rn a{0.5_r};
  REQUIRE_THROWS_AS(([&]{ a /= 0_r; }()), bnd::bound_error);
}

TEST_CASE("increment / decrement", "[bound][compound][inc]")
{
  using u10 = bound<{0, 10}>;
  // The ++/-- happens inside each lambda body, fully sequenced before the lambda
  // returns; the `== N` compares the lambda's *result*, not a mutated operand. So
  // bugprone-inc-dec-in-conditions (which assumes a side-effect inside the
  // condition) is a false positive from the STATIC_REQUIRE macro expansion.
  // NOLINTBEGIN(bugprone-inc-dec-in-conditions)
  STATIC_REQUIRE([]{ u10 a{5}; ++a; return a.as<imax>(); }() == 6);
  STATIC_REQUIRE([]{ u10 a{5}; a++; return a.as<imax>(); }() == 6);
  STATIC_REQUIRE([]{ u10 a{5}; --a; return a.as<imax>(); }() == 4);
  STATIC_REQUIRE([]{ u10 a{5}; a--; return a.as<imax>(); }() == 4);

  // post-inc/dec returns the old value
  STATIC_REQUIRE([]{ u10 a{5}; auto r = a++; return r.as<imax>(); }() == 5);
  STATIC_REQUIRE([]{ u10 a{5}; auto r = a--; return r.as<imax>(); }() == 5);
  // NOLINTEND(bugprone-inc-dec-in-conditions)
}

//---------------------------------------------------------------------------
// operator-= raw fast path (perf): with equal notches `-=` subtracts raws
// directly (index rhs debits a compile-time Lower/Notch bias) instead of
// delegating to `+= (-rhs)`. These cases pin value equality with the binary
// route and the policy tail at both raw edges. [perf-paths]
//---------------------------------------------------------------------------
TEST_CASE("compound -=: raw fast path agrees with binary subtraction",
          "[bound][compound][perf-paths]")
{
  // Q8.8 — index storage both sides, bias 0.
  using q88 = bound<{{0, 255}, notch<1, 256>}, snap>;
  for (int whole : {0, 1, 100, 255})
    for (int sub : {0, 1, 55, 100})
    {
      if (whole - sub < 0) continue;
      q88 lhs{whole};
      lhs -= q88{sub};
      REQUIRE(rational{lhs} == rational{whole - sub});
    }

  // fractional raws on the same grid
  q88 frac_lhs{rational{771, 256}};        // 3 + 3/256
  frac_lhs -= q88{rational{515, 256}};     // 2 + 3/256
  REQUIRE(rational{frac_lhs} == rational{1});

  // offset index grid (negative Lower): bias = Lower/Notch = -8.
  using off = bound<{{-2, 2}, notch<1, 4>}, snap>;
  off offset_lhs{rational{3, 4}};
  offset_lhs -= off{rational{-1, 2}};      // 3/4 − (−1/2) = 5/4
  REQUIRE(rational{offset_lhs} == rational{5, 4});

  // mixed storages on the same unit notch: [1000,2000] stores an unsigned
  // index (Lower-relative raw), [-3000,3000] stores the value directly.
  using hi   = bound<{1000, 2000}, snap>;
  using lo   = bound<{0, 500}, snap>;
  using wide = bound<{-3000, 3000}, snap>;
  hi index_lhs{1500};                      // index-raw lhs, value-raw rhs
  index_lhs -= lo{300};
  REQUIRE(rational{index_lhs} == rational{1200});
  wide value_lhs{1500};                    // value-raw lhs, index-raw rhs:
  value_lhs -= hi{1200};                   // bias = Lower<R>/Notch = 1000
  REQUIRE(rational{value_lhs} == rational{300});
}

TEST_CASE("compound -=: policy tail at the raw edges", "[bound][compound][perf-paths]")
{
  using q_clamp = bound<{{0, 255}, notch<1, 256>}, clamp | snap>;
  q_clamp clamped{1};
  clamped -= q_clamp{100};                 // 1 − 100 < 0 -> clamps to Lower
  REQUIRE(rational{clamped} == rational{0});

  using q_wrap = bound<{{0, 3}, notch<1, 4>}, wrap | snap>;
  q_wrap wrapped{0};
  wrapped -= q_wrap{rational{1, 4}};       // 0 − 1/4 wraps to Upper − ... = 3
  REQUIRE(rational{wrapped} == rational{3});

  using q_sent = bound<{{0, 255}, notch<1, 256>}, sentinel | snap>;
  q_sent sent{1};
  sent -= q_sent{100};
  REQUIRE(sent.is_sentinel());

  using q_checked = bound<{{0, 255}, notch<1, 256>}, checked>;
  q_checked reported{1};
  REQUIRE_THROWS_AS(([&]{ reported -= q_checked{100}; }()), bnd::bound_error);

  // in-range edge stays exact under every policy
  q_clamp exact_edge{255};
  exact_edge -= q_clamp{255};
  REQUIRE(rational{exact_edge} == rational{0});
}

TEST_CASE("compound -=: non-fast storages still route through += (-rhs)",
          "[bound][compound][perf-paths]")
{
  // rational raw falls back and stays exact
  using ex = bound<{{0, 4}, notch<1, 3>}, exact | round_nearest>;
  ex exact_lhs{rational{7, 3}};
  exact_lhs -= ex{rational{2, 3}};
  REQUIRE(rational{exact_lhs} == rational{5, 3});

  // f64-backed falls back (fp raws are excluded from the raw fast path)
  using rl = bound<{{-4, 4}, notch<1, 256>}, real | round_nearest>;
  rl real_lhs{rational{3, 2}};
  real_lhs -= rl{rational{1, 4}};
  REQUIRE(rational{real_lhs} == rational{5, 4});

  // cross-notch operands take the binary route
  using tenths   = bound<{{0, 10}, notch<1, 10>}, round_nearest>;
  using quarters = bound<{{0, 10}, notch<1, 4>}, round_nearest>;
  tenths cross{rational{5, 2}};            // 2.5 on the 1/10 grid
  cross -= quarters{rational{1, 2}};       // 2.0 — exact on both grids
  REQUIRE(rational{cross} == rational{2});
}
