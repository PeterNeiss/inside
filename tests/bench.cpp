#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

#include "bound/bound.hpp"

using namespace bnd;

//---------------------------------------------------------------------------
// compiler barriers
//---------------------------------------------------------------------------
template <typename T>
inline void do_not_optimize(T const& value)
{
  asm volatile("" : : "r,m"(value) : "memory");
}

inline void clobber() { asm volatile("" : : : "memory"); }

//---------------------------------------------------------------------------
// timing helper — returns nanoseconds per iteration
//---------------------------------------------------------------------------
template <typename F>
double measure(std::size_t iterations, F&& fn)
{
  // warmup
  for (std::size_t i = 0; i < iterations / 10; ++i)
    fn();
  clobber();

  auto start = std::chrono::high_resolution_clock::now();
  for (std::size_t i = 0; i < iterations; ++i)
    fn();
  auto end = std::chrono::high_resolution_clock::now();

  double ns = std::chrono::duration<double, std::nano>(end - start).count();
  return ns / static_cast<double>(iterations);
}

//---------------------------------------------------------------------------
// checked_u8: minimal runtime-checked bounded integer [0, 200]
//---------------------------------------------------------------------------
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
    checked_u8 r; r.value = static_cast<std::uint8_t>(sum); return r;
  }

  friend checked_u8 operator*(checked_u8 a, checked_u8 b)
  {
    int prod = a.value * b.value;
    if (prod > 200) throw std::out_of_range("checked_u8 *");
    checked_u8 r; r.value = static_cast<std::uint8_t>(prod); return r;
  }
};

//---------------------------------------------------------------------------
// result table printing
//---------------------------------------------------------------------------
struct row { std::string scenario; double native, clamped, checked, bounded; };

void print_table(std::vector<row> const& rows)
{
  std::cout << std::left << std::setw(18) << "Scenario"
            << std::right
            << std::setw(12) << "native"
            << std::setw(12) << "clamped"
            << std::setw(12) << "checked"
            << std::setw(12) << "bound"
            << "\n";
  std::cout << std::string(66, '-') << "\n";
  for (auto const& r : rows)
  {
    std::cout << std::left << std::setw(18) << r.scenario
              << std::right << std::fixed << std::setprecision(2)
              << std::setw(10) << r.native << " ns"
              << std::setw(10) << r.clamped << " ns"
              << std::setw(10) << r.checked << " ns"
              << std::setw(10) << r.bounded << " ns"
              << "\n";
  }
}

//---------------------------------------------------------------------------
// benchmarks
//---------------------------------------------------------------------------
static constexpr std::size_t N = 5'000'000;

using u200 = bound<{0, 200}>;

row bench_construct()
{
  row r{"construct"};

  r.native = measure(N, [i = std::uint8_t{0}]() mutable {
    auto v = static_cast<std::uint8_t>(i % 201);
    do_not_optimize(v);
    ++i;
  });

  r.clamped = measure(N, [i = 0]() mutable {
    auto v = static_cast<std::uint8_t>(std::clamp(i % 256, 0, 200));
    do_not_optimize(v);
    ++i;
  });

  r.checked = measure(N, [i = 0]() mutable {
    checked_u8 v(i % 201);
    do_not_optimize(v.value);
    ++i;
  });

  r.bounded = measure(N, [i = 0]() mutable {
    u200 v(i % 201);
    do_not_optimize(v.Raw);
    ++i;
  });

  return r;
}

row bench_add()
{
  row r{"addition"};
  constexpr int a_val = 30, b_val = 50;

  r.native = measure(N, [] {
    std::uint8_t a = a_val, b = b_val;
    auto c = static_cast<std::uint8_t>(a + b);
    do_not_optimize(c);
  });

  r.clamped = measure(N, [] {
    std::uint8_t a = a_val, b = b_val;
    auto c = static_cast<std::uint8_t>(std::clamp(a + b, 0, 200));
    do_not_optimize(c);
  });

  r.checked = measure(N, [] {
    checked_u8 a(a_val), b(b_val);
    auto c = a + b;
    do_not_optimize(c.value);
  });

  r.bounded = measure(N, [] {
    u200 a(a_val), b(b_val);
    auto c = a + b;
    do_not_optimize(c.Raw);
  });

  return r;
}

row bench_mul()
{
  row r{"multiply"};
  constexpr int a_val = 7, b_val = 14;

  r.native = measure(N, [] {
    std::uint8_t a = a_val, b = b_val;
    auto c = static_cast<std::uint8_t>(a * b);
    do_not_optimize(c);
  });

  r.clamped = measure(N, [] {
    std::uint8_t a = a_val, b = b_val;
    auto c = static_cast<std::uint8_t>(std::clamp(a * b, 0, 200));
    do_not_optimize(c);
  });

  r.checked = measure(N, [] {
    checked_u8 a(a_val), b(b_val);
    auto c = a * b;
    do_not_optimize(c.value);
  });

  r.bounded = measure(N, [] {
    u200 a(a_val), b(b_val);
    auto c = a * b;
    do_not_optimize(c.Raw);
  });

  return r;
}

row bench_accumulate()
{
  row r{"accumulate"};
  constexpr std::size_t SZ = 1000;
  constexpr std::size_t ITERS = N / SZ;

  // prepare native vector
  std::vector<std::uint8_t> nv(SZ);
  for (std::size_t i = 0; i < SZ; ++i)
    nv[i] = static_cast<std::uint8_t>(i % 5); // small values to avoid overflow

  // prepare checked vector
  std::vector<checked_u8> cv(SZ);
  for (std::size_t i = 0; i < SZ; ++i)
    cv[i] = checked_u8(static_cast<int>(i % 5));

  // prepare bound vector
  std::vector<u200> bv(SZ);
  for (std::size_t i = 0; i < SZ; ++i)
    bv[i] = static_cast<int>(i % 5);

  r.native = measure(ITERS, [&] {
    std::uint16_t sum = 0;
    for (auto v : nv) sum = static_cast<std::uint16_t>(sum + v);
    do_not_optimize(sum);
  });

  r.clamped = measure(ITERS, [&] {
    int sum = 0;
    for (auto v : nv) sum = std::clamp(sum + v, 0, 200 * 1000);
    do_not_optimize(sum);
  });

  r.checked = measure(ITERS, [&] {
    int acc = 0;
    for (auto v : cv)
    {
      acc += v.value;
      if (acc < 0 || acc > 200'000) throw std::out_of_range("checked accumulate");
    }
    do_not_optimize(acc);
  });

  using u200k = bound<{0, 200'000}>;
  r.bounded = measure(ITERS, [&] {
    u200k sum(0);
    for (auto v : bv) sum = sum + v;
    do_not_optimize(sum.Raw);
  });

  return r;
}

//---------------------------------------------------------------------------
// main
//---------------------------------------------------------------------------
int main()
{
  std::cout << "bound<> benchmark (" << N << " iterations)\n\n";

  std::vector<row> results;
  results.push_back(bench_construct());
  results.push_back(bench_add());
  results.push_back(bench_mul());
  results.push_back(bench_accumulate());

  print_table(results);
  return 0;
}
