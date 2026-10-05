// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// wide_int against builtin oracles. Small limbs (uint8_t, uint16_t) make the
// multi-limb paths — carries, Knuth D's q̂ correction and add-back, shifts
// across limbs — reachable with exhaustive or dense sweeps whose results a
// builtin integer can check exactly.

#include <beman/inside/detail/wide_int.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <vector>

using namespace beman::inside::detail;

namespace
{
  // Values that stress limb boundaries at the given width, plus a dense stride.
  template <typename U>
  std::vector<U> probe_values()
  {
    constexpr int B = std::numeric_limits<U>::digits;
    std::vector<U> v;
    for (int b = 0; b < B; ++b)
    {
      const U p = static_cast<U>(U{1} << b);
      for (U d : {U(0), U(1), U(2)})
      {
        v.push_back(static_cast<U>(p + d));
        v.push_back(static_cast<U>(p - d));
        v.push_back(static_cast<U>(~p + d));
      }
    }
    const U step = static_cast<U>(std::numeric_limits<U>::max() / 251);
    for (U x = 0; x < static_cast<U>(std::numeric_limits<U>::max() - step); x = static_cast<U>(x + step))
      v.push_back(x);
    v.push_back(std::numeric_limits<U>::max());
    return v;
  }

  // Deterministic 64-bit generator (splitmix64).
  struct rng
  {
    std::uint64_t State;
    std::uint64_t operator()()
    {
      std::uint64_t z = (State += 0x9e3779b97f4a7c15ull);
      z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
      z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
      return z ^ (z >> 31);
    }
  };

  // Check every operation of W (unsigned, `bits` wide) against the builtin U,
  // and of its signed twin against S, over all pairs from `vals`.
  template <typename WU, typename WS, typename U, typename S>
  void check_against(std::vector<U> const& vals)
  {
    constexpr int B = std::numeric_limits<U>::digits;
    for (U a : vals)
      for (U b : vals)
      {
        const WU wa{a}, wb{b};
        ASSERT_EQ(static_cast<U>(wa + wb), static_cast<U>(a + b)) << a << " + " << b;
        ASSERT_EQ(static_cast<U>(wa - wb), static_cast<U>(a - b)) << a << " - " << b;
        // (Promote through uint64 first: uint16 * uint16 is an int product.)
        const U ab = static_cast<U>(static_cast<std::uint64_t>(a) * b);
        ASSERT_EQ(static_cast<U>(wa * wb), ab) << a << " * " << b;
        ASSERT_EQ(wa < wb, a < b);
        ASSERT_EQ(wa == wb, a == b);
        if (b != 0)
        {
          ASSERT_EQ(static_cast<U>(wa / wb), static_cast<U>(a / b)) << a << " / " << b;
          ASSERT_EQ(static_cast<U>(wa % wb), static_cast<U>(a % b)) << a << " % " << b;
        }
        const S sa = static_cast<S>(a), sb = static_cast<S>(b);
        const WS xa{sa}, xb{sb};
        ASSERT_EQ(xa < xb, sa < sb) << sa << " < " << sb;
        ASSERT_EQ(static_cast<S>(xa * xb), static_cast<S>(ab));
        // Builtin S / S overflows for min / −1; wide_int wraps.
        if (sb != 0 && !(sa == std::numeric_limits<S>::min() && sb == -1))
        {
          ASSERT_EQ(static_cast<S>(xa / xb), static_cast<S>(sa / sb)) << sa << " / " << sb;
          ASSERT_EQ(static_cast<S>(xa % xb), static_cast<S>(sa % sb)) << sa << " % " << sb;
        }
      }
    for (U a : vals)
      for (int s = 0; s < B; ++s)
      {
        ASSERT_EQ(static_cast<U>(WU{a} << s), static_cast<U>(static_cast<std::uint64_t>(a) << s));
        ASSERT_EQ(static_cast<U>(WU{a} >> s), static_cast<U>(a >> s));
        const S sa = static_cast<S>(a);
        ASSERT_EQ(static_cast<S>(WS{sa} >> s), static_cast<S>(sa >> s));   // arithmetic
      }
  }
} // namespace

