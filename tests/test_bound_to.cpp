#include "bound/bound.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <limits>
#include <type_traits>
#include <vector>

using namespace bnd;
using namespace bnd::detail;

TEST_CASE("bound::to<unsigned T>", "[bound][to]")
{
  SECTION("trivially-fits grid takes the fast path")
  {
    using B = bound<{0, 100}>;
    REQUIRE(B{42}.to<std::uint8_t>().value() == 42);
    REQUIRE(B{0}.to<std::uint32_t>().value() == 0);
    REQUIRE(B{100}.to<std::size_t>().value() == 100);
  }

  SECTION("runtime overflow when Upper > T::max")
  {
    using B = bound<{0, 1000}>;
    auto r = B{500}.to<std::uint8_t>();
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error() == errc::overflow);
  }

  SECTION("negative bound to unsigned -> domain_error")
  {
    using B = bound<{-10, 10}>;
    auto r = B{-5}.to<std::uint8_t>();
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error() == errc::domain_error);
  }

  SECTION("fractional notch truncates silently (matches rational::to)")
  {
    using B = bound<{{0, 1}, notch<1, 2>}>;
    REQUIRE(B{0.5}.to<std::uint8_t>().value() == 0);   // 0.5 -> 0
    REQUIRE(B{1}.to<std::uint8_t>().value() == 1);
  }

  SECTION("sentinel-state bound -> overflow")
  {
    using B = bound<{0, 100}, sentinel>;
    B b = B::make_sentinel();
    auto r = b.to<std::uint8_t>();
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error() == errc::overflow);
  }

  SECTION("Q-format fast path")
  {
    using B = bound<{{0, 255}, notch<1, 256>}, round_nearest>;
    REQUIRE(B{42.5}.to<std::uint8_t>().value() == 42);
  }
}

TEST_CASE("bound::to<signed T>", "[bound][to]")
{
  SECTION("trivially-fits grid takes the fast path")
  {
    using B = bound<{-50, 50}>;
    REQUIRE(B{-7}.to<std::int8_t>().value() == -7);
    REQUIRE(B{42}.to<std::int32_t>().value() == 42);
  }

  SECTION("overflow upward -> errc::overflow")
  {
    using B = bound<{0, 1000}>;
    auto r = B{500}.to<std::int8_t>();
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error() == errc::overflow);
  }

  SECTION("overflow downward -> errc::overflow")
  {
    using B = bound<{-1000, 0}>;
    auto r = B{-500}.to<std::int8_t>();
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error() == errc::overflow);
  }
}

TEST_CASE("bound::to<floating T>", "[bound][to]")
{
  SECTION("ordinary value")
  {
    using B = bound<{{0, 1}, notch<1, 2>}>;
    REQUIRE(B{0.5}.to<double>().value() == 0.5);
  }

  SECTION("sentinel-state -> overflow")
  {
    using B = bound<{0, 100}, sentinel>;
    B b = B::make_sentinel();
    auto r = b.to<double>();
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error() == errc::overflow);
  }

  SECTION("to<double> works even on a strict-policy bound (no policy gate)")
  {
    using B = bound<{0, 10}, checked>;
    REQUIRE(B{3}.to<double>().value() == 3.0);
  }
}

TEST_CASE("bound -> rational conversion", "[bound][to]")
{
  SECTION("ordinary value")
  {
    using B = bound<{0, 10}>;
    REQUIRE(static_cast<rational>(B{5}) == 5);
  }

  SECTION("fractional grid is preserved exactly")
  {
    using B = bound<{{0, 1}, notch<1, 2>}>;
    REQUIRE(static_cast<rational>(B{0.5}) == 0.5_r);
  }
}

TEST_CASE("operator imax is gated on fit-in-imax", "[bound][to][operator]")
{
  using narrow = bound<{0, 100}>;
  using wide   = bound<{0, std::numeric_limits<std::uint64_t>::max()}>;

  // Narrow grids still convert implicitly.
  STATIC_REQUIRE(std::is_convertible_v<narrow, imax>);
  imax i = narrow{42};
  REQUIRE(i == 42);

  // Wide grids (max > INT64_MAX) lose the implicit imax conversion. The
  // implicit operator size_t() is also gated on Upper <= imax_max, so wide
  // bounds don't sneak in via size_t -> imax integer conversion.
  STATIC_REQUIRE_FALSE(std::is_convertible_v<wide, imax>);
  STATIC_REQUIRE_FALSE(std::is_convertible_v<wide, std::size_t>);

  // The typed-error path is what wide grids should use instead. (We do not
  // construct a `wide` instance here: assignment-side overflow checks on
  // grids spanning the full uint64 range are out of scope for this test.)
}

TEST_CASE("operator size_t for index-shaped bounds", "[bound][to][operator][index]")
{
  using idx_t  = bound<{0, 9}>;            // notch 1, Lower 0 — index shape
  using offset = bound<{5, 10}>;            // notch 1, Lower > 0 — also OK

  // Index-shaped bounds convert implicitly to size_t (silences
  // -Wsign-conversion at the `vec[bound_idx]` call sites in examples).
  STATIC_REQUIRE(std::is_convertible_v<idx_t,  std::size_t>);
  STATIC_REQUIRE(std::is_convertible_v<offset, std::size_t>);

  // (Bounds with Lower < 0 don't get the direct size_t conversion, but
  // they're still convertible via the imax -> size_t standard step; the
  // bound shape that we *want* to remain non-convertible is the wide one,
  // verified in the test case above.)

  // Use the conversion to index a vector — no `.as<>()` needed.
  std::vector<int> v{0, 10, 20, 30, 40, 50, 60, 70, 80, 90};
  idx_t i{3};
  REQUIRE(v[i] == 30);

  // Direct-init to size_t picks operator size_t() unambiguously.
  std::size_t s = i;
  REQUIRE(s == 3);
}

TEST_CASE("operator double is gated on rounding policy", "[bound][to][operator]")
{
  using B_round   = bound<{{0, 1}, notch<1, 2>}, round_nearest>;
  using B_ignore  = bound<{{0, 1}, notch<1, 2>}, ignore_round>;
  using B_strict  = bound<{{0, 1}, notch<1, 2>}, checked>;
  using B_floor   = bound<{{0, 1}, notch<1, 2>}, round_floor>;

  // operator double() is explicit, so use is_constructible_v to detect it.
  STATIC_REQUIRE(std::is_constructible_v<double, B_round>);
  STATIC_REQUIRE(std::is_constructible_v<double, B_ignore>);
  STATIC_REQUIRE(std::is_constructible_v<double, B_floor>);
  STATIC_REQUIRE_FALSE(std::is_constructible_v<double, B_strict>);

  // The typed-error path is always available.
  REQUIRE(B_strict{0.5}.to<double>().value() == 0.5);
}

TEST_CASE("as<T>() is a non-expected shortcut for to<T>().value()", "[bound][as]")
{
  using narrow = bound<{0, 100}>;
  using frac   = bound<{{0, 100}, notch<1, 10>}, round_nearest>;

  // Matches the integer-extraction value path.
  REQUIRE(narrow{42}.as<imax>()        == 42);
  REQUIRE(narrow{42}.as<std::size_t>() == 42u);

  // Fractional notch: extracting to imax truncates (matches to<imax>()).
  REQUIRE(frac{7.5}.as<imax>() == 7);

  // Double target behaves the same as `to<T>().value()`; the bound -> rational
  // conversion is the implicit operator.
  REQUIRE(static_cast<rational>(narrow{42}) == 42);
  REQUIRE(frac{7.5}.as<double>()            == 7.5);
}
