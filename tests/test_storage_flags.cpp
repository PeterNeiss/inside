// Representation policy flags — `exact` / `direct` / `indexed` force a raw
// representation the way `real` does; without one the grid deduces it.
// Selection resolves widest-wins (exact > real > direct > indexed > deduced),
// matching the OR-propagation of policies through arithmetic.
//
// (The grid-shape gates — `direct` needs Notch == 1, `indexed` needs a notch —
// are in-class static_asserts, so an invalid combination is a compile error,
// not probe-able via concepts.)

#include "bound/bound.hpp"
#include "bound/cmath.hpp"
#include "bound/format.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>

using namespace bnd;
using bnd::detail::rational;

TEST_CASE("exact forces rational raw on any grid", "[storage][exact]")
{
  // A notched grid that deduction would store as an integer index.
  using E = bound<{{0, 10}, notch<1, 4>}, exact | round_nearest>;
  STATIC_REQUIRE(std::is_same_v<E::raw_type, rational>);
  STATIC_REQUIRE(detail::rational_raw<E>);

  // The value is held as an exact fraction and still obeys the grid.
  E q{rational{3, 4}};
  REQUIRE(q.raw() == rational{3, 4});
  REQUIRE(rational{q} == rational{3, 4});

  // round_nearest snaps an off-grid value to the nearest quarter.
  E r{rational{2, 5}};                       // 0.4 → 0.5? no: nearest 1/4 is 2/4
  REQUIRE(rational{r} == rational{1, 2});

  // Integral rhs and arithmetic stay exact.
  E one{1};
  REQUIRE(rational{one} == 1);
  auto sum = q + q;
  REQUIRE(rational{sum} == rational{3, 2});

  // Non-dyadic grids are fine (this is what `real` cannot do).
  using T = bound<{{0, 1}, notch<1, 3>}, exact>;
  STATIC_REQUIRE(detail::rational_raw<T>);
  T third{rational{1, 3}};
  REQUIRE(rational{third} == rational{1, 3});
  REQUIRE(bnd::to_string(third) == "1/3");
}

TEST_CASE("direct forces raw == value where deduction picks an index",
          "[storage][direct]")
{
  // Deduced: {5,100} is index storage (unsigned raw 0..95).
  STATIC_REQUIRE(detail::index_raw<bound<{5, 100}>>);

  // Forced: raw holds the value 5..100 itself.
  using D = bound<{5, 100}, direct>;
  STATIC_REQUIRE(detail::value_raw<D>);
  STATIC_REQUIRE(std::is_same_v<D::raw_type, std::uint8_t>);

  D d{42};
  REQUIRE(d.raw() == 42);                    // raw IS the value
  REQUIRE(bound<{5, 100}>{42}.raw() == 37);  // deduced index for contrast

  // Value extraction goes through the kind-aware decode, not the
  // raw-signedness heuristic (which would have returned 42 + Lower = 47).
  REQUIRE(rational{d} == 42);
  REQUIRE(static_cast<imax>(d) == 42);
  REQUIRE(d.to<double>().value() == 42.0);

  // Round-trip through assignment and comparison.
  D e{d};
  REQUIRE(e == d);
  REQUIRE(e == 42);
}

TEST_CASE("indexed forces raw == 0-based index where deduction picks a value",
          "[storage][indexed]")
{
  // Deduced: {-5,5} is signed direct storage.
  STATIC_REQUIRE(detail::value_raw<bound<{-5, 5}>>);

  // Forced: dense unsigned 0-based index 0..10.
  using I = bound<{-5, 5}, indexed>;
  STATIC_REQUIRE(detail::index_raw<I>);
  STATIC_REQUIRE(std::is_same_v<I::raw_type, std::uint8_t>);

  REQUIRE(I{-5}.raw() == 0);
  REQUIRE(I{0}.raw()  == 5);
  REQUIRE(I{5}.raw()  == 10);
  REQUIRE(rational{I{-5}} == -5);
  REQUIRE(static_cast<imax>(I{3}) == 3);
}

TEST_CASE("representation flags resolve widest-wins", "[storage][policy]")
{
  // exact beats real: a mixed math chain falls back to exact fractions.
  using Ex = bound<{{0, 4}, notch<1, 256>}, exact | round_nearest>;
  using Re = bound<{{0, 4}, notch<1, 256>}, round_nearest | real>;
  using Sum = decltype(Ex{} + Re{});
  STATIC_REQUIRE((BoundPolicy<Sum> & exact) == exact);
  STATIC_REQUIRE(detail::rational_raw<Sum>);
  REQUIRE(rational{Sum{Ex{rational{1, 256}} + Re{rational{2, 256}}}}
          == rational{3, 256});

  // exact | real spelled directly on one bound: exact wins, both engines.
  using Both = bound<{{0, 4}, notch<1, 256>}, exact | real>;
  STATIC_REQUIRE(detail::rational_raw<Both>);

  // real beats direct on a dyadic unit grid (default engine only — under
  // BND_MATH_FIXED the real arm is elided and direct wins).
  using RD = bound<{0, 4}, real | direct>;
#ifndef BND_MATH_FIXED
  STATIC_REQUIRE(detail::real_raw<RD>);
#else
  STATIC_REQUIRE(detail::value_raw<RD>);
#endif

  // direct beats indexed.
  using DI = bound<{5, 100}, direct | indexed>;
  STATIC_REQUIRE(detail::value_raw<DI>);
  REQUIRE(DI{42}.raw() == 42);
}

