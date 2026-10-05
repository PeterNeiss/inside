// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_CMATH_ADAPTIVE_HPP
#define BEMAN_INSIDE_CMATH_ADAPTIVE_HPP

#include <beman/inside/detail/math_adaptive.hpp>

#include <cstddef>
#include <expected>
#include <optional>

//---------------------------------------------------------------------------
// beman::inside::math::adaptive — the adaptive math engine.
//
// Every result is the correctly rounded value on the output grid, under the
// output's rounding mode: the engine works at the precision that grid needs
// (see detail/math_adaptive.hpp for the decision step and the driver). That
// makes results independent of how they were computed — the same on every
// platform, at compile time and at runtime, with or without an FPU — and it
// lifts the 64-bit limits: inputs and outputs may be grids past 64 bits.
//
// The domains are the mathematical ones (log needs x > 0, asin |x| ≤ 1, ...);
// there is no working-scale envelope. A result past Out's range goes through
// Out's policy, as any assignment.
//
// This is the wide tier: exact integer square and cube roots, and series in
// wide fixed point for the transcendentals.
//---------------------------------------------------------------------------
namespace beman::inside::math::detail::ax
{
  //---------------------------------------------------------------------------
  // Inputs as exact values, in as few limbs as their grid needs.
  //---------------------------------------------------------------------------
  constexpr int grid_bits(grid_wide const& v)
  {
#if BEMAN_INSIDE_BIG_GRIDS
    return v.bit_width();
#else
    return bit_width_of(v.negative() ? -v : v);
#endif
  }

  // A value of In is n/d with d dividing the notch's denominator and
  // |n| < 2^magnitude·d: its bits, plus a sign. A continuous grid takes the
  // 64-bit rational's.
  template <insidable In>
  inline constexpr int input_bits = [] {
    if constexpr (notch_of<In> == 0) return 130;
    else return grid_magnitude_bits<In> + grid_bits(wide_denominator(notch_of<In>)) + 2;
  }();

  template <insidable In>
  inline constexpr std::size_t input_limbs = limbs_for_bits(input_bits<In>);

  template <insidable In>
  constexpr exact_frac<input_limbs<In>> exact_input(In const& x)
  {
    using I = wide_sint<input_limbs<In>>;
    if constexpr (exact_valued<In>)
    {
      const auto v = exact_of(x);
      return {static_cast<I>(v.Num), static_cast<I>(v.Den)};
    }
    else
    {
      const auto v = exact_of<2>(as_rational(x));
      return {static_cast<I>(v.Num), static_cast<I>(v.Den)};
    }
  }

  // |x| < 2^in_mag for every value of In.
  template <insidable In>
  inline constexpr int in_mag = grid_magnitude_bits<In> > 1 ? grid_magnitude_bits<In> : 1;

  // Bits of an exact input's numerator or denominator.
  template <std::size_t E>
  inline constexpr int frac_bits = 64 * static_cast<int>(E);

  // Result magnitude bound: values of Out lie below 2^out_kmax.
  template <insidable Out>
  inline constexpr int out_kmax = mag_bits<Out> + 1;

  // The start precision for Out: its notch's bits plus guard bits.
  template <insidable Out>
  inline constexpr int start_bits = out_bits<Out> + 8 > 16 ? out_bits<Out> + 8 : 16;

  template <std::size_t K>
  constexpr exact_frac<K> exact_one() noexcept { return {wide_sint<K>{1}, wide_sint<K>{1}}; }
  template <std::size_t K>
  constexpr exact_frac<K> exact_int(imax v) noexcept { return {wide_sint<K>{v}, wide_sint<K>{1}}; }

  //---------------------------------------------------------------------------
  // Exact-value helpers at a compile-time scale S.
  //---------------------------------------------------------------------------
  // ⌊√f·2^S⌋ for f ≥ 0 (within 1 unit).
  template <int S, std::size_t K, std::size_t E>
  constexpr wide_sint<K> sqrt_exact_q(exact_frac<E> const& f) noexcept
  {
    using I = wide_sint<2 * E + limbs_for_bits(2 * S) + 1>;
    const I n = (I{f.Num} * I{f.Den}) << (2 * S);
    return static_cast<wide_sint<K>>(isqrt(n) / I{f.Den});
  }

  // log x for x > 0 at scale S: x = m·2^e exactly, m in [0.7, 1.42] rounded to
  // scale S, log x = log m + e·ln 2.
  template <int S, std::size_t K, std::size_t E>
  constexpr fx<K> log_exact(exact_frac<E> const& x) noexcept
  {
    using I = wide_sint<K>;
    constexpr std::size_t KI = E + K + limbs_for_bits(frac_bits<E> + S) + 1;
    int e = floor_log2(x);
    I m = to_q_at<K, KI>(x, S - e);
    if (mul_q(m, m, S) > (one_q<K>(S) << 1)) { ++e; m = to_q_at<K, KI>(x, S - e); }
    const fx<K> l = log_series(m, 1, S);
    const I ln2 = static_cast<I>(ln2_q<S>);
    return {l.Value + I{e} * ln2, l.Error + static_cast<umax>(e < 0 ? -e : e) + 1};
  }

  // An approx moved to scale A ≤ its own, rounding (error shrinks, plus ½).
  template <std::size_t K>
  constexpr fx<K> rescale(approx<K> const& a, int A) noexcept
  {
    const int sh = a.Scale - A;
    if (sh <= 0) return {a.Value << (-sh), a.Error << (-sh)};
    return {round_shift(a.Value, sh), (a.Error >> sh) + 1};
  }

  // asin x for |x| ≤ 1/2 (exact): atan(x/√(1−x²)).
  template <int S, std::size_t K, std::size_t E>
  constexpr fx<K> asin_small(exact_frac<E> const& x) noexcept
  {
    using I = wide_sint<K>;
    using F = exact_frac<2 * E + 1>;
    const F xx{x};
    const F c = exact_one<2 * E + 1>() + exact_frac<2 * E + 1>{-(xx.Num * xx.Num), xx.Den * xx.Den};
    const I s = sqrt_exact_q<S, K>(c);                  // ≥ 0.866, within 1
    const I t = div_q(to_q<S, K>(x), s, S);             // |t| ≤ 0.58, within 3
    return atan_fixed(t, 3, S);
  }

  // asin √y for 0 ≤ y ≤ 1/4 (exact): u = √y within 1, then atan(u/√(1−u²)).
  template <int S, std::size_t K, std::size_t E>
  constexpr fx<K> asin_sqrt(exact_frac<E> const& y) noexcept
  {
    using I = wide_sint<K>;
    const I one = one_q<K>(S);
    const I u = sqrt_exact_q<S, K>(y);
    const I c = sqrt_q(one - mul_q(u, u, S), S);         // within 3
    const I t = div_q(u, c, S);                          // within 5
    return atan_fixed(t, 5, S);
  }

  //---------------------------------------------------------------------------
  // The cores: one per function. Each holds its exact inputs and computes the
  // result within its error bound at a precision W (run<W>), plus the exact
  // rational results (exact()), which are the only values a rounding boundary
  // can hold.
  //---------------------------------------------------------------------------
  template <std::size_t E>
  using maybe_exact = std::optional<exact_frac<E>>;

