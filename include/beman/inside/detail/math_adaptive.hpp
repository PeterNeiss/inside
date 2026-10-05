// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_DETAIL_MATH_ADAPTIVE_HPP
#define BEMAN_INSIDE_DETAIL_MATH_ADAPTIVE_HPP

#include <beman/inside/inside.hpp>

#include <bit>

#include <cstddef>
#include <expected>
#include <optional>

//---------------------------------------------------------------------------
// The adaptive math engine's foundation: correctly rounded results on any
// output grid, at a precision chosen from that grid.
//
// A core computes f(x) in signed fixed point Q.W (an integer Y standing for
// Y·2^-W) and says how far off it may be: the true value lies within
// Error·2^-W of Y (an `approx`). `decide` maps both ends of that interval onto
// the output grid under the grid's rounding mode. When they land on the same
// slot, that slot is the correctly rounded result — whichever core, tier or
// precision produced it. When they straddle a rounding boundary, the driver
// (`evaluate`) runs the core again at twice the precision (Ziv's strategy).
//
// Transcendental results land exactly on a boundary only at a few exact
// inputs (exp(0), log(1), ...); the cores return those with Error 0, so for
// every other input the escalation ends. A cap stops it in any case; past the
// cap the result is the slot nearest Y.
//
// Everything here is constexpr and integer-only (wide_int), so a result is the
// same at compile time, at runtime, with or without an FPU.
//---------------------------------------------------------------------------
namespace beman::inside::math::detail::ax
{
  using namespace ::beman::inside::detail;

  // Signed fixed point with at least Bits value bits (the word of a Q.W value
  // whose magnitude stays below 2^(Bits − W)).
  template <int Bits>
  using fixed_t = wide_sint<limbs_for_bits(Bits + 1)>;

  template <typename T>
  inline constexpr std::size_t limbs_of = sizeof(T) / sizeof(umax);

  // The same value in a word of twice the limbs (for products and shifted
  // dividends).
  template <std::size_t K>
  using double_t = wide_sint<2 * K>;

  //---------------------------------------------------------------------------
  // Q.W arithmetic. Products and quotients truncate toward zero: each is
  // within one unit 2^-W of the true value, and a series' shrinking terms
  // reach 0.
  //---------------------------------------------------------------------------
  template <std::size_t K>
  constexpr wide_sint<K> mul_q(wide_sint<K> const& a, wide_sint<K> const& b, int W) noexcept
  {
    if constexpr (K == 1)
    {
      // One limb: the 128-bit product of the magnitudes, shifted.
      const bool na = a.negative(), nb = b.negative();
      const umax ma = na ? umax{0} - a.Word[0] : a.Word[0];
      const umax mb = nb ? umax{0} - b.Word[0] : b.Word[0];
      const limb::pair<umax> p = limb::mul(ma, mb);
      const umax m = W == 0 ? p.Lo : W < 64 ? (p.Lo >> W) | (p.Hi << (64 - W)) : p.Hi >> (W - 64);
      wide_sint<1> r;
      r.Word[0] = (na != nb) ? umax{0} - m : m;
      return r;
    }
    else
    {
      // Schoolbook on the magnitudes into 2K limbs, then the shift.
      const bool na = a.negative(), nb = b.negative();
      const wide_uint<K> ma{na ? -a : a}, mb{nb ? -b : b};
      umax prod[2 * K]{};
      for (std::size_t i = 0; i < K; ++i)
      {
        umax carry = 0;
        for (std::size_t j = 0; j < K; ++j)
        {
          const limb::pair<umax> t = limb::mul(ma.Word[i], mb.Word[j]);
          umax c1 = 0, c2 = 0;
          umax v = limb::add_carry(prod[i + j], t.Lo, c1);
          v = limb::add_carry(v, carry, c2);
          prod[i + j] = v;
          carry = t.Hi + c1 + c2;
        }
        prod[i + K] = carry;
      }
      const std::size_t ws = static_cast<std::size_t>(W / 64);
      const int bs = W % 64;
      wide_uint<K> m;
      for (std::size_t i = 0; i < K; ++i)
      {
        const umax lo = i + ws < 2 * K ? prod[i + ws] : 0;
        const umax hi = i + ws + 1 < 2 * K ? prod[i + ws + 1] : 0;
        m.Word[i] = bs == 0 ? lo : (lo >> bs) | (hi << (64 - bs));
      }
      const wide_sint<K> r{m};
      return na != nb ? -r : r;
    }
  }

