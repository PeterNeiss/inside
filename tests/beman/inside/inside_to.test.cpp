// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include <beman/inside/inside.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <type_traits>
#include <vector>

using namespace beman::inside;
using namespace beman::inside::detail;

// inside::to<unsigned T>
TEST(InsideToTest, inside_to_unsigned_t)
{
  {
    SCOPED_TRACE("trivially-fits grid takes the fast path");
    using B = inside<{0, 100}>;
    ASSERT_TRUE(B{42}.to<std::uint8_t>().value() == 42);
    ASSERT_TRUE(B{0}.to<std::uint32_t>().value() == 0);
    ASSERT_TRUE(B{100}.to<std::size_t>().value() == 100);
  }

  {
    SCOPED_TRACE("runtime overflow when Upper > T::max");
    using B = inside<{0, 1000}>;
    auto r = B{500}.to<std::uint8_t>();
    ASSERT_FALSE(r.has_value());
    ASSERT_EQ(r.error(), errc::overflow);
  }

  {
    SCOPED_TRACE("negative inside to unsigned -> domain_error");
    using B = inside<{-10, 10}>;
    auto r = B{-5}.to<std::uint8_t>();
    ASSERT_FALSE(r.has_value());
    ASSERT_EQ(r.error(), errc::domain_error);
  }

  {
    SCOPED_TRACE("fractional notch truncates silently (matches rational::to)");
    using B = inside<{{0, 1}, notch<1, 2>}>;
    ASSERT_TRUE(B{0.5}.to<std::uint8_t>().value() == 0);   // 0.5 -> 0
    ASSERT_TRUE(B{1}.to<std::uint8_t>().value() == 1);
  }

  {
    SCOPED_TRACE("sentinel-state inside -> not_a_value");
    using B = inside<{0, 100}, sentinel>;
    B b = B::make_sentinel();
    auto r = b.to<std::uint8_t>();
    ASSERT_FALSE(r.has_value());
    ASSERT_EQ(r.error(), errc::not_a_value);
  }

  {
    SCOPED_TRACE("Q-format fast path");
    using B = inside<{{0, 255}, notch<1, 256>}, round_nearest>;
    ASSERT_TRUE(B{42.5}.to<std::uint8_t>().value() == 42);
  }
}

// inside::to<signed T>
TEST(InsideToTest, inside_to_signed_t)
{
  {
    SCOPED_TRACE("trivially-fits grid takes the fast path");
    using B = inside<{-50, 50}>;
    ASSERT_TRUE(B{-7}.to<std::int8_t>().value() == -7);
    ASSERT_TRUE(B{42}.to<std::int32_t>().value() == 42);
  }

  {
    SCOPED_TRACE("overflow upward -> errc::overflow");
    using B = inside<{0, 1000}>;
    auto r = B{500}.to<std::int8_t>();
    ASSERT_FALSE(r.has_value());
    ASSERT_EQ(r.error(), errc::overflow);
  }

  {
    SCOPED_TRACE("overflow downward -> errc::overflow");
    using B = inside<{-1000, 0}>;
    auto r = B{-500}.to<std::int8_t>();
    ASSERT_FALSE(r.has_value());
    ASSERT_EQ(r.error(), errc::overflow);
  }
}

// inside::to<floating T>
TEST(InsideToTest, inside_to_floating_t)
{
  {
    SCOPED_TRACE("ordinary value");
    using B = inside<{{0, 1}, notch<1, 2>}>;
    ASSERT_TRUE(B{0.5}.to<double>().value() == 0.5);
  }

  {
    SCOPED_TRACE("sentinel-state -> not_a_value");
    using B = inside<{0, 100}, sentinel>;
    B b = B::make_sentinel();
    auto r = b.to<double>();
    ASSERT_FALSE(r.has_value());
    ASSERT_EQ(r.error(), errc::not_a_value);
  }

  {
    SCOPED_TRACE("to<double> works even on a strict-policy inside (no policy gate)");
    using B = inside<{0, 10}, checked>;
    ASSERT_TRUE(B{3}.to<double>().value() == 3.0);
  }
}

// inside -> rational conversion
TEST(InsideToTest, inside_to_rational_conversion)
{
  {
    SCOPED_TRACE("ordinary value");
    using B = inside<{0, 10}>;
    ASSERT_TRUE(static_cast<rational>(B{5}) == 5);
  }

  {
    SCOPED_TRACE("fractional grid is preserved exactly");
    using B = inside<{{0, 1}, notch<1, 2>}>;
    ASSERT_TRUE(static_cast<rational>(B{0.5}) == 0.5_r);
  }
}

// operator imax is gated on fit-in-imax
TEST(InsideToTest, operator_imax_is_gated_on_fit_in_imax)
{
  using narrow = inside<{0, 100}>;
  using wide   = inside<{0, std::numeric_limits<std::uint64_t>::max()}>;

  // Narrow grids still convert implicitly.
  static_assert(std::is_convertible_v<narrow, imax>);
  imax i = narrow{42};
  ASSERT_EQ(i, 42);

  // Wide grids (max > INT64_MAX) lose the implicit imax conversion. The
  // implicit operator size_t() is also gated on Upper <= imax_max, so wide
  // bounds don't sneak in via size_t -> imax integer conversion.
  static_assert(!(std::is_convertible_v<wide, imax>));
  static_assert(!(std::is_convertible_v<wide, std::size_t>));

  // The typed-error path is what wide grids should use instead. (We do not
  // construct a `wide` instance here: assignment-side overflow checks on
  // grids spanning the full uint64 range are out of scope for this test.)
}

