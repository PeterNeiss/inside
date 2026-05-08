//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
// bound_fuzz: deterministic property-based fuzz harness.
//
//   ./bound_fuzz             # random seed (printed at startup)
//   ./bound_fuzz <seed>      # reproduce a previous run
//   ./bound_fuzz <seed> N    # N iterations per property (default 10000)
//
// On any failure, prints the seed and the failing property + iteration.
// Run with the same seed to reproduce.
//---------------------------------------------------------------------------

#include "bound/bound.hpp"

#include <algorithm>
#include <compare>
#include <cstdint>
#include <iostream>
#include <random>
#include <string>

using namespace bnd;

//---------------------------------------------------------------------------
// State
//---------------------------------------------------------------------------
struct fuzz_state
{
  std::mt19937_64 rng;
  std::uint64_t   seed = 0;
  long passed = 0;
  long failed = 0;
  const char* current_prop = "";
  const char* current_grid = "";
  long iter = 0;
};

#define FUZZ_REQUIRE(s, expr)                                              \
  do {                                                                     \
    if (expr) { ++(s).passed; }                                            \
    else {                                                                 \
      ++(s).failed;                                                        \
      std::cerr << "FAIL [" << (s).current_grid << "/"                     \
                << (s).current_prop << "] iter=" << (s).iter               \
                << " expr=" #expr " seed=" << (s).seed << "\n";            \
    }                                                                      \
  } while (0)

template <typename Fn>
void guarded(fuzz_state& s, Fn&& fn)
{
  try { fn(); }
  catch (std::exception& e)
  {
    ++s.failed;
    std::cerr << "THROW [" << s.current_grid << "/" << s.current_prop
              << "] iter=" << s.iter << " what=" << e.what()
              << " seed=" << s.seed << "\n";
  }
}

//---------------------------------------------------------------------------
// Helpers
//---------------------------------------------------------------------------
template <boundable B>
constexpr B make_from_raw(typename B::raw_type r)
{
  B b; b.Raw = r; return b;
}

// Random in-range raw value. Direct storage uses [Lower, Upper];
// notch storage uses [0, NotchCount].
template <boundable B>
typename B::raw_type random_in_range_raw(std::mt19937_64& rng)
{
  using raw = typename B::raw_type;
  if constexpr (IsDirectStorage<B>)
  {
    auto lo = static_cast<imax>(Lower<B>);
    auto hi = static_cast<imax>(Upper<B>);
    std::uniform_int_distribution<imax> dist(lo, hi);
    return static_cast<raw>(dist(rng));
  }
  else
  {
    std::uniform_int_distribution<umax> dist(0, NotchCount<B>);
    return static_cast<raw>(dist(rng));
  }
}

inline imax random_wide_int(std::mt19937_64& rng, imax span)
{
  std::uniform_int_distribution<imax> dist(-span, span);
  return dist(rng);
}

template <boundable B>
rational to_rational(B b) { return static_cast<rational>(b); }

//---------------------------------------------------------------------------
// Properties
//---------------------------------------------------------------------------

template <boundable B>
void prop_storage_size(fuzz_state& s)
{
  s.current_prop = "storage_size";
  s.iter = 0;
  FUZZ_REQUIRE(s, sizeof(B) == sizeof(typename B::raw_type));
}

template <boundable B>
void prop_round_trip(fuzz_state& s, long iters)
{
  s.current_prop = "round_trip";
  for (long i = 0; i < iters; ++i)
  {
    s.iter = i;
    auto raw = random_in_range_raw<B>(s.rng);
    B b = make_from_raw<B>(raw);
    rational v = to_rational(b);
    FUZZ_REQUIRE(s, v >= Lower<B>);
    FUZZ_REQUIRE(s, v <= Upper<B>);
  }
}

template <boundable B>
void prop_native_compare(fuzz_state& s, long iters)
{
  if constexpr (IsIntegerAligned<B> && !IsRawRational<B>)
  {
    s.current_prop = "native_compare";
    auto lo = static_cast<imax>(Lower<B>);
    auto hi = static_cast<imax>(Upper<B>);
    std::uniform_int_distribution<imax> dist(lo, hi);
    for (long i = 0; i < iters; ++i)
    {
      s.iter = i;
      imax v = dist(s.rng);
      B b{v};
      FUZZ_REQUIRE(s, b == v);
      FUZZ_REQUIRE(s, !(b != v));
      if (v < hi) FUZZ_REQUIRE(s, b < v + 1);
      if (v > lo) FUZZ_REQUIRE(s, b > v - 1);
    }
  }
}

