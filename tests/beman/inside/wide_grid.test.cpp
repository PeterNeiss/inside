// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Grids with more than 2^64 slots store a wide_int index instead of falling
// back to a rational: construction, assignment under every policy, comparison,
// arithmetic, conversions, io, hashing and sampling.

#include <beman/inside/inside.hpp>
#include <beman/inside/formats.hpp>
#include <beman/inside/io.hpp>
#include <beman/inside/numeric_limits.hpp>
#include <beman/inside/random.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <random>
#include <unordered_set>

using namespace beman::inside;
using beman::inside::detail::rational;

namespace
{
  constexpr umax p32 = umax{1} << 32;
  constexpr umax p34 = umax{1} << 34;

  // {0, 2^34} in steps of 2^-32: 2^66 + 1 slots, a 67-bit index.
  using fine         = inside<{{0, p34}, per<p32>}>;
  using fine_nearest = inside<{{0, p34}, per<p32>}, round_nearest>;
  using fine_clamp   = inside<{{0, p34}, per<p32>}, clamp>;
  using fine_wrap    = inside<{{0, p34}, per<p32>}, wrap>;
  // Signed and offset: {−2^34, 2^34} in steps of 2^-32, Lower ≠ 0.
  using fine_signed  = inside<{{-rational{p34}, rational{p34}}, per<p32>}>;

  constexpr rational tick{umax{1}, static_cast<imax>(p32)};          // 2^-32
}

TEST(WideGridTest, deduces_wide_index)
{
  static_assert(std::is_same_v<fine::raw_type, detail::wide_uint<2>>);
  static_assert(std::is_same_v<fine_signed::raw_type, detail::wide_uint<2>>);
  static_assert(sizeof(fine) == 16);
  static_assert(detail::wide_raw<fine> && detail::index_raw<fine>);
  static_assert(grid_of<fine>.slot_bits() == 67);
  // `indexed` sizes from the same slot count.
  static_assert(std::is_same_v<inside<{{0, p34}, per<p32>}, indexed>::raw_type, detail::wide_uint<2>>);
  // A 64-bit rational cannot hold every value: no implicit conversion.
  static_assert(!std::is_convertible_v<fine, rational>);
  static_assert(!std::is_convertible_v<fine, imax>);
}

TEST(WideGridTest, construction_from_scalars)
{
  fine a = 5;
  EXPECT_TRUE(a == 5);
  EXPECT_TRUE(a.raw() == detail::wide_uint<2>{5 * p32});
  fine b = 0.5;
  EXPECT_TRUE(b == 0.5);
  fine c = tick;                                       // the first notch
  EXPECT_TRUE(c == tick);
  EXPECT_TRUE(c.raw() == detail::wide_uint<2>{1});
  fine top = p34;
  EXPECT_TRUE(top == p34);
  EXPECT_TRUE(top.raw() == (detail::wide_uint<2>{1} << 66));

  fine_signed s = -3.25;
  EXPECT_TRUE(s == -3.25);
  EXPECT_TRUE(s < 0);
}

TEST(WideGridTest, assignment_policies)
{
  // checked: off-lattice and out-of-range report.
  fine a = 1;
  EXPECT_THROW((a = rational{1, imax{1} << 33}), inside_error);   // half a notch
  EXPECT_THROW(a = p34 + 1, inside_error);
  EXPECT_THROW(a = -1, inside_error);
  EXPECT_TRUE(a == 1);                                           // unchanged

  // rounding: half a notch rounds away from zero.
  fine_nearest n = 0;
  n = rational{1, imax{1} << 33};
  EXPECT_TRUE(n == tick);

  // clamp and wrap.
  fine_clamp c = 0;
  c = p34 + 7;
  EXPECT_TRUE(c == p34);
  c = -2.0;
  EXPECT_TRUE(c == 0);
  fine_wrap w = 0;
  w = p34 + 1;                       // one past the top: 2^32 notches past it
  EXPECT_TRUE(w == (rational{1} - tick).value());

  // errc-reporting policy.
  errc e{};
  fine x = 3;
  x.policy(e) = p34 * 2;
  EXPECT_EQ(e, errc::overflow);
  EXPECT_TRUE(x == 3);

  // non-finite doubles.
  EXPECT_THROW(x = std::numeric_limits<double>::infinity(), inside_error);
  c = std::numeric_limits<double>::infinity();
  EXPECT_TRUE(c == p34);
}

