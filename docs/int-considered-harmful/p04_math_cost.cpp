// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// What correct rounding costs. beman::inside::math returns the correctly
// rounded point of its OUTPUT grid, so its cost follows that grid: onto the
// grid deduced from a 2^-14 input it often keeps pace with <cmath>; onto a
// grid as fine as a double's own significand it is several times slower.
// std:: is neither correctly rounded nor reproducible across libraries.
//
//   g++ -std=c++23 -O3 -mfma -DNDEBUG -I ../../include p04_math_cost.cpp -o p04
//   taskset -c 2 ./p04

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

#include <beman/inside/inside.hpp>
#include <beman/inside/cmath.hpp>

using namespace beman::inside;

namespace
{
  volatile double g_sink;

  template <typename F>
  double ns_per_op(F f)
  {
    constexpr long n = 3'000'000;
    const auto t0 = std::chrono::steady_clock::now();
    for (long i = 0; i < n; ++i) f(i);
    return std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count() / n;
  }

  using angle = inside<{{-8, 8}, per<16384>}, round_nearest>;          // inputs
  using pos   = inside<{{rational{1, 256}, 256}, per<256>}, round_nearest>;
  using unit4 = inside<{{0, 4}, per<65536>}, round_nearest>;

  // Outputs as fine as a double: every one has close to 2^53 slots.
  using sin_fine  = math::amp<(std::uint64_t{1} << 52)>;                                      // 2^-52
  using exp_fine  = inside<{{0, 4096}, per<(std::uint64_t{1} << 40)>}, round_nearest>;  // 2^-40
  using log_fine  = inside<{{-8, 8}, per<(std::uint64_t{1} << 48)>}, round_nearest>;    // 2^-48
  using sqrt_fine = inside<{{0, 2}, per<(std::uint64_t{1} << 51)>}, round_nearest>;     // 2^-51
}

int main()
{
  constexpr int m = 4096, mask = m - 1;
  std::vector<angle> a; std::vector<pos> p; std::vector<unit4> s;
  std::vector<double> da, dp, ds;
  for (int j = 0; j < m; ++j)
  {
    const rational qa{(j * 16 & 0xFFFF) - 32768, 16384}, qp{(j * 16 & 0xFFFF) % 65535 + 1, 256},
                   qs{j * 16 & 0xFFFF, 16384};
    a.push_back(angle{qa}); p.push_back(pos{qp}); s.push_back(unit4{qs});
    da.push_back(static_cast<double>(qa)); dp.push_back(static_cast<double>(qp));
    ds.push_back(static_cast<double>(qs));
  }

  std::printf("%-6s %10s %18s %18s\n", "fn", "std ns", "deduced grid ns", "double-fine ns");
  std::printf("%-6s %10.2f %18.2f %18.2f\n", "sin",
              ns_per_op([&](long i) { g_sink = std::sin(da[i & mask]); }),
              ns_per_op([&](long i) { g_sink = math::sin(a[i & mask]).raw(); }),
              ns_per_op([&](long i) { g_sink = math::sin_into<sin_fine>(a[i & mask]).raw(); }));
  std::printf("%-6s %10.2f %18.2f %18.2f\n", "exp",
              ns_per_op([&](long i) { g_sink = std::exp(da[i & mask]); }),
              ns_per_op([&](long i) { g_sink = math::exp(a[i & mask]).raw(); }),
              ns_per_op([&](long i) { g_sink = math::exp_into<exp_fine>(a[i & mask]).raw(); }));
  std::printf("%-6s %10.2f %18.2f %18.2f\n", "log",
              ns_per_op([&](long i) { g_sink = std::log(dp[i & mask]); }),
              ns_per_op([&](long i) { g_sink = math::log(p[i & mask]).raw(); }),
              ns_per_op([&](long i) { g_sink = math::log_into<log_fine>(p[i & mask]).raw(); }));
  std::printf("%-6s %10.2f %18.2f %18.2f\n", "sqrt",
              ns_per_op([&](long i) { g_sink = std::sqrt(ds[i & mask]); }),
              ns_per_op([&](long i) { g_sink = math::sqrt(s[i & mask]).raw(); }),
              ns_per_op([&](long i) { g_sink = math::sqrt_into<sqrt_fine>(s[i & mask]).raw(); }));
  return 0;
}