  // exp x. Exact only at 0.
  template <std::size_t E, int Mag, int KMax>
  struct exp_core
  {
    exact_frac<E> X;
    constexpr maybe_exact<E> exact() const
    { return is_zero(X) ? maybe_exact<E>{exact_one<E>()} : std::nullopt; }
    template <int W>
    constexpr auto run() const
    {
      constexpr int S = W + 8 + KMax;
      using I = fixed_t<S + Mag + 8>;
      constexpr std::size_t K = limbs_of<I>;
      return exp_fixed(to_q<S, K>(X), 1, S, static_cast<I>(ln2_q<S>), KMax);
    }
  };

  // 2^x = 2^k·e^(f·ln 2), x = k + f. Exact at integers.
  template <std::size_t E, int Mag, int KMax>
  struct exp2_core
  {
    exact_frac<E> X;
    template <int W>
    constexpr auto run() const
    {
      constexpr int S = W + 8 + KMax;
      using I = fixed_t<S + 8>;
      using J = wide_sint<E>;
      constexpr std::size_t K = limbs_of<I>;
      const J k = rounded_div<round_mode::floor>(X.Num, X.Den);
      if (J{KMax} < k)      return approx<K>{I{1}, -(KMax + 2), 0};
      if (k < J{-(S + 2)})  return approx<K>{I{1}, S + 2, 1};
      int kk = static_cast<int>(static_cast<imax>(k));
      const exact_frac<E> f{X.Num - k * X.Den, X.Den};   // in [0, 1)
      if (is_zero(f)) return approx<K>{I{1}, -kk, 0};
      const I ln2 = static_cast<I>(ln2_q<S>);
      I r = mul_q(to_q<S, K>(f), ln2, S);                // within 2
      if (r > (ln2 >> 1)) { r -= ln2; ++kk; }
      const fx<K> e = exp_series(r, S);
      return approx<K>{e.Value, S - kk, e.Error + 6};
    }
  };

  // log x, x > 0. Exact at 1.
  template <std::size_t E>
  struct log_core
  {
    exact_frac<E> X;
    constexpr maybe_exact<E> exact() const
    { return is_one(X) ? maybe_exact<E>{exact_frac<E>{wide_sint<E>{0}, wide_sint<E>{1}}} : std::nullopt; }
    template <int W>
    constexpr auto run() const
    {
      constexpr int S = W + 8 + std::bit_width(static_cast<unsigned>(frac_bits<E>));
      using I = fixed_t<S + std::bit_width(static_cast<unsigned>(frac_bits<E>)) + 8>;
      constexpr std::size_t K = limbs_of<I>;
      const fx<K> l = log_exact<S, K>(X);
      return approx<K>{l.Value, S, l.Error};
    }
  };

  // The integer k with x = B^k (B = 2 or 10), if any.
  template <std::size_t E>
  constexpr std::optional<imax> exact_log(exact_frac<E> const& x, int B) noexcept
  {
    using I = wide_sint<E>;
    const exact_frac<E> r = reduced(x);
    if (r.Num.negative() || r.Num.is_zero()) return std::nullopt;
    const bool up = r.Den == I{1};
    if (!up && r.Num != I{1}) return std::nullopt;
    I v = up ? r.Num : r.Den;
    imax k = 0;
    while (v != I{1})
    {
      if (!(v % I{B}).is_zero()) return std::nullopt;
      v = v / I{B};
      ++k;
    }
    return up ? k : -k;
  }

  // log2 x and log10 x: log x / ln B. Exact at powers of B.
  template <std::size_t E, int B>
  struct logb_core
  {
    exact_frac<E> X;
    constexpr maybe_exact<E> exact() const
    {
      if (const auto k = exact_log(X, B)) return exact_int<E>(*k);
      return std::nullopt;
    }
    template <int W>
    constexpr auto run() const
    {
      constexpr int S = W + 10 + std::bit_width(static_cast<unsigned>(frac_bits<E>));
      using I = fixed_t<S + std::bit_width(static_cast<unsigned>(frac_bits<E>)) + 8>;
      constexpr std::size_t K = limbs_of<I>;
      const fx<K> l = log_exact<S, K>(X);
      const I lnb = B == 2 ? static_cast<I>(ln2_q<S>) : static_cast<I>(ln10_q<S>);
      return approx<K>{div_q(l.Value, lnb, S), S, 2 * l.Error + 2};
    }
  };

  // √x, x ≥ 0: ⌊√x·2^P⌋ exactly. Exact when x is a square of a rational.
  template <std::size_t E>
  struct sqrt_core
  {
    exact_frac<E> X;
    constexpr maybe_exact<E> exact() const
    {
      const exact_frac<E> r = reduced(X);
      const auto n = isqrt(r.Num), d = isqrt(r.Den);
      if (n * n == r.Num && d * d == r.Den) return exact_frac<E>{n, d};
      return std::nullopt;
    }
    template <int W>
    constexpr auto run() const
    {
      constexpr int P = W + 2;
      using I = fixed_t<frac_bits<E> + P + 4>;
      constexpr std::size_t K = limbs_of<I>;
      return approx<K>{sqrt_exact_q<P, K>(X), P, 1};
    }
  };

  // ∛x: ⌊∛(n·d²·2^(3P))⌋/d exactly. Exact when x is a cube of a rational.
  template <std::size_t E>
  struct cbrt_core
  {
    exact_frac<E> X;
    constexpr maybe_exact<E> exact() const
    {
      const exact_frac<E> r = reduced(abs(X));
      const auto n = icbrt(r.Num), d = icbrt(r.Den);
      if (n * n * n == r.Num && d * d * d == r.Den) return exact_frac<E>{X.Num.negative() ? -n : n, d};
      return std::nullopt;
    }
    template <int W>
    constexpr auto run() const
    {
      constexpr int P = W + 2;
      using I = fixed_t<frac_bits<E> + P + 4>;
      constexpr std::size_t K = limbs_of<I>;
      using J = wide_sint<3 * E + limbs_for_bits(3 * P) + 1>;
      const exact_frac<E> a = abs(X);
      const J n = (J{a.Num} * J{a.Den} * J{a.Den}) << (3 * P);
      const I y = static_cast<I>(icbrt(n) / J{a.Den});
      return approx<K>{X.Num.negative() ? -y : y, P, 1};
    }
  };

  // sin, cos and tan: x = k·π/2 + r, |r| ≤ π/4, then the series and the
  // quadrant. Exact at 0.
  enum class trig { sin, cos, tan };

