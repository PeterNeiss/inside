// Differential tests for the portable overflow-detection fallbacks
// (bnd::non_builtin_{add,sub,mul}_overflow) against the compiler builtins.
//
// On any host with __builtin_*_overflow (the common case) the library uses the
// builtins and the hand-rolled fallbacks are NEVER compiled into normal paths —
// so they are exactly the kind of code that rots untested. Here the builtins are
// the trusted oracle: the fallbacks must agree on both the overflow flag and the
// stored result, across exhaustive small widths and boundary-heavy 32/64-bit.

#include "bound/detail/overflow.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <limits>
#include <random>
#include <vector>

using namespace bnd;

namespace
{
  // The builtins are only the oracle when the host actually provides them.
#ifdef BOUND_HAVE_BUILTIN
  template <class T> bool ref_add(T a, T b, T* r) { return __builtin_add_overflow(a, b, r); }
  template <class T> bool ref_sub(T a, T b, T* r) { return __builtin_sub_overflow(a, b, r); }
  template <class T> bool ref_mul(T a, T b, T* r) { return __builtin_mul_overflow(a, b, r); }

  template <class T>
  void check_all(T a, T b)
  {
    T rr, rn;
    bool or_ = ref_add(a, b, &rr), on = non_builtin_add_overflow(a, b, &rn);
    REQUIRE(or_ == on);
    if (!or_) REQUIRE(rr == rn);

    or_ = ref_sub(a, b, &rr); on = non_builtin_sub_overflow(a, b, &rn);
    REQUIRE(or_ == on);
    if (!or_) REQUIRE(rr == rn);

    or_ = ref_mul(a, b, &rr); on = non_builtin_mul_overflow(a, b, &rn);
    REQUIRE(or_ == on);
    if (!or_) REQUIRE(rr == rn);
  }

  // Boundary-heavy value set for the wide types.
  template <class T>
  std::vector<T> boundary_values()
  {
    using L = std::numeric_limits<T>;
    std::vector<long long> raw = {
      0, 1, -1, 2, -2, 3, -3, 7, -7, 255, 256, 257,
      46340, 46341, -46341, 3037000499LL, 3037000500LL, -3037000500LL,
      1LL << 16, 1LL << 31, 1LL << 32, (1LL << 32) + 1, -(1LL << 32),
      static_cast<long long>(L::min()), static_cast<long long>(L::max()),
      static_cast<long long>(L::min()) + 1, static_cast<long long>(L::max()) - 1,
    };
    std::vector<T> out;
    for (long long v : raw)
      if (v >= static_cast<long long>(L::min()) && v <= static_cast<long long>(L::max()))
        out.push_back(static_cast<T>(v));
    return out;
  }
#endif
}

#ifdef BOUND_HAVE_BUILTIN

TEST_CASE("overflow fallbacks: exhaustive 8-bit vs builtins", "[overflow][fallback]")
{
  for (int a = -128; a <= 127; ++a)
    for (int b = -128; b <= 127; ++b)
      check_all<std::int8_t>(static_cast<std::int8_t>(a), static_cast<std::int8_t>(b));

  for (int a = 0; a <= 255; ++a)
    for (int b = 0; b <= 255; ++b)
      check_all<std::uint8_t>(static_cast<std::uint8_t>(a), static_cast<std::uint8_t>(b));
}

TEST_CASE("overflow fallbacks: 16-bit boundary + sampled vs builtins", "[overflow][fallback]")
{
  // Boundary pairs (exact corner behaviour).
  for (auto a : boundary_values<std::int16_t>())
    for (auto b : boundary_values<std::int16_t>())
      check_all<std::int16_t>(a, b);
  for (auto a : boundary_values<std::uint16_t>())
    for (auto b : boundary_values<std::uint16_t>())
      check_all<std::uint16_t>(a, b);

  // Deterministic random sweep across the full 16-bit range.
  std::mt19937 rng(20260613);
  std::uniform_int_distribution<int> d(-40000, 70000);
  for (int i = 0; i < 200000; ++i)
  {
    int a = d(rng), b = d(rng);
    check_all<std::int16_t>(static_cast<std::int16_t>(a), static_cast<std::int16_t>(b));
    check_all<std::uint16_t>(static_cast<std::uint16_t>(a), static_cast<std::uint16_t>(b));
  }
}

TEST_CASE("overflow fallbacks: 32/64-bit boundary + random vs builtins", "[overflow][fallback]")
{
  for (auto a : boundary_values<std::int32_t>())
    for (auto b : boundary_values<std::int32_t>())
      check_all<std::int32_t>(a, b);
  for (auto a : boundary_values<std::int64_t>())
    for (auto b : boundary_values<std::int64_t>())
      check_all<std::int64_t>(a, b);

  // The unsigned 64-bit multiply fallback uses a hand-rolled high/low split —
  // hammer it at the 2^32 split point and across the full width.
  std::mt19937_64 rng(0xC0FFEE);
  std::vector<std::uint64_t> seeds = {
    0, 1, 2, 3, 0xFFFFFFFFull, 0x1'0000'0000ull, 0x1'0000'0001ull,
    3037000499ull, 3037000500ull, 4294967296ull,
    1ull << 40, 1ull << 50, 1ull << 63,
    0x7FFFFFFFFFFFFFFFull, 0x8000000000000000ull, ~0ull,
  };
  for (int i = 0; i < 300000; ++i) seeds.push_back(rng());
  for (std::uint64_t b : {0ull, 1ull, 2ull, 3ull, 0xFFFFFFFFull, 0x1'0000'0000ull,
                          1ull << 32, 1ull << 40, ~0ull, 0x8000000000000000ull})
    for (std::uint64_t a : seeds)
    {
      std::uint64_t rr, rn;
      bool or_ = __builtin_mul_overflow(a, b, &rr);
      bool on  = non_builtin_mul_overflow(a, b, &rn);
      REQUIRE(or_ == on);
      if (!or_) REQUIRE(rr == rn);
    }

  // Signed 64-bit subtraction near INT64_MIN — the negate-before-subtract trap.
  constexpr std::int64_t i64min = std::numeric_limits<std::int64_t>::min();
  for (auto a : boundary_values<std::int64_t>())
  {
    std::int64_t rr, rn;
    bool or_ = __builtin_sub_overflow(a, i64min, &rr);
    bool on  = non_builtin_sub_overflow<std::int64_t>(a, i64min, &rn);
    REQUIRE(or_ == on);
    if (!or_) REQUIRE(rr == rn);
  }
}

TEST_CASE("overflow fallbacks are usable in constant expressions", "[overflow][fallback][constexpr]")
{
  constexpr auto add_ok = [] { std::int32_t r{}; return !non_builtin_add_overflow<std::int32_t>(2, 3, &r) && r == 5; }();
  constexpr auto sub_min = [] { std::int8_t r{}; return non_builtin_sub_overflow<std::int8_t>(5, std::numeric_limits<std::int8_t>::min(), &r); }();
  constexpr auto mul_ovf = [] { std::int8_t r{}; return non_builtin_mul_overflow<std::int8_t>(100, 100, &r); }();
  STATIC_REQUIRE(add_ok);
  STATIC_REQUIRE(sub_min);   // 5 - (-128) = 133 > 127 → overflow
  STATIC_REQUIRE(mul_ovf);
}

#endif // BOUND_HAVE_BUILTIN
