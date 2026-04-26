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

void bench_div()
{
  constexpr int a_val = 100, b_val = 7;

  for (std::size_t i = 0; i < N; ++i)
  {
    { CTRACK_NAME("div native");
      std::uint8_t a = a_val, b = b_val;
      auto c = static_cast<std::uint8_t>(a / b);
      do_not_optimize(c); }

    { CTRACK_NAME("div bound (rational)");
      u200 a(a_val), b(b_val);
      auto c = a / b;
      do_not_optimize(c->Raw); }

    { CTRACK_NAME("div bound (integer)");
      u200 a(a_val), b(b_val);
      auto c = div(a, b, make_policy<ignore_round>());
      do_not_optimize(c->Raw); }
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

void bench_round_nearest()
{
  // Compare truncation (ignore_round) vs round_nearest for float->bound assignment
  // Celsius: -40 to 60 in 0.5 steps
  using celsius_trunc = bound<{{-40, 60}, 0.5}, ignore_round>;
  using celsius_round = bound<{{-40, 60}, 0.5}, round_nearest>;

  for (std::size_t i = 0; i < N; ++i)
  {
    double val = -40.0 + static_cast<double>(i % 200) * 0.5 + 0.3;

    { CTRACK_NAME("assign truncate");
      celsius_trunc c = val;
      do_not_optimize(c.Raw); }

    { CTRACK_NAME("assign round_nearest");
      celsius_round c = val;
      do_not_optimize(c.Raw); }
  }
}

//---------------------------------------------------------------------------
// STL algorithm benchmarks
//---------------------------------------------------------------------------
namespace rng = std::ranges;

using u255 = bound<{0, 255}>;
using s9k  = bound<{-500, 9000}>;

void bench_sort_algo()
{
  constexpr std::size_t SZ = 10'000;
  constexpr std::size_t ITERS = N / SZ;

  // prepare identical data
  std::vector<std::int16_t> nv(SZ);
  std::vector<s9k> bv(SZ);
  for (std::size_t i = 0; i < SZ; ++i)
  {
    auto val = static_cast<std::int16_t>((i * 7 + 13) % 9501 - 500);
    nv[i] = val;
    bv[i] = static_cast<int>(val);
  }

  auto nv_copy = nv;
  auto bv_copy = bv;

  for (std::size_t i = 0; i < ITERS; ++i)
  {
    { CTRACK_NAME("sort native");
      nv = nv_copy;
      rng::sort(nv);
      do_not_optimize(nv[0]); }

    { CTRACK_NAME("sort bound");
      bv = bv_copy;
      rng::sort(bv);
      do_not_optimize(bv[0].Raw); }
  }
}

void bench_find_algo()
{
  constexpr std::size_t SZ = 10'000;
  constexpr std::size_t ITERS = N / SZ;

  std::vector<std::int16_t> nv(SZ);
  std::vector<s9k> bv(SZ);
  for (std::size_t i = 0; i < SZ; ++i)
  {
    auto val = static_cast<std::int16_t>(i % 9501 - 500);
    nv[i] = val;
    bv[i] = static_cast<int>(val);
  }

  auto target_n = static_cast<std::int16_t>(7000);
  s9k target_b(7000);

  for (std::size_t i = 0; i < ITERS; ++i)
  {
    { CTRACK_NAME("find native");
      auto it = rng::find(nv, target_n);
      do_not_optimize(*it); }

    { CTRACK_NAME("find bound");
      auto it = rng::find(bv, target_b);
      do_not_optimize(it->Raw); }

    { CTRACK_NAME("count native");
      auto c = rng::count(nv, static_cast<std::int16_t>(42));
      do_not_optimize(c); }

    { CTRACK_NAME("count bound");
      auto c = rng::count(bv, s9k{42});
      do_not_optimize(c); }
  }
}

void bench_transform_algo()
{
  constexpr std::size_t SZ = 10'000;
  constexpr std::size_t ITERS = N / SZ;

  std::vector<std::uint8_t> nv(SZ);
  std::vector<u255> bv(SZ);
  std::vector<std::uint8_t> nout(SZ);
  std::vector<u255> bout(SZ);

  for (std::size_t i = 0; i < SZ; ++i)
  {
    nv[i] = static_cast<std::uint8_t>(i % 250);
    bv[i] = static_cast<int>(i % 250);
  }

  for (std::size_t i = 0; i < ITERS; ++i)
  {
    { CTRACK_NAME("transform native");
      std::transform(nv.begin(), nv.end(), nout.begin(),
        [](std::uint8_t v) -> std::uint8_t { return static_cast<std::uint8_t>(v + 1); });
      do_not_optimize(nout[0]); }

    { CTRACK_NAME("transform bound");
      std::transform(bv.begin(), bv.end(), bout.begin(),
        [](u255 v) { v += 1; return v; });
      do_not_optimize(bout[0].Raw); }
  }
}

void bench_minmax_algo()
{
  constexpr std::size_t SZ = 10'000;
  constexpr std::size_t ITERS = N / SZ;

  std::vector<std::int16_t> nv(SZ);
  std::vector<s9k> bv(SZ);
  for (std::size_t i = 0; i < SZ; ++i)
  {
    auto val = static_cast<std::int16_t>((i * 7 + 13) % 9501 - 500);
    nv[i] = val;
    bv[i] = static_cast<int>(val);
  }

  for (std::size_t i = 0; i < ITERS; ++i)
  {
    { CTRACK_NAME("min_element native");
      auto it = rng::min_element(nv);
      do_not_optimize(*it); }

    { CTRACK_NAME("min_element bound");
      auto it = rng::min_element(bv);
      do_not_optimize(it->Raw); }

    { CTRACK_NAME("max_element native");
      auto it = rng::max_element(nv);
      do_not_optimize(*it); }

    { CTRACK_NAME("max_element bound");
      auto it = rng::max_element(bv);
      do_not_optimize(it->Raw); }
  }
}

void bench_lower_bound_algo()
{
  constexpr std::size_t SZ = 10'000;
  constexpr std::size_t ITERS = N / SZ;

  std::vector<std::int16_t> nv(SZ);
  std::vector<s9k> bv(SZ);
  for (std::size_t i = 0; i < SZ; ++i)
  {
    auto val = static_cast<std::int16_t>(static_cast<int>(i) - 500);
    nv[i] = val;
    bv[i] = static_cast<int>(val);
  }
  // already sorted

  for (std::size_t i = 0; i < ITERS; ++i)
  {
    auto target = static_cast<std::int16_t>(static_cast<int>(i % 9501) - 500);

    { CTRACK_NAME("lower_bound native");
      auto it = rng::lower_bound(nv, target);
      do_not_optimize(*it); }

    { CTRACK_NAME("lower_bound bound");
      auto it = rng::lower_bound(bv, s9k{static_cast<int>(target)});
      do_not_optimize(it->Raw); }
  }
}

void bench_std_sort()
{
  constexpr std::size_t SZ = 10'000;
  constexpr std::size_t ITERS = N / SZ;

  std::vector<std::int16_t> nv(SZ);
  std::vector<s9k> bv(SZ);
  for (std::size_t i = 0; i < SZ; ++i)
  {
    auto val = static_cast<std::int16_t>((i * 7 + 13) % 9501 - 500);
    nv[i] = val;
    bv[i] = static_cast<int>(val);
  }

  auto nv_copy = nv;
  auto bv_copy = bv;

  for (std::size_t i = 0; i < ITERS; ++i)
  {
    { CTRACK_NAME("std::sort native");
      nv = nv_copy;
      std::sort(nv.begin(), nv.end());
      do_not_optimize(nv[0]); }

    { CTRACK_NAME("std::sort bound");
      bv = bv_copy;
      std::sort(bv.begin(), bv.end());
      do_not_optimize(bv[0].Raw); }
  }
}

void bench_nth_element()
{
  constexpr std::size_t SZ = 10'000;
  constexpr std::size_t ITERS = N / SZ;

  std::vector<std::int16_t> nv(SZ);
  std::vector<s9k> bv(SZ);
  for (std::size_t i = 0; i < SZ; ++i)
  {
    auto val = static_cast<std::int16_t>((i * 7 + 13) % 9501 - 500);
    nv[i] = val;
    bv[i] = static_cast<int>(val);
  }

  auto nv_copy = nv;
  auto bv_copy = bv;
  auto nth = static_cast<long>(SZ / 2);

  for (std::size_t i = 0; i < ITERS; ++i)
  {
    { CTRACK_NAME("nth_element native");
      nv = nv_copy;
      std::nth_element(nv.begin(), nv.begin() + nth, nv.end());
      do_not_optimize(nv[static_cast<std::size_t>(nth)]); }

    { CTRACK_NAME("nth_element bound");
      bv = bv_copy;
      std::nth_element(bv.begin(), bv.begin() + nth, bv.end());
      do_not_optimize(bv[static_cast<std::size_t>(nth)].Raw); }
  }
}

void bench_partition()
{
  constexpr std::size_t SZ = 10'000;
  constexpr std::size_t ITERS = N / SZ;

  std::vector<std::int16_t> nv(SZ);
  std::vector<s9k> bv(SZ);
  for (std::size_t i = 0; i < SZ; ++i)
  {
    auto val = static_cast<std::int16_t>((i * 7 + 13) % 9501 - 500);
    nv[i] = val;
    bv[i] = static_cast<int>(val);
  }

  auto nv_copy = nv;
  auto bv_copy = bv;

  for (std::size_t i = 0; i < ITERS; ++i)
  {
    { CTRACK_NAME("partition native");
      nv = nv_copy;
      auto p = std::partition(nv.begin(), nv.end(),
        [](std::int16_t v) { return v >= 0; });
      do_not_optimize(*p); }

    { CTRACK_NAME("partition bound");
      bv = bv_copy;
      auto p = std::partition(bv.begin(), bv.end(),
        [](s9k v) { return v >= 0; });
      do_not_optimize(p->Raw); }
  }
}

void bench_std_accumulate()
{
  constexpr std::size_t SZ = 1000;
  constexpr std::size_t ITERS = N / SZ;

  using u200k = bound<{0, 200'000}>;

  std::vector<std::uint32_t> nv(SZ);
  std::vector<u200k> bv(SZ);
  for (std::size_t i = 0; i < SZ; ++i)
  {
    nv[i] = static_cast<std::uint32_t>(i % 5);
    bv[i] = static_cast<int>(i % 5);
  }

  for (std::size_t i = 0; i < ITERS; ++i)
  {
    { CTRACK_NAME("std::accumulate native");
      auto sum = std::accumulate(nv.begin(), nv.end(), std::uint32_t{0});
      do_not_optimize(sum); }

    { CTRACK_NAME("std::accumulate bound");
      auto sum = std::accumulate(bv.begin(), bv.end(), u200k{0}, std::plus<>{});
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
  bench_div();
  bench_accumulate();
  bench_int_vs_bound();
  bench_fixed_point();
  bench_round_nearest();
  bench_sort_algo();
  bench_find_algo();
  bench_transform_algo();
  bench_minmax_algo();
  bench_lower_bound_algo();
  bench_std_sort();
  bench_nth_element();
  bench_partition();
  bench_std_accumulate();

  ctrack::result_print();
  return 0;
}
