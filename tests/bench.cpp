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

  // All use the same element type: uint32_t / bound<{0, 200'000}>
  using u200k         = bound<{0, 200'000}>;
  using u200k_checked = bound<{0, 200'000}, checked>;

  std::vector<std::uint32_t> nv(SZ);
  std::vector<u200k> bv(SZ);
  std::vector<u200k_checked> bv_checked(SZ);
  for (std::size_t i = 0; i < SZ; ++i)
  {
    nv[i] = static_cast<std::uint32_t>(i % 5);
    bv[i] = static_cast<int>(i % 5);
    bv_checked[i] = static_cast<int>(i % 5);
  }

  for (std::size_t i = 0; i < ITERS; ++i)
  {
    { CTRACK_NAME("accum native");
      std::uint32_t sum = 0;
      for (auto v : nv) sum += v;
      do_not_optimize(sum); }

    { CTRACK_NAME("accum bound");
      u200k sum(0);
      for (auto v : bv) sum += v;
      do_not_optimize(sum.Raw); }

    { CTRACK_NAME("accum bound<checked>");
      u200k_checked sum(0);
      for (auto v : bv_checked) sum += v;
      do_not_optimize(sum.Raw); }
  }
}

void bench_int_vs_bound()
{
  constexpr std::size_t SZ = 1000;
  constexpr std::size_t ITERS = N / SZ;

  // int (signed 32-bit) vs bound<{-100'000, 100'000}> (uint32_t storage)
  // Uses negative numbers throughout
  using s100k = bound<{-100'000, 100'000}>;

  std::vector<int> iv(SZ);
  std::vector<s100k> bv(SZ);
  for (std::size_t i = 0; i < SZ; ++i)
  {
    int val = static_cast<int>(i % 11) - 5; // -5 to +5
    iv[i] = val;
    bv[i] = val;
  }

  for (std::size_t i = 0; i < ITERS; ++i)
  {
    { CTRACK_NAME("int construct");
      int v = static_cast<int>(i % 201) - 100; // -100 to +100
      do_not_optimize(v); }

    { CTRACK_NAME("bound construct");
      s100k v(static_cast<int>(i % 201) - 100);
      do_not_optimize(v.Raw); }

    { CTRACK_NAME("int add");
      int a = -300, b = 500;
      int c = a + b;
      do_not_optimize(c); }

    { CTRACK_NAME("bound add");
      s100k a(-300), b(500);
      auto c = a + b;
      do_not_optimize(c.Raw); }

    { CTRACK_NAME("int mul");
      int a = -7, b = 14;
      int c = a * b;
      do_not_optimize(c); }

    { CTRACK_NAME("bound mul");
      s100k a(-7), b(14);
      auto c = a * b;
      do_not_optimize(c.Raw); }

    { CTRACK_NAME("int accum");
      int sum = 0;
      for (auto v : iv) sum += v;
      do_not_optimize(sum); }

    { CTRACK_NAME("bound accum");
      s100k sum(0);
      for (auto v : bv) sum += v;
      do_not_optimize(sum.Raw); }
  }
}

void bench_fixed_point()
{
  constexpr std::size_t SZ = 1000;
  constexpr std::size_t ITERS = N / SZ;

  // Fixed-point 8.8: native int16_t with manual shift vs bound<{{0, 255}, 1.0/256}>
  using fp = bound<{{0, 255}, 1.0/256}>;

  std::vector<std::int32_t> nv(SZ);
  std::vector<fp> bv(SZ);
  for (std::size_t i = 0; i < SZ; ++i)
  {
    int val = static_cast<int>(i % 5);
    nv[i] = val << 8; // fixed-point: value * 256
    bv[i] = val;
  }

  for (std::size_t i = 0; i < ITERS; ++i)
  {
    { CTRACK_NAME("fixed native construct");
      std::int32_t v = static_cast<std::int32_t>((i % 201)) << 8;
      do_not_optimize(v); }

    { CTRACK_NAME("fixed bound construct");
      fp v(static_cast<int>(i % 201));
      do_not_optimize(v.Raw); }

    { CTRACK_NAME("fixed native add");
      std::int32_t a = 30 << 8, b = 50 << 8;
      std::int32_t c = a + b;
      do_not_optimize(c); }

    { CTRACK_NAME("fixed bound add");
      fp a(30), b(50);
      auto c = a + b;
      do_not_optimize(c.Raw); }

    { CTRACK_NAME("fixed native accum");
      std::int32_t sum = 0;
      for (auto v : nv) sum += v;
      do_not_optimize(sum); }

    { CTRACK_NAME("fixed bound accum");
      fp sum(0);
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
  bench_int_vs_bound();
  bench_fixed_point();

  ctrack::result_print();
  return 0;
}
