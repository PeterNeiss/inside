// The error vocabulary has three shapes, each by role (see docs/internals.md):
//   policy cascade        — narrowing INTO a bound,
//   slim::optional<bound> — fallible bound arithmetic (zero-cost, chaining),
//   slim::expected<T,errc>— fallible queries/math where the cause matters.
// These tests cover the BRIDGES between the families: bnd::ok() and the
// expected-lift operators.

#include "bound/arithmetic.hpp"
#include "bound/cmath.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace bnd;
using bnd::detail::rational;

namespace
{
  // Mixed-sign sqrt: deterministic expected results under both engines.
  using sq_in  = bound<{{-4, 4}, notch<1, 256>}, round_nearest | real>;
  using num_t  = bound<{0, 100}>;
  using den_t  = bound<{-5, 5}>;     // spans zero → division returns optional
}

TEST_CASE("ok() drops the cause and enters the optional world",
          "[errors][ok]")
{
  auto good = math::sqrt(sq_in{1});            // expected, value 1
  auto bad  = math::sqrt(sq_in{-1});           // expected, domain_error

  auto og = ok(good);
  auto ob = ok(bad);
  STATIC_REQUIRE(detail::is_slim_optional_v<decltype(og)>);
  REQUIRE(og.has_value());
  REQUIRE(*og == 1);
  REQUIRE(!ob.has_value());

  // ...and chains on through the existing optional lifts.
  auto chained = ok(math::sqrt(sq_in{1})) + num_t{4};
  REQUIRE(chained.has_value());
  REQUIRE(*chained == 5);
  REQUIRE(!(ok(math::sqrt(sq_in{-1})) + num_t{4}).has_value());
}

TEST_CASE("expected-lift operators keep the cause end to end",
          "[errors][expected][lift]")
{
  // Value path: identical to the manual unwrap.
  auto r = math::sqrt(sq_in{1}) + num_t{4};
  STATIC_REQUIRE(detail::expected_like<decltype(r)>);
  REQUIRE(r.has_value());
  REQUIRE(*r == *math::sqrt(sq_in{1}) + num_t{4});
  REQUIRE(*r == 5);

  // Error path: the errc propagates through the whole chain.
  auto e = (math::sqrt(sq_in{-1}) + num_t{4}) * num_t{2};
  REQUIRE(!e.has_value());
  REQUIRE(e.error() == errc::domain_error);

  // First (left) error wins when two errors meet.
  slim::expected<num_t, errc> left {slim::unexpected{errc::domain_error}};
  slim::expected<num_t, errc> right{slim::unexpected{errc::overflow}};
  auto both = left + right;
  REQUIRE(!both.has_value());
  REQUIRE(both.error() == errc::domain_error);
}

TEST_CASE("division inside an expected chain maps nullopt to its cause",
          "[errors][expected][division]")
{
  slim::expected<num_t, errc> ea{num_t{10}};

  auto q = ea / den_t{2};
  REQUIRE(q.has_value());
  REQUIRE(rational{*q} == 5);

  auto z = ea / den_t{0};
  REQUIRE(!z.has_value());
  REQUIRE(z.error() == errc::division_by_zero);
}

TEST_CASE("operator% participates in both lift families", "[errors][mod]")
{
  // mod requires snapping in the merged policy (integer-valued grids).
  using mnum = bound<{0, 100}, snapping>;
  using nz   = bound<{1, 5},  snapping>;   // divisor grid excludes zero
  using span = bound<{-5, 5}, snapping>;   // divisor grid spans zero

  // optional chain.
  slim::optional<mnum> on{mnum{17}};
  auto m = on % nz{5};
  REQUIRE(m.has_value());
  REQUIRE(*m == 2);

  // expected chain: value path and zero-divisor mapping.
  slim::expected<mnum, errc> en{mnum{17}};
  auto q = en % span{5};
  REQUIRE(q.has_value());
  REQUIRE(rational{*q} == 2);
  auto z = en % span{0};
  REQUIRE(!z.has_value());
  REQUIRE(z.error() == errc::division_by_zero);
}