  // v / d for a small positive d, truncating toward zero (series terms).
  template <std::size_t K>
  constexpr wide_sint<K> div_small(wide_sint<K> const& v, umax d) noexcept;

  // a / b at scale 2^W. Pre: b != 0.
  template <std::size_t K>
  constexpr wide_sint<K> div_q(wide_sint<K> const& a, wide_sint<K> const& b, int W) noexcept
  {
    using D = double_t<K>;
    return static_cast<wide_sint<K>>((D{a} << W) / D{b});
  }

  // 2^W as a Q.W one.
  template <std::size_t K>
  constexpr wide_sint<K> one_q(int W) noexcept { return wide_sint<K>{1} << W; }

  // v rounded to the nearest multiple of 2^-sh (half away from zero), shifted
  // down by sh: changes the scale from 2^(W+sh) to 2^W within ½ unit.
  template <std::size_t K>
  constexpr wide_sint<K> round_shift(wide_sint<K> const& v, int sh) noexcept
  {
    if (sh <= 0) return v << (-sh);
    const wide_sint<K> half = wide_sint<K>{1} << (sh - 1);
    return v.negative() ? -((-v + half) >> sh) : (v + half) >> sh;
  }

  //---------------------------------------------------------------------------
  // Exact values into fixed point.
  //---------------------------------------------------------------------------
  // x·2^W rounded to the nearest integer (half away from zero): within ½ unit.
  template <int W, std::size_t K, std::size_t E>
  constexpr wide_sint<K> to_q(exact_frac<E> const& x) noexcept
  {
    constexpr int shift = W >= 0 ? W : -W;
    using I = wide_sint<exact_max<E, K> + limbs_for_bits(shift) + 1>;
    I n{x.Num}, d{x.Den};
    if constexpr (W >= 0) n = n << W; else d = d << shift;
    return static_cast<wide_sint<K>>(rounded_div<round_mode::nearest>(n, d));
  }

  // ⌊log2 |x|⌋ for x != 0.
  template <std::size_t E>
  constexpr int floor_log2(exact_frac<E> const& x) noexcept
  {
    using I = wide_sint<E>;
    const I n = x.Num.negative() ? -x.Num : x.Num;
    const I& d = x.Den;
    int e = bit_width_of(n) - bit_width_of(d);
    // n/d ≥ 2^e  ⇔  n ≥ d·2^e (or n·2^-e ≥ d).
    const bool ge = e >= 0 ? n >= (d << e) : (n << (-e)) >= d;
    return ge ? e : e - 1;
  }

  //---------------------------------------------------------------------------
  // Output precision. out_bits: fractional bits that resolve Out's notch
  // (2^-out_bits ≤ notch/2). mag_bits: integer bits of Out's largest value.
  //---------------------------------------------------------------------------
  template <insidable Out>
  inline constexpr int out_bits = [] {
    if constexpr (notch_of<Out> == 0)
      return 64;
    else
    {
      // notch = p/q: need 2^-b ≤ p/(2q), i.e. b ≥ log2(2q/p).
      const grid_wide p = wide_numerator(notch_of<Out>), q = wide_denominator(notch_of<Out>);
      const int b = bit_width_of(q) - bit_width_of(p) + 2;
      return b > 0 ? b : 0;
    }
  }();

  template <insidable Out>
  inline constexpr int mag_bits = grid_magnitude_bits<Out> > 0 ? grid_magnitude_bits<Out> : 0;

  //---------------------------------------------------------------------------
  // approx — a core's result: the true value lies in
  // [(Value − Error)·2^-Scale, (Value + Error)·2^-Scale]. Error 0 means exact.
  //---------------------------------------------------------------------------
  template <std::size_t K>
  struct approx
  {
    wide_sint<K> Value;
    int Scale;
    umax Error;
  };

  //---------------------------------------------------------------------------
  // decide — the slot offset of an approx on Out's grid under Out's rounding
  // mode, when both ends of its error interval round to the same slot.
  //---------------------------------------------------------------------------
  template <insidable Out>
  inline constexpr round_mode out_rounding = rounding_of(policy_of<Out>);

