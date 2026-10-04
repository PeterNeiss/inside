// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// The error vocabulary has two shapes, each by role (see docs/internals.md):
//   policy cascade        — narrowing INTO an inside,
//   std::expected<T,errc> — every fallible result: inside arithmetic (division,
//                           modulo, checked rational overflow), queries and math.
// These tests cover the expected-lift operators that chain those results.

#include <beman/inside/arithmetic.hpp>
#include <beman/inside/cmath.hpp>

#include <gtest/gtest.h>

#include <expected>

using namespace beman::inside;
using beman::inside::detail::rational;

namespace
{
  // Mixed-sign sqrt: deterministic expected results under both engines.
  using sq_in  = inside<{{-4, 4}, per<256>}, round_nearest | f64>;
  using num_t  = inside<{0, 100}>;
  using den_t  = inside<{-5, 5}>;     // spans zero → division returns expected

  std::expected<num_t, errc> num_ok(int v) { return num_t{v}; }
  std::expected<num_t, errc> num_err(errc e) { return std::unexpected{e}; }
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
  auto both = num_err(errc::domain_error) + num_err(errc::overflow);
  ASSERT_TRUE(!both.has_value());
  ASSERT_EQ(both.error(), errc::domain_error);
}

// inside division returns expected with the division_by_zero cause
TEST(ErrorVocabularyTest, inside_division_reports_division_by_zero)
{
  auto q = num_t{10} / den_t{2};
  static_assert(detail::expected_like<decltype(q)>);
  ASSERT_TRUE(q.has_value());
  ASSERT_EQ(rational{*q}, 5);

  auto z = num_t{10} / den_t{0};
  ASSERT_TRUE(!z.has_value());
  ASSERT_EQ(z.error(), errc::division_by_zero);

  // A division error keeps its cause through the rest of the chain.
  auto chain = num_t{10} / den_t{0} * num_t{3} + num_t{1};
  ASSERT_TRUE(!chain.has_value());
  ASSERT_EQ(chain.error(), errc::division_by_zero);
}

// division inside an expected chain reports its own cause
TEST(ErrorVocabularyTest, division_inside_an_expected_chain_reports_its_cause)
{
  auto q = num_ok(10) / den_t{2};
  ASSERT_TRUE(q.has_value());
  ASSERT_EQ(rational{*q}, 5);

  auto z = num_ok(10) / den_t{0};
  ASSERT_TRUE(!z.has_value());
  ASSERT_EQ(z.error(), errc::division_by_zero);
}

// operator% chains through expected
TEST(ErrorVocabularyTest, modulo_chains_through_expected)
{
  // mod requires snap in the merged policy (integer-valued grids).
  using mnum = inside<{0, 100}, snap>;
  using nz   = inside<{1, 5},  snap>;   // divisor grid excludes zero
  using span = inside<{-5, 5}, snap>;   // divisor grid spans zero
  auto en = []() -> std::expected<mnum, errc> { return mnum{17}; };

  auto m = en() % nz{5};
  ASSERT_TRUE(m.has_value());
  ASSERT_EQ(*m, 2);

  // value path and zero-divisor cause.
  auto q = en() % span{5};
  ASSERT_TRUE(q.has_value());
  ASSERT_EQ(rational{*q}, 2);
  auto z = en() % span{0};
  ASSERT_TRUE(!z.has_value());
  ASSERT_EQ(z.error(), errc::division_by_zero);
}
