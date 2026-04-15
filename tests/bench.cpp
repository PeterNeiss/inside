#include <algorithm>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "ctrack.hpp"
#include "bound/bound.hpp"

using namespace bnd;

//---------------------------------------------------------------------------
// compiler barrier
//---------------------------------------------------------------------------
template <typename T>
inline void do_not_optimize(T const& value)
{
  asm volatile("" : : "r,m"(value) : "memory");
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
// benchmarks
//---------------------------------------------------------------------------
static constexpr std::size_t N = 5'000'000;

using u200 = bound<{0, 200}>;

void bench_construct()
{
  for (std::size_t i = 0; i < N; ++i)
  {
    { CTRACK_NAME("construct native");
      auto v = static_cast<std::uint8_t>(i % 201);
      do_not_optimize(v); }

    { CTRACK_NAME("construct clamped");
      auto v = static_cast<std::uint8_t>(std::clamp(i % std::size_t{256}, std::size_t{0}, std::size_t{200}));
      do_not_optimize(v); }

    { CTRACK_NAME("construct checked");
      checked_u8 v(static_cast<int>(i % 201));
      do_not_optimize(v.value); }

    { CTRACK_NAME("construct bound");
      u200 v(static_cast<int>(i % 201));
      do_not_optimize(v.Raw); }
  }
}

void bench_add()
{
  constexpr int a_val = 30, b_val = 50;

  for (std::size_t i = 0; i < N; ++i)
  {
    { CTRACK_NAME("add native");
      std::uint8_t a = a_val, b = b_val;
      auto c = static_cast<std::uint8_t>(a + b);
      do_not_optimize(c); }

    { CTRACK_NAME("add clamped");
      std::uint8_t a = a_val, b = b_val;
      auto c = static_cast<std::uint8_t>(std::clamp(a + b, 0, 200));
      do_not_optimize(c); }

    { CTRACK_NAME("add checked");
      checked_u8 a(a_val), b(b_val);
      auto c = a + b;
      do_not_optimize(c.value); }

    { CTRACK_NAME("add bound");
      u200 a(a_val), b(b_val);
      auto c = a + b;
      do_not_optimize(c.Raw); }
  }
}

void bench_mul()
{
  constexpr int a_val = 7, b_val = 14;

  for (std::size_t i = 0; i < N; ++i)
  {
    { CTRACK_NAME("mul native");
      std::uint8_t a = a_val, b = b_val;
      auto c = static_cast<std::uint8_t>(a * b);
      do_not_optimize(c); }

    { CTRACK_NAME("mul clamped");
      std::uint8_t a = a_val, b = b_val;
      auto c = static_cast<std::uint8_t>(std::clamp(a * b, 0, 200));
      do_not_optimize(c); }

    { CTRACK_NAME("mul checked");
      checked_u8 a(a_val), b(b_val);
      auto c = a * b;
      do_not_optimize(c.value); }

    { CTRACK_NAME("mul bound");
      u200 a(a_val), b(b_val);
      auto c = a * b;
      do_not_optimize(c.Raw); }
  }
}

void bench_accumulate()
{
  constexpr std::size_t SZ = 1000;
  constexpr std::size_t ITERS = N / SZ;

  std::vector<std::uint8_t> nv(SZ);
  std::vector<checked_u8> cv(SZ);
  std::vector<u200> bv(SZ);
  for (std::size_t i = 0; i < SZ; ++i)
  {
    nv[i] = static_cast<std::uint8_t>(i % 5);
    cv[i] = checked_u8(static_cast<int>(i % 5));
    bv[i] = static_cast<int>(i % 5);
  }

  using u200k = bound<{0, 200'000}>;

  for (std::size_t i = 0; i < ITERS; ++i)
  {
    { CTRACK_NAME("accum native");
      std::uint16_t sum = 0;
      for (auto v : nv) sum = static_cast<std::uint16_t>(sum + v);
      do_not_optimize(sum); }

    { CTRACK_NAME("accum clamped");
      int sum = 0;
      for (auto v : nv) sum = std::clamp(sum + v, 0, 200 * 1000);
      do_not_optimize(sum); }

    { CTRACK_NAME("accum checked");
      int acc = 0;
      for (auto v : cv)
      {
        acc += v.value;
        if (acc < 0 || acc > 200'000) throw std::out_of_range("checked accumulate");
      }
      do_not_optimize(acc); }

    { CTRACK_NAME("accum bound");
      u200k sum(0);
      for (auto v : bv) sum += v;
      do_not_optimize(sum.Raw); }
  }
}

//---------------------------------------------------------------------------
// main
//---------------------------------------------------------------------------
int main()
{
  std::cout << "bound<> benchmark (" << N << " iterations)\n\n";

  bench_construct();
  bench_add();
  bench_mul();
  bench_accumulate();

  ctrack::result_print();
  return 0;
}