  // Limbs for the exact value Y/2^W of a K-limb approx, with room for the
  // product with Out's notch in exact_index.
  template <insidable Out, std::size_t K>
  inline constexpr std::size_t decide_limbs = K + 1 + exact_limbs<Out>;

  template <insidable Out, std::size_t K>
  struct decision
  {
    bool Decided;
    wide_sint<decide_limbs<Out, K>> Index;               // Out's slot offset (may be out of range)
  };

  template <insidable Out, std::size_t K>
  constexpr exact_frac<decide_limbs<Out, K>> scaled_value(wide_sint<K> const& y, int W) noexcept
  {
    using I = wide_sint<decide_limbs<Out, K>>;
    if (W < 0) return {I{y} << (-W), I{1}};                // a scale past the units: Y·2^|W|
    return {I{y}, I{1} << W};
  }

  // The fast index: a notch p/q with p and q in 64 bits and a scale S ≥ 1.
  // value/notch = Y·q/(p·2^S): split Y·q = A·2^S + R, then A = B·p + r with
  // one-limb divisions; the rounding reads r and R exactly (see below).
  template <insidable Out>
  inline constexpr bool notch_fits64 = [] {
    if constexpr (notch_of<Out> == 0) return false;
    else
    {
      const grid_wide p = wide_numerator(notch_of<Out>), q = wide_denominator(notch_of<Out>);
      return !(grid_wide{std::numeric_limits<umax>::max()} < p) && !(grid_wide{std::numeric_limits<umax>::max()} < q);
    }
  }();

  // n / d for a one-limb d: the quotient and the remainder.
  template <std::size_t K>
  struct small_divmod { wide_uint<K> Quotient; umax Remainder; };

  template <std::size_t K>
  constexpr small_divmod<K> divmod_small(wide_uint<K> const& n, umax d) noexcept
  {
    small_divmod<K> r{wide_uint<K>{0}, 0};
    for (std::size_t i = K; i-- > 0;)
    {
      const limb::pair<umax> qr = limb::div(r.Remainder, n.Word[i], d);
      r.Quotient.Word[i] = qr.Hi;
      r.Remainder = qr.Lo;
    }
    return r;
  }

  template <std::size_t K>
  constexpr wide_sint<K> div_small(wide_sint<K> const& v, umax d) noexcept
  {
    const bool neg = v.negative();
    const wide_sint<K> q{divmod_small(wide_uint<K>{neg ? -v : v}, d).Quotient};
    return neg ? -q : q;
  }

  // Out's slot offset of y·2^-S (S ≥ 1) rounded by M, as value-index rounding
  // (the sign rules of rounded_div).
  template <insidable Out, round_mode M, std::size_t K>
  constexpr wide_sint<K + 2> fast_index(wide_sint<K> const& y, int S) noexcept
  {
    using U = wide_uint<K + 2>;
    using J = wide_sint<K + 2>;
    constexpr umax p = static_cast<umax>(wide_numerator(notch_of<Out>));
    constexpr umax q = static_cast<umax>(wide_denominator(notch_of<Out>));
    const bool neg = y.negative();
    const wide_uint<K> mag{neg ? -y : y};
    U a{0};                                               // |y|·q, one limb at a time
    umax carry = 0;
    for (std::size_t i = 0; i < K; ++i)
    {
      const limb::pair<umax> t = limb::mul(mag.Word[i], q);
      umax c = 0;
      a.Word[i] = limb::add_carry(t.Lo, carry, c);
      carry = t.Hi + c;
    }
    a.Word[K] = carry;
    const U hi = a >> S;
    const U low = a - (hi << S);                          // R, in [0, 2^S)
    const auto [b, r] = divmod_small(hi, p);
    // Fraction of the magnitude past b: (r·2^S + R)/(p·2^S).
    const bool inexact = r != 0 || !low.is_zero();
    bool up = false;
    if constexpr (M == round_mode::floor)     up = neg && inexact;
    else if constexpr (M == round_mode::ceil) up = !neg && inexact;
    else if constexpr (M == round_mode::nearest || M == round_mode::half_even)
    {
      // Compare 2·(r·2^S + R) with p·2^S, i.e. r with d = p − r (no 2r
      // overflow): r > d is past half, r < d − 1 below; r == d is a tie when
      // R = 0, and r == d − 1 leaves the decision to R against 2^(S−1).
      const U half = U{1} << (S - 1);
      const umax d = p - r;
      int cmp;                                            // −1 below half, 0 tie, 1 past
      if (r > d)          cmp = 1;
      else if (r == d)    cmp = low.is_zero() ? 0 : 1;
      else if (r + 1 < d) cmp = -1;
      else                cmp = (low < half) ? -1 : (low == half) ? 0 : 1;
      if constexpr (M == round_mode::nearest) up = cmp >= 0;
      else up = cmp > 0 || (cmp == 0 && (b.Word[0] & 1u) != 0);
    }
    J m{b};
    if (up) m += J{1};
    return (neg ? -m : m) - static_cast<J>(slot_base<Out>);
  }

