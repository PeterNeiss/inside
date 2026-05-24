#include "bound/bound.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <limits>
#include <type_traits>

using namespace bnd;

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
    B b;
    b.Raw = sentinel_raw<B>();
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
    B b;
    b.Raw = sentinel_raw<B>();
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

TEST_CASE("bound::to<rational>", "[bound][to]")
{
  SECTION("ordinary value")
  {
    using B = bound<{0, 10}>;
    REQUIRE(B{5}.to<rational>().value() == 5_r);
  }

  SECTION("fractional grid is preserved exactly")
  {
    using B = bound<{{0, 1}, notch<1, 2>}>;
    REQUIRE(B{0.5}.to<rational>().value() == rational{1u, 2});
  }

  SECTION("sentinel-state -> overflow")
  {
    using B = bound<{0, 100}, sentinel>;
    B b;
    b.Raw = sentinel_raw<B>();
    auto r = b.to<rational>();
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error() == errc::overflow);
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

  // Wide grids (max > INT64_MAX) lose the implicit imax conversion.
  STATIC_REQUIRE_FALSE(std::is_convertible_v<wide, imax>);

  // The typed-error path is what wide grids should use instead. (We do not
  // construct a `wide` instance here: assignment-side overflow checks on
  // grids spanning the full uint64 range are out of scope for this test.)
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

  // Rational and double targets behave the same as `to<T>().value()`.
  REQUIRE(narrow{42}.as<rational>() == rational{42});
  REQUIRE(frac{7.5}.as<double>()    == 7.5);
}