  template <std::size_t E, int Mag, trig Fn, int KMax>
  struct trig_core
  {
    exact_frac<E> X;
    constexpr maybe_exact<E> exact() const
    {
      if (!is_zero(X)) return std::nullopt;
      return Fn == trig::cos ? exact_one<E>() : exact_frac<E>{wide_sint<E>{0}, wide_sint<E>{1}};
    }
    template <int W>
    constexpr auto run() const
    {
      constexpr int S = W + 10 + (Fn == trig::tan ? KMax + 4 : 0);
      constexpr int T = S + Mag + 4;                     // reduce with Mag more bits
      using I = fixed_t<(Fn == trig::tan ? 2 * S : S) + 8>;
      using R = fixed_t<T + Mag + 8>;
      constexpr std::size_t K = limbs_of<I>;
      const R xq = to_q<T, limbs_of<R>>(X);
      const R hp = static_cast<R>(pi_q<T - 1>);          // π/2 within 1
      const R k = rounded_div<round_mode::nearest>(xq, hp);
      const I r = static_cast<I>(round_shift(xq - k * hp, T - S));   // within 2
      const auto sc = sincos_series(r, 2, S);
      const unsigned q = static_cast<unsigned>(k.Word[0] & 3u);
      // sin x and cos x by quadrant: (s, c), (c, −s), (−s, −c), (−c, s).
      const fx<K> sn = (q == 0) ? sc.Sin : (q == 1) ? sc.Cos : (q == 2) ? fx<K>{-sc.Sin.Value, sc.Sin.Error} : fx<K>{-sc.Cos.Value, sc.Cos.Error};
      const fx<K> cs = (q == 0) ? sc.Cos : (q == 1) ? fx<K>{-sc.Sin.Value, sc.Sin.Error} : (q == 2) ? fx<K>{-sc.Cos.Value, sc.Cos.Error} : sc.Sin;
      if constexpr (Fn == trig::sin) return approx<K>{sn.Value, S, sn.Error};
      else if constexpr (Fn == trig::cos) return approx<K>{cs.Value, S, cs.Error};
      else
      {
        // tan = sin/cos. Error: (δs + |t|·δc)/|c| + 1 units.
        const I ac = cs.Value.negative() ? -cs.Value : cs.Value;
        const I as = sn.Value.negative() ? -sn.Value : sn.Value;
        const I ec{cs.Error}, es{sn.Error};
        if (!(ec < ac)) return approx<K>{I{0}, S, ~umax{0}};        // cos unresolved
        // |t| ≥ (|s| − δs)/(|c| + δc): past 2^KMax, the result is past Out.
        const I tlo = div_q(as - es, ac + ec, S);
        if ((one_q<K>(S) << KMax) < tlo)
          return approx<K>{(sn.Value.negative() != cs.Value.negative()) ? I{-1} : I{1}, -(KMax + 2), 0};
        const I t = div_q(sn.Value, cs.Value, S);
        const I at = t.negative() ? -t : t;
        const I err = ((es + mul_q(at, ec, S) + I{1}) << S) / (ac - ec) + I{2};
        const umax e = (I{~umax{0} >> 1} < err) ? ~umax{0} : static_cast<umax>(err);
        return approx<K>{t, S, e};
      }
    }
  };

  // atan x: |x| ≤ 1 directly, else ±π/2 − atan(1/x). Exact at 0.
  template <int S, std::size_t K, std::size_t E>
  constexpr fx<K> atan_exact(exact_frac<E> const& x) noexcept
  {
    using I = wide_sint<K>;
    if (!(exact_one<E>() < abs(x)))
      return atan_fixed(to_q<S, K>(x), 1, S);
    const fx<K> a = atan_fixed(to_q<S, K>(inverse(x)), 1, S);
    const I hp = static_cast<I>(pi_q<S - 1>);
    return {(x.Num.negative() ? -hp : hp) - a.Value, a.Error + 1};
  }

  template <std::size_t E>
  struct atan_core
  {
    exact_frac<E> X;
    constexpr maybe_exact<E> exact() const
    { return is_zero(X) ? maybe_exact<E>{X} : std::nullopt; }
    template <int W>
    constexpr auto run() const
    {
      constexpr int S = W + 10;
      using I = fixed_t<S + 8>;
      constexpr std::size_t K = limbs_of<I>;
      const fx<K> a = atan_exact<S, K>(X);
      return approx<K>{a.Value, S, a.Error};
    }
  };

  // atan2(y, x) in [−π, π]; 0 for y = 0, x ≥ 0 (and the (0, 0) convention).
  template <std::size_t E>
  struct atan2_core
  {
    exact_frac<E> Y, X;
    constexpr maybe_exact<E> exact() const
    {
      if (is_zero(Y) && !X.Num.negative()) return exact_frac<E>{wide_sint<E>{0}, wide_sint<E>{1}};
      return std::nullopt;
    }
    template <int W>
    constexpr auto run() const
    {
      constexpr int S = W + 10;
      using I = fixed_t<S + 8>;
      constexpr std::size_t K = limbs_of<I>;
      using F = exact_frac<2 * E + 1>;
      const F y{Y}, x{X};
      const I pi = static_cast<I>(pi_q<S>);
      if (!(abs(x) < abs(y)))
      {
        const fx<K> a = atan_exact<S, K>(y / x);
        if (!x.Num.negative()) return approx<K>{a.Value, S, a.Error};
        return approx<K>{a.Value + (y.Num.negative() ? -pi : pi), S, a.Error + 1};
      }
      const fx<K> a = atan_exact<S, K>(x / y);
      const I hp = static_cast<I>(pi_q<S - 1>);
      return approx<K>{(y.Num.negative() ? -hp : hp) - a.Value, S, a.Error + 1};
    }
  };

  // asin x, |x| ≤ 1: small arguments directly; past 1/2, the half-angle form
  // ±(π/2 − 2·asin √((1−|x|)/2)), which stays accurate up to ±1.
  template <std::size_t E>
  struct asin_core
  {
    exact_frac<E> X;
    constexpr maybe_exact<E> exact() const
    { return is_zero(X) ? maybe_exact<E>{X} : std::nullopt; }
    template <int W>
    constexpr auto run() const
    {
      constexpr int S = W + 12;
      using I = fixed_t<S + 8>;
      constexpr std::size_t K = limbs_of<I>;
      const exact_frac<E> half{wide_sint<E>{1}, wide_sint<E>{2}};
      if (!(half < abs(X)))
      {
        const fx<K> a = asin_small<S, K>(X);
        return approx<K>{a.Value, S, a.Error};
      }
      using F = exact_frac<E + 1>;
      const F one = exact_one<E + 1>();
      const F y = (one + F{-abs(X)}) * F{half};          // (1 − |x|)/2
      const fx<K> a = asin_sqrt<S, K>(y);
      const I v = static_cast<I>(pi_q<S - 1>) - (a.Value << 1);
      return approx<K>{X.Num.negative() ? -v : v, S, 2 * a.Error + 1};
    }
  };

  // acos x = π/2 − asin x for |x| ≤ 1/2; 2·asin √((1−x)/2) above;
  // π − 2·asin √((1+x)/2) below. Exact at 1.
  template <std::size_t E>
  struct acos_core
  {
    exact_frac<E> X;
    constexpr maybe_exact<E> exact() const
    { return is_one(X) ? maybe_exact<E>{exact_frac<E>{wide_sint<E>{0}, wide_sint<E>{1}}} : std::nullopt; }
    template <int W>
    constexpr auto run() const
    {
      constexpr int S = W + 12;
      using I = fixed_t<S + 8>;
      constexpr std::size_t K = limbs_of<I>;
      const exact_frac<E> half{wide_sint<E>{1}, wide_sint<E>{2}};
      if (!(half < abs(X)))
      {
        const fx<K> a = asin_small<S, K>(X);
        return approx<K>{static_cast<I>(pi_q<S - 1>) - a.Value, S, a.Error + 1};
      }
      using F = exact_frac<E + 1>;
      const F one = exact_one<E + 1>();
      const F x{X};
      const bool neg = X.Num.negative();
      const F y = (neg ? one + x : one + F{-x}) * F{half};
      const fx<K> a = asin_sqrt<S, K>(y);
      if (!neg) return approx<K>{a.Value << 1, S, 2 * a.Error};
      return approx<K>{static_cast<I>(pi_q<S>) - (a.Value << 1), S, 2 * a.Error + 1};
    }
  };