  template <insidable Out, std::size_t K>
  constexpr decision<Out, K> decide(approx<K> const& a) noexcept
  {
    constexpr round_mode M = out_rounding<Out>;
    using I = wide_sint<decide_limbs<Out, K>>;
    const wide_sint<K> e{a.Error};
    auto index = [&](wide_sint<K> const& y) -> I {
      if constexpr (notch_fits64<Out>)
        if (a.Scale >= 1) return static_cast<I>(fast_index<Out, M>(y, a.Scale));
      return static_cast<I>(exact_index<Out, M>(scaled_value<Out>(y, a.Scale)).Index);
    };
    if (a.Error == 0) return {true, index(a.Value)};
    const I lo = index(a.Value - e), hi = index(a.Value + e);
    // Both ends past the same end of Out's range also decide: the policy
    // (clamp, or an overflow report) sees the same thing for every value there.
    constexpr I count = static_cast<I>(grid_of<Out>.slot_count());
    const bool below = lo.negative() && hi.negative();
    const bool above = count < lo && count < hi;
    return {lo == hi || below || above, lo};
  }

  // The slot nearest the approx's midpoint under Out's rounding (the capped
  // escalation's answer).
  template <insidable Out, std::size_t K>
  constexpr auto nearest_index(approx<K> const& a) noexcept
  {
    using I = wide_sint<decide_limbs<Out, K>>;
    return static_cast<I>(exact_index<Out, out_rounding<Out>>(scaled_value<Out>(a.Value, a.Scale)).Index);
  }

  //---------------------------------------------------------------------------
  // store — Out at slot offset `index`. In range it is a raw; out of range the
  // grid point goes through the policy cascade (clamp, wrap or report), as any
  // assignment. Policy is Out's, optionally with an errc sink.
  //---------------------------------------------------------------------------
  template <insidable Out, std::size_t K, typename P>
  constexpr Out store(wide_sint<K> const& index, P&& policy)
  {
    using I = wide_sint<K>;
    constexpr I count = static_cast<I>(grid_of<Out>.slot_count());
    if (!index.negative() && !(count < index)) [[likely]]
      return Out::from_raw(raw_of_index<Out>(index));
    // The grid point (index + Lower/Notch)·Notch, exact.
    constexpr std::size_t KK = exact_max<K, exact_limbs<Out>>;
    using J = wide_sint<KK>;
    const exact_frac<KK> v{(J{index} + static_cast<J>(slot_base<Out>)) * static_cast<J>(wide_numerator(notch_of<Out>)),
                           static_cast<J>(wide_denominator(notch_of<Out>))};
    Out out{};
    assign_exact<rational>(out, v, policy, no_action{});
    return out;
  }

  template <insidable Out, std::size_t K>
  constexpr Out store(wide_sint<K> const& index)
  { return store<Out>(index, make_policy<policy_of<Out>>()); }

  // An exact value through Out's policy (an output without slots, or an exact
  // result: rounding an exact value by the mode is correct rounding).
  template <insidable Out, std::size_t K, typename P>
  constexpr Out store_exact(exact_frac<K> const& v, P&& policy)
  {
    Out out{};
    assign_exact<rational>(out, v, policy, no_action{});
    return out;
  }

