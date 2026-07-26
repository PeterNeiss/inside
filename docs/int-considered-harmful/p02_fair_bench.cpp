// A like-for-like benchmark: bound against native code that enforces the
// SAME guarantee, not against unchecked native code that enforces nothing.
//
// The four contenders, all computing the same sums over [0, 200]:
//   1. native unchecked   -- plain uint8_t. No range enforcement at all.
//   2. native clamped     -- std::clamp on every result.
//   3. native checked     -- hand-rolled bounded struct, throws out_of_range.
//   4. bound<checked>     -- the library's default policy.
//   5. bound<unsafe>      -- the library with checking switched off.
//
// Contenders 2, 3 and 4 provide the same guarantee. Those are the rows that
// belong in a fair comparison. 1 is included only to show what the guarantee
// costs in the first place.
//
//   g++ -std=c++23 -O2 -I ../../include p02_fair_bench.cpp -o p02 && ./p02

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <vector>

#include "bound/bound.hpp"

using namespace bnd;

// ---------------------------------------------------------------- helpers ---
template<typename T>
[[gnu::always_inline]] inline void sink(T&& v)
{
  asm volatile("" : : "r,m"(v) : "memory");
}

static constexpr int kN    = 4096;     // values per pass
static constexpr int kReps = 20000;    // passes

// A hand-rolled checked bounded integer: what you would write to get the
// guarantee bound gives you, in plain C++.
struct checked_u8
{
  std::uint8_t value{};
  checked_u8() = default;
  explicit checked_u8(int v)
  {
    if (v < 0 || v > 200) throw std::out_of_range("checked_u8");
    value = static_cast<std::uint8_t>(v);
  }
  friend checked_u8 operator+(checked_u8 a, checked_u8 b)
  {
    int sum = a.value + b.value;
    if (sum > 200) throw std::out_of_range("checked_u8 +");
    checked_u8 r;
    r.value = static_cast<std::uint8_t>(sum);
    return r;
  }
};

using bchecked = bound<{0, 200}, checked>;
using bunsafe  = bound<{0, 200}, unsafe>;

template<typename F>
static double time_ns_per_op(F&& f)
{
  // one warm-up pass, then the measured run
  f();
  auto t0 = std::chrono::steady_clock::now();
  f();
  auto t1 = std::chrono::steady_clock::now();
  auto ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
  return ns / (double(kN) * kReps);
}

int main()
{
  std::vector<int> a(kN), b(kN);
  for (int i = 0; i < kN; ++i)
  {
    a[i] = i % 90;          // sums stay <= 180, so no contender ever throws
    b[i] = (i * 7) % 90;
  }

  const double native = time_ns_per_op([&] {
    for (int r = 0; r < kReps; ++r)
      for (int i = 0; i < kN; ++i)
      {
        auto c = static_cast<std::uint8_t>(a[i] + b[i]);
        sink(c);
      }
  });

  const double clamped = time_ns_per_op([&] {
    for (int r = 0; r < kReps; ++r)
      for (int i = 0; i < kN; ++i)
      {
        auto c = static_cast<std::uint8_t>(std::clamp(a[i] + b[i], 0, 200));
        sink(c);
      }
  });

  const double checked_native = time_ns_per_op([&] {
    for (int r = 0; r < kReps; ++r)
      for (int i = 0; i < kN; ++i)
      {
        checked_u8 x(a[i]), y(b[i]);
        auto c = x + y;
        sink(c.value);
      }
  });

  const double bound_checked = time_ns_per_op([&] {
    for (int r = 0; r < kReps; ++r)
      for (int i = 0; i < kN; ++i)
      {
        bchecked x(a[i]), y(b[i]);
        bchecked c = x + y;      // widened sum narrowed back: the checked store
        sink(c.raw());
      }
  });

  const double bound_unsafe = time_ns_per_op([&] {
    for (int r = 0; r < kReps; ++r)
      for (int i = 0; i < kN; ++i)
      {
        bunsafe x(a[i]), y(b[i]);
        bunsafe c = x + y;
        sink(c.raw());
      }
  });

  auto row = [&](const char* name, double t, bool enforces) {
    std::printf("%-22s %8.3f %9.1f%% %10s %8s\n", name, t,
                100.0 * native / t,
                enforces ? "yes" : "no",
                enforces ? "" : "--");
  };

  std::printf("%-22s %8s %10s %10s\n", "implementation", "ns/op",
              "vs native", "enforces?");
  std::printf("--------------------------------------------------------\n");
  row("native unchecked",  native,         false);
  row("native clamped",    clamped,        true);
  row("native checked",    checked_native, true);
  row("bound<checked>",    bound_checked,  true);
  row("bound<unsafe>",     bound_unsafe,   false);

  std::printf("\nfair comparison (same guarantee, vs best safe native):\n");
  const double best_safe = std::min(clamped, checked_native);
  std::printf("  best safe native   = %.3f ns/op\n", best_safe);
  std::printf("  bound<checked>     = %.3f ns/op  -> %.1f%% of safe native\n",
              bound_checked, 100.0 * best_safe / bound_checked);
  return 0;
}