  // sinh, cosh and tanh from e^|x| and e^−|x| at absolute scale A. Exact at 0
  // (sinh, tanh: 0; cosh: 1).
  enum class hyp { sinh, cosh, tanh };

  template <std::size_t E, int Mag, hyp Fn, int KMax>
  struct hyp_core
  {
    exact_frac<E> X;
    constexpr maybe_exact<E> exact() const
    {
      if (!is_zero(X)) return std::nullopt;
      return Fn == hyp::cosh ? exact_one<E>() : X;
    }
    template <int W>
    constexpr auto run() const
    {
      constexpr int A = W + 10;
      constexpr int KM = Fn == hyp::tanh ? 1 : KMax + 1;
      constexpr int S = A + KM + 4;
      using I = fixed_t<S + Mag + 8>;
      constexpr std::size_t K = limbs_of<I>;
      const I ln2 = static_cast<I>(ln2_q<S>);
      const bool neg = X.Num.negative();
      const I ax = to_q<S, K>(abs(X));                   // within ½
      if constexpr (Fn == hyp::tanh)
      {
        // tanh |x| = (1 − g)/(1 + g), g = e^(−2|x|) ≤ 1.
        const fx<K> g = rescale(exp_fixed(-(ax << 1), 1, S, ln2, KM), A);
        const I one = one_q<K>(A);
        const I t = div_q(one - g.Value, one + g.Value, A);
        return approx<K>{neg ? -t : t, A, 2 * g.Error + 2};
      }
      else
      {
        const approx<K> p = exp_fixed(ax, 1, S, ln2, KM);
        if (p.Scale < 0 && p.Error == 0)                  // e^|x| past 2^KM: past Out
          return approx<K>{(Fn == hyp::sinh && neg) ? I{-1} : I{1}, -(KMax + 2), 0};
        const fx<K> P = rescale(p, A);
        const fx<K> M = rescale(exp_fixed(-ax, 1, S, ln2, KM), A);
        if constexpr (Fn == hyp::cosh)
          return approx<K>{(P.Value + M.Value) >> 1, A, (P.Error + M.Error) / 2 + 1};
        else
        {
          const I s = (P.Value - M.Value) >> 1;
          return approx<K>{neg ? -s : s, A, (P.Error + M.Error) / 2 + 1};
        }
      }
    }
  };

  // asinh x = ±log(|x| + √(x²+1)); acosh x = log(x + √((x−1)(x+1))), x ≥ 1;
  // atanh x = ±½·log((1+|x|)/(1−|x|)), |x| < 1.
  enum class ahyp { asinh, acosh, atanh };

  template <std::size_t E, int Mag, ahyp Fn>
  struct ahyp_core
  {
    exact_frac<E> X;
    constexpr maybe_exact<E> exact() const
    {
      if (Fn == ahyp::acosh) return is_one(X) ? maybe_exact<E>{exact_frac<E>{wide_sint<E>{0}, wide_sint<E>{1}}} : std::nullopt;
      return is_zero(X) ? maybe_exact<E>{X} : std::nullopt;
    }
    template <int W>
    constexpr auto run() const
    {
      constexpr int S = W + 12 + std::bit_width(static_cast<unsigned>(frac_bits<E> + Mag));
      using I = fixed_t<S + Mag + 2 * frac_bits<E> + 8>;
      constexpr std::size_t K = limbs_of<I>;
      using F = exact_frac<2 * E + 1>;
      const F one = exact_one<2 * E + 1>();
      const bool neg = X.Num.negative();
      const F a = abs(F{X});
      if constexpr (Fn == ahyp::atanh)
      {
        const fx<K> l = log_exact<S, K>((one + a) / (one + F{-a}));
        return approx<K>{neg ? -(l.Value >> 1) : (l.Value >> 1), S, l.Error / 2 + 1};
      }
      else
      {
        const F r = Fn == ahyp::asinh ? a * a + one : (a + F{-one}) * (a + one);
        const I v = to_q<S, K>(a) + sqrt_exact_q<S, K>(r);   // ≥ 1, within 2
        const fx<K> l = log_fixed(v, 2, S, static_cast<I>(ln2_q<S>));
        return approx<K>{(Fn == ahyp::asinh && neg) ? -l.Value : l.Value, S, l.Error};
      }
    }
  };

  // b^e for b > 0: e^(e·log b). Exact for e = 0, b = 1, and integer e when
  // the power fits PowLimbs.
  inline constexpr std::size_t pow_limbs = 32;           // exact integer powers up to 2048 bits

  template <std::size_t EB, std::size_t EE>
  constexpr std::optional<exact_frac<pow_limbs>> exact_pow(exact_frac<EB> const& b, exact_frac<EE> const& e) noexcept
  {
    using P = wide_sint<pow_limbs>;
    if (is_zero(e) || is_one(b)) return exact_frac<pow_limbs>{P{1}, P{1}};
    if (!is_integer(e)) return std::nullopt;
    const auto k = e.Num / e.Den;
    const auto ak = k.negative() ? -k : k;
    const exact_frac<EB> r = reduced(b);
    const int bits = bit_width_of(r.Num) + bit_width_of(r.Den);
    if (bit_width_of(ak) > 12 || static_cast<imax>(ak) * bits > 64 * static_cast<imax>(pow_limbs) - 8)
      return std::nullopt;
    P n{1}, d{1};
    for (imax i = 0; i < static_cast<imax>(ak); ++i) { n = n * P{r.Num}; d = d * P{r.Den}; }
    return k.negative() ? inverse(exact_frac<pow_limbs>{n, d}) : exact_frac<pow_limbs>{n, d};
  }

  template <std::size_t EB, std::size_t EE, int MagE, int KMax>
  struct pow_core
  {
    exact_frac<EB> B;
    exact_frac<EE> Exp;
    constexpr std::optional<exact_frac<pow_limbs>> exact() const { return exact_pow(B, Exp); }
    template <int W>
    constexpr auto run() const
    {
      // log b at MagE more bits, so e·log b (|e| < 2^MagE) is within 2 units
      // at scale S.
      constexpr int S = W + 12 + KMax;
      constexpr int SL = S + MagE + std::bit_width(static_cast<unsigned>(frac_bits<EB>));
      using I = fixed_t<SL + MagE + std::bit_width(static_cast<unsigned>(frac_bits<EB>)) + 2 * frac_bits<EE> + 8>;
      constexpr std::size_t K = limbs_of<I>;
      const fx<K> l = log_exact<SL, K>(B);
      const I t = round_shift(mul_q(l.Value, static_cast<I>(Exp.Num), 0) / static_cast<I>(Exp.Den), SL - S);
      const umax dt = (l.Error >> (SL - S - MagE)) + 2;
      return exp_fixed(t, dt, S, static_cast<I>(ln2_q<S>), KMax);
    }
  };

