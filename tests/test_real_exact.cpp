// real (double-backed) arithmetic vs. the exact rational oracle.
//
// Premise of the `real` policy: on a dyadic grid every on-grid value is exact in
// IEEE-754 double, so `real` arithmetic must equal the exact grid arithmetic.
// `dyadic_grid<G>` (the current storage guard) only checks power-of-two
// denominators — it ignores the 53-bit significand. This test exercises the
// invariant directly: decode the real result to rational and compare to the
// exact rational result of the same operands.
//
// It SHOULD FAIL on the unfixed build wherever a stored/result value needs more
// than 53 significant bits (mantissa) or a coarser-ULP binade than the notch
// (exponent), and pass once `real` is only selected on double-exact grids (the
// result silently dropping `real` and falling back to exact storage).

#include "bound/bound.hpp"
#include "bound/io.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_get_random_seed.hpp>

#include <cmath>
#include <random>
#include <vector>

// `real` (double-backed) storage is elided under the fixed-point engine, so this
// whole file is double-engine only (it asserts raw_type == double).
#ifndef BND_MATH_FIXED

using namespace bnd;
using namespace bnd::detail;

namespace
{
  // A stored real result must be exactly: the sentinel, ±0, or a normal double.
  // Never a non-sentinel NaN/inf/subnormal.
  template <class R>
  void check_bits(const R& r, const char* op)
  {
    if constexpr (f64_raw<R>)
    {
      const double v = r.raw();
      INFO(op << " raw=" << v);
      if (!r.is_sentinel())
      {
        REQUIRE(std::isfinite(v));
        REQUIRE((v == 0.0 || std::isnormal(v)));
      }
    }
  }

  // Compare real-bound +,-,* against the exact rational oracle on the *stored*
  // operand values (so this isolates arithmetic divergence from input snap).
  template <class A, class B>
  void oracle_check(A a, B b)
  {
    const rational ar = static_cast<rational>(a);
    const rational br = static_cast<rational>(b);

    if constexpr (requires { a + b; })
    {
      auto r = a + b;
      INFO("+ a=" << to_string(ar) << " b=" << to_string(br));
      REQUIRE(static_cast<rational>(r) == *(ar + br));
      check_bits(r, "+");
    }
    if constexpr (requires { a - b; })
    {
      auto r = a - b;
      INFO("- a=" << to_string(ar) << " b=" << to_string(br));
      REQUIRE(static_cast<rational>(r) == *(ar - br));
      check_bits(r, "-");
    }
    if constexpr (requires { a * b; })
    {
      auto r = a * b;
      INFO("* a=" << to_string(ar) << " b=" << to_string(br));
      REQUIRE(static_cast<rational>(r) == *(ar * br));
      check_bits(r, "*");
    }
  }

  // Random on-grid value for a real bound: lo + k*notch, k in [0, NotchCount].
  template <class T>
  double on_grid_value(std::mt19937_64& rng)
  {
    const double lo = static_cast<double>(Lower<T>);
    const double nd = static_cast<double>(Notch<T>);
    const umax   cnt = NotchCount<T>;
    std::uniform_int_distribution<umax> d(0, cnt);
    return lo + static_cast<double>(d(rng)) * nd;
  }

  template <class A, class B>
  void sweep(std::mt19937_64& rng, int iters)
  {
    // include the endpoints (max-magnitude is the binding case)
    oracle_check<A, B>(A{static_cast<double>(Upper<A>)}, B{static_cast<double>(Upper<B>)});
    oracle_check<A, B>(A{static_cast<double>(Lower<A>)}, B{static_cast<double>(Lower<B>)});
    for (int i = 0; i < iters; ++i)
      oracle_check<A, B>(A{on_grid_value<A>(rng)}, B{on_grid_value<B>(rng)});
  }
}

//---------------------------------------------------------------------------
// Deterministic regression: a real `×` whose product needs a bit below the
// product binade's ULP silently drops it. Both operands are exact (index
// 2^28-1 < 2^53); the product 16 - 2^-23 + 2^-52 rounds off the 2^-52 term
// because |product| ~ 16 has ULP 2^-49. The exact path keeps it.
//---------------------------------------------------------------------------
TEST_CASE("real * drops bits below the product-binade ULP", "[real][exact][mantissa]")
{
  using U = bound<{{0, 4}, notch<1, (1u << 26)>}, real>;   // f=26, exact operand
  static_assert(std::is_same_v<U::raw_type, double>);

  const U a = 4.0 - std::ldexp(1.0, -26);
  const rational ar = static_cast<rational>(a);

  auto p = a * a;
  INFO("a=" << to_string(ar));
  REQUIRE(static_cast<rational>(p) == *(ar * ar));
}

