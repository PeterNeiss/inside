// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Where inside's compile-time range information actually pays: a CHAIN of
// operations on values that are already typed.
//
// A native checked type must re-validate after every step, because each
// intermediate has to fit back into the checked type. inside lets the
// intermediates widen -- their grids are computed at compile time and provably
// cannot overflow -- so the only check is the final store.
//
//   4 inputs in [0,50]  ->  sum in [0,200]
//     native checked : 3 checks (one per addition)
//     inside<checked> : 0 checks
//
// Zero, not one: four values in [0,50] sum to [0,200], which is exactly the
// declared range of `total`, so even the final store is proved safe at compile
// time. The emitted code is the unchecked native version's two adds and an lea:
//
//     endbr64; add %esi,%edi; add %edx,%edi; lea (%rdi,%rcx,1),%eax; ret
//
// Widen `total` to [0,199] and the store check reappears -- the proof is
// genuinely being done, not skipped.
//
//   g++ -std=c++23 -O2 -I ../../include p03_check_elision.cpp -o p03 && ./p03

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <vector>

#include <beman/inside/inside.hpp>

using namespace beman::inside;

template<typename T>
[[gnu::always_inline]] inline void sink(T&& v)
{
  asm volatile("" : : "r,m"(v) : "memory");
}

static constexpr int kN    = 4096;
static constexpr int kReps = 20000;

// The natural native spelling: a checked type that validates on every op.
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
    int sum = a.value + b.value;                 // must check: could exceed 200
    if (sum > 200) throw std::out_of_range("checked_u8 +");
    checked_u8 r;
    r.value = static_cast<std::uint8_t>(sum);
    return r;
  }
};

using part  = inside<{0, 50}, checked>;    // an input
using total = inside<{0, 200}, checked>;   // the declared result range

template<typename F>
static double time_ns_per_op(F&& f)
{
  f();
  auto t0 = std::chrono::steady_clock::now();
  f();
  auto t1 = std::chrono::steady_clock::now();
  return std::chrono::duration<double, std::nano>(t1 - t0).count()
       / (double(kN) * kReps);
}

int main()
{
  // Pre-converted inputs: the per-element construction cost is paid once,
  // outside the measured loop, exactly as it would be in real code that
  // keeps its values typed.
  std::vector<checked_u8> na(kN), nb(kN), nc(kN), nd(kN);
  std::vector<part>       ba(kN), bb(kN), bc(kN), bd(kN);

  for (int i = 0; i < kN; ++i)
  {
    int v1 = i % 50, v2 = (i * 7) % 50, v3 = (i * 13) % 50, v4 = (i * 29) % 50;
    na[i] = checked_u8(v1); nb[i] = checked_u8(v2);
    nc[i] = checked_u8(v3); nd[i] = checked_u8(v4);
    ba[i] = part(v1); bb[i] = part(v2);
    bc[i] = part(v3); bd[i] = part(v4);
  }

  const double native = time_ns_per_op([&] {
    for (int r = 0; r < kReps; ++r)
      for (int i = 0; i < kN; ++i)
      {
        checked_u8 s = na[i] + nb[i] + nc[i] + nd[i];   // 3 checks
        sink(s.value);
      }
  });

  const double ins_chain = time_ns_per_op([&] {
    for (int r = 0; r < kReps; ++r)
      for (int i = 0; i < kN; ++i)
      {
        total s = ba[i] + bb[i] + bc[i] + bd[i];        // 0 checks
        sink(s.raw());
      }
  });

  // For reference: the same chain with no checking anywhere.
  std::vector<std::uint8_t> ra(kN), rb(kN), rc(kN), rd(kN);
  for (int i = 0; i < kN; ++i)
  {
    ra[i] = na[i].value; rb[i] = nb[i].value;
    rc[i] = nc[i].value; rd[i] = nd[i].value;
  }
  const double raw = time_ns_per_op([&] {
    for (int r = 0; r < kReps; ++r)
      for (int i = 0; i < kN; ++i)
      {
        auto s = static_cast<std::uint8_t>(ra[i] + rb[i] + rc[i] + rd[i]);
        sink(s);
      }
  });

  std::printf("%-24s %8s %10s %8s\n", "implementation", "ns/op", "vs native", "checks");
  std::printf("-----------------------------------------------------------\n");
  std::printf("%-24s %8.3f %9.1f%% %8s\n", "native unchecked", raw, 100.0 * raw / raw, "0");
  std::printf("%-24s %8.3f %9.1f%% %8s\n", "native checked",   native, 100.0 * raw / native, "3");
  std::printf("%-24s %8.3f %9.1f%% %8s\n", "inside<checked>",   ins_chain, 100.0 * raw / ins_chain, "0");
  std::printf("\ninside<checked> vs native checked: %.1f%% "
              "(>100%% means inside is faster)\n", 100.0 * native / ins_chain);
  return 0;
}