  //---------------------------------------------------------------------------
  // Auto output grids. An endpoint of a deduced output is a core's result at
  // an endpoint of the input, rounded outward to the input's notch: down for
  // a lower end, up for an upper end. The rounding runs on the core's error
  // interval, so the bound always covers the true value; one escalation makes
  // it tight except within 2^-2W of a lattice point.
  //---------------------------------------------------------------------------
  template <grid_rational Notch>
  inline constexpr std::size_t notch_limbs =
      limbs_for_bits(grid_bits(wide_numerator(Notch)) + grid_bits(wide_denominator(Notch)) + 2);

  // k·Notch as a grid number.
  template <grid_rational Notch, std::size_t K>
  constexpr grid_rational lattice_point(wide_sint<K> const& k)
  {
#if BEMAN_INSIDE_BIG_GRIDS
    return grid_rational{big_int{k}} * Notch;
#else
    return static_cast<imax>(k) * Notch;
#endif
  }

  // ⌊v/Notch⌋ (Up false) or ⌈v/Notch⌉ (Up true) for v = n/d, d > 0.
  template <grid_rational Notch, bool Up, std::size_t K>
  constexpr wide_sint<K + notch_limbs<Notch>> lattice_index(wide_sint<K> const& n, wide_sint<K> const& d)
  {
    using J = wide_sint<K + notch_limbs<Notch>>;
    const J p = static_cast<J>(wide_numerator(Notch)), q = static_cast<J>(wide_denominator(Notch));
    return rounded_div<Up ? round_mode::ceil : round_mode::floor>(J{n} * q, J{d} * p);
  }

  template <grid_rational Notch, bool Up, int W, int Cap, typename Core>
  constexpr grid_rational lattice_bound_from(Core const& core)
  {
    const auto a = core.template run<W>();
    constexpr std::size_t K = limbs_of<decltype(a.Value)> + 1;
    using I = wide_sint<K>;
    const I one{1};
    const I d = a.Scale >= 0 ? one << a.Scale : one;
    auto at = [&](I const& y) { return lattice_index<Notch, Up>(a.Scale >= 0 ? y : y << (-a.Scale), d); };
    const I y{a.Value}, e{a.Error};
    const auto lo = at(y - e), hi = at(y + e);
    if constexpr (2 * W <= Cap)
      if (!(lo == hi)) return lattice_bound_from<Notch, Up, 2 * W, Cap>(core);
    return lattice_point<Notch>(Up ? hi : lo);
  }

  template <grid_rational Notch, bool Up, typename Core>
  constexpr grid_rational lattice_bound(Core const& core)
  {
    if constexpr (requires { core.exact(); })
      if (const auto v = core.exact())
      {
        return lattice_point<Notch>(lattice_index<Notch, Up>(v->Num, v->Den));
      }
    constexpr int W0 = static_cast<int>(64 * notch_limbs<Notch>) + 16;
    return lattice_bound_from<Notch, Up, W0, 2 * W0>(core);
  }

  // An In endpoint as an exact input.
  template <insidable In>
  constexpr exact_frac<input_limbs<In>> grid_input(grid_rational const& r)
  { return exact_of_grid<input_limbs<In>>(r); }

  // ⌈|r|⌉ as an int (deduction bounds).
  constexpr imax ceil_abs(grid_rational const& r)
  {
    const grid_wide n = wide_numerator(r), d = wide_denominator(r);
    const grid_wide a = n.negative() ? -n : n;
    return static_cast<imax>((a + d - grid_wide{1}) / d);
  }

  template <insidable In>
  inline constexpr imax max_abs_int = ceil_abs(lower_of<In>) > ceil_abs(upper_of<In>) ? ceil_abs(lower_of<In>) : ceil_abs(upper_of<In>);

  // Policy of a deduced output: the input's, minus a fixed storage width,
  // rounding to nearest.
  template <insidable In>
  inline constexpr policy_flag auto_policy = (policy_of<In> & ~raw_width_mask) | round_nearest;

  template <insidable In, grid_rational Lo, grid_rational Hi>
  using auto_grid_t = inside<{{Lo, Hi}, notch_of<In>}, auto_policy<In>>;

  // The bound of Core's result at In's lower or upper end.
  template <insidable In, typename Core, bool AtUpper, bool Up>
  inline constexpr grid_rational bound_at =
      lattice_bound<notch_of<In>, Up>(Core{grid_input<In>(AtUpper ? upper_of<In> : lower_of<In>)});

  template <insidable In, typename Core>
  using increasing_t = auto_grid_t<In, bound_at<In, Core, false, false>, bound_at<In, Core, true, true>>;
  template <insidable In, typename Core>
  using decreasing_t = auto_grid_t<In, bound_at<In, Core, true, false>, bound_at<In, Core, false, true>>;

  // Growth bounds for the deduction: |result| < 2^kmax over In.
  template <insidable In>
  inline constexpr int exp_kmax = [] {
    static_assert(max_abs_int<In> <= 4096,
        "beman::inside::math::adaptive: the deduced output of exp, exp2, sinh and cosh needs |x| <= 4096 - "
        "name an output grid with fn_into<Out> instead");
    return static_cast<int>(max_abs_int<In> * 3 / 2) + 2;
  }();

  // Bits of the larger of |b| and 1/|b| over In (b > 0), for pow's growth.
  template <insidable In>
  inline constexpr int log2_span = [] {
    const imax hi = ceil_abs(upper_of<In>);
    const imax inv = ceil_abs(grid_rational{1} / lower_of<In>);
    const imax m = hi > inv ? hi : inv;
    return std::bit_width(static_cast<umax>(m)) + 1;
  }();

  template <insidable InB, insidable InE>
  inline constexpr int pow_kmax = [] {
    static_assert(max_abs_int<InE> * log2_span<InB> <= 1 << 16,
        "beman::inside::math::adaptive::pow: the deduced output would pass 2^65536 - name an output grid "
        "with pow_into<Out> instead");
    return static_cast<int>(max_abs_int<InE>) * log2_span<InB> + 2;
  }();

  // gcd of two notches (0 when either is 0), for two-input outputs.
  template <insidable A, insidable B>
  inline constexpr grid_rational gcd_notch = [] {
    if constexpr (notch_of<A> == 0 || notch_of<B> == 0) return grid_rational{0};
#if BEMAN_INSIDE_BIG_GRIDS
    else return grid_gcd(notch_of<A>, notch_of<B>);
#else
    else return *grid_gcd(notch_of<A>, notch_of<B>);
#endif
  }();
} // namespace beman::inside::math::detail::ax

namespace beman::inside::math::adaptive
{
  namespace ax = ::beman::inside::math::detail::ax;
  using ::beman::inside::detail::exact_valued;
  using ::beman::inside::detail::grid_rational;

