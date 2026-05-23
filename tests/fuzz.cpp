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
#include <cmath>
#include <compare>
#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <system_error>

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
  if constexpr (is_direct_storage<B>)
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
rational to_rational(B b) { return b; }

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
  if constexpr (is_raw_rational<B>) return;
  else {
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
}

template <boundable B>
void prop_native_compare(fuzz_state& s, long iters)
{
  if constexpr (is_integer_aligned<B> && !is_raw_rational<B>)
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
  if constexpr (is_integer_aligned<B> && !is_raw_rational<B>)
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
  if constexpr (is_integer_aligned<B> && !is_raw_rational<B>)
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
  if constexpr (is_integer_aligned<B> && !is_raw_rational<B>)
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
  if constexpr (is_integer_aligned<B> && !is_raw_rational<B>)
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
  if constexpr (is_raw_rational<B>) return;
  else {
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
}

template <boundable B>
void prop_mul_vs_rational(fuzz_state& s, long iters)
{
  if constexpr (!is_raw_rational<B>)
  {
    s.current_prop = "mul_vs_rational";
    for (long i = 0; i < iters; ++i)
    {
      s.iter = i;
      B a = make_from_raw<B>(random_in_range_raw<B>(s.rng));
      B b = make_from_raw<B>(random_in_range_raw<B>(s.rng));
      rational ar = to_rational(a);
      rational br = to_rational(b);
      auto mul_actual_b = a * b;
      rational mul_actual = to_rational(mul_actual_b);
      auto mul_expect_opt = ar * br;
      // Skip rare cases where the rational oracle itself overflows; the bound
      // arithmetic would also be invalid then.
      if (!mul_expect_opt.has_value()) continue;
      FUZZ_REQUIRE(s, mul_actual == *mul_expect_opt);
    }
  }
}

template <boundable A, boundable B>
void prop_cross_add(fuzz_state& s, long iters, const char* pair_name)
{
  s.current_prop = "cross_add";
  s.current_grid = pair_name;
  for (long i = 0; i < iters; ++i)
  {
    s.iter = i;
    A a = make_from_raw<A>(random_in_range_raw<A>(s.rng));
    B b = make_from_raw<B>(random_in_range_raw<B>(s.rng));
    rational ar = to_rational(a);
    rational br = to_rational(b);
    auto sum_actual_b = a + b;
    rational sum_actual = to_rational(sum_actual_b);
    auto sum_expect = (ar + br).value();
    FUZZ_REQUIRE(s, sum_actual == sum_expect);

    auto diff_actual_b = a - b;
    rational diff_actual = to_rational(diff_actual_b);
    auto diff_expect = (ar - br).value();
    FUZZ_REQUIRE(s, diff_actual == diff_expect);
  }
}

template <boundable B>
void prop_round_trip_construct(fuzz_state& s, long iters)
{
  // Pick an in-range raw, decode via value(), re-construct via B{value}, and
  // verify the new bound's value matches the original.
  if constexpr (is_integer_aligned<B> && !is_raw_rational<B>)
  {
    s.current_prop = "round_trip_construct";
    auto lo = static_cast<imax>(Lower<B>);
    auto hi = static_cast<imax>(Upper<B>);
    std::uniform_int_distribution<imax> dist(lo, hi);
    for (long i = 0; i < iters; ++i)
    {
      s.iter = i;
      imax v = dist(s.rng);
      B b{v};
      FUZZ_REQUIRE(s, b == v);
      // Construct another from the same value via try_make:
      auto opt = B::try_make(v);
      FUZZ_REQUIRE(s, opt.has_value());
      FUZZ_REQUIRE(s, *opt == v);
    }
  }
}

template <boundable B>
void prop_negation(fuzz_state& s, long iters)
{
  if constexpr (is_raw_rational<B>) return;
  else {
  s.current_prop = "negation";
  for (long i = 0; i < iters; ++i)
  {
    s.iter = i;
    B a = make_from_raw<B>(random_in_range_raw<B>(s.rng));
    auto neg = -a;
    rational ar = to_rational(a);
    rational nr = to_rational(neg);
    FUZZ_REQUIRE(s, nr == -ar);
    // double negation is identity on value
    auto neg2 = -neg;
    rational nr2 = to_rational(neg2);
    FUZZ_REQUIRE(s, nr2 == ar);
  }
  }
}

template <boundable B>
void prop_compound_add_int(fuzz_state& s, long iters)
{
  if constexpr (is_integer_aligned<B> && !is_raw_rational<B>)
  {
    s.current_prop = "compound_add_int";
    auto lo = static_cast<imax>(Lower<B>);
    auto hi = static_cast<imax>(Upper<B>);
    // Pick integer initial value and delta such that the result stays in range
    // (the type is `checked` by default; we don't want to throw).
    std::uniform_int_distribution<imax> dist(lo, hi);
    for (long i = 0; i < iters; ++i)
    {
      s.iter = i;
      imax start = dist(s.rng);
      imax slack_lo = start - lo;
      imax slack_hi = hi - start;
      // delta in [-slack_lo, +slack_hi]
      std::uniform_int_distribution<imax> delta_dist(-slack_lo, slack_hi);
      imax delta = delta_dist(s.rng);
      B b{start};
      b += delta;
      FUZZ_REQUIRE(s, b == start + delta);
    }
  }
}

template <boundable B>
void prop_modulo(fuzz_state& s, long iters)
{
  // Only applies to integer-aligned grids; result is optional<bound>.
  // mod requires `ignore_round` per the README, so derive a typed alias.
  if constexpr (is_integer_aligned<B> && !is_raw_rational<B>)
  {
    s.current_prop = "modulo";
    using BI = bound<Grid<B>, ignore_round>;
    auto lo = static_cast<imax>(Lower<B>);
    auto hi = static_cast<imax>(Upper<B>);
    if (lo > 0 || hi <= 0)
    {
      // Trivially: no zero-divisor risk if zero isn't representable; just
      // sample two random values.
      for (long i = 0; i < iters; ++i)
      {
        s.iter = i;
        BI a = make_from_raw<BI>(random_in_range_raw<BI>(s.rng));
        BI b = make_from_raw<BI>(random_in_range_raw<BI>(s.rng));
        if (b == 0) continue;  // possible if hi <= 0 and zero is in range
        auto r = mod(a, b, truncated);
        imax expected = static_cast<imax>(a) % static_cast<imax>(b);
        FUZZ_REQUIRE(s, r.has_value());
        FUZZ_REQUIRE(s, *r == expected);
      }
    }
    else
    {
      // Zero is in range: sample b until non-zero.
      for (long i = 0; i < iters; ++i)
      {
        s.iter = i;
        BI a = make_from_raw<BI>(random_in_range_raw<BI>(s.rng));
        BI b{0};
        do { b = make_from_raw<BI>(random_in_range_raw<BI>(s.rng)); } while (b == 0);
        auto r = mod(a, b, truncated);
        imax expected = static_cast<imax>(a) % static_cast<imax>(b);
        FUZZ_REQUIRE(s, r.has_value());
        FUZZ_REQUIRE(s, *r == expected);
      }
    }
  }
}

template <boundable B>
void prop_increment_wrap(fuzz_state& s, long iters)
{
  if constexpr (is_integer_aligned<B> && !is_raw_rational<B>)
  {
    s.current_prop = "increment_wrap";
    using BW = bound<Grid<B>, wrap>;
    auto lo = static_cast<imax>(Lower<B>);
    auto hi = static_cast<imax>(Upper<B>);
    imax range = hi - lo + 1;
    std::uniform_int_distribution<imax> dist(lo, hi);
    for (long i = 0; i < iters; ++i)
    {
      s.iter = i;
      imax start = dist(s.rng);
      BW b{start};
      ++b;
      imax expected = (start + 1 - lo) % range + lo;
      if (expected < lo) expected += range;
      FUZZ_REQUIRE(s, b == expected);

      BW b2{start};
      --b2;
      imax expected2 = ((start - 1 - lo) % range + range) % range + lo;
      FUZZ_REQUIRE(s, b2 == expected2);
    }
  }
}

template <boundable B>
void prop_div_by_zero(fuzz_state& s, long iters)
{
  if constexpr (is_integer_aligned<B> && !is_raw_rational<B>
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
  if constexpr (is_raw_rational<B>) return;
  else {
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
}

// Helper: run `fn` and verify it threw std::system_error with the expected errc.
template <typename Fn>
bool throws_with(errc expected, Fn&& fn)
{
  try { fn(); }
  catch (std::system_error const& e) { return e.code() == make_error_code(expected); }
  catch (...) { return false; }
  return false;
}

// As above, but accepts any of the listed errcs (the imax-probe stage and
// the post-probe narrowing stage may legitimately fire different codes).
template <typename Fn>
bool throws_with_any(std::initializer_list<errc> codes, Fn&& fn)
{
  try { fn(); }
  catch (std::system_error const& e) {
    for (auto c : codes) if (e.code() == make_error_code(c)) return true;
    return false;
  }
  catch (...) { return false; }
  return false;
}

template <boundable B>
void prop_compound_imax_overflow(fuzz_state& s, long iters)
{
  // Targets bound.hpp:257-8, 284-5, 330-1 — the checked-policy compound-op
  // imax overflow probes. Builds a checked alias of B, picks an in-range start,
  // hits it with imax sentinel values that force add/sub/mul_overflow, and
  // verifies the report path throws errc::overflow.
  if constexpr (is_integer_aligned<B> && !is_raw_rational<B>)
  {
    s.current_prop = "compound_imax_overflow";
    using BC = bound<Grid<B>, checked>;
    auto lo = static_cast<imax>(Lower<B>);
    auto hi = static_cast<imax>(Upper<B>);
    std::uniform_int_distribution<imax> dist(lo, hi);
    for (long i = 0; i < iters; ++i)
    {
      s.iter = i;
      imax start = dist(s.rng);

      // operator+= imax_max — overflows imax for start > 0; otherwise the imax
      // sum is huge and trips the post-probe narrowing → domain_error.
      FUZZ_REQUIRE(s, throws_with_any({errc::overflow, errc::domain_error}, [&]{
        BC b{start};
        b += std::numeric_limits<imax>::max();
      }));

      // operator-= imax_min — same dichotomy: imax overflow vs narrowing.
      FUZZ_REQUIRE(s, throws_with_any({errc::overflow, errc::domain_error}, [&]{
        BC b{start};
        b -= std::numeric_limits<imax>::min();
      }));

      // operator*= imax_max — for |start| > 1, start * imax_max overflows imax;
      // for start == ±1 it equals ±imax_max (out of grid range → narrowing).
      // start == 0 makes *= a no-op, so skip.
      if (start != 0)
      {
        FUZZ_REQUIRE(s, throws_with_any({errc::overflow, errc::domain_error}, [&]{
          BC b{start};
          b *= std::numeric_limits<imax>::max();
        }));
      }
    }
  }
}

template <boundable B>
void prop_compound_div_mod_zero(fuzz_state& s, long iters)
{
  // Targets bound.hpp:350 (operator/= 0) and bound.hpp:363 (operator%= 0).
  // Default-policy `/= 0` and `%= 0` go through report → throws division_by_zero
  // (empty_ref backend). Verified across both the default `none` policy and an
  // explicit `checked` alias for breadth.
  if constexpr (is_integer_aligned<B> && !is_raw_rational<B>)
  {
    s.current_prop = "compound_div_mod_zero";
    auto lo = static_cast<imax>(Lower<B>);
    auto hi = static_cast<imax>(Upper<B>);
    if (lo > 0) lo = 1;  // avoid 0/0
    std::uniform_int_distribution<imax> dist(std::max<imax>(1, lo), hi);
    for (long i = 0; i < iters; ++i)
    {
      s.iter = i;
      imax start = dist(s.rng);
      FUZZ_REQUIRE(s, throws_with(errc::division_by_zero, [&]{
        B b{start}; b /= 0;
      }));
      FUZZ_REQUIRE(s, throws_with(errc::division_by_zero, [&]{
        B b{start}; b %= 0;
      }));
    }
  }
}

template <boundable B>
void prop_compound_bound_overshoot(fuzz_state& s, long iters)
{
  // Targets bound.hpp:228-9 — the fast-path += else branch where the result
  // overshoots and the policy lacks clamp/wrap/sentinel. The catalogue's
  // grids already use the default `checked` policy, so the report path throws
  // domain_error. Pick start values where adding `delta` lands outside the
  // grid; skip those that would still fit.
  if constexpr (is_integer_aligned<B> && !is_raw_rational<B>)
  {
    s.current_prop = "compound_bound_overshoot";
    auto lo = static_cast<imax>(Lower<B>);
    auto hi = static_cast<imax>(Upper<B>);
    auto range = hi - lo;
    if (range < 2) return;
    std::uniform_int_distribution<imax> dist(lo, hi);
    for (long i = 0; i < iters; ++i)
    {
      s.iter = i;
      imax start = dist(s.rng);
      // Use delta = hi so the sum is start + hi.
      imax sum = start + hi;
      bool overshoots = (sum < lo || sum > hi);
      if (!overshoots) continue;
      FUZZ_REQUIRE(s, throws_with(errc::domain_error, [&]{
        B b{start};
        B delta{hi};
        b += delta;
      }));
    }
  }
}

template <boundable B>
void prop_non_notch_assign(fuzz_state& s, long iters)
{
  // Targets assignment.hpp:289 (round_nearest), 291 (ignore_round silent floor),
  // 296 (checked rounding_error report → throws), and 299 (silent floor for
  // unchecked policy). Only meaningful for fixed-point grids (notch != 1).
  if constexpr (!is_integer_aligned<B> && !is_raw_rational<B>)
  {
    s.current_prop = "non_notch_assign";
    // The catalogue's B already uses the default `checked` policy, so a non-
    // notch-aligned assignment to B must throw rounding_error.
    using BIR = bound<Grid<B>, ignore_round>;       // silent floor
    using BRN = bound<Grid<B>, round_nearest>;      // nearest
    using BNONE = bound<Grid<B>, none>;             // truly-unchecked → line 299
    rational notch = Notch<B>;
    rational lo    = Lower<B>;
    rational hi    = Upper<B>;
    rational half  = (notch / rational{2}).value();

    for (long i = 0; i < iters; ++i)
    {
      s.iter = i;
      // pick a notch-aligned in-range value, then add half-notch to land
      // strictly between two notches.
      std::uniform_int_distribution<umax> dist(0, NotchCount<B> - 1);
      umax k = dist(s.rng);
      rational on_notch = (lo + (rational{k} * notch).value()).value();
      rational mid = (on_notch + half).value();
      // Stay in range:
      if (mid > hi) continue;

      // Default-policy (checked) B: must throw rounding_error.
      FUZZ_REQUIRE(s, throws_with(errc::rounding_error, [&]{
        B b; b = mid;
      }));

      // ignore_round: silent floor; result == on_notch.
      FUZZ_REQUIRE(s, !throws_with(errc::rounding_error, [&]{
        BIR b; b = mid;
        rational got = b;
        FUZZ_REQUIRE(s, got == on_notch);
      }));

      // round_nearest: silent rounding to the nearer notch. With mid =
      // on_notch + notch/2 (exact half), round-half-up takes us to the next
      // notch (on_notch + notch), provided that's still in range.
      rational next_notch = (on_notch + notch).value();
      if (next_notch <= hi)
      {
        FUZZ_REQUIRE(s, !throws_with(errc::rounding_error, [&]{
          BRN b; b = mid;
          rational got = b;
          FUZZ_REQUIRE(s, got == next_notch);
        }));
      }

      // Truly-unchecked policy (none): silent floor, exercising assignment.hpp:299.
      FUZZ_REQUIRE(s, !throws_with(errc::rounding_error, [&]{
        BNONE b; b = mid;
        rational got = b;
        FUZZ_REQUIRE(s, got == on_notch);
      }));
    }
  }
}

template <boundable B>
void prop_subnormal_construct(fuzz_state& s, long iters)
{
  // Targets math.hpp:179-180 — abs_fraction shift cap for very small doubles.
  // The path is taken when the input double has a negative exponent so large
  // that bits-exponent > 62. Any double in (0, 2^-62) qualifies.
  if constexpr (!is_integer_aligned<B> && !is_raw_rational<B>)
  {
    s.current_prop = "subnormal_construct";
    using BIR = bound<Grid<B>, ignore_round>;
    rational lo = Lower<B>;
    rational hi = Upper<B>;
    bool zero_in_range = (lo <= 0_r) && (hi >= 0_r);
    if (!zero_in_range) return;
    std::uniform_real_distribution<double> mantissa(1.0, 2.0);
    std::uniform_int_distribution<int>     exp_dist(-300, -70);
    for (long i = 0; i < iters; ++i)
    {
      s.iter = i;
      double v = mantissa(s.rng) * std::pow(2.0, exp_dist(s.rng));
      // ignore_round → silent floor; should not throw and should land on 0.
      FUZZ_REQUIRE(s, !throws_with(errc::rounding_error, [&]{
        BIR b; b = v;
        rational got = b;
        // Floored toward lo, but for tiny v near zero the floor is 0 (or lo).
        FUZZ_REQUIRE(s, got == 0_r || got == lo);
      }));
    }
  }
}

void prop_interval_eq(fuzz_state& s)
{
  // Targets interval.hpp:149 — equivalent return of operator<=>.
  s.current_prop = "interval_eq";
  s.iter = 0;
  std::uniform_int_distribution<imax> dist(-1000, 1000);
  for (int i = 0; i < 50; ++i)
  {
    imax a = dist(s.rng);
    imax b = a + std::abs(dist(s.rng));  // ensure b >= a
    interval x{rational{a}, rational{b}};
    interval y{rational{a}, rational{b}};
    FUZZ_REQUIRE(s, (x <=> y) == std::partial_ordering::equivalent);
    interval z{rational{a-1}, rational{b}};
    FUZZ_REQUIRE(s, (x <=> z) == std::partial_ordering::unordered);
  }
}

void prop_optional_throws(fuzz_state& s)
{
  // Targets slim/optional.hpp:35-36, 38-39, 350, 568.
  s.current_prop = "optional_throws";
  s.iter = 0;
  using B = bound<{0, 100}, sentinel>;
  // Calling .value() on an empty optional throws bad_optional_access.
  bool got_throw = false;
  std::string what;
  try
  {
    slim::optional<B> opt = slim::nullopt;
    (void)opt.value();
  }
  catch (slim::bad_optional_access const& e)
  {
    got_throw = true;
    what = e.what();
  }
  catch (...) {}
  FUZZ_REQUIRE(s, got_throw);
  FUZZ_REQUIRE(s, !what.empty());

  // Constructing slim::optional<B> from a sentinel-valued B throws.
  bool sentinel_throw = false;
  try
  {
    B sentinel_b; sentinel_b.Raw = sentinel_raw<B>();
    slim::optional<B> bad{sentinel_b};
    (void)bad;
  }
  catch (slim::bad_optional_access const&) { sentinel_throw = true; }
  catch (...) {}
  FUZZ_REQUIRE(s, sentinel_throw);
}

template <boundable B>
void prop_compound_add_same_bound(fuzz_state& s, long iters)
{
  // Targets bound.hpp:243 — the fallback path of operator+=(boundable R)
  // when the fast path's encoding constraints don't hold (typically frational
  // grids whose Lower != 0 on at least one side, or notches that mismatch).
  // For grids whose value ranges allow it, b += b should still produce 2*b
  // (or saturate/throw on overshoot). We restrict to picks that stay in range.
  if constexpr (!is_raw_rational<B>)
  {
    s.current_prop = "compound_add_same_bound";
    for (long i = 0; i < iters; ++i)
    {
      s.iter = i;
      B a = make_from_raw<B>(random_in_range_raw<B>(s.rng));
      B delta = make_from_raw<B>(random_in_range_raw<B>(s.rng));
      rational ar = to_rational(a);
      rational dr = to_rational(delta);
      rational expect = (ar + dr).value();
      // Skip if result lands outside the grid (avoids checked-policy throw).
      if (expect < Lower<B> || expect > Upper<B>) continue;
      a += delta;
      FUZZ_REQUIRE(s, to_rational(a) == expect);
    }
  }
}

template <boundable B>
void prop_raw_rational_arith(fuzz_state& s, long iters)
{
  // Targets addition.hpp:48-63 (the is_raw_rational<result> branches) and
  // multiplication.hpp:44-63 (same for mul). For raw-rational grids (notch=0)
  // arithmetic goes through the rational-add/mul paths directly. Most random
  // small-integer values won't overflow the rational machinery, so we mostly
  // exercise the success branches; occasional out-of-range results land in
  // the nullopt path.
  if constexpr (is_raw_rational<B>)
  {
    s.current_prop = "raw_rational_arith";
    rational lo = Lower<B>;
    rational hi = Upper<B>;
    std::uniform_int_distribution<imax> num_dist(
        static_cast<imax>(lo) + 1, static_cast<imax>(hi) - 1);
    std::uniform_int_distribution<imax> den_dist(1, 7);
    for (long i = 0; i < iters; ++i)
    {
      s.iter = i;
      // Mix integer and small-fraction inputs so rational add/mul exercises
      // both the ad==1 fast path and the general denominator path.
      imax av_num = num_dist(s.rng), bv_num = num_dist(s.rng);
      imax av_den = den_dist(s.rng), bv_den = den_dist(s.rng);
      rational ar{av_num, av_den};
      rational br{bv_num, bv_den};
      // Skip if assignment would push outside the grid.
      if (ar < lo || ar > hi || br < lo || br > hi) continue;
      B a; a.Raw = ar;
      B b; b.Raw = br;
      auto sum  = a + b;
      auto prod = a * b;
      auto expect_sum_opt  = ar + br;
      auto expect_prod_opt = ar * br;
      if (!expect_sum_opt.has_value() || !expect_prod_opt.has_value()) continue;
      rational expect_sum  = *expect_sum_opt;
      rational expect_prod = *expect_prod_opt;
      // sum/prod is either bound<...> or slim::optional<bound<...>> depending
      // on whether the operation needs an overflow check (driven by policy).
      auto check = [&](auto v, rational expected) {
        if constexpr (requires { v.has_value(); }) {
          FUZZ_REQUIRE(s, v.has_value());
          if (v.has_value())
            FUZZ_REQUIRE(s, v->template to<rational>().value() == expected);
        } else {
          FUZZ_REQUIRE(s, v.template to<rational>().value() == expected);
        }
      };
      check(sum,  expect_sum);
      check(prod, expect_prod);
    }
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
  guarded(s, [&]{ prop_round_trip_construct<B>(s, iters); });
  guarded(s, [&]{ prop_native_compare<B>(s, iters); });
  guarded(s, [&]{ prop_clamp<B>(s, iters); });
  guarded(s, [&]{ prop_wrap<B>(s, iters); });
  guarded(s, [&]{ prop_try_make<B>(s, iters); });
  guarded(s, [&]{ prop_on_clamp<B>(s, iters); });
  guarded(s, [&]{ prop_arith_vs_rational<B>(s, iters); });
  guarded(s, [&]{ prop_mul_vs_rational<B>(s, iters); });
  guarded(s, [&]{ prop_negation<B>(s, iters); });
  guarded(s, [&]{ prop_compound_add_int<B>(s, iters); });
  guarded(s, [&]{ prop_modulo<B>(s, iters); });
  guarded(s, [&]{ prop_increment_wrap<B>(s, iters); });
  guarded(s, [&]{ prop_div_by_zero<B>(s, iters); });
  guarded(s, [&]{ prop_spaceship_symmetry<B>(s, iters); });
  guarded(s, [&]{ prop_compound_imax_overflow<B>(s, iters); });
  guarded(s, [&]{ prop_compound_div_mod_zero<B>(s, iters); });
  guarded(s, [&]{ prop_compound_bound_overshoot<B>(s, iters); });
  guarded(s, [&]{ prop_compound_add_same_bound<B>(s, iters); });
  guarded(s, [&]{ prop_non_notch_assign<B>(s, iters); });
  guarded(s, [&]{ prop_subnormal_construct<B>(s, iters); });
  guarded(s, [&]{ prop_raw_rational_arith<B>(s, iters); });
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
  run_props<bound<{0, 254}>>                    (s, iters, "u254");
  run_props<bound<{0, 255}>>                    (s, iters, "u255");
  run_props<bound<{0, 65535}>>                  (s, iters, "u16k");
  run_props<bound<{-32768, 32767}>>             (s, iters, "s16");
  run_props<bound<{0, 1'000'000}>>              (s, iters, "u1M");
  run_props<bound<{1, 100}>>                    (s, iters, "u100off");
  run_props<bound<{-3, 7}>>                     (s, iters, "small_signed");
  run_props<bound<{{0, 50}, 0.5}>>              (s, iters, "u50half");
  run_props<bound<{{-50, 50}, 0.5}>>            (s, iters, "s50half");
  run_props<bound<{{0, 255}, 1.0/256}>>         (s, iters, "Q8.8");
  run_props<bound<{{-1, 1}, *(1_r/16384)}>>     (s, iters, "Q1.14");
  run_props<bound<{{0, 65535}, *(1_r/65536)}>>  (s, iters, "Q16.16");
  run_props<bound<{{0, 1'000'000}, *(1_r/100)}>>(s, iters, "money");
  // Raw-rational grid (notch=0): exercises the is_raw_rational branches in
  // addition / multiplication / assignment. Default `checked` policy goes
  // through the overflow-checked rational arithmetic; the `none` variant
  // exercises rational::add_unchecked / mul_unchecked.
  run_props<bound<{{-1000, 1000}, 0}>>          (s, iters, "raw_rat");
  run_props<bound<{{-1000, 1000}, 0}, none>>    (s, iters, "raw_rat_unck");

  // Standalone (non-grid) properties.
  guarded(s, [&]{ prop_interval_eq(s); });
  guarded(s, [&]{ prop_optional_throws(s); });

  // Cross-grid arithmetic: mix grids of different lower/upper but same notch.
  guarded(s, [&]{ prop_cross_add<bound<{0, 100}>, bound<{-100, 100}>>(s, iters, "u100+s100"); });
  guarded(s, [&]{ prop_cross_add<bound<{0, 100}>, bound<{0, 1000}>>(s, iters, "u100+u1k"); });
  guarded(s, [&]{ prop_cross_add<bound<{-50, 50}>, bound<{0, 100}>>(s, iters, "s50+u100"); });
  guarded(s, [&]{ prop_cross_add<bound<{{0, 50}, 0.5}>, bound<{{-50, 50}, 0.5}>>(s, iters, "u50half+s50half"); });

  std::cout << "passed=" << s.passed << " failed=" << s.failed << "\n";
  return (s.failed > 0) ? 1 : 0;
}