TEST_CASE("representation flags compose with behavior policies",
          "[storage][policy]")
{
  // exact + clamp: out-of-range snaps to the endpoint, stored exactly.
  using EC = bound<{{0, 10}, notch<1, 4>}, exact | clamp | round_nearest>;
  REQUIRE(rational{EC{rational{15}}} == 10);
  REQUIRE(rational{EC{rational{-3}}} == 0);

  // exact + wrap: modular reduction onto the exact grid
  // (range = Upper − Lower + Notch = 10.25, so 11.25 wraps to 1).
  using EW = bound<{{0, 10}, notch<1, 4>}, exact | wrap | round_nearest>;
  REQUIRE(rational{EW{rational{45, 4}}} == 1);

  // direct + sentinel: out-of-range yields the empty slot.
  using DS = bound<{5, 100}, direct | sentinel>;
  auto ok   = DS::try_make(42);
  auto fail = DS::try_make(200);
  REQUIRE(ok.has_value());
  REQUIRE(*ok == 42);
  REQUIRE(!fail.has_value());
}

TEST_CASE("representation flags print the value, not the raw",
          "[storage][format]")
{
  using E = bound<{{0, 1}, notch<1, 3>}, exact>;
  REQUIRE(bnd::to_string(E{rational{2, 3}}) == "2/3");

  using I = bound<{-5, 5}, indexed>;
  REQUIRE(bnd::to_string(I{-3}) == "-3");      // value, not index 2
}

TEST_CASE("real storage runs the full out-of-range policy cascade",
          "[storage][real][policy]")
{
  // clamp: saturate to the (grid-point) endpoint.
  using RC = bound<{{0, 4}, notch<1, 256>}, real | clamp>;
  REQUIRE(static_cast<double>(rational{RC{9.5}})  == 4.0);
  REQUIRE(static_cast<double>(rational{RC{-1.5}}) == 0.0);

  // wrap: fold into [Lower, Lower + span + notch) — same convention as the
  // fractional path. Span 0..359 with notch 1 wraps 370 → 10, -10 → 350.
  using RW = bound<{{0, 359}, notch<1>}, real | wrap>;
  REQUIRE(static_cast<double>(rational{RW{370.0}}) == 10.0);
  REQUIRE(static_cast<double>(rational{RW{-10.0}}) == 350.0);

  // checked: out-of-range reports (throws) instead of silently storing.
  using RK = bound<{{0, 4}, notch<1, 256>}, real | checked>;
  REQUIRE_THROWS_AS(RK{9.5}, std::system_error);
  REQUIRE(static_cast<double>(rational{RK{2.5}}) == 2.5);

  // sentinel: out-of-range yields the empty slot.
  using RS = bound<{{0, 4}, notch<1, 256>}, real | sentinel>;
  REQUIRE(RS::try_make(2.0).has_value());
  REQUIRE(!RS::try_make(9.5).has_value());

  // unchecked (bare real): stores as-is — unchanged legacy behavior.
  using RU = bound<{{0, 4}, notch<1, 256>}, real>;
  REQUIRE(static_cast<double>(rational{RU{2.5}}) == 2.5);
}

TEST_CASE("non-finite doubles are rejected, both engines",
          "[storage][real][domain]")
{
  // Default engine: store_real guards before the grid snap; fixed engine:
  // the integer-backed path throws in rational(double). Same observable.
  using R = bound<{{0, 4}, notch<1, 256>}, round_nearest | real>;
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double inf = std::numeric_limits<double>::infinity();
  REQUIRE_THROWS_AS(R{nan}, std::system_error);
  REQUIRE_THROWS_AS(R{inf}, std::system_error);
  REQUIRE_THROWS_AS(R{-inf}, std::system_error);
}

// Full-domain inverse trig (improvement #2): atan beyond |x| ≤ 1 via
// reciprocal reduction; atan2 beyond the unit square via max-magnitude
// normalization. Engine-neutral (both engines accept the same programs).
TEST_CASE("atan / atan2 accept magnitudes beyond 1", "[cmath][atan][domain]")
{
  using wide_t = bound<{{-16, 16}, notch<1, 16384>}, round_nearest | real>;
  const double tol = 2.0 / 16384;

  auto val = [](auto b) { return static_cast<double>(rational{b}); };

  REQUIRE(std::fabs(val(math::atan(wide_t{2}))   - 1.1071487177) < tol);
  REQUIRE(std::fabs(val(math::atan(wide_t{-3}))  + 1.2490457724) < tol);
  REQUIRE(std::fabs(val(math::atan(wide_t{16}))  - 1.5083775168) < tol);
  REQUIRE(std::fabs(val(math::atan(wide_t{rational{1, 2}})) - 0.4636476090) < tol);

  REQUIRE(std::fabs(val(math::atan2(wide_t{3},  wide_t{1}))  - 1.2490457724) < tol);
  REQUIRE(std::fabs(val(math::atan2(wide_t{1},  wide_t{-5})) - 2.9441970937) < tol);
  REQUIRE(std::fabs(val(math::atan2(wide_t{-7}, wide_t{2}))  + 1.2924966677) < tol);
}
