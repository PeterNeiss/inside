// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_CMATH_ADAPTIVE_HPP
#define BEMAN_INSIDE_CMATH_ADAPTIVE_HPP

#include <beman/inside/detail/math_adaptive.hpp>
#include <beman/inside/detail/math_fp.hpp>   // the double tier's kernels
#include <beman/inside/detail/math_dd.hpp>   // the dd tier's kernels

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
  // Bits bounds the numerator's and denominator's bit widths.
  template <int S, std::size_t K, int Bits, std::size_t E>
  constexpr wide_sint<K> sqrt_exact_q(exact_frac<E> const& f) noexcept
  {
    using I = wide_sint<limbs_for_bits(2 * Bits + 2 * S + 2)>;
    const I n = (I{f.Num} * I{f.Den}) << (2 * S);
    return static_cast<wide_sint<K>>(isqrt(n) / I{f.Den});
  }

  // log x for x > 0 at scale S: x = m·2^e exactly, m in [0.7, 1.42] rounded to
  // scale S, log x = log m + e·ln 2.
  template <int S, std::size_t K, int Bits, std::size_t E>
  constexpr fx<K> log_exact(exact_frac<E> const& x) noexcept
  {
    using I = wide_sint<K>;
    constexpr std::size_t KI = limbs_for_bits(2 * Bits + S + 2);
    int e = floor_log2(x);
    I m = to_q_at<K, KI>(x, S - e);
    if (mul_q(m, m, S) > (one_q<K>(S) << 1)) { ++e; m = to_q_at<K, KI>(x, S - e); }
    const fx<K> l = log_series<S>(m, 1);
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

  // asin x for |x| ≤ 1/2 (exact): atan(x/√(1−x²)), the square root as
  // x·(1/√(1−x²)) with 1 − x² ≥ 3/4 in fixed point.
  template <int S, std::size_t K, int Bits, std::size_t E>
  constexpr fx<K> asin_small(exact_frac<E> const& x) noexcept
  {
    using I = wide_sint<K>;
    const I xq = to_q<S, K>(x);                          // within ½
    const I c = one_q<K>(S) - mul_q(xq, xq, S);          // within 1.5, ≥ 3/4
    const I t = mul_q(xq, rsqrt_q<S>(c), S);             // |t| ≤ 0.58, within 6
    return atan_fixed<S>(t, 6);
  }

  // asin √y for 0 ≤ y ≤ 1/4 (exact): u = √y exactly rounded (within 1), then
  // atan(u·(1/√(1−u²))) with 1 − u² ≥ 3/4.
  template <int S, std::size_t K, int Bits, std::size_t E>
  constexpr fx<K> asin_sqrt(exact_frac<E> const& y) noexcept
  {
    using I = wide_sint<K>;
    const I u = sqrt_exact_q<S, K, Bits + 2>(y);
    const I w = one_q<K>(S) - mul_q(u, u, S);            // within 2, ≥ 3/4
    const I t = mul_q(u, rsqrt_q<S>(w), S);              // within 7
    return atan_fixed<S>(t, 7);
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
      return exp_fixed<S>(to_q<S, K>(X), 1, KMax);
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
      const fx<K> e = exp_series<S>(r, 3);
      return approx<K>{e.Value, S - kk, e.Error};
    }
  };

  // log x, x > 0. Exact at 1.
  template <std::size_t E, int Bits = frac_bits<E>>
  struct log_core
  {
    exact_frac<E> X;
    constexpr maybe_exact<E> exact() const
    { return is_one(X) ? maybe_exact<E>{exact_frac<E>{wide_sint<E>{0}, wide_sint<E>{1}}} : std::nullopt; }
    template <int W>
    constexpr auto run() const
    {
      constexpr int S = W + 8 + std::bit_width(static_cast<unsigned>(Bits));
      using I = fixed_t<S + std::bit_width(static_cast<unsigned>(Bits)) + 8>;
      constexpr std::size_t K = limbs_of<I>;
      const fx<K> l = log_exact<S, K, Bits>(X);
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
  template <std::size_t E, int B, int Bits = frac_bits<E>>
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
      constexpr int S = W + 10 + std::bit_width(static_cast<unsigned>(Bits));
      using I = fixed_t<S + std::bit_width(static_cast<unsigned>(Bits)) + 8>;
      constexpr std::size_t K = limbs_of<I>;
      const fx<K> l = log_exact<S, K, Bits>(X);
      // log x · (1/ln B): within 1.45·error + 2 (the constant is within 1).
      const I inv = B == 2 ? static_cast<I>(log2e_q<S>) : static_cast<I>(log10e_q<S>);
      return approx<K>{mul_q(l.Value, inv, S), S, 2 * l.Error + 2 + static_cast<umax>(Bits)};
    }
  };

  // √x, x ≥ 0: ⌊√x·2^P⌋ exactly. Exact when x is a square of a rational.
  template <std::size_t E, int Bits = frac_bits<E>>
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
      using I = fixed_t<Bits + P + 4>;
      constexpr std::size_t K = limbs_of<I>;
      return approx<K>{sqrt_exact_q<P, K, Bits>(X), P, 1};
    }
  };

  // ∛x: ⌊∛(n·d²·2^(3P))⌋/d exactly. Exact when x is a cube of a rational.
  template <std::size_t E, int Bits = frac_bits<E>>
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
      using I = fixed_t<Bits + P + 4>;
      constexpr std::size_t K = limbs_of<I>;
      using J = wide_sint<limbs_for_bits(3 * Bits + 3 * P + 4)>;
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
      using I = fixed_t<(Fn == trig::tan ? S + KMax + 8 : S + 2)>;   // |sin|, |cos| ≤ 1; |tan| ≤ 2^(KMax+3)
      using R = fixed_t<T + Mag + 8>;
      constexpr std::size_t K = limbs_of<I>;
      const R xq = to_q<T, limbs_of<R>>(X);
      const R hp = static_cast<R>(pi_q<T - 1>);          // π/2 within 1
      const R k = round_shift(mul_q(xq, static_cast<R>(two_over_pi_q<T>), T), T);   // round(x·2/π)
      const I r = static_cast<I>(round_shift(xq - k * hp, T - S));   // |r| ≤ π/4 + a hair, within 2
      const unsigned q = static_cast<unsigned>(k.Word[0] & 3u);
      // sin x and cos x by quadrant: (s, c), (c, −s), (−s, −c), (−c, s).
      auto neg = [](fx<K> f) { return fx<K>{-f.Value, f.Error}; };
      auto sin_x = [&] { return (q == 0) ? sin_series<S>(r, 2) : (q == 1) ? cos_series<S>(r, 2) : (q == 2) ? neg(sin_series<S>(r, 2)) : neg(cos_series<S>(r, 2)); };
      auto cos_x = [&] { return (q == 0) ? cos_series<S>(r, 2) : (q == 1) ? neg(sin_series<S>(r, 2)) : (q == 2) ? neg(cos_series<S>(r, 2)) : sin_series<S>(r, 2); };
      if constexpr (Fn == trig::sin) { const fx<K> sn = sin_x(); return approx<K>{sn.Value, S, sn.Error}; }
      else if constexpr (Fn == trig::cos) { const fx<K> cs = cos_x(); return approx<K>{cs.Value, S, cs.Error}; }
      else
      {
        const fx<K> sn = sin_x(), cs = cos_x();
        // tan = sin/cos. The error bound, (δs + |t|·δc)/(|c| − δc) + 2 units,
        // and the test for a result past 2^KMax only need to be upper and
        // lower bounds: computed in doubles, widened by a hair.
        const double cd = static_cast<double>(cs.Value), sd = static_cast<double>(sn.Value);
        const double ac = cd < 0 ? -cd : cd, as = sd < 0 ? -sd : sd;
        const double ec = static_cast<double>(cs.Error), es = static_cast<double>(sn.Error);
        if (!(2 * ec < ac)) return approx<K>{I{0}, S, ~umax{0}};          // cos unresolved
        const double unit = ::beman::inside::detail::ldexp(1.0, S);
        if ((as - es) / (ac + ec) * (1 - 0x1p-40) > ::beman::inside::detail::ldexp(1.0, KMax))
          return approx<K>{(sn.Value.negative() != cs.Value.negative()) ? I{-1} : I{1}, -(KMax + 2), 0};
        const I t = div_q(sn.Value, cs.Value, S);
        const double at = (as + es) / (ac - ec);
        const double err = ((es + at * ec) / (ac - ec) * unit + 2) * (1 + 0x1p-40) + 1;
        const umax e = err < 0x1p62 ? static_cast<umax>(err) : ~umax{0};
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
      return atan_fixed<S>(to_q<S, K>(x), 1);
    const fx<K> a = atan_fixed<S>(to_q<S, K>(inverse(x)), 1);
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
      using I = fixed_t<S + 3>;                          // |atan| < 2, 1 + t·c ≤ 2
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
  template <std::size_t E, int Bits = frac_bits<E>>
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
        const fx<K> a = asin_small<S, K, Bits>(X);
        return approx<K>{a.Value, S, a.Error};
      }
      using F = exact_frac<E + 1>;
      const F one = exact_one<E + 1>();
      const F y = (one + F{-abs(X)}) * F{half};          // (1 − |x|)/2
      const fx<K> a = asin_sqrt<S, K, Bits + 1>(y);
      const I v = static_cast<I>(pi_q<S - 1>) - (a.Value << 1);
      return approx<K>{X.Num.negative() ? -v : v, S, 2 * a.Error + 1};
    }
  };

  // acos x = π/2 − asin x for |x| ≤ 1/2; 2·asin √((1−x)/2) above;
  // π − 2·asin √((1+x)/2) below. Exact at 1.
  template <std::size_t E, int Bits = frac_bits<E>>
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
        const fx<K> a = asin_small<S, K, Bits>(X);
        return approx<K>{static_cast<I>(pi_q<S - 1>) - a.Value, S, a.Error + 1};
      }
      using F = exact_frac<E + 1>;
      const F one = exact_one<E + 1>();
      const F x{X};
      const bool neg = X.Num.negative();
      const F y = (neg ? one + x : one + F{-x}) * F{half};
      const fx<K> a = asin_sqrt<S, K, Bits + 1>(y);
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
      const bool neg = X.Num.negative();
      const I ax = to_q<S, K>(abs(X));                   // within ½
      if constexpr (Fn == hyp::tanh)
      {
        // tanh |x| = (1 − g)/(1 + g), g = e^(−2|x|) ≤ 1.
        const fx<K> g = rescale(exp_fixed<S>(-(ax << 1), 1, KM), A);
        const I one = one_q<K>(A);
        const I t = div_q(one - g.Value, one + g.Value, A);
        return approx<K>{neg ? -t : t, A, 2 * g.Error + 2};
      }
      else
      {
        // One reduction |x| = k·ln 2 + r: e^r = E + O and e^−r = E − O from
        // the even and odd halves of the series.
        const I k = round_shift(mul_q(ax, static_cast<I>(log2e_q<S>), S), S);
        if (I{KM} < k)                                    // e^|x| past 2^KM: past Out
          return approx<K>{(Fn == hyp::sinh && neg) ? I{-1} : I{1}, -(KMax + 2), 0};
        const int kk = static_cast<int>(static_cast<imax>(k));
        const I r = ax - k * static_cast<I>(ln2_q<S>);    // within ½ + k
        const I z = mul_q(r, r, S);
        const I ev = horner<series::cosh, S, false>(z);
        const I od = mul_q(r, horner<series::sinh, S, false>(z), S);
        const umax e = 24 + 2 * (static_cast<umax>(kk) + 2);
        const I P = round_shift(ev + od, S - kk - A);     // e^|x| at scale A
        const I M = round_shift(ev - od, S + kk - A);     // e^−|x| at scale A
        const umax ep = (e >> (S - kk - A)) + 1, em = (e >> (S + kk - A)) + 1;
        if constexpr (Fn == hyp::cosh)
          return approx<K>{(P + M) >> 1, A, (ep + em) / 2 + 1};
        else
        {
          const I sv = (P - M) >> 1;
          return approx<K>{neg ? -sv : sv, A, (ep + em) / 2 + 1};
        }
      }
    }
  };

  // asinh x = ±log(|x| + √(x²+1)); acosh x = log(x + √((x−1)(x+1))), x ≥ 1;
  // atanh x = ±½·log((1+|x|)/(1−|x|)), |x| < 1.
  enum class ahyp { asinh, acosh, atanh };

  template <std::size_t E, int Mag, ahyp Fn, int Bits = frac_bits<E>>
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
      constexpr int S = W + 12 + std::bit_width(static_cast<unsigned>(2 * Bits + Mag));
      using I = fixed_t<S + Mag + 8>;
      constexpr std::size_t K = limbs_of<I>;
      using F = exact_frac<2 * E + 1>;
      const F one = exact_one<2 * E + 1>();
      const bool neg = X.Num.negative();
      const F a = abs(F{X});
      if constexpr (Fn == ahyp::atanh)
      {
        const fx<K> l = log_exact<S, K, 2 * Bits + 2>((one + a) / (one + F{-a}));
        return approx<K>{neg ? -(l.Value >> 1) : (l.Value >> 1), S, l.Error / 2 + 1};
      }
      else
      {
        if constexpr (Fn == ahyp::asinh)
        {
          // c = x² + 1 = m·4^h, m in [1, 4): √c = √m·2^h with √m = m·(1/√m);
          // log of v/2^h = (|x| + √c)/2^h, plus h·ln 2.
          const I aq = to_q<S, K>(a);                    // within ½
          const I c = mul_q(aq, aq, S) + one_q<K>(S);    // within |x| + 1
          const int h = (bit_width_of(c) - 1 - S) / 2;
          const I m = c >> (2 * h);                      // within 3
          const I v = (aq >> h) + mul_q(m, rsqrt_q<S>(m), S);   // in [1, 3), within 30
          const fx<K> l = log_fixed<S>(v, 30);
          const I lv = l.Value + I{h} * static_cast<I>(ln2_q<S>);
          return approx<K>{neg ? -lv : lv, S, l.Error + static_cast<umax>(h) + 1};
        }
        else
        {
          const F r = (a + F{-one}) * (a + one);
          const I v = to_q<S, K>(a) + sqrt_exact_q<S, K, 2 * Bits + 2>(r);   // ≥ 1, within 2
          const fx<K> l = log_fixed<S>(v, 2);
          return approx<K>{l.Value, S, l.Error};
        }
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

  // ConstBase, when nonzero, is the base known at compile time (pow_base): its
  // log is then a constant of each precision.
  template <std::size_t EB, std::size_t EE, int MagE, int KMax, int BitsB = frac_bits<EB>, int BitsE = frac_bits<EE>,
            imax ConstBase = 0>
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
      constexpr int SL = S + MagE + std::bit_width(static_cast<unsigned>(BitsB));
      using I = fixed_t<SL + MagE + std::bit_width(static_cast<unsigned>(BitsB)) + BitsE + 8>;
      constexpr std::size_t K = limbs_of<I>;
      const fx<K> l = [&] {
        if constexpr (ConstBase != 0)
        {
          constexpr fx<K> c = log_exact<SL, K, BitsB>(exact_int<EB>(ConstBase));
          return c;
        }
        else
          return log_exact<SL, K, BitsB>(B);
      }();
      const I prod = mul_q(l.Value, static_cast<I>(Exp.Num), 0);
      const I q = bit_width_of(Exp.Den) <= 64 ? div_small(prod, static_cast<umax>(Exp.Den))
                                               : prod / static_cast<I>(Exp.Den);
      const I t = round_shift(q, SL - S);
      const umax dt = (l.Error >> (SL - S - MagE)) + 2;
      return exp_fixed<S>(t, dt, KMax);
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
        "beman::inside::math: the deduced output of exp, exp2, sinh and cosh needs |x| <= 4096 - "
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
        "beman::inside::math::pow: the deduced output would pass 2^65536 - name an output grid "
        "with pow_into<Out> instead");
    return static_cast<int>(max_abs_int<InE>) * log2_span<InB> + 2;
  }();

  // Bits of x² + y² as one fraction: each square doubles its input's bits,
  // and the sum over two denominators adds them.
  template <insidable InX, insidable InY>
  inline constexpr int hypot_bits = 2 * (input_bits<InX> + input_bits<InY>) + 2;

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

  //---------------------------------------------------------------------------
  // The double tier. Where an FPU is present, the double engine's kernels
  // (detail/math_fp.hpp) give the value first, with an error bound computed per
  // call: the kernel's evaluation error — 2^-40 of the result plus
  // 2^-44·max(1, |x|), a wide margin over their measured error of about one
  // ulp (2^-52) inside the argument ranges below; tan, pow and acosh add their
  // condition numbers — plus, for an input that is not a double exactly, its
  // rounding (2^-50 of |x|, covering the conversion's roundings) times the
  // function's slope. When the bound places the result in one slot, that slot
  // is the correctly rounded result; otherwise the integer path decides, so a
  // result never depends on which path ran. Runtime only, for outputs of up to
  // 36 bits.
  //---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_MATH_NO_FP
  inline constexpr bool fp_tier_available = true;