//---------------------------------------------------------------------------
// Fuzz sweep across dyadic real grids spanning double-exact and
// double-inexact cases (the latter via product numerators crossing 2^53).
//---------------------------------------------------------------------------
TEST_CASE("real +,-,* match the exact rational oracle", "[real][exact][fuzz]")
{
  std::mt19937_64 rng(Catch::getSeed() ^ 0x9E3779B97F4A7C15ull);

  SECTION("double-exact: small notches, modest range (must always hold)")
  {
    using A = bound<{{-8, 8}, notch<1, 65536>}, real>;
    using B = bound<{{-8, 8}, notch<1, 256>}, real>;
    sweep<A, A>(rng, 2000);
    sweep<A, B>(rng, 2000);
  }

  SECTION("mantissa: product numerator crosses 2^53")
  {
    using A = bound<{{0, 4}, notch<1, (1u << 26)>}, real>;     // f=26
    using B = bound<{{0, 4}, notch<1, (1u << 27)>}, real>;     // f=27 -> f_prod=53
    sweep<A, A>(rng, 2000);
    sweep<A, B>(rng, 2000);
  }

  SECTION("exponent-coarsening: fine value combined with a large one")
  {
    // A lives in a high binade (ULP ~2^-12); B carries bits down to 2^-20.
    // A+B / A*B must keep B's sub-ULP bits, but the double op drops them.
    using A = bound<{{0, (umax{1} << 40)}, notch<1, 2>}, real>;   // large, coarse
    using B = bound<{{0, 1}, notch<1, (1u << 20)>}, real>;        // small, fine
    sweep<A, B>(rng, 2000);
  }

  SECTION("signed grids cross zero (all four multiply quadrants)")
  {
    using A = bound<{{-8, 8}, notch<1, 1024>}, real>;
    using B = bound<{{-4, 12}, notch<1, 4096>}, real>;            // asymmetric, crosses 0
    sweep<A, A>(rng, 3000);
    sweep<A, B>(rng, 3000);
    // inexact signed product: drops real, must stay exact through the quadrants
    using C = bound<{{-4, 4}, notch<1, (1u << 27)>}, real>;
    sweep<C, C>(rng, 3000);
  }

  SECTION("mixed: real operand with a non-real one")
  {
    using Re  = bound<{{-8, 8}, notch<1, 1024>}, real>;
    using Int = bound<{-5, 5}>;                       // integer-direct storage
    using Fr  = bound<{{-8, 8}, notch<1, 4>}>;        // fractional notch-offset storage
    using Ex  = bound<{{-8, 8}, notch<1, 1024>}, exact>;   // rational storage
    sweep<Re, Int>(rng, 3000);
    sweep<Int, Re>(rng, 3000);
    sweep<Re, Fr>(rng, 3000);
    sweep<Re, Ex>(rng, 3000);
  }
}

namespace
{
  // Snapping oracle: assigning an arbitrary (possibly off-grid) value to a
  // double-exact real bound must land on the nearest grid point, ties away from
  // zero. std::round is exactly that rule, so it is the trusted reference.
  template <class R>
  void check_snap(double x)
  {
    R r = x;
    const double lo = static_cast<double>(Lower<R>);
    const double nd = static_cast<double>(Notch<R>);
    const double expect = lo + std::round((x - lo) / nd) * nd;
    INFO("x=" << x << " expect=" << expect << " got=" << static_cast<double>(r));
    REQUIRE(static_cast<double>(r) == expect);
  }
}

//---------------------------------------------------------------------------
// snap_double rounding: half away from zero, no predecessor-of-0.5 error, on
// both signs. Exercises exact ties (k+0.5 notches) and random off-grid inputs.
//---------------------------------------------------------------------------
TEST_CASE("real assignment snaps to nearest grid, ties away from zero",
          "[real][exact][snap]")
{
  using R = bound<{{-4, 4}, notch<1, 256>}, real>;    // double-exact, crosses zero
  const double nd = static_cast<double>(Notch<R>);

  // exact half-way ties on both sides of zero
  for (int k = -1000; k < 1000; ++k)
  {
    const double mid = (k + 0.5) * nd;                // halfway between k·nd and (k+1)·nd
    if (mid > -4.0 && mid < 4.0) check_snap<R>(mid);
  }

  // random off-grid inputs
  std::mt19937_64 rng(Catch::getSeed() ^ 0xD1B54A32D192ED03ull);
  std::uniform_real_distribution<double> d(-3.999, 3.999);
  for (int i = 0; i < 20000; ++i) check_snap<R>(d(rng));

  // the classic floor(x+0.5) trap: x just below a tie must round to the lower
  // grid point, not jump up.
  const double justBelow = nd * (3.0 + std::nextafter(0.5, 0.0));
  check_snap<R>(justBelow);
}