// operator size_t for index-shaped bounds
TEST(InsideToTest, operator_size_t_for_index_shaped_bounds)
{
  using idx_t  = inside<{0, 9}>;            // notch 1, Lower 0 — index shape
  using offset = inside<{5, 10}>;            // notch 1, Lower > 0 — also OK

  // Index-shaped bounds convert implicitly to size_t (silences
  // -Wsign-conversion at the `vec[inside_idx]` call sites in examples).
  static_assert(std::is_convertible_v<idx_t,  std::size_t>);
  static_assert(std::is_convertible_v<offset, std::size_t>);

  // (Bounds with Lower < 0 don't get the direct size_t conversion, but
  // they're still convertible via the imax -> size_t standard step; the
  // inside shape that we *want* to remain non-convertible is the wide one,
  // verified in the test case above.)

  // Use the conversion to index a vector — no `.as<>()` needed.
  std::vector<int> v{0, 10, 20, 30, 40, 50, 60, 70, 80, 90};
  idx_t i{3};
  ASSERT_EQ(v[i], 30);

  // Direct-init to size_t picks operator size_t() unambiguously.
  std::size_t s = i;
  ASSERT_EQ(s, 3);
}

// operator double is gated on rounding policy
TEST(InsideToTest, operator_double_is_gated_on_rounding_policy)
{
  using B_round   = inside<{{0, 1}, notch<1, 2>}, round_nearest>;
  using B_ignore  = inside<{{0, 1}, notch<1, 2>}, snap>;
  using B_strict  = inside<{{0, 1}, notch<1, 2>}, checked>;
  using B_floor   = inside<{{0, 1}, notch<1, 2>}, round_floor>;

  // operator double() is explicit, so use is_constructible_v to detect it.
  static_assert(std::is_constructible_v<double, B_round>);
  static_assert(std::is_constructible_v<double, B_ignore>);
  static_assert(std::is_constructible_v<double, B_floor>);
  static_assert(!(std::is_constructible_v<double, B_strict>));

  // The typed-error path is always available.
  ASSERT_TRUE(B_strict{0.5}.to<double>().value() == 0.5);
}

// as<T>() is a non-expected shortcut for to<T>().value()
TEST(InsideToTest, as_t_is_a_non_expected_shortcut_for_to_t_value)
{
  using narrow = inside<{0, 100}>;
  using frac   = inside<{{0, 100}, notch<1, 10>}, round_nearest>;

  // Matches the integer-extraction value path.
  ASSERT_TRUE(narrow{42}.as<imax>()        == 42);
  ASSERT_TRUE(narrow{42}.as<std::size_t>() == 42u);

  // Fractional notch: extracting to imax truncates (matches to<imax>()).
  ASSERT_TRUE(frac{7.5}.as<imax>() == 7);

  // Double target behaves the same as `to<T>().value()`; the inside -> rational
  // conversion is the implicit operator.
  ASSERT_TRUE(static_cast<rational>(narrow{42}) == 42);
  ASSERT_TRUE(frac{7.5}.as<double>()            == 7.5);

  // Free-function forms — template-friendly (no `.template` disambiguator),
  // found by ADL, same semantics as the members.
  ASSERT_TRUE(as<imax>(narrow{42})          == 42);
  ASSERT_TRUE(as<double>(frac{7.5})         == 7.5);
  ASSERT_TRUE(to<imax>(frac{7.5}).value()   == 7);
  ASSERT_TRUE(to<double>(narrow{3}).value() == 3.0);
}

// Probes must stay dependent so a gated call yields `false` instead of a
// hard error (requires-expressions only SFINAE during substitution).
template <typename B, typename T>
concept has_member_as = requires(B b) { b.template as<T>(); };
template <typename B, typename T>
concept has_free_as   = requires(B b) { as<T>(b); };
template <typename B, typename T>
concept has_member_to = requires(B b) { b.template to<T>(); };

// as<floating> shares operator double's policy gate
TEST(InsideToTest, as_floating_shares_operator_double_s_policy_gate)
{
  using strict_b  = inside<{0, 100}>;                 // no rounding flag
  using rounded_b = inside<{0, 100}, round_nearest>;

  // Strict: both spellings of the direct double read are rejected;
  // to<double>() stays the explicit opt-in.
  static_assert(!has_member_as<strict_b, double>);
  static_assert(!has_free_as<strict_b, double>);
  static_assert(has_member_to<strict_b, double>);

  // Integral as<> is not policy-gated.
  static_assert(has_member_as<strict_b, imax>);

  // A rounding flag opens the gate.
  static_assert(has_member_as<rounded_b, double>);
  ASSERT_TRUE(rounded_b{42}.as<double>() == 42.0);
}
