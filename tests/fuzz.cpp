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
#include "bound/casts.hpp"
#include "bound/cmath.hpp"
#include "bound/predicates.hpp"
#include "bound/range.hpp"

#include <algorithm>
#include <cmath>
#include <compare>
#include <cstdint>
#include <iostream>
#include <limits>
#include <numbers>
#include <random>
#include <string>

using namespace bnd;
using namespace bnd::detail;

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
// Random in-range raw value. Direct storage uses [Lower, Upper];
// notch storage uses [0, NotchCount].
template <boundable B>
typename B::raw_type random_in_range_raw(std::mt19937_64& rng)
{
  using raw = typename B::raw_type;
  if constexpr (real_raw<B>)
  {
    // `real` (double-backed) bounds hold a grid point as a double — generate a
    // random in-range grid point Lower + k·Notch (the integer-cast branch below
    // would truncate a fractional Lower and land below range, e.g. log2 of ~0).
    if constexpr (Notch<B> == rational{0})
    {
      std::uniform_real_distribution<double> dist(static_cast<double>(Lower<B>),
                                                  static_cast<double>(Upper<B>));
      return dist(rng);
    }
    else
    {
      std::uniform_int_distribution<umax> dist(0, NotchCount<B>);
      return static_cast<double>(Lower<B>)
           + static_cast<double>(dist(rng)) * static_cast<double>(Notch<B>);
    }
  }
  else if constexpr (!index_raw<B>)
  {
    auto lo = trunc(Lower<B>);
    auto hi = trunc(Upper<B>);
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

// |a - b| <= tol. The math operand under the default engine is a `real` bound:
// its value is a grid point (snapped, low-denominator) and converts to an exact
// double, while the std:: oracle is a full-precision double. We compare in
// double — subtracting two rationals whose denominators are 1/notch and ~2^52
// would overflow imax. The args stay `rational` so every call site is unchanged.
inline bool approx_le(rational a, rational b, rational tol)
{
  return std::fabs(static_cast<double>(a) - static_cast<double>(b))
         <= static_cast<double>(tol);
}

// Relative tolerance for wide-range transcendentals (exp, pow): the allowed
// error grows with the oracle's magnitude. allowed = abstol + rel*|oracle|.
inline bool approx_rel(rational got, double oracle, double rel, rational abstol)
{
  return std::fabs(static_cast<double>(got) - oracle)
         <= static_cast<double>(abstol) + std::fabs(oracle) * rel;
}

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
  if constexpr (rational_raw<B>) return;
  else {
  s.current_prop = "round_trip";
  for (long i = 0; i < iters; ++i)
  {
    s.iter = i;
    auto raw = random_in_range_raw<B>(s.rng);
    B b = B::from_raw(raw);
    rational v = to_rational(b);
    FUZZ_REQUIRE(s, v >= Lower<B>);
    FUZZ_REQUIRE(s, v <= Upper<B>);
  }
  }
}

template <boundable B>
void prop_native_compare(fuzz_state& s, long iters)
{
  if constexpr (IsIntegerAligned<B> && !rational_raw<B>)
  {
    s.current_prop = "native_compare";
    auto lo = trunc(Lower<B>);
    auto hi = trunc(Upper<B>);
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
  if constexpr (IsIntegerAligned<B> && !rational_raw<B>)
  {
    s.current_prop = "clamp";
    using BC = bound<Grid<B>, clamp>;
    auto lo = trunc(Lower<B>);
    auto hi = trunc(Upper<B>);
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
  if constexpr (IsIntegerAligned<B> && !rational_raw<B>)
  {
    s.current_prop = "wrap";
    using BW = bound<Grid<B>, wrap>;
    auto lo = trunc(Lower<B>);
    auto hi = trunc(Upper<B>);
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
  if constexpr (IsIntegerAligned<B> && !rational_raw<B>)
  {
    s.current_prop = "try_make";
    auto lo = trunc(Lower<B>);
    auto hi = trunc(Upper<B>);
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
  if constexpr (IsIntegerAligned<B> && !rational_raw<B>)
  {
    s.current_prop = "on_clamp";
    using BC = bound<Grid<B>, clamp>;
    auto lo = trunc(Lower<B>);
    auto hi = trunc(Upper<B>);
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
  if constexpr (rational_raw<B>) return;
  else {
  s.current_prop = "arith_vs_rational";
  for (long i = 0; i < iters; ++i)
  {
    s.iter = i;
    B a = B::from_raw(random_in_range_raw<B>(s.rng));
    B b = B::from_raw(random_in_range_raw<B>(s.rng));
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
  if constexpr (!rational_raw<B>)
  {
    s.current_prop = "mul_vs_rational";
    for (long i = 0; i < iters; ++i)
    {
      s.iter = i;
      B a = B::from_raw(random_in_range_raw<B>(s.rng));
      B b = B::from_raw(random_in_range_raw<B>(s.rng));
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
    A a = A::from_raw(random_in_range_raw<A>(s.rng));
    B b = B::from_raw(random_in_range_raw<B>(s.rng));
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
  if constexpr (IsIntegerAligned<B> && !rational_raw<B>)
  {
    s.current_prop = "round_trip_construct";
    auto lo = trunc(Lower<B>);
    auto hi = trunc(Upper<B>);
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
  if constexpr (rational_raw<B>) return;
  else {
  s.current_prop = "negation";
  for (long i = 0; i < iters; ++i)
  {
    s.iter = i;
    B a = B::from_raw(random_in_range_raw<B>(s.rng));
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
void prop_compound_add_bound(fuzz_state& s, long iters)
{
  if constexpr (IsIntegerAligned<B> && !rational_raw<B>)
  {
    s.current_prop = "compound_add_bound";
    // The delta is itself a bound (raw int RHS is now ill-formed). A signed grid
    // spanning ±(Upper−Lower) covers every in-range delta.
    using Delta = bound<{Lower<B> - Upper<B>, Upper<B> - Lower<B>}>;
    auto lo = trunc(Lower<B>);
    auto hi = trunc(Upper<B>);
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
      b += Delta{delta};
      FUZZ_REQUIRE(s, b == start + delta);
    }
  }
}

template <boundable B>
void prop_modulo(fuzz_state& s, long iters)
{
  // Only applies to integer-aligned grids; result is optional<bound>.
  // mod requires `snap` per the README, so derive a typed alias.
  if constexpr (IsIntegerAligned<B> && !rational_raw<B>)
  {
    s.current_prop = "modulo";
    using BI = bound<Grid<B>, snap>;
    auto lo = trunc(Lower<B>);
    auto hi = trunc(Upper<B>);
    if (lo > 0 || hi <= 0)
    {
      // Trivially: no zero-divisor risk if zero isn't representable; just
      // sample two random values.
      for (long i = 0; i < iters; ++i)
      {
        s.iter = i;
        BI a = BI::from_raw(random_in_range_raw<BI>(s.rng));
        BI b = BI::from_raw(random_in_range_raw<BI>(s.rng));
        if (b == 0) continue;  // possible if hi <= 0 and zero is in range
        auto r = mod(a, b, truncated);
        imax expected = as<imax>(a) % as<imax>(b);
        // mod returns a plain bound when B's grid excludes zero, else optional.
        if constexpr (is_slim_optional_v<decltype(r)>)
        {
          FUZZ_REQUIRE(s, r.has_value());
          FUZZ_REQUIRE(s, *r == expected);
        }
        else
          FUZZ_REQUIRE(s, r == expected);
      }
    }
    else
    {
      // Zero is in range: sample b until non-zero.
      for (long i = 0; i < iters; ++i)
      {
        s.iter = i;
        BI a = BI::from_raw(random_in_range_raw<BI>(s.rng));
        BI b{0};
        do { b = BI::from_raw(random_in_range_raw<BI>(s.rng)); } while (b == 0);
        auto r = mod(a, b, truncated);
        imax expected = as<imax>(a) % as<imax>(b);
        // mod returns a plain bound when B's grid excludes zero, else optional.
        if constexpr (is_slim_optional_v<decltype(r)>)
        {
          FUZZ_REQUIRE(s, r.has_value());
          FUZZ_REQUIRE(s, *r == expected);
        }
        else
          FUZZ_REQUIRE(s, r == expected);
      }
    }
  }
}

template <boundable B>
void prop_increment_wrap(fuzz_state& s, long iters)
{
  if constexpr (IsIntegerAligned<B> && !rational_raw<B>)
  {
    s.current_prop = "increment_wrap";
    using BW = bound<Grid<B>, wrap>;
    auto lo = trunc(Lower<B>);
    auto hi = trunc(Upper<B>);
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
  if constexpr (IsIntegerAligned<B> && !rational_raw<B>
                && Lower<B> <= 0 && Upper<B> >= 0)
  {
    s.current_prop = "div_by_zero";
    B zero{0};
    for (long i = 0; i < iters; ++i)
    {
      s.iter = i;
      auto a = B::from_raw(random_in_range_raw<B>(s.rng));
      auto q = a / zero;
      FUZZ_REQUIRE(s, !q.has_value());
    }
  }
}

template <boundable B>
void prop_spaceship_symmetry(fuzz_state& s, long iters)
{
  if constexpr (rational_raw<B>) return;
  else {
  s.current_prop = "spaceship_symmetry";
  for (long i = 0; i < iters; ++i)
  {
    s.iter = i;
    B a = B::from_raw(random_in_range_raw<B>(s.rng));
    B b = B::from_raw(random_in_range_raw<B>(s.rng));
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

// Helper: run `fn` and verify it threw bnd::bound_error with the expected errc.
template <typename Fn>
bool throws_with(errc expected, Fn&& fn)
{
  try { fn(); }
  catch (bnd::bound_error const& e) { return e.code == expected; }
  catch (...) { return false; }
  return false;
}

// As above, but accepts any of the listed errcs (the imax-probe stage and
// the post-probe narrowing stage may legitimately fire different codes).
template <typename Fn>
bool throws_with_any(std::initializer_list<errc> codes, Fn&& fn)
{
  try { fn(); }
  catch (bnd::bound_error const& e) {
    for (auto c : codes) if (e.code == c) return true;
    return false;
  }
  catch (...) { return false; }
  return false;
}

// (Removed: prop_compound_imax_overflow — it forced add/sub/mul_overflow by
// hitting a checked bound with raw imax sentinel RHS. Raw compound assigns are
// gone, and a range-bounded operand can't overflow imax, so the path is moot.)

template <boundable B>
void prop_compound_div_mod_zero(fuzz_state& s, long iters)
{
  // `b /= 0_r` (rational zero) routes through the rational compound-assign's
  // zero guard → report → throws division_by_zero. (Raw `b /= 0` is now ill-
  // formed; the boundable `%= zero-bound` path needs a snap integer divisor
  // and is covered for snap bounds in test_compound_assign.)
  if constexpr (IsIntegerAligned<B> && !rational_raw<B>)
  {
    s.current_prop = "compound_div_zero";
    // The divisor is the constant 0_r, so any in-range dividend throws; pick
    // `start` from the grid's actual [lo, hi] (a fully-negative grid has hi < 1,
    // so the old std::max(1, lo) built an inverted, UB distribution range).
    const imax lo = trunc(Lower<B>);
    const imax hi = trunc(Upper<B>);
    std::uniform_int_distribution<imax> dist(lo, hi);
    for (long i = 0; i < iters; ++i)
    {
      s.iter = i;
      imax start = dist(s.rng);
      FUZZ_REQUIRE(s, throws_with(errc::division_by_zero, [&]{
        B b{start}; b /= 0_r;
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
  if constexpr (IsIntegerAligned<B> && !rational_raw<B>)
  {
    s.current_prop = "compound_bound_overshoot";
    auto lo = trunc(Lower<B>);
    auto hi = trunc(Upper<B>);
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
  // Targets assignment.hpp:289 (round_nearest), 291 (snap silent floor),
  // 296 (checked rounding_error report → throws), and 299 (silent floor for
  // unchecked policy). Only meaningful for fixed-point grids (notch != 1).
  if constexpr (!IsIntegerAligned<B> && !rational_raw<B>)
  {
    s.current_prop = "non_notch_assign";
    // The catalogue's B already uses the default `checked` policy, so a non-
    // notch-aligned assignment to B must throw rounding_error.
    using BIR = bound<Grid<B>, snap>;       // silent floor
    using BRN = bound<Grid<B>, round_nearest>;      // nearest
    using BNONE = bound<Grid<B>, none>;             // truly-unchecked → line 299
    rational notch = Notch<B>;
    rational lo    = Lower<B>;
    rational hi    = Upper<B>;
    rational half  = (notch / 2_r).value();

    for (long i = 0; i < iters; ++i)
    {
      s.iter = i;
      // pick a notch-aligned in-range value, then add half-notch to land
      // strictly between two notches.
      std::uniform_int_distribution<umax> dist(0, NotchCount<B> - 1);
      umax k = dist(s.rng);
      rational on_notch   = (lo + rational{k} * notch).value();
      rational mid        = (on_notch + half).value();
      rational next_notch = (on_notch + notch).value();
      // Stay in range:
      if (mid > hi) continue;

      // `mid` is an exact half between on_notch (lower) and next_notch (higher).
      // The library rounds in VALUE space (generic.hpp round_quotient): snap
      // and `none` truncate TOWARD ZERO (not floor — that's round_floor), and
      // round_nearest rounds HALF AWAY FROM ZERO. So the expected notch is the
      // smaller-|·| candidate for snap/none and the larger-|·| candidate for
      // round_nearest. For mid >= 0 these are on_notch / next_notch; for mid < 0
      // they swap — which is what the negative-Lower grids exercise.
      const bool on_smaller = bnd::detail::abs(on_notch) <= bnd::detail::abs(next_notch);
      rational toward_zero = on_smaller ? on_notch : next_notch;   // snap / none
      rational away_zero   = on_smaller ? next_notch : on_notch;   // round_nearest
      const bool tz_in = toward_zero >= lo && toward_zero <= hi;
      const bool az_in = away_zero   >= lo && away_zero   <= hi;

      // Default-policy (checked) B: must throw rounding_error (sign-independent).
      FUZZ_REQUIRE(s, throws_with(errc::rounding_error, [&]{
        B b; b = mid;
      }));

      // snap: silent truncate toward zero.
      if (tz_in)
        FUZZ_REQUIRE(s, !throws_with(errc::rounding_error, [&]{
          BIR b; b = mid;
          rational got = b;
          FUZZ_REQUIRE(s, got == toward_zero);
        }));

      // round_nearest: silent round half away from zero.
      if (az_in)
        FUZZ_REQUIRE(s, !throws_with(errc::rounding_error, [&]{
          BRN b; b = mid;
          rational got = b;
          FUZZ_REQUIRE(s, got == away_zero);
        }));

      // Truly-unchecked policy (none): silent truncate toward zero (assignment.hpp).
      if (tz_in)
        FUZZ_REQUIRE(s, !throws_with(errc::rounding_error, [&]{
          BNONE b; b = mid;
          rational got = b;
          FUZZ_REQUIRE(s, got == toward_zero);
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
  if constexpr (!IsIntegerAligned<B> && !rational_raw<B>)
  {
    s.current_prop = "subnormal_construct";
    using BIR = bound<Grid<B>, snap>;
    rational lo = Lower<B>;
    rational hi = Upper<B>;
    bool zero_in_range = (lo <= 0) && (hi >= 0);
    if (!zero_in_range) return;
    std::uniform_real_distribution<double> mantissa(1.0, 2.0);
    std::uniform_int_distribution<int>     exp_dist(-300, -70);
    for (long i = 0; i < iters; ++i)
    {
      s.iter = i;
      double v = mantissa(s.rng) * std::pow(2.0, exp_dist(s.rng));
      // snap → silent floor; should not throw and should land on 0.
      FUZZ_REQUIRE(s, !throws_with(errc::rounding_error, [&]{
        BIR b; b = v;
        rational got = b;
        // Floored toward lo, but for tiny v near zero the floor is 0 (or lo).
        FUZZ_REQUIRE(s, got == 0 || got == lo);
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
    B sentinel_b = B::make_sentinel();
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
  if constexpr (!rational_raw<B>)
  {
    s.current_prop = "compound_add_same_bound";
    for (long i = 0; i < iters; ++i)
    {
      s.iter = i;
      B a = B::from_raw(random_in_range_raw<B>(s.rng));
      B delta = B::from_raw(random_in_range_raw<B>(s.rng));
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
  // Targets addition.hpp:48-63 (the rational_raw<result> branches) and
  // multiplication.hpp:44-63 (same for mul). For raw-rational grids (notch=0)
  // arithmetic goes through the rational-add/mul paths directly. Most random
  // small-integer values won't overflow the rational machinery, so we mostly
  // exercise the success branches; occasional out-of-range results land in
  // the nullopt path.
  if constexpr (rational_raw<B>)
  {
    s.current_prop = "raw_rational_arith";
    rational lo = Lower<B>;
    rational hi = Upper<B>;
    std::uniform_int_distribution<imax> num_dist(
        trunc(lo) + 1, trunc(hi) - 1);
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
      B a = B::from_raw(ar);
      B b = B::from_raw(br);
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
            FUZZ_REQUIRE(s, rational{*v} == expected);
        } else {
          FUZZ_REQUIRE(s, rational{v} == expected);
        }
      };
      check(sum,  expect_sum);
      check(prod, expect_prod);
    }
  }
}

// True for fixed-point grids whose notch is 1/2^K — the only ones whose grid
// points (and half-notch midpoints) are exactly representable as doubles, so
// the arithmetic-only cast/predicate APIs can be driven without rounding the
// oracle. (money's 1/100 notch is deliberately excluded.)
template <boundable B>
inline constexpr bool DyadicNotch = []{
  if constexpr (rational_raw<B> || IsIntegerAligned<B>) return false;
  else {
    imax d = abs_den(Notch<B>.Denominator);
    return Notch<B>.Numerator == 1 && (d & (d - 1)) == 0;
  }
}();

template <boundable B>
void prop_casts(fuzz_state& s, long iters)
{
  // Free-function cast API on integer grids: exact integer oracles.
  if constexpr (IsIntegerAligned<B> && !rational_raw<B>)
  {
    s.current_prop = "casts";
    auto lo = trunc(Lower<B>);
    auto hi = trunc(Upper<B>);
    imax range = hi - lo + 1;
    imax span = range * 3 + 100;
    for (long i = 0; i < iters; ++i)
    {
      s.iter = i;
      imax v = lo + random_wide_int(s.rng, span);
      imax cl = std::clamp<imax>(v, lo, hi);
      FUZZ_REQUIRE(s, clamp_cast<B>(v) == cl);
      imax wexpect = ((v - lo) % range + range) % range + lo;
      FUZZ_REQUIRE(s, wrap_cast<B>(v) == wexpect);
      // notch == 1, so floor/ceil/round are no-ops after the clamp.
      FUZZ_REQUIRE(s, clamp_floor<B>(v) == cl);
      FUZZ_REQUIRE(s, clamp_ceil<B>(v)  == cl);
      FUZZ_REQUIRE(s, clamp_round<B>(v) == cl);
      if (v >= lo && v <= hi)
      {
        FUZZ_REQUIRE(s, checked_cast<B>(v)   == v);
        FUZZ_REQUIRE(s, unchecked_cast<B>(v) == v);
      }
      else
        FUZZ_REQUIRE(s, throws_with(errc::domain_error,
                                    [&]{ (void)checked_cast<B>(v); }));
    }
  }
}

template <boundable B>
void prop_casts_fixed(fuzz_state& s, long iters)
{
  // clamp_floor / clamp_ceil on dyadic fixed-point grids: a half-notch
  // midpoint floors to the notch below and ceils to the notch above.
  if constexpr (DyadicNotch<B>)
  {
    s.current_prop = "casts_fixed";
    rational notch = Notch<B>;
    rational lo    = Lower<B>;
    rational half  = (notch / 2_r).value();
    for (long i = 0; i < iters; ++i)
    {
      s.iter = i;
      std::uniform_int_distribution<umax> dist(0, NotchCount<B> - 1);
      umax k = dist(s.rng);
      rational on_notch = (lo + (rational{k} * notch).value()).value();
      rational next     = (on_notch + notch).value();
      double   mid      = static_cast<double>((on_notch + half).value());
      FUZZ_REQUIRE(s, to_rational(clamp_floor<B>(mid)) == on_notch);
      FUZZ_REQUIRE(s, to_rational(clamp_ceil<B>(mid))  == next);
    }
  }
}

template <boundable B>
void prop_predicates(fuzz_state& s, long iters)
{
  // will_conversion_* / is_conversion_lossy must agree with the actual
  // checked conversion outcome.
  if constexpr (IsIntegerAligned<B> && !rational_raw<B>)
  {
    s.current_prop = "predicates";
    auto lo = trunc(Lower<B>);
    auto hi = trunc(Upper<B>);
    imax span = (hi - lo) * 3 + 100;
    for (long i = 0; i < iters; ++i)
    {
      s.iter = i;
      imax v = lo + random_wide_int(s.rng, span);
      bool in_range = (v >= lo && v <= hi);
      FUZZ_REQUIRE(s, will_conversion_overflow<B>(v) == !in_range);
      FUZZ_REQUIRE(s, !will_conversion_trunc<B>(v));      // integer grid never truncates
      bool lossy = is_conversion_lossy<B>(v);
      bool threw = throws_with_any({errc::domain_error, errc::rounding_error},
                                   [&]{ (void)checked_cast<B>(v); });
      FUZZ_REQUIRE(s, lossy == threw);
    }
  }
  else if constexpr (DyadicNotch<B>)
  {
    s.current_prop = "predicates";
    rational notch = Notch<B>;
    rational lo    = Lower<B>;
    rational hi    = Upper<B>;
    rational half  = (notch / 2_r).value();
    for (long i = 0; i < iters; ++i)
    {
      s.iter = i;
      std::uniform_int_distribution<umax> dist(0, NotchCount<B>);
      umax k = dist(s.rng);
      double on_notch = static_cast<double>((lo + (rational{k} * notch).value()).value());
      // On-notch, in range: nothing lost.
      FUZZ_REQUIRE(s, !will_conversion_overflow<B>(on_notch));
      FUZZ_REQUIRE(s, !will_conversion_trunc<B>(on_notch));
      FUZZ_REQUIRE(s, !is_conversion_lossy<B>(on_notch));
      // Half-notch midpoint, in range: truncates (lossy) but does not overflow.
      if (k < NotchCount<B>)
      {
        double mid = on_notch + static_cast<double>(half);
        FUZZ_REQUIRE(s, !will_conversion_overflow<B>(mid));
        FUZZ_REQUIRE(s, will_conversion_trunc<B>(mid));
        FUZZ_REQUIRE(s, is_conversion_lossy<B>(mid));
        FUZZ_REQUIRE(s, throws_with(errc::rounding_error,
                                    [&]{ (void)checked_cast<B>(mid); }));
      }
      // Above the top notch: overflow (lossy).
      double over = static_cast<double>((hi + notch).value());
      FUZZ_REQUIRE(s, will_conversion_overflow<B>(over));
      FUZZ_REQUIRE(s, is_conversion_lossy<B>(over));
    }
  }
}

template <boundable B>
void prop_range(fuzz_state& s, long iters)
{
  // bound_range walks every notch slot exactly once; a mid-range start rotates
  // the sequence. Capped so the billion-wide catalogue grids stay fast.
  if constexpr (!rational_raw<B>)
  {
    constexpr umax N = NotchCount<B> + 1;
    if constexpr (N <= 100000)
    {
      s.current_prop = "range";
      using R = bound_range<Grid<B>>;
      long it = std::min<long>(iters, 200);
      std::uniform_int_distribution<umax> kdist(0, N - 1);
      for (long i = 0; i < it; ++i)
      {
        s.iter = i;
        umax k = kdist(s.rng);
        rational start = (Lower<B> + (rational{k} * Notch<B>).value()).value();
        R r{typename R::value_type{start}};
        FUZZ_REQUIRE(s, r.size() == N);
        umax idx = 0;
        for (auto b : r)
        {
          umax slot = (k + idx) % N;
          rational expect = (Lower<B> + (rational{slot} * Notch<B>).value()).value();
          FUZZ_REQUIRE(s, to_rational(b) == expect);
          // random-access indexing agrees with the sequential walk.
          FUZZ_REQUIRE(s, to_rational(r.begin()[static_cast<imax>(idx)]) == expect);
          ++idx;
        }
        FUZZ_REQUIRE(s, idx == N);
        FUZZ_REQUIRE(s, r.begin() + static_cast<imax>(N) == r.end());
      }
    }
  }
}

//---------------------------------------------------------------------------
// Standalone cmath properties — dedicated grids satisfying each function's
// domain/notch static_asserts. Oracles use <cmath> (test-only floating point).
//---------------------------------------------------------------------------
void prop_cmath_exact(fuzz_state& s, long iters)
{
  // abs/floor/ceil/round/trunc/fmod against exact rational oracles.
  using M = bound<{{-8, 8}, notch<1, 16384>}, round_nearest>;
  s.current_grid = "cmath";
  s.current_prop = "cmath_exact";
  for (long i = 0; i < iters; ++i)
  {
    s.iter = i;
    M x = M::from_raw(random_in_range_raw<M>(s.rng));
    M y = M::from_raw(random_in_range_raw<M>(s.rng));
    rational xr = x, yr = y;
    FUZZ_REQUIRE(s, rational{math::abs(x)}   == bnd::detail::abs(xr));
    FUZZ_REQUIRE(s, rational{math::floor(x)} == rational{floor(xr)});
    FUZZ_REQUIRE(s, rational{math::trunc(x)} == rational{trunc(xr)});
    FUZZ_REQUIRE(s, rational{math::round(x)} == rational{round(xr)});
    FUZZ_REQUIRE(s, rational{math::ceil(x)}  == -rational{floor((-xr))});
    if (yr != 0)
    {
      // Truncated-division remainder: x - trunc(x/y)*y (matches std::fmod).
      rational q   = (xr / yr).value();
      rational rem = (xr - (rational{trunc(q)} * yr).value()).value();
      FUZZ_REQUIRE(s, to_rational(math::fmod(x, y)) == rem);
    }
  }
}

void prop_sqrt(fuzz_state& s, long iters)
{
  using In  = bound<{{0, 4}, notch<1, 65536>}, round_nearest | real>;
  using Out = bound<{{0, 2}, notch<1, 16384>}, round_nearest>;
  s.current_grid = "cmath";
  s.current_prop = "sqrt";
  const rational tol{4, 16384};            // ~2 output notches (double-rounding)
  for (long i = 0; i < iters; ++i)
  {
    s.iter = i;
    In x = In::from_raw(random_in_range_raw<In>(s.rng));
    rational xr = x;
    rational rr = Out{math::sqrt(x)};
    double   oracle = std::sqrt(static_cast<double>(xr));
    FUZZ_REQUIRE(s, approx_le(rr, rational{oracle}, tol));
  }

  // Mixed-sign overload returns slim::expected (domain_error for negatives).
  s.current_prop = "sqrt_signed";
  using SIn = bound<{{-1, 1}, notch<1, 65536>}, round_nearest | real>;
  for (long i = 0; i < iters; ++i)
  {
    s.iter = i;
    SIn x = SIn::from_raw(random_in_range_raw<SIn>(s.rng));
    rational xr = x;
    auto exp = math::sqrt(x);
    FUZZ_REQUIRE(s, exp.has_value() == (xr >= 0));
    if (exp.has_value())
      FUZZ_REQUIRE(s, approx_le(rational{*exp},
                                rational{std::sqrt(static_cast<double>(xr))}, tol));
    else
      FUZZ_REQUIRE(s, exp.error() == errc::domain_error);
  }
}

void prop_sin_cos(fuzz_state& s, long iters)
{
  using A = bound<{{-8, 8}, notch<1, 16384>}, round_nearest | real>;
  s.current_grid = "cmath";
  s.current_prop = "sin_cos";
  const rational tol{8, 16384};
  const rational id_tol{64, 16384};
  for (long i = 0; i < iters; ++i)
  {
    s.iter = i;
    A a = A::from_raw(random_in_range_raw<A>(s.rng));
    double   ad = a;
    rational sn = math::sin(a);
    rational cs = math::cos(a);
    FUZZ_REQUIRE(s, approx_le(sn, rational{std::sin(ad)}, tol));
    FUZZ_REQUIRE(s, approx_le(cs, rational{std::cos(ad)}, tol));
    rational sum = ((sn * sn).value() + (cs * cs).value()).value();
    FUZZ_REQUIRE(s, approx_le(sum, 1_r, id_tol));
  }
}

void prop_tan(fuzz_state& s, long iters)
{
  using A = bound<{{-8, 8}, notch<1, 16384>}, round_nearest | real>;
  s.current_grid = "cmath";
  s.current_prop = "tan";
  const rational tol{16, 1024};
  for (long i = 0; i < iters; ++i)
  {
    s.iter = i;
    A a = A::from_raw(random_in_range_raw<A>(s.rng));
    double ad = a;
    auto   t  = math::tan(a);
    if (t.has_value())
    {
      double o = std::tan(ad);
      // Skip the near-pole region where the fixed-point approximation diverges
      // faster than the tolerance; the in-range result there is still valid.
      if (std::abs(o) <= 8.0)
        FUZZ_REQUIRE(s, approx_le(rational{*t}, rational{o}, tol));
    }
    else
      FUZZ_REQUIRE(s, t.error() == errc::division_by_zero
                   || t.error() == errc::overflow);
  }
}

void prop_exp_log(fuzz_state& s, long iters)
{
  s.current_grid = "cmath";

  {
    using In = bound<{{-4, 4}, notch<1, 16384>}, round_nearest | real>;
    s.current_prop = "exp2";
    for (long i = 0; i < iters; ++i)
    {
      s.iter = i;
      In x = In::from_raw(random_in_range_raw<In>(s.rng));
      double xd = x;
      FUZZ_REQUIRE(s, approx_rel(rational{math::exp2(x)}, std::exp2(xd),
                                 0.01, rational{8, 16384}));
    }
  }
  {
    using In = bound<{{0x1p-8_r, 256}, notch<1, 16384>}, round_nearest | real>;
    s.current_prop = "log2";
    for (long i = 0; i < iters; ++i)
    {
      s.iter = i;
      In x = In::from_raw(random_in_range_raw<In>(s.rng));
      double xd = x;
      FUZZ_REQUIRE(s, approx_le(rational{math::log2(x)},
                                rational{std::log2(xd)}, rational{16, 16384}));
    }
  }
  {
    using In = bound<{{-10, 10}, notch<1, 16384>}, round_nearest | real>;
    s.current_prop = "exp";
    for (long i = 0; i < iters; ++i)
    {
      s.iter = i;
      In x = In::from_raw(random_in_range_raw<In>(s.rng));
      double xd = x;
      FUZZ_REQUIRE(s, approx_rel(rational{math::exp(x)}, std::exp(xd),
                                 0.01, rational{4, 256}));
    }
  }
  {
    using In = bound<{{0x1p-8_r, 256}, notch<1, 256>}, round_nearest | real>;
    s.current_prop = "log";
    for (long i = 0; i < iters; ++i)
    {
      s.iter = i;
      In x = In::from_raw(random_in_range_raw<In>(s.rng));
      double xd = x;
      // log's auto output is Q.8 (notch 1/256), so accuracy is a few Q.8 ULP.
      FUZZ_REQUIRE(s, approx_le(rational{math::log(x)},
                                rational{std::log(xd)}, rational{8, 256}));
    }
  }
  {
    using In = bound<{{-9, 9}, notch<1, 16384>}, round_nearest | real>;
    s.current_prop = "pow_base";
    for (long i = 0; i < iters; ++i)
    {
      s.iter = i;
      In x = In::from_raw(random_in_range_raw<In>(s.rng));
      double xd = x;
      double o  = std::pow(10.0, xd);
      if (o > 60000.0) continue;            // stay inside the output grid range
      FUZZ_REQUIRE(s, approx_rel(rational{math::pow_base<10>(x)}, o,
                                 0.01, rational{4, 256}));
    }
  }
}

void prop_atan2(fuzz_state& s, long iters)
{
  using In = bound<{{-1, 1}, notch<1, 16384>}, round_nearest | real>;
  s.current_grid = "cmath";
  s.current_prop = "atan2";
  const rational tol{16, 16384};          // radians: a few output notches
  const double   pi = std::numbers::pi_v<double>;
  for (long i = 0; i < iters; ++i)
  {
    s.iter = i;
    In y = In::from_raw(random_in_range_raw<In>(s.rng));
    In x = In::from_raw(random_in_range_raw<In>(s.rng));
    rational yr = y, xr = x;
    if (yr == 0 && xr == 0) continue;       // degenerate
    double rad = std::atan2(static_cast<double>(yr), static_cast<double>(xr));
    if (std::abs(rad) > 0.98 * pi) continue; // ±π wraparound boundary
    FUZZ_REQUIRE(s, approx_le(rational{math::atan2(y, x)}, rational{rad}, tol));
  }
}

// Extended transcendentals (#3): inverse trig (radians), hyperbolic, log10,
// cbrt, hypot, pow. Each is checked against the std::<cmath> oracle in double.
void prop_extended_math(fuzz_state& s, long iters)
{
  s.current_grid = "cmath";
  const rational tol{16, 16384};

  // Inverse trig — input [-1, 1], output radians.
  {
    using In = bound<{{-1, 1}, notch<1, 65536>}, round_nearest | real>;
    for (long i = 0; i < iters; ++i)
    {
      s.iter = i;
      In x = In::from_raw(random_in_range_raw<In>(s.rng));
      double xd = x;
      s.current_prop = "atan";
      FUZZ_REQUIRE(s, approx_le(rational{math::atan(x)}, rational{std::atan(xd)}, tol));
      s.current_prop = "asin";
      FUZZ_REQUIRE(s, approx_le(rational{math::asin(x)}, rational{std::asin(xd)}, tol));
      s.current_prop = "acos";
      FUZZ_REQUIRE(s, approx_le(rational{math::acos(x)}, rational{std::acos(xd)}, tol));
    }
  }

  // Hyperbolic — input [-10, 10].
  {
    using In = bound<{{-10, 10}, notch<1, 65536>}, round_nearest | real>;
    for (long i = 0; i < iters; ++i)
    {
      s.iter = i;
      In x = In::from_raw(random_in_range_raw<In>(s.rng));
      double xd = x;
      s.current_prop = "sinh";
      FUZZ_REQUIRE(s, approx_rel(rational{math::sinh(x)}, std::sinh(xd), 0.01, tol));
      s.current_prop = "cosh";
      FUZZ_REQUIRE(s, approx_rel(rational{math::cosh(x)}, std::cosh(xd), 0.01, tol));
      s.current_prop = "tanh";
      FUZZ_REQUIRE(s, approx_le(rational{math::tanh(x)}, rational{std::tanh(xd)}, tol));
    }
  }

  // log10 — input (0, 1024].
  {
    using In = bound<{{1, 1024}, notch<1, 65536>}, round_nearest | real>;
    s.current_prop = "log10";
    for (long i = 0; i < iters; ++i)
    {
      s.iter = i;
      In x = In::from_raw(random_in_range_raw<In>(s.rng));
      double xd = x;
      FUZZ_REQUIRE(s, approx_le(rational{math::log10(x)}, rational{std::log10(xd)}, tol));
    }
  }

  // cbrt — signed input.
  {
    using In = bound<{{-16, 16}, notch<1, 65536>}, round_nearest | real>;
    s.current_prop = "cbrt";
    for (long i = 0; i < iters; ++i)
    {
      s.iter = i;
      In x = In::from_raw(random_in_range_raw<In>(s.rng));
      double xd = x;
      FUZZ_REQUIRE(s, approx_rel(rational{math::cbrt(x)}, std::cbrt(xd), 0.01, tol));
    }
  }

  // hypot — two signed inputs.
  {
    using In = bound<{{-16, 16}, notch<1, 65536>}, round_nearest | real>;
    s.current_prop = "hypot";
    for (long i = 0; i < iters; ++i)
    {
      s.iter = i;
      In x = In::from_raw(random_in_range_raw<In>(s.rng));
      In y = In::from_raw(random_in_range_raw<In>(s.rng));
      double xd = x;
      double yd = y;
      FUZZ_REQUIRE(s, approx_rel(rational{math::hypot(x, y)}, std::hypot(xd, yd), 0.01, tol));
    }
  }

  // pow — positive base, returns expected.
  {
    using B = bound<{{1, 16}, notch<1, 65536>}, round_nearest | real>;
    using E = bound<{{-4, 8}, notch<1, 65536>}, round_nearest | real>;
    s.current_prop = "pow";
    for (long i = 0; i < iters; ++i)
    {
      s.iter = i;
      B b = B::from_raw(random_in_range_raw<B>(s.rng));
      E e = E::from_raw(random_in_range_raw<E>(s.rng));
      double bd = b;
      double ed = e;
      double o  = std::pow(bd, ed);
      auto r = math::pow(b, e);
      if (r.has_value() && o < 1e6)
        FUZZ_REQUIRE(s, approx_rel(rational{*r}, o, 0.02, rational{8, 256}));
    }
  }
}

// Wrap on fractional / notch grids with a boundable rhs (#6). The integer
// fast path only fires on unit-integer grids; these destinations route through
// the rational modular wrap. Oracle: the wrapped value must land in range and
// within one notch of v reduced modulo the period (Upper - Lower + Notch).
void prop_wrap_fractional(fuzz_state& s, long iters)
{
  s.current_grid = "wrap_frac";

  auto check = [&](auto dst_tag, auto src_tag, const char* name, rational period,
                   rational lo, rational hi, rational notch)
  {
    using Dst = typename decltype(dst_tag)::type;
    using Src = typename decltype(src_tag)::type;
    s.current_prop = name;
    for (long i = 0; i < iters; ++i)
    {
      s.iter = i;
      Src src = Src::from_raw(random_in_range_raw<Src>(s.rng));
      rational v = src;
      Dst d{src};
      rational out = d;
      // In range.
      FUZZ_REQUIRE(s, out >= lo && out <= hi);
      // out ≡ v (mod period): (v - out)/period is (near) an integer.
      rational shifted = (v - lo).value();
      imax q = floor((shifted / period).value());
      rational reduced = (v - (rational{q} * period).value()).value();
      FUZZ_REQUIRE(s, bnd::detail::abs((out - reduced).value()) <= notch);
    }
  };

  struct A { using type = bound<{{0, 1}, notch<1, 4>}, wrap | round_nearest>; };
  struct B1{ using type = bound<{{-2, 2}, notch<1, 4>}, round_nearest>; };
  check(A{}, B1{}, "wf_0_1_q4", rational{5, 4}, rational{0}, rational{1}, rational{1, 4});

  struct C { using type = bound<{{rational{1,2}, rational{5,2}}, notch<1,2>}, wrap | round_nearest>; };
  struct D { using type = bound<{{-4, 4}, notch<1, 2>}, round_nearest>; };
  check(C{}, D{}, "wf_half_q2", rational{5, 2}, rational{1, 2}, rational{5, 2}, rational{1, 2});
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
  guarded(s, [&]{ prop_compound_add_bound<B>(s, iters); });
  guarded(s, [&]{ prop_modulo<B>(s, iters); });
  guarded(s, [&]{ prop_increment_wrap<B>(s, iters); });
  guarded(s, [&]{ prop_div_by_zero<B>(s, iters); });
  guarded(s, [&]{ prop_spaceship_symmetry<B>(s, iters); });
  guarded(s, [&]{ prop_compound_div_mod_zero<B>(s, iters); });
  guarded(s, [&]{ prop_compound_bound_overshoot<B>(s, iters); });
  guarded(s, [&]{ prop_compound_add_same_bound<B>(s, iters); });
  guarded(s, [&]{ prop_non_notch_assign<B>(s, iters); });
  guarded(s, [&]{ prop_subnormal_construct<B>(s, iters); });
  guarded(s, [&]{ prop_raw_rational_arith<B>(s, iters); });
  guarded(s, [&]{ prop_casts<B>(s, iters); });
  guarded(s, [&]{ prop_casts_fixed<B>(s, iters); });
  guarded(s, [&]{ prop_predicates<B>(s, iters); });
  guarded(s, [&]{ prop_range<B>(s, iters); });
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
  run_props<bound<{{-1, 1}, notch<1, 16384>}>>  (s, iters, "Q1.14");
  run_props<bound<{{0, 65535}, notch<1, 65536>}>>(s, iters, "Q16.16");
  run_props<bound<{{0, 1'000'000}, notch<1, 100>}>>(s, iters, "money");
  // Extended catalogue: extreme/asymmetric/tiny shapes through every property.
  run_props<bound<{0, 1'000'000'000}>>          (s, iters, "u1G");
  run_props<bound<{-1'000'000'000, 1'000'000'000}>>(s, iters, "s1G");
  run_props<bound<{{-7, 11}, 0.25}>>            (s, iters, "asym_q");
  // Fully-negative ranges: every notch is exercised with both round candidates
  // < 0, the strongest test of toward-zero / half-away-from-zero rounding.
  run_props<bound<{-100, -10}>>                 (s, iters, "neg_int");
  run_props<bound<{{-20, -5}, 0.25}>>           (s, iters, "neg_q");
  run_props<bound<{{-13, 5}, notch<1, 8>}>>     (s, iters, "asym_q8");
  run_props<bound<{{-50, -10}, 0.5}>>           (s, iters, "neg_half");
  run_props<bound<{0, 3}>>                      (s, iters, "tiny");
  run_props<bound<{-1, 1}>>                     (s, iters, "unit_signed");
  run_props<bound<{{0, 4}, notch<1, 65536>}>>     (s, iters, "Q_sqrt");
  // Raw-rational grid (notch=0): exercises the rational-storage branches in
  // addition / multiplication / assignment. Default `checked` policy goes
  // through the overflow-checked rational arithmetic; the `none` variant
  // exercises rational::add_unchecked / mul_unchecked.
  run_props<bound<{{-1000, 1000}, 0}>>          (s, iters, "raw_rat");
  run_props<bound<{{-1000, 1000}, 0}, none>>    (s, iters, "raw_rat_unck");

  // Standalone (non-grid) properties.
  guarded(s, [&]{ prop_interval_eq(s); });
  guarded(s, [&]{ prop_optional_throws(s); });

  // Standalone cmath properties (dedicated domain grids).
  guarded(s, [&]{ prop_cmath_exact(s, iters); });
  guarded(s, [&]{ prop_sqrt(s, iters); });
  guarded(s, [&]{ prop_sin_cos(s, iters); });
  guarded(s, [&]{ prop_tan(s, iters); });
  guarded(s, [&]{ prop_exp_log(s, iters); });
  guarded(s, [&]{ prop_atan2(s, iters); });
  guarded(s, [&]{ prop_extended_math(s, iters); });
  guarded(s, [&]{ prop_wrap_fractional(s, iters); });

  // Cross-grid arithmetic: mix grids of different lower/upper but same notch.
  guarded(s, [&]{ prop_cross_add<bound<{0, 100}>, bound<{-100, 100}>>(s, iters, "u100+s100"); });
  guarded(s, [&]{ prop_cross_add<bound<{0, 100}>, bound<{0, 1000}>>(s, iters, "u100+u1k"); });
  guarded(s, [&]{ prop_cross_add<bound<{-50, 50}>, bound<{0, 100}>>(s, iters, "s50+u100"); });
  guarded(s, [&]{ prop_cross_add<bound<{{0, 50}, 0.5}>, bound<{{-50, 50}, 0.5}>>(s, iters, "u50half+s50half"); });

  std::cout << "passed=" << s.passed << " failed=" << s.failed << "\n";
  return (s.failed > 0) ? 1 : 0;
}