TEST(WideGridTest, assignment_between_insides)
{
  fine a = inside<{0, 100}>{42};
  EXPECT_TRUE(a == 42);
  // wide → narrow: the narrow grid's notch is coarser, so it rounds under snap.
  fine b = 41.75;
  inside<{0, 100}, round_nearest> n = 0;
  n = b;
  EXPECT_EQ(static_cast<imax>(n), 42);
  // wide → wide of another lattice.
  fine_signed s = b;
  EXPECT_TRUE(s == 41.75);
}

TEST(WideGridTest, comparisons)
{
  fine a = 1.5, b = 2;
  EXPECT_TRUE(a < b && b > a && a != b);
  EXPECT_TRUE((a == 1.5 && a == rational{3, 2}));
  EXPECT_TRUE(a > 1 && a < 2u && a < 1e30 && a > -1e30);
  EXPECT_TRUE((a == inside<{{0, 10}, per<2>}>{1.5}));
  EXPECT_TRUE((a < inside<{0, 10}>{2}));
  fine_signed s = -1.5;
  EXPECT_TRUE(s < a);
  EXPECT_TRUE(-a == s);
  EXPECT_EQ(a <=> b, std::strong_ordering::less);
}

TEST(WideGridTest, arithmetic)
{
  fine a = 1.5, b = rational{1} + tick;
  auto sum = a + b;                                    // {0, 2^35}, notch 2^-32
  static_assert(detail::wide_raw<decltype(sum)>);
  EXPECT_TRUE(sum == (rational{5, 2} + tick).value());
  auto diff = a - b;                                   // {−2^34, 2^34}
  EXPECT_TRUE(diff == (rational{1, 2} - tick).value());
  auto neg = -a;
  EXPECT_TRUE(neg == -1.5);
  auto prod = a * inside<{0, 3}>{3};                   // {0, 3·2^34}, notch 2^-32
  static_assert(detail::wide_raw<decltype(prod)>);
  EXPECT_TRUE(prod == 4.5);
  // narrow + wide and back.
  auto mixed = inside<{0, 100}>{7} + a;
  EXPECT_TRUE(mixed == 8.5);
}

TEST(WideGridTest, uint64_difference_widens)
{
  constexpr umax kUM = ~umax{0};
  qword x{kUM}, y{3};
  auto d = x - y;                                      // spans 2^65 values
  static_assert(detail::wide_raw<decltype(d)>);
  EXPECT_TRUE(d == kUM - 3);
  EXPECT_TRUE(y - x == -rational{kUM - 3});
  EXPECT_EQ(x.to<std::uint64_t>().value(), kUM);
}

TEST(WideGridTest, conversions)
{
  fine a = 41.75;
  EXPECT_EQ(a.to<int>().value(), 41);
  EXPECT_EQ(a.to<double>().value(), 41.75);
  EXPECT_EQ(a.to<std::uint8_t>().value(), 41);
  fine big = p34;
  EXPECT_EQ(big.to<std::int32_t>().error(), errc::overflow);
  EXPECT_EQ(fine_signed{-1}.to<unsigned>().error(), errc::domain_error);
}

TEST(WideGridTest, to_string)
{
  EXPECT_EQ(to_string(fine{1.5}), to_string(rational{3, 2}));
  // 2^33 + 2^-32 = (2^65 + 1) / 2^32: the numerator passes 64 bits.
  const fine odd = fine::from_raw((detail::wide_uint<2>{1} << 65) + detail::wide_uint<2>{1});
  EXPECT_EQ(to_string(odd), "36893488147419103233/4294967296");
  EXPECT_EQ(to_string(detail::wide_uint<2>{1} << 100), "1267650600228229401496703205376");
  EXPECT_NE(to_string_debug(odd).find("wide_uint<2>"), std::string::npos);
}

TEST(WideGridTest, hashing)
{
  std::unordered_set<fine> set{fine{1}, fine{1.5}, fine{1}};
  EXPECT_EQ(set.size(), 2u);
  EXPECT_EQ(std::hash<fine>{}(fine{2}), std::hash<fine>{}(fine{2}));
}

TEST(WideGridTest, uniform_sampling)
{
  std::mt19937_64 rng{1};
  bool above_2_33 = false;
  for (int i = 0; i < 1000; ++i)
  {
    const fine v = uniform<fine>(rng);
    ASSERT_TRUE(v >= 0 && v <= p34);
    above_2_33 = above_2_33 || v > (umax{1} << 33);
  }
  EXPECT_TRUE(above_2_33);
}

// The wide paths are constexpr.
namespace
{
  static_assert(fine{1.5} + fine{2} == 3.5);
  static_assert(fine{1.5} < fine_signed{2});
  static_assert(-fine{1} == -1);
  static_assert([] { fine_clamp c = 0; c = p34 + 1; return c == p34; }());
}