#else
  inline constexpr bool fp_tier_available = false;
#endif

  // Every value of In is a double: a dyadic notch, and values of at most 53
  // significant bits.
  template <insidable In>
  inline constexpr bool fp_exact_input = [] {
    if constexpr (exact_valued<In> || rational_raw<In> || notch_of<In> == 0) return false;
    else
    {
      const grid_wide q = wide_denominator(notch_of<In>), p = wide_numerator(notch_of<In>);
      const bool dyadic = (grid_wide{1} << (grid_bits(q) - 1)) == q;
      return dyadic && grid_magnitude_bits<In> + grid_bits(q) + grid_bits(p) <= 53;
    }
  }();

  template <insidable Out>
  inline constexpr bool fp_output = slotted<Out> && !exact_valued<Out> && out_bits<Out> + mag_bits<Out> <= 36;

  // Inputs the tier reads as doubles: anything within the 64-bit rationals.
  template <insidable Out, insidable... Ins>
  inline constexpr bool fp_tier = fp_tier_available && fp_output<Out> && (!exact_valued<Ins> && ...);

  // A grid's notch p/q as doubles (both below 2^63, so within 2^-53 of p and
  // q), and whether it is a power of two, so that scaling by it is exact.
  template <insidable Out>
  inline constexpr double notch_p = static_cast<double>(static_cast<imax>(wide_numerator(notch_of<Out>)));
  template <insidable Out>
  inline constexpr double notch_q = static_cast<double>(static_cast<imax>(wide_denominator(notch_of<Out>)));
  template <insidable Out>
  inline constexpr bool dyadic_notch = [] {
    const grid_wide p = wide_numerator(notch_of<Out>), q = wide_denominator(notch_of<Out>);
    return (grid_wide{1} << (grid_bits(p) - 1)) == p && (grid_wide{1} << (grid_bits(q) - 1)) == q;
  }();

  // Value indices of In below 2^53: exact as doubles.
  template <insidable In>
  inline constexpr bool small_index = !exact_valued<In> && !rational_raw<In> && notch_of<In> != 0
      && grid_magnitude_bits<In> + grid_bits(wide_denominator(notch_of<In>)) - grid_bits(wide_numerator(notch_of<In>)) + 1 <= 53;

  // An input's value as a double: exactly (its value index times the dyadic
  // notch) when fp_exact_input; else its index times p over q, within 2^-51
  // (four roundings of 2^-53: p, q, the product and the quotient); else the
  // conversion's nearest double.
  template <insidable In>
  constexpr double input_double(In const& x) noexcept
  {
    if constexpr (fp_raw<In> || point_raw<In> || rational_raw<In>) return static_cast<double>(x);
    else if constexpr (fp_exact_input<In>) return static_cast<double>(value_index<imax>(x)) * (notch_p<In> / notch_q<In>);
    else if constexpr (small_index<In>) return static_cast<double>(value_index<imax>(x)) * notch_p<In> / notch_q<In>;
    else return static_cast<double>(x);
  }

  // The relative rounding of an input's double: 0 when exact.
  template <insidable In>
  inline constexpr double input_rel = fp_exact_input<In> ? 0.0 : 0x1p-50;

  constexpr double fabs_d(double v) noexcept { return __builtin_fabs(v); }

  // An integer near t (the nearest in the default rounding mode); NaN stays
  // NaN. The tests below only need J to be an integer, so neither the
  // rounding mode nor reassociation can make them pass wrongly — unlike the
  // add-and-subtract-2^52 trick, which -ffast-math folds away.
  inline double nearest_int(double t) noexcept { return __builtin_nearbyint(t); }

  // The slot of a kernel value v within an absolute bound, decided in double
  // arithmetic. t = v·q/p is v's value index (exact for a dyadic notch, else
  // within |t|·2^-52) and bt the bound in index units; the bound's own 1.5
  // margin covers the roundings of its arithmetic. Every point of
  // [t − bt, t + bt] must round to the same index J: for the nearest modes
  // |t − J| < ½ − bt, for the directed ones a floor F with F < t − bt and
  // t + bt < F + 1. Exact ties and integers fail the strict tests and go to
  // the integer path. NaN and infinities fail every comparison, and the range
  // check runs in double before J becomes an integer, so no finite check is
  // needed. A decided slot inside Out's range is stored in out.
  template <insidable Out>
  inline bool fp_decide(double v, double bound, Out& out) noexcept
  {
    constexpr round_mode M = out_rounding<Out>;
    constexpr double s = notch_q<Out> / notch_p<Out>;
    constexpr double lo = static_cast<double>(static_cast<imax>(slot_base<Out>));
    constexpr double hi = lo + static_cast<double>(static_cast<imax>(grid_of<Out>.slot_count()));
    const double t = v * s;
    double bt = bound * s;
    if constexpr (!dyadic_notch<Out>) bt += fabs_d(t) * 0x1p-51;
    double j = nearest_int(t);
    if constexpr (M == round_mode::nearest || M == round_mode::half_even)
    {
      if (!(fabs_d(t - j) < 0.5 - bt)) return false;
    }
    else
    {
      if (j > t) j -= 1;                                   // floor(t)
      if (!(t - j > bt && j + 1 - t > bt)) return false;
      if constexpr (M == round_mode::ceil) j += 1;
      if constexpr (M == round_mode::trunc) { if (j < 0) j += 1; }
    }
    if (!(j >= lo && j <= hi)) return false;
    if constexpr (integer_raw<Out>)
      out = Out::from_raw(raw_from_offset<Out>(static_cast<umax>(static_cast<imax>(j - lo))));
    else if constexpr (rational_raw<Out>)                 // the grid point as a fraction
      out = store<Out>(wide_sint<2>{static_cast<umax>(static_cast<imax>(j - lo))});
    else                                                  // fp raw: the grid point, exact
      out = Out::from_raw(static_cast<raw_t<Out>>(j * (notch_p<Out> / notch_q<Out>)));
    return true;
  }

  // The kernels' safe argument ranges (compile-time, from In's grid).
  template <insidable In>
  inline constexpr double in_max = static_cast<double>(max_abs_int<In>);

  inline constexpr double kEvalRel = 0x1p-40, kEvalAbs = 0x1p-44;

  // Evaluation bound of the plain kernels.
  constexpr double eval_bound(double x, double v) noexcept
  { return kEvalRel * fabs_d(v) + kEvalAbs * (fabs_d(x) > 1 ? fabs_d(x) : 1.0); }