  // Every transcendental rounds onto the output grid: Out's policy needs a
  // rounding mode, like any assignment that may round.
  template <insidable Out>
  consteval void require_rounding() noexcept
  {
    static_assert(has_flag(policy_of<Out>, snap) || !ax::slotted<Out>,
        "beman::inside::math::adaptive: the result is rounded onto Out's grid - declare Out "
        "with a rounding mode (round_nearest, round_floor, ...)");
  }

#define BEMAN_INSIDE_AX_UNARY(fn, ...)                                                  \
  template <insidable Out, insidable In>                                                \
  [[nodiscard]] constexpr Out fn##_into(In x)                                           \
  {                                                                                     \
    require_rounding<Out>();                                                            \
    using core = __VA_ARGS__;                                                           \
    return ax::evaluate<Out, ax::start_bits<Out>>(core{ax::exact_input(x)});            \
  }

  BEMAN_INSIDE_AX_UNARY(exp, ax::exp_core<ax::input_limbs<In>, ax::in_mag<In>, ax::out_kmax<Out>>)
  BEMAN_INSIDE_AX_UNARY(exp2, ax::exp2_core<ax::input_limbs<In>, ax::in_mag<In>, ax::out_kmax<Out>>)
  BEMAN_INSIDE_AX_UNARY(sin, ax::trig_core<ax::input_limbs<In>, ax::in_mag<In>, ax::trig::sin, 1>)
  BEMAN_INSIDE_AX_UNARY(cos, ax::trig_core<ax::input_limbs<In>, ax::in_mag<In>, ax::trig::cos, 1>)
  BEMAN_INSIDE_AX_UNARY(atan, ax::atan_core<ax::input_limbs<In>>)
  BEMAN_INSIDE_AX_UNARY(sinh, ax::hyp_core<ax::input_limbs<In>, ax::in_mag<In>, ax::hyp::sinh, ax::out_kmax<Out>>)
  BEMAN_INSIDE_AX_UNARY(cosh, ax::hyp_core<ax::input_limbs<In>, ax::in_mag<In>, ax::hyp::cosh, ax::out_kmax<Out>>)
  BEMAN_INSIDE_AX_UNARY(tanh, ax::hyp_core<ax::input_limbs<In>, ax::in_mag<In>, ax::hyp::tanh, 1>)
  BEMAN_INSIDE_AX_UNARY(asinh, ax::ahyp_core<ax::input_limbs<In>, ax::in_mag<In>, ax::ahyp::asinh>)
  BEMAN_INSIDE_AX_UNARY(cbrt, ax::cbrt_core<ax::input_limbs<In>>)
#undef BEMAN_INSIDE_AX_UNARY