  // An approx's midpoint as an exact value.
  template <std::size_t K>
  constexpr auto midpoint(approx<K> const& a) noexcept
  {
    constexpr std::size_t KK = exact_max<K + 1, exact_min_limbs>;
    using I = wide_sint<KK + 8>;
    return a.Scale >= 0 ? exact_frac<KK + 8>{I{a.Value}, I{1} << a.Scale}
                        : exact_frac<KK + 8>{I{a.Value} << (-a.Scale), I{1}};
  }

  // Outputs `decide` serves: an integer raw on a grid with slots. Others (a
  // rational or floating-point raw) take the value at the start precision.
  template <insidable Out>
  inline constexpr bool slotted = integer_raw<Out> && notch_of<Out> != 0;

  //---------------------------------------------------------------------------
  // evaluate — the Ziv driver. Core is a callable object with a member
  // template `run<W>()` returning an approx within about 2^-W, and optionally
  // `exact()` returning a std::optional exact_frac when the result is an
  // exact rational (taken as is). It starts at W0 and doubles W while the
  // result is undecided, up to Cap.
  //---------------------------------------------------------------------------
  template <insidable Out, int W, int Cap, typename Core, typename P>
  constexpr Out evaluate_from(Core const& core, P&& policy)
  {
    const auto a = core.template run<W>();
    if constexpr (!slotted<Out>)
    {
      // A 64-bit value: keep the denominator a 64-bit power of two.
      constexpr int A = 60 - mag_bits<Out> > 1 ? 60 - mag_bits<Out> : 1;
      if (a.Scale <= A) return store_exact<Out>(midpoint(a), policy);
      using I = decltype(a.Value);
      const int sh = a.Scale - A;
      const I half = I{1} << (sh - 1);
      const I v = a.Value.negative() ? -((-a.Value + half) >> sh) : (a.Value + half) >> sh;
      return store_exact<Out>(midpoint(approx<limbs_of<I>>{v, A, 0}), policy);
    }
    else
    {
      const auto d = decide<Out>(a);
      if (d.Decided) [[likely]] return store<Out>(d.Index, policy);
      if constexpr (2 * W > Cap)
        return store<Out>(nearest_index<Out>(a), policy);
      else
        return evaluate_from<Out, 2 * W, Cap>(core, policy);
    }
  }

  // The precision cap for a start precision W0.
  constexpr int precision_cap(int W0) noexcept { return 8 * W0 > 1024 ? 8 * W0 : 1024; }

  template <insidable Out, int W0, typename Core, typename P>
  constexpr Out evaluate(Core const& core, P&& policy)
  {
    if constexpr (requires { core.exact(); })
      if (const auto e = core.exact())
        return store_exact<Out>(*e, policy);
    return evaluate_from<Out, W0, precision_cap(W0)>(core, policy);
  }

  template <insidable Out, int W0, typename Core>
  constexpr Out evaluate(Core const& core)
  { return evaluate<Out, W0>(core, make_policy<policy_of<Out>>()); }

  // The checked form: a result Out's policy reports (out of range without
  // clamp) comes back as its errc instead.
  template <insidable Out, int W0, typename Core>
  constexpr std::expected<Out, errc> evaluate_checked(Core const& core)
  {
    errc ec{};
    const Out out = evaluate<Out, W0>(core, make_policy<policy_of<Out>>(ec));
    if (ec != errc{}) return std::unexpected{ec};
    return out;
  }

  //---------------------------------------------------------------------------
  // Constants at any precision, computed at compile time with wide integer
  // series. Each is within 1 unit 2^-W of the true value.
  //---------------------------------------------------------------------------
  // Guard bits for a series of about W/2 truncated terms.
  constexpr int series_guard(int W) noexcept { return std::bit_width(static_cast<unsigned>(W)) + 4; }

  // Σ_k s^k / ((2k+1)·n^(2k+1)) at scale 2^G, with s = +1 (atanh(1/n)) or
  // −1 (atan(1/n)). Each term truncates once (p) and once (p/(2k+1)).
  template <std::size_t K>
  constexpr wide_sint<K> inverse_series(int n, int sign, int G) noexcept
  {
    using I = wide_sint<K>;
    const I nn{n}, n2{n * n};
    I p = (I{1} << G) / nn;                               // 2^G / n^(2k+1)
    I sum{0};
    for (int k = 0; !p.is_zero(); ++k)
    {
      const I t = p / I{2 * k + 1};
      sum = (k % 2 != 0 && sign < 0) ? sum - t : sum + t;
      p = p / n2;
    }
    return sum;
  }