#ifndef BEMAN_INSIDE_MATH_NO_FP
  namespace fpk = ::beman::inside::math::detail::fp;

  //---------------------------------------------------------------------------
  // The dd tier: outputs past the double tier whose value indices stay below
  // 2^62. The double-double kernels (detail/math_dd.hpp) are within about
  // 2^-97 of their values (measured against the integer path at 150 bits:
  // 2^-97 relative at worst, 2^-99 absolute); the bound is 2^-88 of the
  // result plus 2^-92·max(1, |x|), the same structure as the double tier's
  // with a margin of 2^9, and inputs that are not doubles add 2^-100 of |x|
  // times the slope. Undecided results go to the integer path as before.
  //---------------------------------------------------------------------------
  namespace ddk = ::beman::inside::math::detail::dd;

  inline constexpr double kDDRel = 0x1p-88, kDDAbs = 0x1p-92;

  constexpr double dd_eval_bound(double x, double v) noexcept
  { return kDDRel * fabs_d(v) + kDDAbs * (fabs_d(x) > 1 ? fabs_d(x) : 1.0); }

  inline constexpr double two53 = 0x1p53;

  // Value indices of Out of at most 2^62 in magnitude, and a notch whose p and q
  // are doubles exactly.
  template <insidable Out>
  inline constexpr bool dd_output = [] {
    if constexpr (!slotted<Out> || exact_valued<Out> || fp_output<Out>) return false;
    else
    {
      if (!(notch_p<Out> < two53 && notch_q<Out> < two53)) return false;
      const grid_wide lim = grid_wide{1} << 62;
      const grid_wide lo = slot_base<Out>, hi = slot_base<Out> + grid_of<Out>.slot_count();
      return !(lo < -lim) && !(lim < hi);
    }
  }();

  // Inputs the tier reads exactly (doubles) or within 2^-100 (index·p/q, or
  // a rational raw's numerator over its denominator).
  template <insidable In>
  inline constexpr bool dd_input = !exact_valued<In> && !point_raw<In>
      && (fp_raw<In> || fp_exact_input<In> || rational_raw<In>
          || (small_index<In> && notch_p<In> < two53 && notch_q<In> < two53));

  // The error-free sums need the additions in program order: a build that
  // lets the compiler reassociate (-fassociative-math, part of -ffast-math)
  // leaves the tier out.