  // Functions with a mathematical domain: checked on In's grid.
#define BEMAN_INSIDE_AX_DOMAIN(fn, cond, msg, ...)                                      \
  template <insidable Out, insidable In>                                                \
  [[nodiscard]] constexpr Out fn##_into(In x)                                           \
  {                                                                                     \
    static_assert(cond, "beman::inside::math::adaptive::" #fn ": " msg);              \
    require_rounding<Out>();                                                            \
    using core = __VA_ARGS__;                                                           \
    return ax::evaluate<Out, ax::start_bits<Out>>(core{ax::exact_input(x)});            \
  }

  BEMAN_INSIDE_AX_DOMAIN(log, (lower_of<In> > 0), "input must be strictly positive", ax::log_core<ax::input_limbs<In>>)
  BEMAN_INSIDE_AX_DOMAIN(log2, (lower_of<In> > 0), "input must be strictly positive", ax::logb_core<ax::input_limbs<In>, 2>)
  BEMAN_INSIDE_AX_DOMAIN(log10, (lower_of<In> > 0), "input must be strictly positive", ax::logb_core<ax::input_limbs<In>, 10>)
  BEMAN_INSIDE_AX_DOMAIN(asin, (lower_of<In> >= -1 && upper_of<In> <= 1), "input must be in [-1, 1]", ax::asin_core<ax::input_limbs<In>>)
  BEMAN_INSIDE_AX_DOMAIN(acos, (lower_of<In> >= -1 && upper_of<In> <= 1), "input must be in [-1, 1]", ax::acos_core<ax::input_limbs<In>>)
  BEMAN_INSIDE_AX_DOMAIN(acosh, (lower_of<In> >= 1), "input must be at least 1", ax::ahyp_core<ax::input_limbs<In>, ax::in_mag<In>, ax::ahyp::acosh>)
  BEMAN_INSIDE_AX_DOMAIN(atanh, (lower_of<In> > -1 && upper_of<In> < 1), "input must be in (-1, 1)", ax::ahyp_core<ax::input_limbs<In>, ax::in_mag<In>, ax::ahyp::atanh>)
#undef BEMAN_INSIDE_AX_DOMAIN

  // sqrt of a non-negative input.
  template <insidable Out, insidable In>
    requires (lower_of<In> >= 0)
  [[nodiscard]] constexpr Out sqrt_into(In x)
  {
    require_rounding<Out>();
    return ax::evaluate<Out, ax::start_bits<Out>>(ax::sqrt_core<ax::input_limbs<In>>{ax::exact_input(x)});
  }

  // sqrt of a mixed-sign input: domain_error on a negative value.
  template <insidable Out, insidable In>
    requires (lower_of<In> < 0)
  [[nodiscard]] constexpr std::expected<Out, errc> sqrt_into(In x)
  {
    require_rounding<Out>();
    const auto v = ax::exact_input(x);
    if (v.Num.negative()) return std::unexpected(errc::domain_error);
    return ax::evaluate<Out, ax::start_bits<Out>>(ax::sqrt_core<ax::input_limbs<In>>{v});
  }

  // tan: overflow when the result leaves Out (without clamp).
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr std::expected<Out, errc> tan_into(In x)
  {
    require_rounding<Out>();
    using core = ax::trig_core<ax::input_limbs<In>, ax::in_mag<In>, ax::trig::tan, ax::out_kmax<Out>>;
    return ax::evaluate_checked<Out, ax::start_bits<Out>>(core{ax::exact_input(x)});
  }

  template <insidable Out, insidable InY, insidable InX>
  [[nodiscard]] constexpr Out atan2_into(InY y, InX x)
  {
    require_rounding<Out>();
    constexpr std::size_t E = ax::input_limbs<InY> > ax::input_limbs<InX> ? ax::input_limbs<InY> : ax::input_limbs<InX>;
    using F = ::beman::inside::detail::exact_frac<E>;
    return ax::evaluate<Out, ax::start_bits<Out>>(ax::atan2_core<E>{F{ax::exact_input(y)}, F{ax::exact_input(x)}});
  }

  template <insidable Out, insidable InX, insidable InY>
  [[nodiscard]] constexpr Out hypot_into(InX x, InY y)
  {
    require_rounding<Out>();
    constexpr std::size_t E = 2 * (ax::input_limbs<InX> > ax::input_limbs<InY> ? ax::input_limbs<InX> : ax::input_limbs<InY>) + 1;
    using F = ::beman::inside::detail::exact_frac<E>;
    const F a{ax::exact_input(x)}, b{ax::exact_input(y)};
    return ax::evaluate<Out, ax::start_bits<Out>>(ax::sqrt_core<E>{a * a + b * b});
  }

  // pow: domain_error for a base ≤ 0; overflow when the result leaves Out
  // (without clamp).
  template <insidable Out, insidable InB, insidable InE>
  [[nodiscard]] constexpr std::expected<Out, errc> pow_into(InB base, InE exp)
  {
    require_rounding<Out>();
    const auto b = ax::exact_input(base);
    if (b.Num.negative() || b.Num.is_zero()) return std::unexpected(errc::domain_error);
    using core = ax::pow_core<ax::input_limbs<InB>, ax::input_limbs<InE>, ax::in_mag<InE>, ax::out_kmax<Out>>;
    return ax::evaluate_checked<Out, ax::start_bits<Out>>(core{b, ax::exact_input(exp)});
  }

  // Base^x for a compile-time integer Base ≥ 2.
  template <insidable Out, imax Base, insidable In>
  [[nodiscard]] constexpr Out pow_base_into(In x)
  {
    static_assert(Base >= 2, "beman::inside::math::adaptive::pow_base: Base must be at least 2");
    require_rounding<Out>();
    using core = ax::pow_core<2, ax::input_limbs<In>, ax::in_mag<In>, ax::out_kmax<Out>>;
    return ax::evaluate<Out, ax::start_bits<Out>>(core{ax::exact_int<2>(Base), ax::exact_input(x)});
  }

  //---------------------------------------------------------------------------
  // Auto forms: Out deduced from In — the range the function takes over In,
  // rounded outward to In's notch, with In's notch and policy (rounding to
  // nearest). sin and cos give [-1, 1], tan [-1024, 1024] (closer to a pole
  // the expected reports overflow), atan2 [-π, π] on the gcd of both notches.
  //---------------------------------------------------------------------------
  template <insidable In>
  consteval bool deducible() noexcept
  {
    static_assert(has_flag(policy_of<In>, snap),
        "beman::inside::math::adaptive: a transcendental result is rounded onto the input's grid - "
        "declare the input with a rounding mode (round_nearest, round_floor, ...)");
    static_assert(notch_of<In> != 0,
        "beman::inside::math::adaptive: a deduced output takes the input's notch - the input needs one");
    return true;
  }

  namespace auto_t
  {
    template <insidable In> using exp   = ax::increasing_t<In, ax::exp_core<ax::input_limbs<In>, ax::in_mag<In>, ax::exp_kmax<In>>>;
    template <insidable In> using exp2  = ax::increasing_t<In, ax::exp2_core<ax::input_limbs<In>, ax::in_mag<In>, ax::exp_kmax<In>>>;
    template <insidable In> using log   = ax::increasing_t<In, ax::log_core<ax::input_limbs<In>>>;
    template <insidable In> using log2  = ax::increasing_t<In, ax::logb_core<ax::input_limbs<In>, 2>>;
    template <insidable In> using log10 = ax::increasing_t<In, ax::logb_core<ax::input_limbs<In>, 10>>;
    template <insidable In> using sqrt  = ax::increasing_t<In, ax::sqrt_core<ax::input_limbs<In>>>;
    template <insidable In> using cbrt  = ax::increasing_t<In, ax::cbrt_core<ax::input_limbs<In>>>;
    template <insidable In> using atan  = ax::increasing_t<In, ax::atan_core<ax::input_limbs<In>>>;
    template <insidable In> using asin  = ax::increasing_t<In, ax::asin_core<ax::input_limbs<In>>>;
    template <insidable In> using acos  = ax::decreasing_t<In, ax::acos_core<ax::input_limbs<In>>>;
    template <insidable In> using sinh  = ax::increasing_t<In, ax::hyp_core<ax::input_limbs<In>, ax::in_mag<In>, ax::hyp::sinh, ax::exp_kmax<In>>>;
    template <insidable In> using tanh  = ax::increasing_t<In, ax::hyp_core<ax::input_limbs<In>, ax::in_mag<In>, ax::hyp::tanh, 1>>;
    template <insidable In> using asinh = ax::increasing_t<In, ax::ahyp_core<ax::input_limbs<In>, ax::in_mag<In>, ax::ahyp::asinh>>;
    template <insidable In> using acosh = ax::increasing_t<In, ax::ahyp_core<ax::input_limbs<In>, ax::in_mag<In>, ax::ahyp::acosh>>;
    template <insidable In> using atanh = ax::increasing_t<In, ax::ahyp_core<ax::input_limbs<In>, ax::in_mag<In>, ax::ahyp::atanh>>;
    template <insidable In> using sin   = inside<{{-1, 1}, notch_of<In>}, ax::auto_policy<In>>;
    template <insidable In> using cos   = sin<In>;
    template <insidable In> using tan   = inside<{{-1024, 1024}, notch_of<In>}, ax::auto_policy<In>>;

    // cosh is even: its least value is 1 when In spans 0, else at the end
    // nearer 0.
    template <insidable In>
    using cosh_core = ax::hyp_core<ax::input_limbs<In>, ax::in_mag<In>, ax::hyp::cosh, ax::exp_kmax<In>>;
    template <insidable In>
    inline constexpr grid_rational cosh_lo =
        (lower_of<In> <= 0 && upper_of<In> >= 0) ? grid_rational{1}
      : (lower_of<In> > 0) ? ax::bound_at<In, cosh_core<In>, false, false> : ax::bound_at<In, cosh_core<In>, true, false>;
    template <insidable In>
    inline constexpr grid_rational cosh_hi =
        ax::bound_at<In, cosh_core<In>, false, true> > ax::bound_at<In, cosh_core<In>, true, true>
      ? ax::bound_at<In, cosh_core<In>, false, true> : ax::bound_at<In, cosh_core<In>, true, true>;
    template <insidable In> using cosh = ax::auto_grid_t<In, cosh_lo<In>, cosh_hi<In>>;

    // sqrt of a mixed-sign input: [0, √max(|Lower|, |Upper|)].
    template <insidable In>
    inline constexpr grid_rational max_abs = (-lower_of<In> > upper_of<In>) ? -lower_of<In> : upper_of<In>;
    template <insidable In>
    using sqrt_signed = ax::auto_grid_t<In, grid_rational{0},
        ax::lattice_bound<notch_of<In>, true>(ax::sqrt_core<ax::input_limbs<In>>{ax::grid_input<In>(max_abs<In>)})>;

    // pow_base: pow_core with the base bound.
    template <imax Base, std::size_t E, int Mag, int KMax>
    struct pow_base_core : ax::pow_core<2, E, Mag, KMax>
    {
      constexpr pow_base_core(::beman::inside::detail::exact_frac<E> x)
        : ax::pow_core<2, E, Mag, KMax>{ax::exact_int<2>(Base), x} {}
    };
    template <imax Base, insidable In>
    using pow_base_t = ax::increasing_t<In, pow_base_core<Base, ax::input_limbs<In>, ax::in_mag<In>,
        static_cast<int>(ax::max_abs_int<In>) * std::bit_width(static_cast<umax>(Base)) + 2>>;

    // atan2: [−π, π] rounded outward on the gcd notch (π = atan2(0, −1)).
    template <insidable A, insidable B>
    inline constexpr grid_rational pi_up = ax::lattice_bound<ax::gcd_notch<A, B>, true>(
        ax::atan2_core<1>{ax::exact_int<1>(0), ax::exact_int<1>(-1)});
    template <insidable InY, insidable InX>
    using atan2 = inside<{{-pi_up<InY, InX>, pi_up<InY, InX>}, ax::gcd_notch<InY, InX>}, ax::auto_policy<InY>>;

    // hypot: [0, hypot of the largest magnitudes] on the gcd notch.
    template <insidable InX, insidable InY>
    inline constexpr std::size_t hypot_limbs = 2 * (ax::input_limbs<InX> > ax::input_limbs<InY> ? ax::input_limbs<InX> : ax::input_limbs<InY>) + 1;
    template <insidable InX, insidable InY>
    inline constexpr grid_rational hypot_hi = [] {
      using F = ::beman::inside::detail::exact_frac<hypot_limbs<InX, InY>>;
      const F a = ::beman::inside::detail::exact_of_grid<hypot_limbs<InX, InY>>(max_abs<InX>);
      const F b = ::beman::inside::detail::exact_of_grid<hypot_limbs<InX, InY>>(max_abs<InY>);
      return ax::lattice_bound<ax::gcd_notch<InX, InY>, true>(ax::sqrt_core<hypot_limbs<InX, InY>>{a * a + b * b});
    }();
    template <insidable InX, insidable InY>
    using hypot = inside<{{0, hypot_hi<InX, InY>}, ax::gcd_notch<InX, InY>}, ax::auto_policy<InX>>;

    // pow: b^e is monotone in each argument for b > 0, so the extremes are at
    // the corners of the input rectangle. Notch of the base.
    template <insidable InB, insidable InE>
    using pow_core = ax::pow_core<ax::input_limbs<InB>, ax::input_limbs<InE>, ax::in_mag<InE>, ax::pow_kmax<InB, InE>>;
    template <insidable InB, insidable InE, bool BUp, bool EUp, bool Up>
    inline constexpr grid_rational pow_corner = ax::lattice_bound<notch_of<InB>, Up>(pow_core<InB, InE>{
        ax::grid_input<InB>(BUp ? upper_of<InB> : lower_of<InB>), ax::grid_input<InE>(EUp ? upper_of<InE> : lower_of<InE>)});
    template <insidable InB, insidable InE>
    inline constexpr grid_rational pow_lo = [] {
      grid_rational m = pow_corner<InB, InE, false, false, false>;
      for (const grid_rational& c : {pow_corner<InB, InE, false, true, false>, pow_corner<InB, InE, true, false, false>,
                                     pow_corner<InB, InE, true, true, false>})
        if (c < m) m = c;
      return m;
    }();
    template <insidable InB, insidable InE>
    inline constexpr grid_rational pow_hi = [] {
      grid_rational m = pow_corner<InB, InE, false, false, true>;
      for (const grid_rational& c : {pow_corner<InB, InE, false, true, true>, pow_corner<InB, InE, true, false, true>,
                                     pow_corner<InB, InE, true, true, true>})
        if (m < c) m = c;
      return m;
    }();
    template <insidable InB, insidable InE>
    using pow = inside<{{pow_lo<InB, InE>, pow_hi<InB, InE>}, notch_of<InB>}, ax::auto_policy<InB>>;
  } // namespace auto_t

#define BEMAN_INSIDE_AX_AUTO(fn, cond)                                                  \
  template <insidable In>                                                               \
  [[nodiscard]] constexpr auto fn(In x)                                                 \
  {                                                                                     \
    static_assert(deducible<In>());                                                     \
    if constexpr (cond) return fn##_into<auto_t::fn<In>>(x);                            \
    else return fn##_into<inside<{0, 1}, round_nearest>>(x);   /* the _into domain message */ \
  }

  BEMAN_INSIDE_AX_AUTO(exp,   true)
  BEMAN_INSIDE_AX_AUTO(exp2,  true)
  BEMAN_INSIDE_AX_AUTO(sin,   true)
  BEMAN_INSIDE_AX_AUTO(cos,   true)
  BEMAN_INSIDE_AX_AUTO(tan,   true)
  BEMAN_INSIDE_AX_AUTO(atan,  true)
  BEMAN_INSIDE_AX_AUTO(sinh,  true)
  BEMAN_INSIDE_AX_AUTO(cosh,  true)
  BEMAN_INSIDE_AX_AUTO(tanh,  true)
  BEMAN_INSIDE_AX_AUTO(asinh, true)
  BEMAN_INSIDE_AX_AUTO(cbrt,  true)
  BEMAN_INSIDE_AX_AUTO(log,   (lower_of<In> > 0))
  BEMAN_INSIDE_AX_AUTO(log2,  (lower_of<In> > 0))
  BEMAN_INSIDE_AX_AUTO(log10, (lower_of<In> > 0))
  BEMAN_INSIDE_AX_AUTO(asin,  (lower_of<In> >= -1 && upper_of<In> <= 1))
  BEMAN_INSIDE_AX_AUTO(acos,  (lower_of<In> >= -1 && upper_of<In> <= 1))
  BEMAN_INSIDE_AX_AUTO(acosh, (lower_of<In> >= 1))
  BEMAN_INSIDE_AX_AUTO(atanh, (lower_of<In> > -1 && upper_of<In> < 1))
#undef BEMAN_INSIDE_AX_AUTO

  template <insidable In>
    requires (lower_of<In> >= 0)
  [[nodiscard]] constexpr auto sqrt(In x)
  { static_assert(deducible<In>()); return sqrt_into<auto_t::sqrt<In>>(x); }

  template <insidable In>
    requires (lower_of<In> < 0)
  [[nodiscard]] constexpr auto sqrt(In x)
  { static_assert(deducible<In>()); return sqrt_into<auto_t::sqrt_signed<In>>(x); }

  template <imax Base, insidable In>
  [[nodiscard]] constexpr auto pow_base(In x)
  {
    static_assert(deducible<In>());
    static_assert(Base >= 2, "beman::inside::math::adaptive::pow_base: Base must be at least 2");
    return pow_base_into<auto_t::pow_base_t<Base, In>, Base>(x);
  }

  template <insidable InY, insidable InX>
  [[nodiscard]] constexpr auto atan2(InY y, InX x)
  {
    static_assert(deducible<InY>() && deducible<InX>());
    return atan2_into<auto_t::atan2<InY, InX>>(y, x);
  }

  template <insidable InX, insidable InY>
  [[nodiscard]] constexpr auto hypot(InX x, InY y)
  {
    static_assert(deducible<InX>() && deducible<InY>());
    return hypot_into<auto_t::hypot<InX, InY>>(x, y);
  }

  template <insidable InB, insidable InE>
    requires (lower_of<InB> > 0)
  [[nodiscard]] constexpr auto pow(InB base, InE exp)
  {
    static_assert(deducible<InB>() && deducible<InE>());
    return pow_into<auto_t::pow<InB, InE>>(base, exp);
  }
} // namespace beman::inside::math::adaptive

#endif // BEMAN_INSIDE_CMATH_ADAPTIVE_HPP
