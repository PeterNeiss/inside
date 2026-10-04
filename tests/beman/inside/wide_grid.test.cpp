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

TEST(WideGridTest, from_chars)
{
  EXPECT_TRUE(from_chars<fine>("1.5").value() == 1.5);
  // 2^33 + 2^-32 needs a 66-bit numerator: the exact fallback parses it.
  const fine odd = fine::from_raw((detail::wide_uint<2>{1} << 65) + detail::wide_uint<2>{1});
  EXPECT_TRUE(from_chars<fine>("36893488147419103233/4294967296").value() == odd);
  EXPECT_TRUE(from_chars<fine>("8589934592.00000000023283064365386962890625").value() == odd);
  EXPECT_EQ(from_chars<fine>("1e40").error(), errc::overflow);         // out of range
  EXPECT_EQ(from_chars<fine>("12x").error(), errc::invalid_format);
  // Round trip through to_string.
  EXPECT_TRUE(from_chars<fine>(to_string(odd)).value() == odd);
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

TEST(WideGridTest, division)
{
  // (fine / fine has a quotient interval past 2^64: it needs C++26 big grids.)
  using divisor = inside<{{1, 4}, per<4>}>;
  fine a = 3;
  auto q = a / divisor{1.5};                            // exact; may report overflow
  ASSERT_TRUE(q.has_value());
  EXPECT_TRUE(*q == 2);
  // (2^33 + 2^-32) / 1 has no 64-bit rational form: reported, not wrapped.
  const fine odd = fine::from_raw((detail::wide_uint<2>{1} << 65) + detail::wide_uint<2>{1});
  EXPECT_EQ((odd / divisor{1}).error(), errc::overflow);
  // narrow ÷ wide, including a zero divisor.
  auto r = inside<{0, 10}>{3} / fine{0.5};
  ASSERT_TRUE(r.has_value());
  EXPECT_TRUE(*r == 6);
  EXPECT_EQ((inside<{0, 10}>{3} / fine{0}).error(), errc::division_by_zero);
}

TEST(WideGridTest, modulo)
{
  // qword differences span 2^65 values: integers past imax.
  constexpr umax kUM = ~umax{0};
  auto d = qword{kUM} - qword{0};
  static_assert(detail::wide_raw<decltype(d)>);
  auto m = mod(d, inside<{1, 1000}>{7}, policy<snap>{});
  EXPECT_TRUE(m == (kUM % 7));
  auto n = mod(-d, inside<{1, 1000}>{7}, policy<snap>{});     // takes the dividend's sign
  EXPECT_TRUE(n == -static_cast<imax>(kUM % 7));
  // qword itself (a uint64 value raw past imax) now has a modulo too.
  EXPECT_TRUE((mod(qword{kUM}, inside<{1, 1000}>{10}, policy<snap>{}) == 5));
}

TEST(WideGridTest, clamp_and_wrap_actions)
{
  // on_clamp: the overshoot is shaped like the builtin paths' (imax for an
  // integral source, an inside for an inside source).
  fine c = 0;
  imax over = 0;
  c.on_clamp([&](auto&, imax o) { over = o; }) = p34 + 5;
  EXPECT_TRUE(c == p34);
  EXPECT_EQ(over, 5);
  rational frac_over{0};
  c.on_clamp([&](auto&, rational o) { frac_over = o; }) = rational{-1, 2};
  EXPECT_TRUE(c == 0);
  EXPECT_TRUE((frac_over == rational{-1, 2}));
  bool got_inside = false;
  c.on_clamp([&](auto&, auto o) { got_inside = insidable<decltype(o)> && o == 6; }) = inside<{0, umax{1} << 40}>{p34 + 6};
  EXPECT_TRUE(got_inside);

  // on_wrap: the carry is the number of full turns.
  fine w = 0;
  imax carry = 0;
  // One turn is 2^34 + 2^-32, so 3·2^34 + 2 = 3 turns + (2 − 3·2^-32).
  w.on_wrap([&](auto&, auto q) { carry = q; }) = 3 * p34 + 2;
  EXPECT_EQ(carry, 3);
  EXPECT_TRUE(w == (rational{2} - (rational{3} * tick).value()).value());
}

// The wide paths are constexpr.
namespace
{
  static_assert(fine{1.5} + fine{2} == 3.5);
  static_assert(fine{1.5} < fine_signed{2});
  static_assert(-fine{1} == -1);
  static_assert([] { fine_clamp c = 0; c = p34 + 1; return c == p34; }());
}