template <boundable B>
void prop_clamp(fuzz_state& s, long iters)
{
  if constexpr (IsIntegerAligned<B> && !IsRawRational<B>)
  {
    s.current_prop = "clamp";
    using BC = bound<Grid<B>, clamp>;
    auto lo = static_cast<imax>(Lower<B>);
    auto hi = static_cast<imax>(Upper<B>);
    imax span = (hi - lo) * 3 + 100;
    for (long i = 0; i < iters; ++i)
    {
      s.iter = i;
      imax v = lo + random_wide_int(s.rng, span);
      BC c{v};
      imax expected = std::clamp<imax>(v, lo, hi);
      FUZZ_REQUIRE(s, c == expected);
    }
  }
}

template <boundable B>
void prop_wrap(fuzz_state& s, long iters)
{
  if constexpr (IsIntegerAligned<B> && !IsRawRational<B>)
  {
    s.current_prop = "wrap";
    using BW = bound<Grid<B>, wrap>;
    auto lo = static_cast<imax>(Lower<B>);
    auto hi = static_cast<imax>(Upper<B>);
    imax range = hi - lo + 1;
    imax span = range * 3 + 100;
    for (long i = 0; i < iters; ++i)
    {
      s.iter = i;
      imax v = lo + random_wide_int(s.rng, span);
      BW w{v};
      imax expected = ((v - lo) % range + range) % range + lo;
      FUZZ_REQUIRE(s, w == expected);
    }
  }
}

template <boundable B>
void prop_try_make(fuzz_state& s, long iters)
{
  if constexpr (IsIntegerAligned<B> && !IsRawRational<B>)
  {
    s.current_prop = "try_make";
    auto lo = static_cast<imax>(Lower<B>);
    auto hi = static_cast<imax>(Upper<B>);
    imax span = (hi - lo) * 3 + 100;
    for (long i = 0; i < iters; ++i)
    {
      s.iter = i;
      imax v = lo + random_wide_int(s.rng, span);
      auto opt = B::try_make(v);
      bool in_range = (v >= lo && v <= hi);
      FUZZ_REQUIRE(s, opt.has_value() == in_range);
      if (in_range) FUZZ_REQUIRE(s, *opt == v);
    }
  }
}

template <boundable B>
void prop_on_clamp(fuzz_state& s, long iters)
{
  if constexpr (IsIntegerAligned<B> && !IsRawRational<B>)
  {
    s.current_prop = "on_clamp";
    using BC = bound<Grid<B>, clamp>;
    auto lo = static_cast<imax>(Lower<B>);
    auto hi = static_cast<imax>(Upper<B>);
    imax span = (hi - lo) * 2 + 50;
    for (long i = 0; i < iters; ++i)
    {
      s.iter = i;
      imax v = lo + random_wide_int(s.rng, span);
      BC c{lo};
      bool fired = false;
      imax overshoot_seen = 0;
      c.on_clamp([&](auto&, auto over) {
        fired = true;
        overshoot_seen = static_cast<imax>(over);
      }) = v;

      // Library convention (assignment.hpp apply_clamp): overshoot = rhs - clamped.
      // Signed: positive when above upper, negative when below lower.
      if (v < lo)
      {
        FUZZ_REQUIRE(s, fired);
        FUZZ_REQUIRE(s, c == lo);
        FUZZ_REQUIRE(s, overshoot_seen == v - lo);
      }
      else if (v > hi)
      {
        FUZZ_REQUIRE(s, fired);
        FUZZ_REQUIRE(s, c == hi);
        FUZZ_REQUIRE(s, overshoot_seen == v - hi);
      }
      else
      {
        FUZZ_REQUIRE(s, !fired);
        FUZZ_REQUIRE(s, c == v);
      }
    }
  }
}

template <boundable B>
void prop_arith_vs_rational(fuzz_state& s, long iters)
{
  s.current_prop = "arith_vs_rational";
  for (long i = 0; i < iters; ++i)
  {
    s.iter = i;
    B a = make_from_raw<B>(random_in_range_raw<B>(s.rng));
    B b = make_from_raw<B>(random_in_range_raw<B>(s.rng));
    rational ar = to_rational(a);
    rational br = to_rational(b);

    rational sum_actual  = to_rational(a + b);
    rational diff_actual = to_rational(a - b);
    rational sum_expect  = (ar + br).value();
    rational diff_expect = (ar - br).value();
    FUZZ_REQUIRE(s, sum_actual == sum_expect);
    FUZZ_REQUIRE(s, diff_actual == diff_expect);
  }
}