  // ln 2 = 2·atanh(1/3).
  template <int W>
  inline constexpr fixed_t<W + 2> ln2_q = [] {
    constexpr int G = W + series_guard(W);
    using I = fixed_t<G + 4>;
    const I v = I{2} * inverse_series<limbs_of<I>>(3, +1, G);
    return static_cast<fixed_t<W + 2>>(round_shift(v, G - W));
  }();

  // π = 16·atan(1/5) − 4·atan(1/239) (Machin).
  template <int W>
  inline constexpr fixed_t<W + 3> pi_q = [] {
    constexpr int G = W + series_guard(W) + 4;
    using I = fixed_t<G + 6>;
    const I v = I{16} * inverse_series<limbs_of<I>>(5, -1, G)
              - I{4} * inverse_series<limbs_of<I>>(239, -1, G);
    return static_cast<fixed_t<W + 3>>(round_shift(v, G - W));
  }();

  // ln 10 = 3·ln 2 + ln(5/4) = 6·atanh(1/3) + 2·atanh(1/9).
  template <int W>
  inline constexpr fixed_t<W + 4> ln10_q = [] {
    constexpr int G = W + series_guard(W) + 2;
    using I = fixed_t<G + 6>;
    const I v = I{6} * inverse_series<limbs_of<I>>(3, +1, G)
              + I{2} * inverse_series<limbs_of<I>>(9, +1, G);
    return static_cast<fixed_t<W + 4>>(round_shift(v, G - W));
  }();

  //---------------------------------------------------------------------------
  // Exact integer helpers.
  //---------------------------------------------------------------------------
  // ⌊√t⌋ and ⌊∛t⌋ for one limb: Newton from above.
  constexpr umax isqrt64(umax t) noexcept
  {
    if (t < 2) return t;
    umax x = umax{1} << ((std::bit_width(t) + 1) / 2);
    for (;;) { const umax y = (x + t / x) >> 1; if (y >= x) return x; x = y; }
  }
  constexpr umax icbrt64(umax t) noexcept
  {
    if (t < 2) return t;
    umax x = umax{1} << ((std::bit_width(t) + 2) / 3);
    for (;;) { const umax y = (2 * x + t / (x * x)) / 3; if (y >= x) return x; x = y; }
  }

  // ⌊√n⌋ for n ≥ 0: Newton from above, seeded from the top 62 bits (about 31
  // correct bits, so two or three steps finish).
  template <std::size_t K>
  constexpr wide_sint<K> isqrt(wide_sint<K> const& n) noexcept
  {
    using I = wide_sint<K>;
    if (n.is_zero()) return n;
    const int bw = bit_width_of(n);
    int sh = bw > 62 ? bw - 62 : 0;
    sh += sh & 1;                                         // even: √(t·2^sh) = √t·2^(sh/2)
    const umax t = static_cast<umax>(n >> sh);
    I x = I{isqrt64(t) + 1} << (sh / 2);                  // ≥ √n
    for (;;)
    {
      const I y = (x + n / x) >> 1;
      if (!(y < x)) return x;
      x = y;
    }
  }

  // ⌊∛n⌋ for n ≥ 0: Newton from above, seeded from the top 63 bits.
  template <std::size_t K>
  constexpr wide_sint<K> icbrt(wide_sint<K> const& n) noexcept
  {
    using I = wide_sint<K>;
    if (n.is_zero()) return n;
    const int bw = bit_width_of(n);
    int sh = bw > 63 ? bw - 63 : 0;
    sh += (3 - sh % 3) % 3;                               // a multiple of 3
    const umax t = static_cast<umax>(n >> sh);
    I x = I{icbrt64(t) + 1} << (sh / 3);                  // ≥ ∛n
    for (;;)
    {
      const I y = (I{2} * x + n / (x * x)) / I{3};
      if (!(y < x)) return x;
      x = y;
    }
  }