#if defined(__ASSOCIATIVE_MATH__) || defined(__FAST_MATH__)
  inline constexpr bool dd_sums_exact = false;
#else
  inline constexpr bool dd_sums_exact = true;
#endif

  template <insidable Out, insidable... Ins>
  inline constexpr bool dd_tier = fp_tier_available && dd_sums_exact && dd_output<Out> && (dd_input<Ins> && ...);

  template <insidable In>
  inline constexpr double dd_input_rel = (fp_raw<In> || fp_exact_input<In>) ? 0.0 : 0x1p-100;

  template <insidable In>
  inline ddk::dd dd_read(In const& x) noexcept
  {
    if constexpr (fp_raw<In>) return {static_cast<double>(x), 0};
    else if constexpr (fp_exact_input<In>) return {input_double(x), 0};
    else if constexpr (rational_raw<In>)
    {
      const rational r = x.raw();                         // ±Numerator/|Denominator|
      auto exact = [](umax n) {                           // two 32-bit halves, each a double exactly
        return ddk::fast_two_sum(static_cast<double>(n & ~umax{0xFFFFFFFF}), static_cast<double>(n & umax{0xFFFFFFFF}));
      };
      const imax d = r.Denominator;
      const ddk::dd q = ddk::div(exact(r.Numerator), exact(static_cast<umax>(d < 0 ? -d : d)));
      return d < 0 ? ddk::neg(q) : q;
    }
    else
    {
      const ddk::dd n = ddk::two_prod(static_cast<double>(value_index<imax>(x)), notch_p<In>);
      return ddk::div(n, ddk::dd{notch_q<In>, 0});
    }
  }

  // The slot of a dd value v within an absolute bound: the value index
  // t = v·q/p as a dd (within 2^-104 of t), split into J1 = nearbyint(t.Hi)
  // and the rest u = (t.Hi − J1) + t.Lo, which rounds to J2 with remainder d.
  // The tests on d are fp_decide's, with the bound widened by u's rounding.
  template <insidable Out>
  inline bool dd_decide(ddk::dd v, double bound, Out& out) noexcept
  {
    constexpr round_mode M = out_rounding<Out>;
    constexpr ddk::dd s = ddk::div(ddk::dd{notch_q<Out>, 0}, ddk::dd{notch_p<Out>, 0});
    constexpr imax first = static_cast<imax>(slot_base<Out>);              // |index| ≤ 2^62
    constexpr imax last = static_cast<imax>(slot_base<Out> + grid_of<Out>.slot_count());
    constexpr double lo = static_cast<double>(first) - 1024, hi = static_cast<double>(last) + 1024;
    // Below 2^52, t.Hi's nearest integer is J itself unless t.Lo carries u
    // past ½, which the tests then reject: one rounding instead of two.
    constexpr bool narrow = lo > -0x1p52 && hi < 0x1p52;
    const ddk::dd t = ddk::mul(v, s);
    const double j1 = nearest_int(t.Hi);
    if (!(j1 >= lo && j1 <= hi)) return false;            // NaN and infinities too
    const double u = (t.Hi - j1) + t.Lo;
    double j2 = narrow ? 0.0 : nearest_int(u);
    double d = u - j2;
    const double bt = bound * s.Hi + fabs_d(t.Hi) * 0x1p-100 + (fabs_d(u) + 1) * 0x1p-52;
    if constexpr (M == round_mode::nearest || M == round_mode::half_even)
    {
      if (!(fabs_d(d) < 0.5 - bt)) return false;
    }
    else
    {
      if (d < 0) { d += 1; j2 -= 1; }                     // j1 + j2 = floor(t)
      if (!(d > bt && 1 - d > bt)) return false;
      if constexpr (M == round_mode::ceil) j2 += 1;
    }
    imax j = static_cast<imax>(j1) + static_cast<imax>(j2);
    if constexpr (M == round_mode::trunc) { if (j < 0) j += 1; }
    if (j < first || j > last) return false;
    if constexpr (integer_raw<Out>)
      out = Out::from_raw(raw_from_offset<Out>(static_cast<umax>(j) - static_cast<umax>(first)));
    else if constexpr (rational_raw<Out>)                 // the grid point as a fraction
      out = store<Out>(wide_sint<2>{static_cast<umax>(j) - static_cast<umax>(first)});
    else                                                  // fp raw: the grid point, exact
      out = Out::from_raw(static_cast<raw_t<Out>>(static_cast<double>(j) * (notch_p<Out> / notch_q<Out>)));
    return true;
  }



  // One kernel per function: its value, its evaluation bound, and its slope
  // |f′(x)| (for the input's rounding).
  struct fp_plain
  {
    static double eval(double x, double v) { return eval_bound(x, v); }
    static double dd_eval(double x, double v) { return dd_eval_bound(x, v); }
  };
