// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// big_int / big_rational: grid numbers of any size (C++26 static reflection).
// Without reflection the header compiles to nothing and the test is skipped.

#include <beman/inside/detail/grid_rational.hpp>

#include <gtest/gtest.h>

#if BEMAN_INSIDE_BIG_GRIDS
using namespace beman::inside::detail;
using beman::inside::umax;
using beman::inside::imax;

namespace
{
  // 2^k as a big_int, built by doubling (every step is exact).
  consteval big_int pow2(int k) { big_int v{1}; for (int i = 0; i < k; ++i) v = v * big_int{2}; return v; }

  // Values are canonical and interned: equal values are equal structurally,
  // so they are the same template argument.
  template <big_int V> struct tag {};
  static_assert(std::is_same_v<tag<pow2(100)>, tag<pow2(50) * pow2(50)>>);
  static_assert(!std::is_same_v<tag<pow2(100)>, tag<pow2(101)>>);
  static_assert(pow2(63).fits_limb() && !pow2(64).fits_limb());
  static_assert((pow2(64) - big_int{1}).fits_limb());               // shrinks back inline

  // Arithmetic against identities.
  static_assert((pow2(200) + pow2(100)) - pow2(200) == pow2(100));
  static_assert(pow2(200) / pow2(70) == pow2(130));
  static_assert((pow2(200) + big_int{5}) % pow2(70) == big_int{5});
  static_assert(-pow2(100) < big_int{0} && -pow2(100) < -pow2(99));
  static_assert(gcd(pow2(150) * big_int{3}, pow2(120) * big_int{9}) == pow2(120) * big_int{3});
  static_assert(pow2(130).bit_width() == 131);
  static_assert(static_cast<umax>(pow2(64) + big_int{7}) == 7);       // truncates like a builtin
  static_assert(static_cast<wide_uint<3>>(pow2(130)) == (wide_uint<3>{1} << 130));
  static_assert(big_int{wide_sint<3>{-1} << 130} == -pow2(130));

  // big_rational: canonical fractions of any size.
  constexpr big_rational huge{pow2(100) * big_int{3}, pow2(40) * big_int{6}};   // = 2^59
  static_assert(huge == big_rational{pow2(59)} && huge.is_integer() && huge.fits_rational());
  constexpr big_rational fine{big_int{1}, pow2(90)};                              // 2^-90
  static_assert(!fine.fits_rational());
  static_assert(fine * big_rational{pow2(90)} == big_rational{1});
  static_assert(fine + fine == big_rational{big_int{1}, pow2(89)});
  static_assert(fine < big_rational{rational{1, 1024}});
  static_assert(gcd(big_rational{rational{1, 4}}, big_rational{rational{1, 6}}) == big_rational{rational{1, 12}});
  static_assert(big_rational{0x1p100} == big_rational{pow2(100)});
  static_assert(big_rational{0x1p-100} == big_rational{big_int{1}, pow2(100)});

  // The implicit 64-bit view of a small value.
  constexpr rational r = big_rational{rational{-3, 7}};
  static_assert(r == rational{-3, 7});
  static_assert(big_rational{rational{5, 2}} == rational{5, 2});         // mixed comparison
}

TEST(BigRationalTest, runtime_small_values)
{
  // Small values compute at runtime without allocation.
  volatile int x = 7;
  const big_int a{static_cast<int>(x)}, b{-3};
  EXPECT_TRUE(a * b == big_int{-21});
  EXPECT_TRUE((big_rational{a, big_int{14}} == big_rational{rational{1, 2}}));
  EXPECT_TRUE(static_cast<rational>(big_rational{a}) == rational{7});
}

TEST(BigRationalTest, runtime_overflow_is_reported)
{
  volatile umax big = ~umax{0};
  EXPECT_THROW((void)(big_int{static_cast<umax>(big)} * big_int{static_cast<umax>(big)}), beman::inside::inside_error);
}
#else
TEST(BigRationalTest, needs_cxx26_reflection) { GTEST_SKIP() << "C++26 static reflection unavailable"; }
#endif