template <boundable B>
void prop_div_by_zero(fuzz_state& s, long iters)
{
  if constexpr (IsIntegerAligned<B> && !IsRawRational<B>
                && Lower<B> <= 0_r && Upper<B> >= 0_r)
  {
    s.current_prop = "div_by_zero";
    B zero{0};
    for (long i = 0; i < iters; ++i)
    {
      s.iter = i;
      auto a = make_from_raw<B>(random_in_range_raw<B>(s.rng));
      auto q = a / zero;
      FUZZ_REQUIRE(s, !q.has_value());
    }
  }
}

template <boundable B>
void prop_spaceship_symmetry(fuzz_state& s, long iters)
{
  s.current_prop = "spaceship_symmetry";
  for (long i = 0; i < iters; ++i)
  {
    s.iter = i;
    B a = make_from_raw<B>(random_in_range_raw<B>(s.rng));
    B b = make_from_raw<B>(random_in_range_raw<B>(s.rng));
    auto cmp_ab = (a <=> b);
    auto cmp_ba = (b <=> a);
    if (cmp_ab == std::strong_ordering::equal)
      FUZZ_REQUIRE(s, cmp_ba == std::strong_ordering::equal);
    else if (cmp_ab == std::strong_ordering::less)
      FUZZ_REQUIRE(s, cmp_ba == std::strong_ordering::greater);
    else
      FUZZ_REQUIRE(s, cmp_ba == std::strong_ordering::less);

    bool eq = (a == b);
    FUZZ_REQUIRE(s, eq == (cmp_ab == std::strong_ordering::equal));
  }
}

//---------------------------------------------------------------------------
// Per-grid runner
//---------------------------------------------------------------------------
template <boundable B>
void run_props(fuzz_state& s, long iters, const char* name)
{
  s.current_grid = name;
  guarded(s, [&]{ prop_storage_size<B>(s); });
  guarded(s, [&]{ prop_round_trip<B>(s, iters); });
  guarded(s, [&]{ prop_native_compare<B>(s, iters); });
  guarded(s, [&]{ prop_clamp<B>(s, iters); });
  guarded(s, [&]{ prop_wrap<B>(s, iters); });
  guarded(s, [&]{ prop_try_make<B>(s, iters); });
  guarded(s, [&]{ prop_on_clamp<B>(s, iters); });
  guarded(s, [&]{ prop_arith_vs_rational<B>(s, iters); });
  guarded(s, [&]{ prop_div_by_zero<B>(s, iters); });
  guarded(s, [&]{ prop_spaceship_symmetry<B>(s, iters); });
}

//---------------------------------------------------------------------------
// main
//---------------------------------------------------------------------------
int main(int argc, char** argv)
{
  fuzz_state s;
  long iters = 10000;
  if (argc > 1) s.seed = std::stoull(argv[1]);
  else s.seed = std::random_device{}();
  if (argc > 2) iters = std::stol(argv[2]);
  s.rng.seed(s.seed);

  std::cout << "bound_fuzz: seed=" << s.seed << " iters=" << iters << "\n";

  run_props<bound<{0, 100}>>                    (s, iters, "u100");
  run_props<bound<{-100, 100}>>                 (s, iters, "s100");
  run_props<bound<{0, 65535}>>                  (s, iters, "u16k");
  run_props<bound<{-32768, 32767}>>             (s, iters, "s16");
  run_props<bound<{0, 1'000'000}>>              (s, iters, "u1M");
  run_props<bound<{{0, 50}, 0.5}>>              (s, iters, "u50half");
  run_props<bound<{{-50, 50}, 0.5}>>            (s, iters, "s50half");
  run_props<bound<{{0, 255}, 1.0/256}>>         (s, iters, "Q8.8");
  run_props<bound<{{-1, 1}, *(1_r/16384)}>>     (s, iters, "Q1.14");
  run_props<bound<{{0, 65535}, *(1_r/65536)}>>  (s, iters, "Q16.16");
  run_props<bound<{{0, 1'000'000}, *(1_r/100)}>>(s, iters, "money");

  std::cout << "passed=" << s.passed << " failed=" << s.failed << "\n";
  return (s.failed > 0) ? 1 : 0;
}