#  define BEMAN_INSIDE_AX_KERNEL(fn, slope_expr)                                        \
  struct fp_##fn : fp_plain                                                             \
  {                                                                                     \
    static double value(double x) { return fpk::fp_##fn(x); }                           \
    template <typename D> static D dd_value(D x) { return ddk::fn(x); }                 \
    static double slope([[maybe_unused]] double x, [[maybe_unused]] double v) { return slope_expr; } \
  };
  BEMAN_INSIDE_AX_KERNEL(sin,   1.0)
  BEMAN_INSIDE_AX_KERNEL(cos,   1.0)
  BEMAN_INSIDE_AX_KERNEL(exp,   fabs_d(v))
  BEMAN_INSIDE_AX_KERNEL(exp2,  fabs_d(v))
  BEMAN_INSIDE_AX_KERNEL(sinh,  fabs_d(v) + 1)
  BEMAN_INSIDE_AX_KERNEL(cosh,  fabs_d(v) + 1)
  BEMAN_INSIDE_AX_KERNEL(tanh,  1.0)
  BEMAN_INSIDE_AX_KERNEL(atan,  1.0)
  BEMAN_INSIDE_AX_KERNEL(asinh, 1.0)
  BEMAN_INSIDE_AX_KERNEL(cbrt,  x == 0 ? 0.0 : fabs_d(v / x))
  BEMAN_INSIDE_AX_KERNEL(sqrt,  x == 0 ? 0.0 : fabs_d(v / x))
  BEMAN_INSIDE_AX_KERNEL(log,   1.0 / fabs_d(x))
  BEMAN_INSIDE_AX_KERNEL(log2,  1.5 / fabs_d(x))
  BEMAN_INSIDE_AX_KERNEL(log10, 1.0 / fabs_d(x))
  BEMAN_INSIDE_AX_KERNEL(asin,  1.0 / fpk::fp_sqrt((1.0 - x) * (1.0 + x)))
  BEMAN_INSIDE_AX_KERNEL(acos,  1.0 / fpk::fp_sqrt((1.0 - x) * (1.0 + x)))
  BEMAN_INSIDE_AX_KERNEL(atanh, 1.0 / ((1.0 - x) * (1.0 + x)))
#  undef BEMAN_INSIDE_AX_KERNEL

  // acosh near 1 loses the bits of 1 − 1/x²: its error grows as 1/√(1 − 1/x²).
  struct fp_acosh
  {
    static double value(double x) { return fpk::fp_acosh(x); }
    template <typename D> static D dd_value(D x) { return ddk::acosh(x); }
    static double eval(double x, double v) { return kEvalAbs / fpk::fp_sqrt(1.0 - 1.0 / (x * x)) + eval_bound(x, v); }
    static double dd_eval(double x, double v) { return kDDAbs / fpk::fp_sqrt(1.0 - 1.0 / (x * x)) + dd_eval_bound(x, v); }
    static double slope(double x, double) { return 1.0 / fpk::fp_sqrt((x - 1.0) * (x + 1.0)); }
  };

  // The tier's attempt for a one-input kernel K at input x (as read from In).
  template <insidable Out, typename K, insidable In>
  inline bool fp_attempt(In const& in, Out& out)
  {
    const double x = input_double(in);
    const double v = K::value(x);
    double bound = K::eval(x, v);
    if constexpr (!fp_exact_input<In>) bound += input_rel<In> * fabs_d(x) * K::slope(x, v);
    return fp_decide(v, bound * 1.5, out);
  }

  // atan2: each input's rounding moves the angle by at most its relative
  // rounding (|∂/∂y|·|y| = |x·y|/r² ≤ ½).
  template <insidable Out, insidable InY, insidable InX>
  inline bool fp_attempt_atan2(InY const& yi, InX const& xi, Out& out)
  {
    const double y = input_double(yi), x = input_double(xi);
    const double v = fpk::fp_atan2(y, x);
    const double bound = eval_bound(1.0, v) + input_rel<InY> + input_rel<InX>;
    return fp_decide(v, bound * 1.5, out);
  }

  template <insidable Out, insidable InX, insidable InY>
  inline bool fp_attempt_hypot(InX const& xi, InY const& yi, Out& out)
  {
    const double x = input_double(xi), y = input_double(yi);
    const double v = fpk::fp_hypot(x, y);
    const double bound = eval_bound(fabs_d(x) > fabs_d(y) ? fabs_d(x) : fabs_d(y), v)
                       + input_rel<InX> * fabs_d(x) + input_rel<InY> * fabs_d(y);
    return fp_decide(v, bound * 1.5, out);
  }

  // tan: the reduction's error grows by sec² = 1 + t².
  template <insidable Out, insidable In>
  inline bool fp_attempt_tan(In const& in, Out& out)
  {
    const double x = input_double(in);
    double t = 0;
    if (!fpk::fp_tan(x, t)) return false;
    const double sec2 = 1 + t * t, mx = fabs_d(x) > 1 ? fabs_d(x) : 1.0;
    const double bound = kEvalRel * sec2 * mx + kEvalAbs * mx + input_rel<In> * fabs_d(x) * sec2;
    return fp_decide(t, bound * 1.5, out);
  }

  // pow = e^(e·ln b): the relative error grows with |e·ln b|; the inputs'
  // roundings move it by |e|·rel(b) and |e·ln b|·rel(e).
  template <insidable Out, insidable InB, insidable InE>
  inline bool fp_attempt_pow(InB const& bi, InE const& ei, Out& out)
  {
    const double b = input_double(bi), e = input_double(ei);
    if (!(b > 0)) return false;                           // the integer path reports the domain
    const double v = fpk::fp_pow(b, e);
    const double L = fabs_d(e * fpk::fp_log(b));
    const double bound = fabs_d(v) * (kEvalRel * (1 + L) + input_rel<InB> * fabs_d(e) + input_rel<InE> * L) + kEvalAbs;
    return fp_decide(v, bound * 1.5, out);
  }

  // Base^x: pow with an exact integer base.
  template <insidable Out, imax Base, insidable In>
  inline bool fp_attempt_pow_base(In const& xi, Out& out)
  {
    const double x = input_double(xi);
    const double v = fpk::fp_pow(static_cast<double>(Base), x);
    const double L = fabs_d(x * fpk::fp_log(static_cast<double>(Base)));
    const double bound = fabs_d(v) * (kEvalRel * (1 + L) + input_rel<In> * L) + kEvalAbs;
    return fp_decide(v, bound * 1.5, out);
  }
  template <insidable Out, typename K, insidable In>
  inline bool dd_attempt(In const& in, Out& out)
  {
    const ddk::dd x = dd_read(in);
    const ddk::dd v = K::dd_value(x);
    double bound = K::dd_eval(x.Hi, v.Hi);
    if constexpr (dd_input_rel<In> != 0) bound += dd_input_rel<In> * fabs_d(x.Hi) * K::slope(x.Hi, v.Hi);
    return dd_decide(v, bound * 1.5, out);
  }

  template <insidable Out, insidable InY, insidable InX>
  inline bool dd_attempt_atan2(InY const& yi, InX const& xi, Out& out)
  {
    const ddk::dd y = dd_read(yi), x = dd_read(xi);
    const ddk::dd v = ddk::atan2(y, x);
    const double bound = dd_eval_bound(1.0, v.Hi) + dd_input_rel<InY> + dd_input_rel<InX>;
    return dd_decide(v, bound * 1.5, out);
  }

  template <insidable Out, insidable InX, insidable InY>
  inline bool dd_attempt_hypot(InX const& xi, InY const& yi, Out& out)
  {
    const ddk::dd x = dd_read(xi), y = dd_read(yi);
    const ddk::dd v = ddk::hypot(x, y);
    const double ax = fabs_d(x.Hi), ay = fabs_d(y.Hi);
    const double bound = dd_eval_bound(ax > ay ? ax : ay, v.Hi) + dd_input_rel<InX> * ax + dd_input_rel<InY> * ay;
    return dd_decide(v, bound * 1.5, out);
  }

  template <insidable Out, insidable In>
  inline bool dd_attempt_tan(In const& in, Out& out)
  {
    const ddk::dd x = dd_read(in);
    const ddk::dd t = ddk::tan(x);
    const double sec2 = 1 + t.Hi * t.Hi, mx = fabs_d(x.Hi) > 1 ? fabs_d(x.Hi) : 1.0;
    const double bound = kDDRel * sec2 * mx + kDDAbs * mx + dd_input_rel<In> * fabs_d(x.Hi) * sec2;
    return dd_decide(t, bound * 1.5, out);
  }

  // pow = e^(e·ln b), as in the double tier; e·ln b within the exp range.
  template <insidable Out, insidable InB, insidable InE>
  inline bool dd_attempt_pow(InB const& bi, InE const& ei, Out& out)
  {
    const ddk::dd b = dd_read(bi), e = dd_read(ei);
    if (!(b.Hi > 0)) return false;
    const ddk::dd y = ddk::mul(ddk::log(b), e);
    const double L = fabs_d(y.Hi);
    if (!(L <= 700)) return false;
    const ddk::dd v = ddk::exp(y);
    const double bound = fabs_d(v.Hi) * (kDDRel * (1 + L) + dd_input_rel<InB> * fabs_d(e.Hi) + dd_input_rel<InE> * L) + kDDAbs;
    return dd_decide(v, bound * 1.5, out);
  }

  // ln Base at compile time, from the integer log.
  template <imax Base>
  inline constexpr ddk::dd ln_base = [] {
    const ddk::fixed a = ddk::fixed{Base} << ddk::kS;
    return ddk::of_fixed(log_fixed<ddk::kS>(a, 0).Value, ddk::kS);
  }();

  template <insidable Out, imax Base, insidable In>
  inline bool dd_attempt_pow_base(In const& xi, Out& out)
  {
    const ddk::dd x = dd_read(xi);
    const ddk::dd y = ddk::mul(x, ln_base<Base>);
    const double L = fabs_d(y.Hi);
    const ddk::dd v = ddk::exp(y);
    const double bound = fabs_d(v.Hi) * (kDDRel * (1 + L) + dd_input_rel<In> * L) + kDDAbs;
    return dd_decide(v, bound * 1.5, out);
  }
#else
  inline constexpr bool dd_sums_exact = false;
  template <insidable Out, insidable... Ins>
  inline constexpr bool dd_tier = false;
#endif

  // Either fast tier ran before the integer path.
  template <insidable Out, insidable... Ins>
  inline constexpr bool fast_tier = fp_tier<Out, Ins...> || dd_tier<Out, Ins...>;
} // namespace beman::inside::math::detail::ax