TEST(WideIntTest, two_uint8_limbs_against_uint16)
{
  check_against<wide_uint<2, std::uint8_t>, wide_sint<2, std::uint8_t>, std::uint16_t, std::int16_t>(
    probe_values<std::uint16_t>());
}

TEST(WideIntTest, four_uint8_limbs_against_uint32)
{
  check_against<wide_uint<4, std::uint8_t>, wide_sint<4, std::uint8_t>, std::uint32_t, std::int32_t>(
    probe_values<std::uint32_t>());
}

TEST(WideIntTest, four_uint16_limbs_against_uint64)
{
  check_against<wide_uint<4, std::uint16_t>, wide_sint<4, std::uint16_t>, std::uint64_t, std::int64_t>(
    probe_values<std::uint64_t>());
}

// Exhaustive 16-bit division over uint8_t limbs: every dividend against a
// dense divisor set covers each q̂ correction and add-back case.
TEST(WideIntTest, exhaustive_division_two_uint8_limbs)
{
  using W = wide_uint<2, std::uint8_t>;
  for (unsigned b = 1; b <= 0xffff; b += (b < 1024 ? 1 : 97))
    for (unsigned a = 0; a <= 0xffff; ++a)
    {
      const auto r = W::divmod(W{a}, W{b});
      ASSERT_EQ(static_cast<unsigned>(r.Quotient), a / b) << a << " / " << b;
      ASSERT_EQ(static_cast<unsigned>(r.Remainder), a % b) << a << " % " << b;
    }
}

#if defined(__SIZEOF_INT128__)
namespace
{
  // __int128 is not std::integral in strict C++23 modes: go through the limbs.
  using u128n = unsigned __int128;
  template <bool S>
  wide_int<2, S> from128(u128n v) { wide_int<2, S> w; w.Word[0] = static_cast<std::uint64_t>(v); w.Word[1] = static_cast<std::uint64_t>(v >> 64); return w; }
  template <bool S>
  u128n to128(wide_int<2, S> const& w) { return (u128n{w.Word[1]} << 64) | w.Word[0]; }
}

TEST(WideIntTest, two_umax_limbs_against_int128)
{
  using U = u128n;
  using S = __int128;
  rng next{42};
  auto pick = [&]() -> U {
    // Mix full-width, half-width and near-limb-boundary values.
    switch (next() % 4)
    {
      case 0:  return (U{next()} << 64) | next();
      case 1:  return U{next()};
      case 2:  return (U{1} << (next() % 128)) + (next() % 5) - 2;
      default: return (U{next() >> (next() % 64)} << 64) | next();
    }
  };
  for (int i = 0; i < 200000; ++i)
  {
    const U a = pick(), b = pick();
    const auto wa = from128<false>(a), wb = from128<false>(b);
    ASSERT_TRUE(to128(wa + wb) == a + b);
    ASSERT_TRUE(to128(wa - wb) == a - b);
    ASSERT_TRUE(to128(wa * wb) == a * b);
    ASSERT_EQ(wa < wb, a < b);
    if (b != 0)
    {
      ASSERT_TRUE(to128(wa / wb) == a / b);
      ASSERT_TRUE(to128(wa % wb) == a % b);
    }
    const S sa = static_cast<S>(a), sb = static_cast<S>(b);
    const auto xa = from128<true>(a), xb = from128<true>(b);
    ASSERT_EQ(xa < xb, sa < sb);
    if (sb != 0 && sb != -1)
    {
      ASSERT_TRUE(static_cast<S>(to128(xa / xb)) == sa / sb);
      ASSERT_TRUE(static_cast<S>(to128(xa % xb)) == sa % sb);
    }
  }
}
#endif

// Three limbs have no builtin oracle: check the division identity and the
// round trip through two-limb values.
TEST(WideIntTest, three_umax_limbs_division_identity)
{
  using W = wide_sint<3>;
  rng next{7};
  for (int i = 0; i < 100000; ++i)
  {
    W a, b;
    for (auto& w : a.Word) w = next();
    for (auto& w : b.Word) w = next();
    b = b >> static_cast<int>(next() % 191);
    if (b.is_zero()) continue;
    const auto r = W::divmod(a, b);
    ASSERT_TRUE(r.Quotient * b + r.Remainder == a);
    // |remainder| < |divisor|, and the remainder takes the dividend's sign.
    const W ar = r.Remainder.negative() ? -r.Remainder : r.Remainder;
    const W ab = b.negative() ? -b : b;
    ASSERT_TRUE(wide_uint<3>(ar) < wide_uint<3>(ab));
    ASSERT_TRUE(r.Remainder.is_zero() || r.Remainder.negative() == a.negative());
  }
}