  // The fraction in lowest terms.
  template <std::size_t K>
  constexpr exact_frac<K> reduced(exact_frac<K> const& f) noexcept
  {
    using I = wide_sint<K>;
    I a = f.Num.negative() ? -f.Num : f.Num, b = f.Den;
    while (!b.is_zero()) { const I t = a % b; a = b; b = t; }
    if (a.is_zero() || a == I{1}) return f;
    return {f.Num / a, f.Den / a};
  }

  template <std::size_t K>
  constexpr bool is_zero(exact_frac<K> const& f) noexcept { return f.Num.is_zero(); }
  template <std::size_t K>
  constexpr bool is_one(exact_frac<K> const& f) noexcept { return f.Num == f.Den; }
  template <std::size_t K>
  constexpr bool is_integer(exact_frac<K> const& f) noexcept { return (f.Num % f.Den).is_zero(); }
  template <std::size_t K>
  constexpr exact_frac<K> abs(exact_frac<K> const& f) noexcept { return {f.Num.negative() ? -f.Num : f.Num, f.Den}; }
  // 1/f for f != 0.
  template <std::size_t K>
  constexpr exact_frac<K> inverse(exact_frac<K> const& f) noexcept
  { return f.Num.negative() ? exact_frac<K>{-f.Den, -f.Num} : exact_frac<K>{f.Den, f.Num}; }

  // x·2^W rounded to the nearest integer, for a W known only at runtime. KI
  // limbs must hold Num·2^W and Den·2^-W.
  template <std::size_t K, std::size_t KI, std::size_t E>
  constexpr wide_sint<K> to_q_at(exact_frac<E> const& x, int W) noexcept
  {
    using I = wide_sint<KI>;
    I n{x.Num}, d{x.Den};
    if (W >= 0) n = n << W; else d = d << (-W);
    return static_cast<wide_sint<K>>(rounded_div<round_mode::nearest>(n, d));
  }

  //---------------------------------------------------------------------------
  // Series kernels on Q.S values. Each returns the value and an error bound in
  // units 2^-S. Every product and quotient truncates (within 1 unit), and the
  // carried term errors stay below a few units, so a sum of n terms is within
  // a small multiple of n.
  //---------------------------------------------------------------------------
  template <std::size_t K>
  struct fx
  {
    wide_sint<K> Value;
    umax Error;
  };

  // e^r for |r| ≤ 1/2.
  template <std::size_t K>
  constexpr fx<K> exp_series(wide_sint<K> const& r, int S) noexcept
  {
    using I = wide_sint<K>;
    I term = one_q<K>(S), sum = term;
    umax n = 0;
    for (int k = 1; !term.is_zero(); ++k, ++n)
    {
      term = div_small(mul_q(term, r, S), static_cast<umax>(k));
      sum += term;
    }
    return {sum, 4 * n + 2};
  }

  // log m for m in [0.7, 1.42], m within dm units: 2·atanh(z), z = (m−1)/(m+1),
  // |z| ≤ 0.18.
  template <std::size_t K>
  constexpr fx<K> log_series(wide_sint<K> const& m, umax dm, int S) noexcept
  {
    using I = wide_sint<K>;
    const I one = one_q<K>(S);
    const I z = div_q(m - one, m + one, S);
    const I z2 = mul_q(z, z, S);
    I p = z, sum{0};
    umax n = 0;
    for (int k = 0; !p.is_zero(); ++k, ++n)
    {
      sum += div_small(p, static_cast<umax>(2 * k + 1));
      p = mul_q(p, z2, S);
    }
    return {sum << 1, 2 * (3 * n + 4) + 3 * dm};
  }

  // sin r and cos r for |r| ≤ 0.8, r within dr units.
  template <std::size_t K>
  struct sincos_fx { fx<K> Sin; fx<K> Cos; };