namespace beman::inside::math::adaptive
{
  namespace ax = ::beman::inside::math::detail::ax;
  using ::beman::inside::detail::exact_valued;
  using ::beman::inside::detail::grid_rational;

  // The double and dd tiers' attempts, inside an _into form (nothing without
  // an FPU).
#ifndef BEMAN_INSIDE_MATH_NO_FP
#  define BEMAN_INSIDE_AX_FP(Out, In, fn, x, ok)                                        \
    if constexpr (ax::fp_tier<Out, In> && (ok))                                         \
      if !consteval                                                                     \
      {                                                                                 \
        if (Out r; ax::fp_attempt<Out, ax::fp_##fn>(x, r)) return r;                    \
      }                                                                                 \
    if constexpr (ax::dd_tier<Out, In> && (ok))                                         \
      if !consteval                                                                     \
      {                                                                                 \
        if (Out r; ax::dd_attempt<Out, ax::fp_##fn>(x, r)) return r;                    \
      }
#else
#  define BEMAN_INSIDE_AX_FP(Out, In, fn, x, ok)
#endif

  // The integer path after a fast tier: rarely taken, so out of line,
  // which keeps its frame off the tier's fast path.
  template <typename F>
  [[gnu::cold, gnu::noinline]] constexpr auto cold_call(F const& f) { return f(); }

#define BEMAN_INSIDE_AX_REST(fp_ran, ...)                                               \
    if constexpr (fp_ran) { return cold_call([&] { return __VA_ARGS__; }); } else { return __VA_ARGS__; }

  // The table tier's lookup, inside an _into form: In has few slots and every
  // result lies in Out's range.
#define BEMAN_INSIDE_AX_TABLE(Out, In, x)                                               \
    if constexpr (ax::table_input<In> && ax::table_output<Out>)                         \
    {                                                                                   \
      using table = ax::result_table<Out, In, ax::start_bits<Out>,                      \
                                     [](In v) { return core{ax::exact_input(v)}; }>;    \
      if constexpr (table::Table.Valid)                                                 \
        return Out::from_raw(table::Table.Raw[ax::offset_of(x)]);                       \
    }

  // Every transcendental rounds onto the output grid: Out's policy needs a
  // rounding mode, like any assignment that may round.
  template <insidable Out>
  consteval void require_rounding() noexcept
  {
    static_assert(has_flag(policy_of<Out>, snap) || !ax::slotted<Out>,
        "beman::inside::math: the result is rounded onto Out's grid - Out must permit rounding "
        "(declare it with round_nearest, round_floor, ...)");
  }

#define BEMAN_INSIDE_AX_UNARY(fn, fp_ok, ...)                                           \
  template <insidable Out, insidable In>                                                \
  [[nodiscard]] constexpr Out fn##_into(In x)                                           \
  {                                                                                     \
    require_rounding<Out>();                                                            \
    using core = __VA_ARGS__;                                                           \
    BEMAN_INSIDE_AX_TABLE(Out, In, x)                                                   \
    BEMAN_INSIDE_AX_FP(Out, In, fn, x, (fp_ok))                                         \
    BEMAN_INSIDE_AX_REST((ax::fast_tier<Out, In> && (fp_ok)),                             \
                         ax::evaluate<Out, ax::start_bits<Out>>(core{ax::exact_input(x)})) \
  }

  BEMAN_INSIDE_AX_UNARY(exp,   ax::in_max<In> <= 700,     ax::exp_core<ax::input_limbs<In>, ax::in_mag<In>, ax::out_kmax<Out>>)
  BEMAN_INSIDE_AX_UNARY(exp2,  ax::in_max<In> <= 1000,    ax::exp2_core<ax::input_limbs<In>, ax::in_mag<In>, ax::out_kmax<Out>>)
  BEMAN_INSIDE_AX_UNARY(sin,   ax::in_max<In> <= 0x1p20,  ax::trig_core<ax::input_limbs<In>, ax::in_mag<In>, ax::trig::sin, 1>)
  BEMAN_INSIDE_AX_UNARY(cos,   ax::in_max<In> <= 0x1p20,  ax::trig_core<ax::input_limbs<In>, ax::in_mag<In>, ax::trig::cos, 1>)
  BEMAN_INSIDE_AX_UNARY(atan,  true,                      ax::atan_core<ax::input_limbs<In>>)
  BEMAN_INSIDE_AX_UNARY(sinh,  ax::in_max<In> <= 700,     ax::hyp_core<ax::input_limbs<In>, ax::in_mag<In>, ax::hyp::sinh, ax::out_kmax<Out>>)
  BEMAN_INSIDE_AX_UNARY(cosh,  ax::in_max<In> <= 700,     ax::hyp_core<ax::input_limbs<In>, ax::in_mag<In>, ax::hyp::cosh, ax::out_kmax<Out>>)
  BEMAN_INSIDE_AX_UNARY(tanh,  ax::in_max<In> <= 300,     ax::hyp_core<ax::input_limbs<In>, ax::in_mag<In>, ax::hyp::tanh, 1>)
  BEMAN_INSIDE_AX_UNARY(asinh, ax::in_max<In> <= 0x1p500, ax::ahyp_core<ax::input_limbs<In>, ax::in_mag<In>, ax::ahyp::asinh, ax::input_bits<In>>)
  BEMAN_INSIDE_AX_UNARY(cbrt,  true,                      ax::cbrt_core<ax::input_limbs<In>, ax::input_bits<In>>)
#undef BEMAN_INSIDE_AX_UNARY

  // Functions with a mathematical domain: checked on In's grid.
#define BEMAN_INSIDE_AX_DOMAIN(fn, cond, msg, fp_ok, ...)                               \
  template <insidable Out, insidable In>                                                \
  [[nodiscard]] constexpr Out fn##_into(In x)                                           \
  {                                                                                     \
    static_assert(cond, "beman::inside::math::" #fn ": " msg);              \
    require_rounding<Out>();                                                            \
    using core = __VA_ARGS__;                                                           \
    BEMAN_INSIDE_AX_TABLE(Out, In, x)                                                   \
    BEMAN_INSIDE_AX_FP(Out, In, fn, x, (fp_ok))                                         \
    BEMAN_INSIDE_AX_REST((ax::fast_tier<Out, In> && (fp_ok)),                             \
                         ax::evaluate<Out, ax::start_bits<Out>>(core{ax::exact_input(x)})) \
  }

  BEMAN_INSIDE_AX_DOMAIN(log, (lower_of<In> > 0), "input must be strictly positive", true, ax::log_core<ax::input_limbs<In>, ax::input_bits<In>>)
  BEMAN_INSIDE_AX_DOMAIN(log2, (lower_of<In> > 0), "input must be strictly positive", true, ax::logb_core<ax::input_limbs<In>, 2, ax::input_bits<In>>)
  BEMAN_INSIDE_AX_DOMAIN(log10, (lower_of<In> > 0), "input must be strictly positive", true, ax::logb_core<ax::input_limbs<In>, 10, ax::input_bits<In>>)
  BEMAN_INSIDE_AX_DOMAIN(asin, (lower_of<In> >= -1 && upper_of<In> <= 1), "input must be in [-1, 1]", true, ax::asin_core<ax::input_limbs<In>, ax::input_bits<In>>)
  BEMAN_INSIDE_AX_DOMAIN(acos, (lower_of<In> >= -1 && upper_of<In> <= 1), "input must be in [-1, 1]", true, ax::acos_core<ax::input_limbs<In>, ax::input_bits<In>>)
  BEMAN_INSIDE_AX_DOMAIN(acosh, (lower_of<In> >= 1), "input must be at least 1", ax::in_max<In> <= 0x1p500, ax::ahyp_core<ax::input_limbs<In>, ax::in_mag<In>, ax::ahyp::acosh, ax::input_bits<In>>)
  BEMAN_INSIDE_AX_DOMAIN(atanh, (lower_of<In> > -1 && upper_of<In> < 1), "input must be in (-1, 1)", true, ax::ahyp_core<ax::input_limbs<In>, ax::in_mag<In>, ax::ahyp::atanh, ax::input_bits<In>>)
#undef BEMAN_INSIDE_AX_DOMAIN

  // sqrt of a non-negative input.
  template <insidable Out, insidable In>
    requires (lower_of<In> >= 0)
  [[nodiscard]] constexpr Out sqrt_into(In x)
  {
    require_rounding<Out>();
    BEMAN_INSIDE_AX_FP(Out, In, sqrt, x, true)
    BEMAN_INSIDE_AX_REST((ax::fast_tier<Out, In>),
        ax::evaluate<Out, ax::start_bits<Out>>(ax::sqrt_core<ax::input_limbs<In>, ax::input_bits<In>>{ax::exact_input(x)}))
  }

  // sqrt of a mixed-sign input: domain_error on a negative value.
  template <insidable Out, insidable In>
    requires (lower_of<In> < 0)
  [[nodiscard]] constexpr std::expected<Out, errc> sqrt_into(In x)
  {
    require_rounding<Out>();
    const auto v = ax::exact_input(x);
    if (v.Num.negative()) return std::unexpected(errc::domain_error);
    return ax::evaluate<Out, ax::start_bits<Out>>(ax::sqrt_core<ax::input_limbs<In>, ax::input_bits<In>>{v});
  }

  // tan: overflow when the result leaves Out (without clamp).
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr std::expected<Out, errc> tan_into(In x)
  {
    require_rounding<Out>();
#ifndef BEMAN_INSIDE_MATH_NO_FP
    if constexpr (ax::fp_tier<Out, In> && ax::in_max<In> <= 0x1p20)
      if !consteval
      {
        if (Out r; ax::fp_attempt_tan<Out>(x, r)) return r;
      }
    if constexpr (ax::dd_tier<Out, In> && ax::in_max<In> <= 0x1p20)
      if !consteval
      {
        if (Out r; ax::dd_attempt_tan<Out>(x, r)) return r;
      }
#endif
    using core = ax::trig_core<ax::input_limbs<In>, ax::in_mag<In>, ax::trig::tan, ax::out_kmax<Out>>;
    BEMAN_INSIDE_AX_REST((ax::fast_tier<Out, In> && ax::in_max<In> <= 0x1p20),
                         ax::evaluate_checked<Out, ax::start_bits<Out>>(core{ax::exact_input(x)}))
  }

  template <insidable Out, insidable InY, insidable InX>
  [[nodiscard]] constexpr Out atan2_into(InY y, InX x)
  {
    require_rounding<Out>();
#ifndef BEMAN_INSIDE_MATH_NO_FP
    if constexpr (ax::fp_tier<Out, InY, InX> && ax::in_max<InY> <= 0x1p500 && ax::in_max<InX> <= 0x1p500)
      if !consteval
      {
        if (Out r; ax::fp_attempt_atan2<Out>(y, x, r)) return r;
      }
    if constexpr (ax::dd_tier<Out, InY, InX>)
      if !consteval
      {
        if (Out r; ax::dd_attempt_atan2<Out>(y, x, r)) return r;
      }
#endif
    constexpr std::size_t E = ax::input_limbs<InY> > ax::input_limbs<InX> ? ax::input_limbs<InY> : ax::input_limbs<InX>;
    using F = ::beman::inside::detail::exact_frac<E>;
    BEMAN_INSIDE_AX_REST((ax::fast_tier<Out, InY, InX> && ax::in_max<InY> <= 0x1p500 && ax::in_max<InX> <= 0x1p500),
                         ax::evaluate<Out, ax::start_bits<Out>>(ax::atan2_core<E>{F{ax::exact_input(y)}, F{ax::exact_input(x)}}))
  }

  template <insidable Out, insidable InX, insidable InY>
  [[nodiscard]] constexpr Out hypot_into(InX x, InY y)
  {
    require_rounding<Out>();
#ifndef BEMAN_INSIDE_MATH_NO_FP
    if constexpr (ax::fp_tier<Out, InX, InY> && ax::in_max<InX> <= 0x1p500 && ax::in_max<InY> <= 0x1p500)
      if !consteval
      {
        if (Out r; ax::fp_attempt_hypot<Out>(x, y, r)) return r;
      }
    if constexpr (ax::dd_tier<Out, InX, InY>)
      if !consteval
      {
        if (Out r; ax::dd_attempt_hypot<Out>(x, y, r)) return r;
      }
#endif
    constexpr int Bits = ax::hypot_bits<InX, InY>;
    constexpr std::size_t E = ::beman::inside::detail::limbs_for_bits(Bits);
    using F = ::beman::inside::detail::exact_frac<E>;
    BEMAN_INSIDE_AX_REST((ax::fast_tier<Out, InX, InY> && ax::in_max<InX> <= 0x1p500 && ax::in_max<InY> <= 0x1p500),
                         ax::evaluate<Out, ax::start_bits<Out>>(ax::sqrt_core<E, Bits>{F{ax::exact_input(x)} * F{ax::exact_input(x)}
                                                                                     + F{ax::exact_input(y)} * F{ax::exact_input(y)}}))
  }

  // pow: domain_error for a base ≤ 0; overflow when the result leaves Out
  // (without clamp).
  template <insidable Out, insidable InB, insidable InE>
  [[nodiscard]] constexpr std::expected<Out, errc> pow_into(InB base, InE exp)
  {
    require_rounding<Out>();
#ifndef BEMAN_INSIDE_MATH_NO_FP
    if constexpr (ax::fp_tier<Out, InB, InE>)
      if !consteval
      {
        if (Out r; ax::fp_attempt_pow<Out>(base, exp, r)) return r;
      }
    if constexpr (ax::dd_tier<Out, InB, InE>)
      if !consteval
      {
        if (Out r; ax::dd_attempt_pow<Out>(base, exp, r)) return r;
      }
#endif
    const auto integer = [&]() -> std::expected<Out, errc> {
      const auto b = ax::exact_input(base);
      if (b.Num.negative() || b.Num.is_zero()) return std::unexpected(errc::domain_error);
      using core = ax::pow_core<ax::input_limbs<InB>, ax::input_limbs<InE>, ax::in_mag<InE>, ax::out_kmax<Out>, ax::input_bits<InB>, ax::input_bits<InE>>;
      return ax::evaluate_checked<Out, ax::start_bits<Out>>(core{b, ax::exact_input(exp)});
    };
    BEMAN_INSIDE_AX_REST((ax::fast_tier<Out, InB, InE>), integer())
  }

  // Base^x for a compile-time integer Base ≥ 2.
  template <insidable Out, imax Base, insidable In>
  [[nodiscard]] constexpr Out pow_base_into(In x)
  {
    static_assert(Base >= 2, "beman::inside::math::pow_base: Base must be at least 2");
    require_rounding<Out>();
#ifndef BEMAN_INSIDE_MATH_NO_FP
    if constexpr (ax::fp_tier<Out, In> && ax::in_max<In> <= 1000)
      if !consteval
      {
        if (Out r; ax::fp_attempt_pow_base<Out, Base>(x, r)) return r;
      }
    if constexpr (ax::dd_tier<Out, In> && ax::in_max<In> <= 700)
      if !consteval
      {
        if (Out r; ax::dd_attempt_pow_base<Out, Base>(x, r)) return r;
      }
#endif
    using core = ax::pow_core<2, ax::input_limbs<In>, ax::in_mag<In>, ax::out_kmax<Out>, 66, ax::input_bits<In>, Base>;
    BEMAN_INSIDE_AX_REST((ax::fast_tier<Out, In> && ax::in_max<In> <= 1000),
                         ax::evaluate<Out, ax::start_bits<Out>>(core{ax::exact_int<2>(Base), ax::exact_input(x)}))
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
        "beman::inside::math: a deduced result is rounded onto the input's grid - its operand "
        "must permit rounding (declare it with round_nearest, round_floor, ...)");
    static_assert(notch_of<In> != 0,
        "beman::inside::math: a deduced output takes the input's notch - the input needs one");
    return true;
  }

  namespace auto_t
  {
    template <insidable In> using exp   = ax::increasing_t<In, ax::exp_core<ax::input_limbs<In>, ax::in_mag<In>, ax::exp_kmax<In>>>;
    template <insidable In> using exp2  = ax::increasing_t<In, ax::exp2_core<ax::input_limbs<In>, ax::in_mag<In>, ax::exp_kmax<In>>>;
    template <insidable In> using log   = ax::increasing_t<In, ax::log_core<ax::input_limbs<In>, ax::input_bits<In>>>;
    template <insidable In> using log2  = ax::increasing_t<In, ax::logb_core<ax::input_limbs<In>, 2, ax::input_bits<In>>>;
    template <insidable In> using log10 = ax::increasing_t<In, ax::logb_core<ax::input_limbs<In>, 10, ax::input_bits<In>>>;
    template <insidable In> using sqrt  = ax::increasing_t<In, ax::sqrt_core<ax::input_limbs<In>, ax::input_bits<In>>>;
    template <insidable In> using cbrt  = ax::increasing_t<In, ax::cbrt_core<ax::input_limbs<In>, ax::input_bits<In>>>;
    template <insidable In> using atan  = ax::increasing_t<In, ax::atan_core<ax::input_limbs<In>>>;
    template <insidable In> using asin  = ax::increasing_t<In, ax::asin_core<ax::input_limbs<In>, ax::input_bits<In>>>;
    template <insidable In> using acos  = ax::decreasing_t<In, ax::acos_core<ax::input_limbs<In>, ax::input_bits<In>>>;
    template <insidable In> using sinh  = ax::increasing_t<In, ax::hyp_core<ax::input_limbs<In>, ax::in_mag<In>, ax::hyp::sinh, ax::exp_kmax<In>>>;
    template <insidable In> using tanh  = ax::increasing_t<In, ax::hyp_core<ax::input_limbs<In>, ax::in_mag<In>, ax::hyp::tanh, 1>>;
    template <insidable In> using asinh = ax::increasing_t<In, ax::ahyp_core<ax::input_limbs<In>, ax::in_mag<In>, ax::ahyp::asinh, ax::input_bits<In>>>;
    template <insidable In> using acosh = ax::increasing_t<In, ax::ahyp_core<ax::input_limbs<In>, ax::in_mag<In>, ax::ahyp::acosh, ax::input_bits<In>>>;
    template <insidable In> using atanh = ax::increasing_t<In, ax::ahyp_core<ax::input_limbs<In>, ax::in_mag<In>, ax::ahyp::atanh, ax::input_bits<In>>>;
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
        ax::lattice_bound<notch_of<In>, true>(ax::sqrt_core<ax::input_limbs<In>, ax::input_bits<In>>{ax::grid_input<In>(max_abs<In>)})>;

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
    inline constexpr std::size_t hypot_limbs = ::beman::inside::detail::limbs_for_bits(ax::hypot_bits<InX, InY>);
    template <insidable InX, insidable InY>
    inline constexpr grid_rational hypot_hi = [] {
      using F = ::beman::inside::detail::exact_frac<hypot_limbs<InX, InY>>;
      const F a = ::beman::inside::detail::exact_of_grid<hypot_limbs<InX, InY>>(max_abs<InX>);
      const F b = ::beman::inside::detail::exact_of_grid<hypot_limbs<InX, InY>>(max_abs<InY>);
      return ax::lattice_bound<ax::gcd_notch<InX, InY>, true>(ax::sqrt_core<hypot_limbs<InX, InY>, ax::hypot_bits<InX, InY>>{a * a + b * b});
    }();
    template <insidable InX, insidable InY>
    using hypot = inside<{{0, hypot_hi<InX, InY>}, ax::gcd_notch<InX, InY>}, ax::auto_policy<InX>>;

    // pow: b^e is monotone in each argument for b > 0, so the extremes are at
    // the corners of the input rectangle. Notch of the base.
    template <insidable InB, insidable InE>
    using pow_core = ax::pow_core<ax::input_limbs<InB>, ax::input_limbs<InE>, ax::in_mag<InE>, ax::pow_kmax<InB, InE>, ax::input_bits<InB>, ax::input_bits<InE>>;
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
    static_assert(Base >= 2, "beman::inside::math::pow_base: Base must be at least 2");
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

#undef BEMAN_INSIDE_AX_FP
#undef BEMAN_INSIDE_AX_REST
#undef BEMAN_INSIDE_AX_TABLE

#endif // BEMAN_INSIDE_CMATH_ADAPTIVE_HPP