TEST(WideIntTest, conversions)
{
  using U2 = wide_uint<2>;
  using S2 = wide_sint<2>;
  using S3 = wide_sint<3>;
  // Sign extension from builtins and between widths.
  EXPECT_EQ(static_cast<std::int64_t>(S3{S2{-7}}), -7);
  EXPECT_TRUE(S3{S2{-1}} == S3{-1});
  EXPECT_TRUE(S3{U2{~0ull}} == S3{~0ull});
  EXPECT_EQ(static_cast<std::uint64_t>(U2{-1} >> 64), ~0ull);  // -1 sign-extends to all ones
  // Truncation to builtins keeps the low bits.
  EXPECT_EQ(static_cast<std::uint8_t>(U2{0x1234}), 0x34u);
  EXPECT_EQ(static_cast<std::int32_t>(S2{-2}), -2);
  // Nearest double, ties to even, sticky bits honoured.
  EXPECT_EQ(static_cast<double>(U2{1} << 100), 0x1p100);
  EXPECT_EQ(static_cast<double>((U2{1} << 100) + U2{1}), 0x1p100);
  EXPECT_EQ(static_cast<double>((U2{1} << 53) + U2{1}), 0x1p53);                    // tie → even
  EXPECT_EQ(static_cast<double>((U2{1} << 120) + (U2{1} << 67) + U2{1}), 0x1p120 + 0x1p68); // tie broken by sticky
  EXPECT_EQ(static_cast<double>(S2{-3}), -3.0);
  EXPECT_EQ(static_cast<double>(std::numeric_limits<S2>::min()), -0x1p127);
}

TEST(WideIntTest, limits_and_bit_width)
{
  using S2 = wide_sint<2>;
  using U3 = wide_uint<3>;
  EXPECT_EQ(std::numeric_limits<S2>::digits, 127);
  EXPECT_EQ(std::numeric_limits<U3>::digits, 192);
  EXPECT_TRUE(std::numeric_limits<S2>::max() + S2{1} == std::numeric_limits<S2>::min());
  EXPECT_EQ(bit_width_of(U3{0}), 0);
  EXPECT_EQ(bit_width_of(U3{1} << 130), 131);
  EXPECT_EQ(bit_width_of(std::numeric_limits<U3>::max()), 192);
}

TEST(WideIntTest, shifts_saturate_past_width)
{
  using S2 = wide_sint<2>;
  EXPECT_TRUE((S2{5} << 128).is_zero());
  EXPECT_TRUE((S2{-5} >> 200) == S2{-1});
  EXPECT_TRUE((S2{5} >> 200).is_zero());
}

// Everything works in constant evaluation.
namespace
{
  using U2 = wide_uint<2>;
  using S2 = wide_sint<2>;
  constexpr U2 big = (U2{1} << 127) + U2{12345};
  static_assert(big / U2{3} * U2{3} + big % U2{3} == big);
  static_assert(S2{-5} / S2{2} == S2{-2} && S2{-5} % S2{2} == S2{-1});
  static_assert(static_cast<double>(U2{1} << 100) == 0x1p100);
  static_assert(std::numeric_limits<S2>::min() < S2{0});
  static_assert(std::is_trivially_copyable_v<U2> && std::is_standard_layout_v<U2>);

  template <U2 V> struct nttp { static constexpr U2 value = V; };
  static_assert(nttp<big>::value == big);                   // structural
}

// GCC 15/16 miscompare a defaulted == over an array member in constant
// evaluation: `a == x && b == y` came out true with b != y (seen as √(1/2)
// "exact" in the math engine's compile-time path). wide_int writes == out.
TEST(WideIntTest, equality_in_constant_evaluation)
{
  using W = beman::inside::detail::wide_sint<3>;
  static_assert(![] { const W n{1}, d{1}; return n == W{1} && d == W{2}; }());
  static_assert([] { const W n{1}, d{2}; return n == W{1} && d == W{2}; }());
  SUCCEED();
}
