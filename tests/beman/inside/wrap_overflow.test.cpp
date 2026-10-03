// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// wrap policy must reduce out-of-range integers without signed overflow.
//
// Regression: apply_wrap computed `range = upper - lower + 1` and
// `shifted = rhs - lower` in imax, which overflowed (UB) when rhs was a large
// integral and Lower was a large-magnitude opposite sign (e.g. assigning
// INT64_MAX to a wrap grid with a negative Lower), and when the grid spanned
// more than imax under unsigned storage. The reduction now runs in umax.
//
// The oracle is an independent __int128 modular wrap, so values are checked, not
// pinned to magic constants.

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

#include <gtest/gtest.h>
#include <cstdint>

using namespace beman::inside;
using namespace beman::inside::detail;

namespace
{
  // Reference: reduce v into [lo, hi] by wrapping (range = hi - lo + 1).
  long long wrap_ref(long long v, long long lo, long long hi)
  {
#if defined(__SIZEOF_INT128__)
    __int128 range = static_cast<__int128>(hi) - static_cast<__int128>(lo) + 1;
    __int128 r = (((static_cast<__int128>(v) - static_cast<__int128>(lo)) % range) + range) % range;
    return static_cast<long long>(r + lo);
#else
    // MSVC has no __int128: u64 modular reduction. `v - lo` can exceed 64 signed bits
    // at the extremes, so reduce in unsigned (two's-complement) space.
    unsigned long long urange = static_cast<unsigned long long>(hi)
                              - static_cast<unsigned long long>(lo) + 1ull;
    unsigned long long off;
    if (v >= lo)
      off = (static_cast<unsigned long long>(v) - static_cast<unsigned long long>(lo)) % urange;
    else
    {
      unsigned long long d = static_cast<unsigned long long>(lo) - static_cast<unsigned long long>(v);
      unsigned long long m = d % urange;
      off = m ? urange - m : 0ull;
    }
    return static_cast<long long>(static_cast<unsigned long long>(lo) + off);
#endif
  }
}

// wrap reduces extreme integers with no signed overflow
TEST(WrapOverflowTest, wrap_reduces_extreme_integers_with_no_signed_overflow)
{
  using W = inside<{-5000000000LL, 5000000000LL}, wrap>;   // span 1e10 > would-be int32
  constexpr long long lo = -5000000000LL, hi = 5000000000LL;

  for (long long v : { hi + 1, lo - 1, 0LL, 7777777777LL, -7777777777LL,
                       std::numeric_limits<long long>::max(),
                       std::numeric_limits<long long>::min() + 1,
                       std::numeric_limits<long long>::min() })
  {
    W b = v;
    const long long got = static_cast<long long>(static_cast<rational>(b).Numerator)
                        * (static_cast<rational>(b).Denominator < 0 ? -1 : 1);
    SCOPED_TRACE(::testing::Message() << "v=" << v);
    ASSERT_EQ(got, (wrap_ref(v, lo, hi)));
    ASSERT_TRUE(got >= lo);
    ASSERT_TRUE(got <= hi);
  }
}

// wrap on a grid spanning more than imax
TEST(WrapOverflowTest, wrap_on_a_grid_spanning_more_than_imax)
{
  // Unsigned-storage grid whose span exceeds INT64_MAX.
  using Big = inside<{0, 12000000000000000000ULL}, wrap>;   // span ~1.2e19 > imax
  constexpr unsigned long long hi = 12000000000000000000ULL;

  // A value above the top wraps to near the bottom; one below 0 wraps to the top.
  Big a = -1;                                              // -1 -> hi
  ASSERT_TRUE((static_cast<rational>(a) == rational{hi, 1}));
  Big b = 0;
  ASSERT_TRUE(static_cast<rational>(b) == 0);
}

// wrap is identity on a grid spanning the whole u64 range
TEST(WrapOverflowTest, wrap_is_identity_on_a_grid_spanning_the_whole_u64_range)
{
  // span == 2^64-1, so `urange = upper - lower + 1` wraps to 0 in umax: every
  // u64 bit pattern is already in range, so the wrap is the identity and the
  // value is stored verbatim. Exercises the urange==0 fast path in apply_wrap.
  // Identity is a raw round-trip: lower is 0, so the stored raw is exactly the
  // input reinterpreted as u64 (a negative input keeps its two's-complement bits
  // — it is not reducible, since the whole u64 range is in-grid).
  using All = inside<{0, 18446744073709551615ULL}, wrap>;
  static_assert(std::is_same_v<All::raw_type, std::uint64_t>);
  for (long long v : { 0LL, 123456789LL, -1LL,
                       std::numeric_limits<long long>::max(),
                       std::numeric_limits<long long>::min() })
  {
    All b = v;
    SCOPED_TRACE(::testing::Message() << "v=" << v);
    ASSERT_TRUE(b.raw() == static_cast<std::uint64_t>(v));     // identity, no reduction
  }
}

// wrap still correct on a small symmetric grid
TEST(WrapOverflowTest, wrap_still_correct_on_a_small_symmetric_grid)
{
  using S = inside<{-3, 3}, wrap>;                          // range 7
  ASSERT_TRUE(static_cast<rational>(S{0}  =  4) == -3);        // 4  -> -3
  ASSERT_TRUE(static_cast<rational>(S{0}  = -4) ==  3);        // -4 ->  3
  ASSERT_TRUE(static_cast<rational>(S{0}  = 10) ==  3);        // 10 ->  3
  ASSERT_TRUE(static_cast<rational>(S{0}  = -3) == -3);        // on-edge unchanged
}
