// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// The error vocabulary has three shapes, each by role (see docs/internals.md):
//   policy cascade        — narrowing INTO an inside,
//   slim::optional<inside> — fallible inside arithmetic (zero-cost, chaining),
//   slim::expected<T,errc>— fallible queries/math where the cause matters.
// These tests cover the BRIDGES between the families: beman::inside::ok() and the
// expected-lift operators.

#include <beman/inside/arithmetic.hpp>
#include <beman/inside/cmath.hpp>

#include <gtest/gtest.h>

using namespace beman::inside;
using beman::inside::detail::rational;

namespace
{
  // Mixed-sign sqrt: deterministic expected results under both engines.
  using sq_in  = inside<{{-4, 4}, notch<1, 256>}, round_nearest | real>;
  using num_t  = inside<{0, 100}>;
  using den_t  = inside<{-5, 5}>;     // spans zero → division returns optional
}

// ok() drops the cause and enters the optional world
TEST(ErrorVocabularyTest, ok_drops_the_cause_and_enters_the_optional_world)
{
  auto good = math::sqrt(sq_in{1});            // expected, value 1
  auto bad  = math::sqrt(sq_in{-1});           // expected, domain_error

  auto og = ok(good);
  auto ob = ok(bad);
  static_assert(detail::is_slim_optional_v<decltype(og)>);
  ASSERT_TRUE(og.has_value());
  ASSERT_EQ(*og, 1);
  ASSERT_TRUE(!ob.has_value());

  // ...and chains on through the existing optional lifts.
  auto chained = ok(math::sqrt(sq_in{1})) + num_t{4};
  ASSERT_TRUE(chained.has_value());
  ASSERT_EQ(*chained, 5);
  ASSERT_TRUE(!(ok(math::sqrt(sq_in{-1})) + num_t{4}).has_value());
}

// expected-lift operators keep the cause end to end
TEST(ErrorVocabularyTest, expected_lift_operators_keep_the_cause_end_to_end)
{
  // Value path: identical to the manual unwrap.
  auto r = math::sqrt(sq_in{1}) + num_t{4};
  static_assert(detail::expected_like<decltype(r)>);
  ASSERT_TRUE(r.has_value());
  ASSERT_EQ(*r, *math::sqrt(sq_in{1}) + num_t{4});
  ASSERT_EQ(*r, 5);

  // Error path: the errc propagates through the whole chain.
  auto e = (math::sqrt(sq_in{-1}) + num_t{4}) * num_t{2};
  ASSERT_TRUE(!e.has_value());
  ASSERT_EQ(e.error(), errc::domain_error);

  // First (left) error wins when two errors meet.
  slim::expected<num_t, errc> left {slim::unexpected{errc::domain_error}};
  slim::expected<num_t, errc> right{slim::unexpected{errc::overflow}};
  auto both = left + right;
  ASSERT_TRUE(!both.has_value());
  ASSERT_EQ(both.error(), errc::domain_error);
}

// division inside an expected chain maps nullopt to its cause
TEST(ErrorVocabularyTest, division_inside_an_expected_chain_maps_nullopt_to_its_cause)
{
  slim::expected<num_t, errc> ea{num_t{10}};

  auto q = ea / den_t{2};
  ASSERT_TRUE(q.has_value());
  ASSERT_EQ(rational{*q}, 5);

  auto z = ea / den_t{0};
  ASSERT_TRUE(!z.has_value());
  ASSERT_EQ(z.error(), errc::division_by_zero);
}

// operator% participates in both lift families
TEST(ErrorVocabularyTest, operator_participates_in_both_lift_families)
{
  // mod requires snap in the merged policy (integer-valued grids).
  using mnum = inside<{0, 100}, snap>;
  using nz   = inside<{1, 5},  snap>;   // divisor grid excludes zero
  using span = inside<{-5, 5}, snap>;   // divisor grid spans zero

  // optional chain.
  slim::optional<mnum> on{mnum{17}};
  auto m = on % nz{5};
  ASSERT_TRUE(m.has_value());
  ASSERT_EQ(*m, 2);

  // expected chain: value path and zero-divisor mapping.
  slim::expected<mnum, errc> en{mnum{17}};
  auto q = en % span{5};
  ASSERT_TRUE(q.has_value());
  ASSERT_EQ(rational{*q}, 2);
  auto z = en % span{0};
  ASSERT_TRUE(!z.has_value());
  ASSERT_EQ(z.error(), errc::division_by_zero);
}