  template <std::size_t K>
  constexpr sincos_fx<K> sincos_series(wide_sint<K> const& r, umax dr, int S) noexcept
  {
    using I = wide_sint<K>;
    const I r2 = mul_q(r, r, S);
    I t = r, s = r;
    umax ns = 0;
    for (int k = 1; !t.is_zero(); ++k, ++ns)
    {
      t = -div_small(mul_q(t, r2, S), static_cast<umax>((2 * k) * (2 * k + 1)));
      s += t;
    }
    I u = one_q<K>(S), c = u;
    umax nc = 0;
    for (int k = 1; !u.is_zero(); ++k, ++nc)
    {
      u = -div_small(mul_q(u, r2, S), static_cast<umax>((2 * k - 1) * (2 * k)));
      c += u;
    }
    return {{s, 3 * ns + 3 + dr}, {c, 3 * nc + 3 + dr}};
  }

  // √a for a ≥ 0 at scale S: ⌊√(a·2^S)⌋, within da/2 + 1 units (a ≥ 1/4).
  template <std::size_t K>
  constexpr wide_sint<K> sqrt_q(wide_sint<K> const& a, int S) noexcept
  {
    using D = double_t<K>;
    return static_cast<wide_sint<K>>(isqrt(D{a} << S));
  }

  // atan t for |t| ≤ 1, t within dt units. Two half-angle steps
  // t ← t/(1 + √(1+t²)) bring |t| below 0.2 (each halves an input error and
  // adds at most 3 units); the series result is then multiplied by 4.
  template <std::size_t K>
  constexpr fx<K> atan_fixed(wide_sint<K> t, umax dt, int S) noexcept
  {
    using I = wide_sint<K>;
    const I one = one_q<K>(S);
    for (int i = 0; i < 2; ++i)
      t = div_q(t, one + sqrt_q(one + mul_q(t, t, S), S), S);
    const I t2 = mul_q(t, t, S);
    I p = t, sum{0};
    umax n = 0;
    for (int k = 0; !p.is_zero(); ++k, ++n)
    {
      const I term = div_small(p, static_cast<umax>(2 * k + 1));
      sum = (k % 2 == 0) ? sum + term : sum - term;
      p = mul_q(p, t2, S);
    }
    return {sum << 2, 4 * (3 * n + 8) + dt};
  }

  // log a for a ≥ 1 at scale S, a within da units: a = m·2^b with m in
  // [0.7, 1.42], log a = log m + b·ln 2. LN2 is ln 2 at scale S.
  template <std::size_t K>
  constexpr fx<K> log_fixed(wide_sint<K> const& a, umax da, int S, wide_sint<K> const& ln2) noexcept
  {
    using I = wide_sint<K>;
    int b = bit_width_of(a) - 1 - S;                     // 2^b ≤ a/2^S < 2^(b+1)
    I m = b >= 0 ? a >> b : a << (-b);
    if (mul_q(m, m, S) > (one_q<K>(S) << 1)) { ++b; m = b >= 0 ? a >> b : a << (-b); }
    const umax dm = (b >= 0 ? (da >> b) : (da << (-b))) + 1;
    const fx<K> l = log_series(m, dm, S);
    return {l.Value + I{b} * ln2, l.Error + static_cast<umax>(b < 0 ? -b : b) + 1};
  }

  // e^t for t at scale S, within dt units: e^r·2^k for t = k·ln 2 + r, as an
  // approx at scale S − k. LN2 is ln 2 at scale S. Past KMax the value lies
  // above 2^(KMax + 1): returned as exactly 2^(KMax + 2), which every output
  // with magnitude below 2^KMax places past its Upper. Below 2^-(S+1) it is
  // returned as the interval [0, 2^-(S+1)].
  template <std::size_t K>
  constexpr approx<K> exp_fixed(wide_sint<K> const& t, umax dt, int S, wide_sint<K> const& ln2, int KMax) noexcept
  {
    using I = wide_sint<K>;
    const I k = rounded_div<round_mode::nearest>(t, ln2);
    if (I{KMax} < k) return {I{1}, -(KMax + 2), 0};
    if (k < I{-(S + 2)}) return {I{1}, S + 2, 1};
    const fx<K> e = exp_series(t - k * ln2, S);
    const int kk = static_cast<int>(static_cast<imax>(k));
    const umax ak = static_cast<umax>(kk < 0 ? -kk : kk);
    return {e.Value, S - kk, e.Error + 2 * (dt + ak + 1)};
  }
} // namespace beman::inside::math::detail::ax

#endif // BEMAN_INSIDE_DETAIL_MATH_ADAPTIVE_HPP