//---------------------------------------------------------------------------
// Composition: chained real arithmetic must equal the exact rational result
// (catches divergence/storage faults that only appear after a real result is
// fed back into another op, possibly after `real` was dropped).
//---------------------------------------------------------------------------
TEST_CASE("chained real arithmetic stays exact vs the rational oracle",
          "[real][exact][chain]")
{
  using A = bound<{{-4, 4}, notch<1, 4096>}, real>;
  std::mt19937_64 rng(Catch::getSeed() ^ 0x243F6A8885A308D3ull);

  auto val = [&](){
    const double nd = static_cast<double>(Notch<A>);
    std::uniform_int_distribution<int> d(-4 * 4096, 4 * 4096);
    return A{ d(rng) * nd };
  };

  for (int i = 0; i < 5000; ++i)
  {
    A a = val(), b = val(), c = val();
    const rational ar = static_cast<rational>(a);
    const rational br = static_cast<rational>(b);
    const rational cr = static_cast<rational>(c);

    INFO("a=" << to_string(ar) << " b=" << to_string(br) << " c=" << to_string(cr));
    REQUIRE(static_cast<rational>((a * b) + c) == *(*(ar * br) + cr));
    REQUIRE(static_cast<rational>((a + b) * c) == *(*(ar + br) * cr));
    REQUIRE(static_cast<rational>((a - b) * c) == *(*(ar - br) * cr));
  }
}

//---------------------------------------------------------------------------
// Sentinel round-trips through is_sentinel, and a real bound's raw is never a
// non-sentinel NaN/inf/subnormal. The real sentinel is a finite, comparable slot.
//---------------------------------------------------------------------------
TEST_CASE("real sentinel round-trips; raw stays clean", "[real][exact][sentinel]")
{
  using R = bound<{{-4, 4}, notch<1, 1024>}, real | sentinel>;
  static_assert(std::is_same_v<R::raw_type, double>);

  R s = R::make_sentinel();
  REQUIRE(s.is_sentinel());

  R v = 1.5;
  REQUIRE_FALSE(v.is_sentinel());
  REQUIRE(std::isnormal(v.raw()));
}

//---------------------------------------------------------------------------
// Real division by zero flows through the error vocabulary (like the integer/
// rational path), instead of silently storing inf/a sentinel. The return type
// widens to optional<result> exactly when the divisor grid can be zero.
//---------------------------------------------------------------------------
TEST_CASE("real division by zero is reported, not stored as inf", "[real][exact][div0]")
{
  using N  = bound<{{1, 4}, notch<1, 1024>}, real>;
  using Dz = bound<{{0, 4}, notch<1, 1024>}, real>;   // divisor grid spans zero

  // divisor can be zero -> return widens to optional; zero divisor -> nullopt
  auto q = N{3.0} / Dz{0.0};
  REQUIRE_FALSE(q.has_value());
  REQUIRE((N{3.0} / Dz{2.0}).has_value());            // nonzero divisor: value present

  // divisor excludes zero -> plain (non-optional) result; double() compiles only
  // because it is a bound, not an optional
  auto p = N{3.0} / N{2.0};
  REQUIRE(static_cast<double>(p) == 1.5);

  // expected lift surfaces the error code
  slim::expected<N, errc> en{N{3.0}};
  auto z = en / Dz{0.0};
  REQUIRE_FALSE(z.has_value());
  REQUIRE(z.error() == errc::division_by_zero);
}

//---------------------------------------------------------------------------
// An over-fine real product (grid index count > umax) deduces rational storage
// — exact, no cryptic compile error — and an unrepresentable product reports
// overflow rather than silently wrapping.
//---------------------------------------------------------------------------
TEST_CASE("over-fine real product deduces rational, stays exact", "[real][exact][fallback]")
{
  using A = bound<{{0, (1u << 17)}, notch<1, (1u << 16)>}, real>;   // N up to 2^33 < 2^53
  static_assert(std::is_same_v<A::raw_type, double>);

  // product grid {0, 2^34} notch 2^-32 → 2^66 slots > umax → rational storage,
  // overflow-checked (return widens to optional). 2^17 * 2^17 = 2^34 is exact.
  A a = static_cast<double>(1u << 17);
  auto p = a * a;
  REQUIRE(p.has_value());
  REQUIRE(static_cast<rational>(*p) == rational{umax{1} << 34});
}

//---------------------------------------------------------------------------
// A real (double-backed) target rejects non-finite assignments: NaN/inf can
// never be an on-grid value, so the store guard reports errc::not_finite rather
// than poisoning the raw double. Exercises the non-finite branch in
// store_checked for fp_raw storage.
//---------------------------------------------------------------------------
TEST_CASE("real storage rejects non-finite assignment", "[real][error]")
{
  using R = bound<{{-8, 8}, notch<1, 65536>}, real>;
  static_assert(std::is_same_v<R::raw_type, double>);

  auto threw_not_finite = [](auto&& fn) {
    try { fn(); return false; }
    catch (bnd::bound_error const& e) { return e.code == errc::not_finite; }
  };

  REQUIRE(threw_not_finite([]{ R x = std::nan(""); (void)x; }));
  REQUIRE(threw_not_finite([]{ R x = std::numeric_limits<double>::infinity(); (void)x; }));
  REQUIRE(threw_not_finite([]{ R x = -std::numeric_limits<double>::infinity(); (void)x; }));
}

#endif // !BND_MATH_FIXED
