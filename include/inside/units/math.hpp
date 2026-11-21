// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Opt-in: the math functions on ins::quantity, over the correctly rounded engine (math.hpp).
// Kept apart from units.hpp, which stays free of the engine's compile time.
// Each math::fn takes quantities too, in every call form (math/detail/functions.hpp):
//
//   math::sqrt(q), math::cbrt(q)       the unit's root (m² → m), the number's
//   abs, floor, ceil, round, trunc     on the number, in q's unit
//   sin, cos, tan(q)                   of an angle in radians: a number; sin and cos also of an angle in a unit
//                                      with a factor of π (°, a turn), reduced exactly by quarter turns,
//                                      exact at Niven's values (name the notch then: sin<per<K>>(q))
//   asin, acos, atan(x), atan2(y, x)   an angle in radians (x a dimensionless quantity; atan2 of two that add)
//   hypot(x, y), gcd(x, y), lcm(x, y)  in the common unit (gcd and lcm exact)
//   exp … atanh, pow_base<B>, pow      of numbers: dimensionless quantities (and insides): a number
//   math::exp(x) of a multivector      units/detail/geoalg_exp.hpp; log(x), sqrt(x) in geoalg_log.hpp
//   value_cast<si::radian, Out>(q)     degree → radian (a pi factor): the exact x·π/180, rounded once onto Out
//
// fn<L>(q) names only the result's number L (the unit follows), fn<C>(q) the whole quantity, whose unit must be
// the result's. Every result rounds as the inside function does: onto the output grid, under its rounding mode.
#ifndef INSIDE_UNITS_MATH_HPP
#define INSIDE_UNITS_MATH_HPP

#include <inside/inside/detail/config.hpp>

#include <inside/math.hpp>
#include <inside/units.hpp>

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace ins::math::detail::engine
{
  // x·(n/d)·π^P for P = ±1: π from pi_q at T = S + B bits, B covering |x·n/d|, so its error adds at most one unit;
  // the division truncates one more.
  // The factor is 64-bit (KF = 1), or KF limbs for a factor past 64 bits (ħ = h/2π in J·s).
  template <std::size_t E, int Bits, int P, std::size_t KF = 1>
  struct pi_scale_core
  {
    using factor = std::conditional_t<KF == 1, std::uint64_t, ins::detail::wide_uint<KF>>;
    wide_rational<E> X;
    factor           N; // the rational factor n/d
    factor           D;
    bool             Negative;

    constexpr maybe_exact<E> exact() const { return is_zero(X) ? maybe_exact<E>{X} : std::nullopt; }

    template <int W>
    constexpr auto run() const
    {
      constexpr int         S {W + 2};
      constexpr int         T {S + Bits + 4};
      constexpr std::size_t KJ{limbs_for_bits(64 * static_cast<int>(E) + 64 * static_cast<int>(KF) + T + 2 * S + 16)};
      using J = wide_sint<KJ>;
      using I = fixed_t<Bits + S + 4>;
      const J pi_fixed{static_cast<J>(pi_q<T>)};
      const J num     {J{X.Num} * J{N}};
      const J den     {J{X.Den} * J{D}};
      J       y;
      if constexpr (P > 0)
        y = (num * pi_fixed) / (den << (T - S));
      else
        y = (num << (S + T)) / (den * pi_fixed);
      const I v{static_cast<I>(y)};
      return approx<limbs_of<I>>{Negative ? -v : v, S, 2};
    }
  };

  // sin or cos of an angle x·(n/d)·π (a unit with a factor of π: °, a turn):
  // t = x·n/d half turns is reduced exactly by k = round(2t) quarter turns, so only r = π·(t − k/2), |r| ≤ π/4,
  // is approximated. Exact where Niven's theorem allows a rational value: 0, ±1/2, ±1.
  template <std::size_t E, bool Cos, std::size_t KF = 1>
  struct pi_trig_core
  {
    using factor = std::conditional_t<KF == 1, std::uint64_t, ins::detail::wide_uint<KF>>;
    wide_rational<E> X;
    factor           N; // the rational factor n/d
    factor           D;
    bool             Negative;

    template <class J>
    constexpr J numerator() const
    {
      const J num{J{X.Num} * J{N}};
      return Negative ? -num : num;
    }

    constexpr maybe_exact<E> exact() const
    {
      using J = wide_sint<limbs_for_bits(64 * static_cast<int>(E + KF) + 8)>;
      const J six{numerator<J>() * J{6}}, den{J{X.Den} * J{D}};
      if (!(six % den).is_zero())
        return std::nullopt;
      J m{(six / den) % J{12}}; // 6t mod 12: the angle in multiples of 30°
      if (m.negative())
        m += J{12};
      // 2·sin and 2·cos at m·30°; 3 marks an irrational value (√3/2).
      constexpr signed char twice_sin[12] = {0, 1, 3, 2, 3, 1, 0, -1, 3, -2, 3, -1};
      constexpr signed char twice_cos[12] = {2, 3, 1, 0, -1, 3, -2, 3, -1, 0, 1, 3};
      const int             v             = (Cos ? twice_cos : twice_sin)[m.Word[0]];
      if (v == 3)
        return std::nullopt;
      return wide_rational<E>{wide_sint<E>{v}, wide_sint<E>{2}};
    }

    template <int W>
    constexpr auto run() const
    {
      constexpr int S{W + 10};
      constexpr int P{S + 4};
      using J = wide_sint<limbs_for_bits(64 * static_cast<int>(E + KF) + P + 16)>;
      using I = fixed_t<S + 2>;
      constexpr std::size_t K{limbs_of<I>};
      const J               two_num{J{2} * numerator<J>()}, den{J{X.Den} * J{D}};
      const J               k{nearest_div(two_num, den)}; // quarter turns
      // r = π·(2t − k)/2 at scale S, within 1 unit.
      const I r{static_cast<I>(nearest_div((two_num - k * den) * static_cast<J>(pi_q<P>), den << (P - S + 1)))};
      const unsigned q{static_cast<unsigned>(k.Word[0] & 3u)};
      auto neg = [](fixed_approx<K> f) { return fixed_approx<K>{-f.Value, f.Error}; };
      const fixed_approx<K> v{Cos ? ((q == 0) ? cos_series<S>(r, 2)
                                   : (q == 1) ? neg(sin_series<S>(r, 2))
                                   : (q == 2) ? neg(cos_series<S>(r, 2))
                                   : sin_series<S>(r, 2))
                            : ((q == 0) ? sin_series<S>(r, 2)
                             : (q == 1) ? cos_series<S>(r, 2)
                             : (q == 2) ? neg(sin_series<S>(r, 2))
                             : neg(cos_series<S>(r, 2)))};
      return approx<K>{v.Value, S, v.Error};
    }
  };

  // asin, acos, atan or atan2 straight into a unit with a factor of π (°, a turn): with the unit x·(n/d)·π = θ,
  // the value θ·d/(n·π) from the radian core's approximation, times d at T bits and over n·π: the arc's error grows by
  // at most d/(n·π) < 2^Amp, π's and the quotient's add a unit. Exact where Niven's theorem allows a rational multiple
  // of π: asin and acos of 0, ±½, ±1, atan of 0, ±1, atan2 where y or x is 0 or |y| = |x|.
  enum class arc_fn : unsigned char { asin, acos, atan, atan2 };
  template <arc_fn Fn, std::size_t E, int Bits, int Amp>
  struct pi_arc_core
  {
    wide_rational<E> Y, X; // atan2(Y, X); the others take X
    std::uint64_t    N, D; // the unit's factor n/d

    constexpr auto radian_core() const
    {
      if constexpr (Fn == arc_fn::asin)
        return asin_core<E, Bits>{X};
      else if constexpr (Fn == arc_fn::acos)
        return acos_core<E, Bits>{X};
      else if constexpr (Fn == arc_fn::atan)
        return atan_core<E>{X};
      else
        return atan2_core<E>{Y, X};
    }

    // θ/π as num/den where it is rational.
    constexpr std::optional<std::pair<int, int>> half_turns() const
    {
      using I     = wide_sint<E>;
      const auto is = [](const wide_rational<E>& a, int n, int d) { return a.Num * I{d} == a.Den * I{n}; };
      if constexpr (Fn == arc_fn::asin || Fn == arc_fn::acos)
      {
        constexpr int v[5][2] = {{-1, 1}, {-1, 2}, {0, 1}, {1, 2}, {1, 1}};
        // asin: -½, -⅙, 0, ⅙, ½ half turns; acos: 1, ⅔, ½, ⅓, 0.
        constexpr int h[2][5][2] = {{{-1, 2}, {-1, 6}, {0, 1}, {1, 6}, {1, 2}}, {{1, 1}, {2, 3}, {1, 2}, {1, 3}, {0, 1}}};
        for (int k = 0; k < 5; ++k)
          if (is(X, v[k][0], v[k][1]))
            return std::pair{h[Fn == arc_fn::acos][k][0], h[Fn == arc_fn::acos][k][1]};
      }
      else if constexpr (Fn == arc_fn::atan)
      {
        if (is_zero(X))
          return std::pair{0, 1};
        if (is(X, 1, 1))
          return std::pair{1, 4};
        if (is(X, -1, 1))
          return std::pair{-1, 4};
      }
      else
      {
        const bool yn{Y.Num.negative()}, xn{X.Num.negative()};
        if (is_zero(Y))
          return xn ? std::pair{1, 1} : std::pair{0, 1};
        if (is_zero(X))
          return yn ? std::pair{-1, 2} : std::pair{1, 2};
        const I yx{Y.Num * X.Den}, xy{X.Num * Y.Den};
        if (yx == xy || yx == -xy)
          return yn ? (xn ? std::pair{-3, 4} : std::pair{-1, 4}) : (xn ? std::pair{3, 4} : std::pair{1, 4});
      }
      return std::nullopt;
    }
    constexpr maybe_exact<3> exact() const
    {
      using R = wide_rational<3>;
      using I = wide_sint<3>;
      const auto h{half_turns()};
      if (!h)
        return std::nullopt;
      return R{I{h->first} * I{D}, I{h->second} * I{N}};
    }

    template <int W>
    constexpr auto run() const
    {
      constexpr int         T {W + 2 * Amp + 64};
      constexpr std::size_t KJ{limbs_for_bits(2 * T + 192)};
      using J = wide_sint<KJ>;
      const auto a{radian_core().template run<W + Amp + 4>()};
      const J    y{(J{a.Value} * J{D} << T) / (J{N} * static_cast<J>(pi_q<T>))}; // at a's scale
      using I = fixed_t<W + 2 * Amp + 40>;
      const double e{static_cast<double>(a.Error) * ::ins::detail::ldexp(1.0, Amp) + 3};
      return approx<limbs_of<I>>{static_cast<I>(y), a.Scale, e < 0x1p62 ? static_cast<std::uint64_t>(e) : ~std::uint64_t{0}};
    }
  };

  // tan of an angle x·(n/d)·π: t = x·n/d half turns reduced exactly by k = round(2t) quarter turns,
  // r = π·(2t − k)/2 with |r| ≤ π/4, then tan = sin r/cos r for an even k and −cos r/sin r for an odd one
  // (sin r is not 0: the input grid holds no pole). The quotient's bound as trig_core's tan. Exact by Niven at tan 0
  // and ±1 (t ≡ 0, ±¼ mod 1).
  template <std::size_t E, int KMax>
  struct pi_tan_core
  {
    wide_rational<E> X;
    std::uint64_t    N; // the unit's factor n/d
    std::uint64_t    D;
    bool             Negative;

    template <class J>
    constexpr J numerator() const
    {
      const J num{J{X.Num} * J{N}};
      return Negative ? -num : num;
    }
    constexpr maybe_exact<E> exact() const
    {
      using J = wide_sint<limbs_for_bits(64 * static_cast<int>(E + 1) + 8)>;
      const J four{numerator<J>() * J{4}}, den{J{X.Den} * J{D}};
      if (!(four % den).is_zero())
        return std::nullopt;
      J q{(four / den) % J{4}}; // 4t mod 4
      if (q.negative())
        q += J{4};
      const int v{static_cast<int>(q.Word[0])};
      if (v == 2) // a pole: none is on the grid
        return std::nullopt;
      return wide_rational<E>{wide_sint<E>{v == 0 ? 0 : v == 1 ? 1 : -1}, wide_sint<E>{1}};
    }
    template <int W>
    constexpr auto run() const
    {
      constexpr int S{W + KMax + 14};
      constexpr int P{S + 4};
      using J = wide_sint<limbs_for_bits(64 * static_cast<int>(E + 1) + P + 16)>;
      using I = fixed_t<S + KMax + 4>;
      constexpr std::size_t K{limbs_of<I>};
      const J               two_num{J{2} * numerator<J>()}, den{J{X.Den} * J{D}};
      const J               k{nearest_div(two_num, den)}; // quarter turns
      const I r{static_cast<I>(nearest_div((two_num - k * den) * static_cast<J>(pi_q<P>), den << (P - S + 1)))};
      const bool            odd{(k.Word[0] & 1u) != 0};
      const fixed_approx<K> sn{sin_series<S>(r, 2)}, cs{cos_series<S>(r, 2)};
      const fixed_approx<K> nu{odd ? fixed_approx<K>{-cs.Value, cs.Error} : sn};
      const fixed_approx<K> de{odd ? sn : cs};
      // (δn + |t|·δd)/(|d| − δd) + 2 units, and the test for a value past 2^KMax: bounds, in doubles widened a hair.
      const double nd{static_cast<double>(nu.Value)}, dd{static_cast<double>(de.Value)};
      const double ad{dd < 0 ? -dd : dd}, an{nd < 0 ? -nd : nd};
      const double ed{static_cast<double>(de.Error)}, en{static_cast<double>(nu.Error)};
      if (ed < ad && (an - en) / (ad + ed) * (1 - 0x1p-40) > ::ins::detail::ldexp(1.0, KMax))
        return approx<K>::beyond(nu.Value.negative() != de.Value.negative(), KMax);
      if (!(2 * ed < ad))
        return approx<K>::unresolved(S);
      const double unit_value{::ins::detail::ldexp(1.0, S)};
      const I      t  {div_q(nu.Value, de.Value, S)};
      const double at {(an + en) / (ad - ed)};
      double       err{((en + at * ed) / (ad - ed) * unit_value + 2) * (1 + 0x1p-40) + 1};
      int          sh{0};
      while (!(err < 0x1p62))
      {
        err *= 0.5;
        ++sh;
      }
      if (sh == 0)
        return approx<K>{t, S, static_cast<std::uint64_t>(err)};
      return approx<K>{round_shift(t, sh), S - sh, static_cast<std::uint64_t>(err) + 2};
    }
  };
} // namespace ins::math::detail::engine

namespace ins
{
  namespace detail
  {
    // The n-th root of a unit: every factor's exponent must divide.
    consteval unit_value unit_root(const unit_value& u, int n)
    {
      unit_value r{};
      for (std::int8_t i = 0; i < u.Size; ++i)
      {
        if (u.Atoms[i].Exp % n != 0)
          ins::detail::constexpr_error<"units: the root of a unit needs exponents it divides (m², not m)">();
        unit_add(r, {u.Atoms[i].U, u.Atoms[i].Prefix, static_cast<std::int8_t>(u.Atoms[i].Exp / n)});
      }
      if (!u.Scale.is_one())
        ins::detail::constexpr_error<"units: the root of a scaled unit - convert it to a named unit first">();
      return r;
    }

    // The root of a reference's spec: its recipe's, when the exponents divide.
    consteval reference root_reference(const reference& a, int n, const unit_value& u)
    {
      if (is_generic(a))
        return reference{&no_kind, false, {}, u};
      const recipe f{detail::has(a.Kind) ? recipe_of(a.KindOf ? kind_root(a.Kind) : a.Kind) : a.Derived};
      recipe       r{};
      for (std::int8_t i = 0; i < f.Size; ++i)
      {
        if (f.F[i].Exp % n != 0)
          return reference{&no_kind, false, {}, u};
        recipe_add(r, f.F[i].Kind, f.F[i].Exp / n);
      }
      if (r.Size == 1 && r.F[0].Exp == 1)
        return reference{r.F[0].Kind, a.KindOf, {}, u};
      return reference{&no_kind, a.KindOf, r, u};
    }

    template <reference R, int N>
    consteval reference root_reference()
    {
      const unit_value u{unit_root(R.U, N)};
      return root_reference(R, N, u);
    }

    // An angle in radians: a dimensionless quantity of magnitude one.
    template <reference R>
    inline constexpr bool radians_v = dimension_of(R.U) == dim_one && magnitude_of(R.U).is_one();

    inline constexpr reference radian_reference = reference{si::radian};

    // An angle in a unit with a factor of π (°, a turn): sin and cos reduce it exactly.
    template <reference R>
    inline constexpr bool pi_angle_v = dimension_of(R.U) == dim_one && magnitude_of(R.U).Pi == 1;

    // The double tier for a π-scaled angle x·(n/d)·π (math/detail/tiers.hpp's decision):
    // x = j·u with u = unit_of<V>, so x·n/d = j·A/B half turns with A = num(u)·n, B = den(u)·d, all integers.
    // Where 2·|j·A| < 2^62 and B < 2^52, the reduction is exact in int64: k = round(2·j·A/B) quarter turns and
    // rem = 2·j·A − k·B, |rem| ≤ B/2.
    // Then r = rem·fl(π/(2B)), |r| ≤ π/4, is within 3·2^-53·|r| of the true reduced angle (π's rounding, the
    // quotient's and the product's; rem and B are doubles exactly), which moves sin and cos by at most as much
    // (their slope is at most 1); the polynomials add the double kernel's proved Rel of the value.
    // The bound takes 2^-50·|r| for the first. fp_decide keeps the result only when the whole interval rounds to
    // one slot; anything else takes the exact engine.
    template <ins::grid_number F, class V>
    struct pi_scaled
    {
      using grid_int = ins::detail::grid_int;
      static constexpr auto     f = F;
      static constexpr grid_int A = []
      {
        if constexpr (insidable<V> && ins::detail::notched<V>)
          return ins::detail::grid_numerator(ins::detail::unit_of<V>) * ins::detail::grid_numerator(f);
        else
          return grid_int{0};
      }();
      static constexpr grid_int B = []
      {
        if constexpr (insidable<V> && ins::detail::notched<V>)
          return ins::detail::grid_denominator(ins::detail::unit_of<V>) * ins::detail::grid_denominator(f);
        else
          return grid_int{1};
      }();
      // j·A in int64 with room for the doubling, and B a double exactly.
      static constexpr bool ints = []
      {
        namespace bd = ins::detail;
        if constexpr (!insidable<V> || !bd::notched<V> || !bd::integer_storage<V> || bd::wide_valued<V>
                      || bd::point_storage<V>)
          return false;
        else
        {
          constexpr grid_number u{bd::unit_of<V>};
          const grid_int        jlo{bd::units_lo<V, u>}, jhi{bd::units_hi<V, u>};
          const grid_int        j{abs(jlo) < abs(jhi) ? abs(jhi) : abs(jlo)};
          return j * abs(A) < (grid_int{1} << 52) && B < (grid_int{1} << 52) && grid_int{0} < B;
        }
      }();
      static constexpr std::int64_t a = ints ? static_cast<std::int64_t>(A) : 0;
      static constexpr std::int64_t b = ints ? static_cast<std::int64_t>(B) : 1;
      template <class X>
      static std::int64_t units(const X& x) noexcept
      {
        return ins::detail::value_in_units<std::int64_t, ins::detail::unit_of<V>>(x);
      }
    };
#ifndef INSIDE_MATH_NO_FP
    template <insidable Out, bool Cos, reference R, class V>
    inline constexpr bool pi_trig_fp = pi_scaled<magnitude_of(R.U).Ratio, V>::ints
                                    && ins::math::detail::engine::fp_tier<Out, ins::math::detail::engine::fp_sin, V>;
    template <insidable Out, bool Cos, reference R, class V>
    inline bool pi_trig_double(const V& x, Out& out) noexcept
    {
      namespace engine = ins::math::detail::engine;
      namespace fpk    = ins::math::detail::fp;
      using S = pi_scaled<magnitude_of(R.U).Ratio, V>;
      using K = fpk::trig_k<engine::fp_target<Out, engine::fp_sin>>;
      constexpr double   c2{3.141592653589793238462643383279502884 / (2.0 * static_cast<double>(S::b))};
      const std::int64_t two{2 * S::units(x) * S::a};
      std::int64_t       k{two / S::b}, rem{two - k * S::b};
      if (2 * rem > S::b)
      {
        ++k;
        rem -= S::b;
      }
      else if (2 * rem < -S::b)
      {
        --k;
        rem += S::b;
      }
      const double   r {static_cast<double>(rem) * c2};
      const unsigned qd{(static_cast<unsigned>(k) + (Cos ? 1u : 0u)) & 3u};
      const double   p {(qd & 1u) != 0 ? fpk::cos_poly<K::NC>(r) : fpk::sin_poly<K::NS>(r)};
      const double   v {(qd & 2u) != 0 ? -p : p};
      return engine::fp_decide<Out>(v, K::Rel * __builtin_fabs(v) + 0x1p-50 * __builtin_fabs(r) + fpk::tiny, out);
    }
#endif

    // sin or cos of a π-scaled angle: one rounding onto Out.
    // The factor n/d in one limb when it is a 64-bit rational, else in as many as it needs.
    template <insidable Out, bool Cos, reference R, class V>
    constexpr Out pi_trig_exact(const quantity<R, V>& q)
    {
      namespace engine = ins::math::detail::engine;
      using ins::detail::grid_int;
      constexpr auto        f {magnitude_of(R.U).Ratio};
      constexpr grid_int    n {ins::detail::grid_numerator(f)};
      constexpr grid_int    d {ins::detail::grid_denominator(f)};
      constexpr bool        ng{n.negative()};
      constexpr std::size_t KF{
        ins::detail::fits_rational(f) ? 1 : ins::detail::limbs_for_bits(std::max(n.bit_width(), d.bit_width()) + 1)};
      using core   = engine::pi_trig_core<engine::input_limbs<V>, Cos, KF>;
      using factor = typename core::factor;
      return engine::evaluate<Out, engine::start_bits<Out>>(core{
        engine::exact_input(ins::detail::value_of(q)), static_cast<factor>(ng ? -n : n), static_cast<factor>(d), ng});
    }
    template <insidable Out, bool Cos, reference R, class V>
    constexpr Out pi_trig(const quantity<R, V>& q)
    {
      ins::math::detail::entry::require_rounding<Out>();
#ifndef INSIDE_MATH_NO_FP
      if constexpr (pi_trig_fp<Out, Cos, R, V>)
        if !consteval
        {
          Out out;
          if (pi_trig_double<Out, Cos, R, V>(ins::detail::value_of(q), out)) [[likely]]
            return out;
          return ins::math::detail::engine::cold_call([q] { return pi_trig_exact<Out, Cos>(q); });
        }
#endif
      return pi_trig_exact<Out, Cos>(q);
    }

    // An angle unit with a factor of π as a named output of asin, acos, atan and atan2.
    template <class C>
    inline constexpr bool pi_angle_output_v = false;
    template <reference R, class V>
    inline constexpr bool pi_angle_output_v<quantity<R, V>> =
      pi_angle_v<R> && grid_number{0} < magnitude_of(R.U).Ratio && ins::detail::fits_rational(magnitude_of(R.U).Ratio);

#ifndef INSIDE_MATH_NO_FP
    // The double tier of pi_arc: the radian kernel's value v within its bound, times c = fl(d/(n·π)) (within 3·2^-53
    // of its value, the product within 2^-53 more), so v·c is within bound·c + 2^-51·|v·c| of the angle in C's unit.
    template <ins::math::detail::engine::arc_fn Fn, insidable Out, class... In>
    inline bool pi_arc_double(double c, Out& out, const In&... x) noexcept
    {
      namespace engine = ins::math::detail::engine;
      namespace fpk    = ins::math::detail::fp;
      double bound{0}, v{0};
      if constexpr (Fn == engine::arc_fn::atan2)
      {
        v = fpk::atan_k<fpk::full_bits>::atan2(engine::input_double(x...[0]), engine::input_double(x...[1]), bound);
        bound += engine::input_rel<In...[0]> + engine::input_rel<In...[1]>;
      }
      else if constexpr (Fn == engine::arc_fn::atan)
        v = fpk::atan_k<fpk::full_bits>::atan(engine::input_double(x...[0]), bound);
      else if constexpr (Fn == engine::arc_fn::asin)
        v = fpk::asin_k<fpk::full_bits>::asin(engine::input_double(x...[0]), bound);
      else
        v = fpk::asin_k<fpk::full_bits>::acos(engine::input_double(x...[0]), bound);
      const double y{v * c};
      return engine::fp_decide<Out>(y, bound * c * (1 + 0x1p-50) + 0x1p-51 * __builtin_fabs(y) + fpk::tiny, out);
    }
#endif

    // The core of asin, acos, atan (of x) or atan2 (of x…[0], x…[1]) into C's unit, and its constants.
    template <class F, class C>
    struct pi_arc_facts
    {
      static constexpr grid_number   ratio{magnitude_of(C::reference.U).Ratio}; // the unit is ratio·π
      static constexpr std::uint64_t n{static_cast<std::uint64_t>(ins::detail::grid_numerator(ratio))};
      static constexpr std::uint64_t d{static_cast<std::uint64_t>(ins::detail::grid_denominator(ratio))};
      static constexpr int           amp{std::max(std::bit_width(d) - std::bit_width(n) + 1, 0)};
      static constexpr auto          fn{std::same_as<F, ins::math::detail::entry::asin_fn>
                                          ? ins::math::detail::engine::arc_fn::asin
                                        : std::same_as<F, ins::math::detail::entry::acos_fn>
                                          ? ins::math::detail::engine::arc_fn::acos
                                        : std::same_as<F, ins::math::detail::entry::atan_fn>
                                          ? ins::math::detail::engine::arc_fn::atan
                                          : ins::math::detail::engine::arc_fn::atan2};
    };
    template <class F, class C, class... In>
    constexpr auto pi_arc_core_of(const In&... x)
    {
      namespace engine = ins::math::detail::engine;
      using P          = pi_arc_facts<F, C>;
      constexpr std::size_t E{std::max({engine::input_limbs<In>...})};
      constexpr int         bits{std::max({engine::input_bits<In>...})};
      using W = ins::detail::wide_rational<E>;
      engine::pi_arc_core<P::fn, E, bits, P::amp> core{};
      core.N = P::n;
      core.D = P::d;
      if constexpr (sizeof...(In) == 2)
      {
        core.Y = W{engine::exact_input(x...[0])};
        core.X = W{engine::exact_input(x...[1])};
      }
      else
        core.X = W{engine::exact_input(x...[0])};
      return core;
    }
    // On the exact engine alone: C's number.
    template <class F, class C, class... In>
    constexpr typename C::rep pi_arc_exact(const In&... x)
    {
      namespace engine = ins::math::detail::engine;
      using NO         = typename C::rep;
      return engine::evaluate<NO, engine::start_bits<NO>>(pi_arc_core_of<F, C>(x...));
    }

    // asin, acos, atan (of x) or atan2 (of x…[0], x…[1]) straight into C's unit, rounded once onto C's number:
    // C itself where its range holds every angle the function gives (θ/π in [-½, ½], [0, 1] or [-1, 1]),
    // else std::expected<C, error>.
    template <class F, class C, class... In>
    constexpr auto pi_arc(const In&... x)
    {
      namespace engine = ins::math::detail::engine;
      namespace entry  = ins::math::detail::entry;
      using NO         = typename C::rep;
      using P          = pi_arc_facts<F, C>;
      entry::require_rounding<NO>();
      static_assert(ins::math::detail::in_domain<F, In...>(),
                    "units: asin and acos take a number in [-1, 1] - the argument's grid lies outside");
      // The angles in C's unit: θ/π times 1/ratio.
      constexpr grid_number top{grid_number{1} / P::ratio};
      constexpr grid_number lo{P::fn == engine::arc_fn::acos ? grid_number{0}
                               : P::fn == engine::arc_fn::atan2 ? -top
                                                                : -top / grid_number{2}};
      constexpr grid_number hi{P::fn == engine::arc_fn::atan2 || P::fn == engine::arc_fn::acos ? top
                                                                                                : top / grid_number{2}};
      constexpr policy_flag p{policy_of<NO>};
      constexpr bool fits{!is_checked(p) || has_any_flag(p, clamp | wrap) || (lower_of<NO> <= lo && hi <= upper_of<NO>)};
#ifndef INSIDE_MATH_NO_FP
      // The double tier where C holds every angle and the inputs read as doubles (asin, acos, atan: exactly).
      if constexpr (fits && engine::fp_output<NO>
                    && (P::fn == engine::arc_fn::atan2 ? ((engine::in_max<In> <= 0x1p500) && ...)
                                                       : (engine::fp_exact_input<In> && ...)))
        if !consteval
        {
          constexpr double c{static_cast<double>(P::d)
                             / (static_cast<double>(P::n) * 3.141592653589793238462643383279502884)};
          NO               out;
          if (!engine::integer_only() && pi_arc_double<P::fn>(c, out, x...)) [[likely]]
            return quantity_access::make<C>(out);
          return quantity_access::make<C>(engine::cold_call([x...] { return pi_arc_exact<F, C>(x...); }));
        }
#endif
      if constexpr (fits)
        return quantity_access::make<C>(pi_arc_exact<F, C>(x...));
      else
        return engine::evaluate_checked<NO, engine::start_bits<NO>>(pi_arc_core_of<F, C>(x...))
          .transform([](const NO& v) { return quantity_access::make<C>(v); });
    }

    // tan of an angle x·r·π (r = n/d) over V's lattice: the poles sit at x·r ≡ ½ (mod 1). None may be on the lattice,
    // and the lattice point nearest one (δ in x) bounds |tan| by 1/(2·δ·r), as |sin πu| ≥ 2|u| for |u| ≤ ½
    // (an end of the grid counts with its distance to the poles beyond it).
    template <grid_number Ratio, class V>
    struct pi_tan_facts
    {
      static_assert(notch_of<V> != 0, "units: tan of an angle with a factor of pi - the angle's grid has a notch");
      static_assert(grid_number{0} < Ratio, "units: tan of an angle with a factor of pi - a positive factor");
      static consteval grid_number floor_of(const grid_number& v)
      {
        return ins::detail::grid_round<ins::detail::round_mode::floor>(v);
      }
      struct scan_result
      {
        bool        Pole;
        grid_number Distance;
      };
      static consteval scan_result scan()
      {
        const grid_number lo{lower_of<V>}, hi{upper_of<V>}, n{notch_of<V>}, half{grid_number{1} / grid_number{2}};
        const grid_number m0{floor_of(lo * Ratio - half) - grid_number{1}}, m1{floor_of(hi * Ratio - half) + grid_number{1}};
        if (grid_number{1000000} < m1 - m0)
          ins::detail::constexpr_error<"units: tan of an angle with a factor of pi - the grid spans too many turns">();
        grid_number best{-1};
        for (grid_number m{m0}; m <= m1; m = m + grid_number{1})
        {
          const grid_number p{(half + m) / Ratio};
          grid_number       dist;
          if (p < lo)
            dist = lo - p;
          else if (hi < p)
            dist = p - hi;
          else
          {
            const grid_number off{p - lo};
            const grid_number rem{off - floor_of(off / n) * n};
            if (rem == grid_number{0})
              return {true, grid_number{0}};
            dist = rem < n - rem ? rem : n - rem;
          }
          if (best < grid_number{0} || dist < best)
            best = dist;
        }
        return {false, best};
      }
      static constexpr scan_result s{scan()};
      static constexpr bool        pole{s.Pole};
      static constexpr grid_number bound{pole ? grid_number{0} : grid_number{1} / (grid_number{2} * s.Distance * Ratio)};
      static constexpr int         kmax{
        pole ? 0
             : ins::detail::grid_numerator(ins::detail::grid_round<ins::detail::round_mode::ceil>(bound)).bit_width() + 2};
    };

    // tan of an angle with a factor of π (°, a turn), rounded once onto Out: Out where its range holds every value tan
    // takes over the angle's grid, else std::expected<Out, error>.
    template <insidable Out, reference R, class V>
    constexpr auto pi_tan(const quantity<R, V>& q)
    {
      namespace engine = ins::math::detail::engine;
      ins::math::detail::entry::require_rounding<Out>();
      constexpr grid_number f{magnitude_of(R.U).Ratio};
      static_assert(ins::detail::fits_rational(f), "units: tan of an angle with a factor of pi - a 64-bit factor");
      using T = pi_tan_facts<f, V>;
      static_assert(!T::pole, "units: tan of an angle whose grid holds a pole (an odd multiple of 90°)");
      constexpr policy_flag p{policy_of<Out>};
      constexpr bool fits{!is_checked(p) || has_any_flag(p, clamp | wrap)
                          || (lower_of<Out> <= -T::bound && T::bound <= upper_of<Out>)};
      constexpr grid_int n{ins::detail::grid_numerator(f)}, d{ins::detail::grid_denominator(f)};
      const engine::pi_tan_core<engine::input_limbs<V>, T::kmax> core{
        engine::exact_input(ins::detail::value_of(q)), static_cast<std::uint64_t>(n), static_cast<std::uint64_t>(d),
        false};
      if constexpr (fits)
        return engine::evaluate<Out, engine::start_bits<Out>>(core);
      else
        return engine::evaluate_checked<Out, engine::start_bits<Out>>(core);
    }
  } // namespace detail

  namespace detail
  {
    // x·(n/d)·π^±1 rounded once onto Out, exactly (the engine).
    template <magnitude F, insidable Out, reference R, class V>
    constexpr Out pi_cast_exact(const quantity<R, V>& q)
    {
      namespace engine = ins::math::detail::engine;
      // The factor n/d in one limb when it is a 64-bit rational, else in as many as it needs (ħ = h/2π in J·s).
      using ins::detail::grid_int;
      constexpr grid_int    n {ins::detail::grid_numerator(F.Ratio)};
      constexpr grid_int    d {ins::detail::grid_denominator(F.Ratio)};
      constexpr bool        ng{n.negative()};
      constexpr int         NB{n.bit_width()};
      constexpr std::size_t KF{
        ins::detail::fits_rational(F.Ratio) ? 1 : ins::detail::limbs_for_bits(std::max(NB, d.bit_width()) + 1)};
      using core   = engine::pi_scale_core<engine::input_limbs<V>, engine::in_mag<V> + NB + 4, F.Pi, KF>;
      using factor = typename core::factor;
      return engine::evaluate<Out, engine::start_bits<Out>>(core{
        engine::exact_input(ins::detail::value_of(q)), static_cast<factor>(ng ? -n : n), static_cast<factor>(d), ng});
    }
  } // namespace detail

  // value_cast<U, Out>(q) across a pi factor (degree ↔ radian): the exact x·r·π^±1, rounded once onto Out's grid
  // under its rounding mode.
  template <auto U, insidable Out, reference R, class V>
    requires detail::is_unit_tag_v<decltype(U)>
          && (!detail::exact_factor_v<detail::conversion_factor<R.U, decltype(U)::value>>)
  [[nodiscard]] constexpr auto value_cast(const quantity<R, V>& q)
  {
    constexpr ins::unit_value u{decltype(U)::value};
    constexpr magnitude       f{detail::conversion_factor<R.U, u>};
    static_assert(dimension_of(u) == dimension_of(R.U), "units: value_cast - the unit has another dimension");
    static_assert(f.Pi == 1 || f.Pi == -1, "units: value_cast - only a factor of pi or 1/pi rounds");
    ins::math::detail::entry::require_rounding<Out>();
    constexpr reference to{R.Kind, R.KindOf, R.Derived, u};
#ifndef INSIDE_MATH_NO_FP
    // The double tier: x·(n/d)·π^±1 = (j·A)·c with c = fl(π/B) or fl(1/(π·B)) (pi_scaled):
    // j·A is a double exactly, c within 3·2^-53 of its value and the product within 2^-53 more,
    // so y is within 2^-50·|y|.
    namespace engine = ins::math::detail::engine;
    using S = detail::pi_scaled<f.Ratio, V>;
    if constexpr (S::ints && engine::fp_output<Out>)
      if !consteval
      {
        constexpr double pi{3.141592653589793238462643383279502884};
        constexpr double c {f.Pi == 1 ? pi / static_cast<double>(S::b) : 1.0 / (pi * static_cast<double>(S::b))};
        const double     y {static_cast<double>(S::units(ins::detail::value_of(q)) * S::a) * c};
        Out              out;
        if (engine::fp_decide<Out>(y, 0x1p-50 * __builtin_fabs(y), out)) [[likely]]
          return detail::quantity_access::make<quantity<to, Out>>(out);
        return detail::quantity_access::make<quantity<to, Out>>(
          engine::cold_call([q] { return detail::pi_cast_exact<f, Out>(q); }));
      }
#endif
    return detail::quantity_access::make<quantity<to, Out>>(detail::pi_cast_exact<f, Out>(q));
  }

  namespace detail
  {
    // x's number as an inside: an inside itself, or a dimensionless quantity's.
    template <class X>
    constexpr auto number_of(const X& x)
    {
      if constexpr (insidable<X>)
        return x;
      else
      {
        static_assert(quantity_like<X> && number_like_v<X::reference>,
                      "units: the argument must be a number - an inside or a dimensionless quantity");
        return rescale<conversion_factor<X::reference.U, unit_value{}>>(ins::detail::value_of(x));
      }
    }
    template <class X>
    using number_of_t = decltype(number_of(std::declval<const X&>()));

    // The math functions on quantities (math/detail/functions.hpp): each takes its arguments' numbers to the inside
    // function, by one of these rules, and puts the result in its unit.
    namespace m = ins::math::detail;
    enum class math_rule
    {
      none,
      same,   // abs, floor, ceil, round, trunc: in q's unit
      root2,  // sqrt: the unit's root (m² → m)
      root3,  // cbrt
      trig,   // sin, cos: of an angle in radians or with a factor of π; a number
      tan,    // tan: of an angle in radians; a number
      arc,    // asin, acos, atan: of a number; an angle in radians
      arc2,   // atan2: of two quantities that add; an angle in radians
      common, // hypot, gcd, lcm: of two quantities that add; in their common unit
      number, // exp … atanh, pow_base, pow: of numbers; a number
    };
    template <class F>
    inline constexpr math_rule rule_of = math_rule::none;
    template <>
    inline constexpr math_rule rule_of<m::abs_fn> = math_rule::same;
    template <>
    inline constexpr math_rule rule_of<m::floor_fn> = math_rule::same;
    template <>
    inline constexpr math_rule rule_of<m::ceil_fn> = math_rule::same;
    template <>
    inline constexpr math_rule rule_of<m::round_fn> = math_rule::same;
    template <>
    inline constexpr math_rule rule_of<m::trunc_fn> = math_rule::same;
    template <>
    inline constexpr math_rule rule_of<m::entry::sqrt_fn> = math_rule::root2;
    template <>
    inline constexpr math_rule rule_of<m::entry::cbrt_fn> = math_rule::root3;
    template <>
    inline constexpr math_rule rule_of<m::entry::sin_fn> = math_rule::trig;
    template <>
    inline constexpr math_rule rule_of<m::entry::cos_fn> = math_rule::trig;
    template <>
    inline constexpr math_rule rule_of<m::entry::tan_fn> = math_rule::tan;
    template <>
    inline constexpr math_rule rule_of<m::entry::asin_fn> = math_rule::arc;
    template <>
    inline constexpr math_rule rule_of<m::entry::acos_fn> = math_rule::arc;
    template <>
    inline constexpr math_rule rule_of<m::entry::atan_fn> = math_rule::arc;
    template <>
    inline constexpr math_rule rule_of<m::entry::atan2_fn> = math_rule::arc2;
    template <>
    inline constexpr math_rule rule_of<m::entry::hypot_fn> = math_rule::common;
    template <>
    inline constexpr math_rule rule_of<m::gcd_fn> = math_rule::common;
    template <>
    inline constexpr math_rule rule_of<m::lcm_fn> = math_rule::common;
#define INSIDE_UNITS_NUMBER_RULE(fn) \
  template <>                        \
  inline constexpr math_rule rule_of<m::entry::fn##_fn> = math_rule::number;
    INSIDE_UNITS_NUMBER_RULE(exp)
    INSIDE_UNITS_NUMBER_RULE(exp2)
    INSIDE_UNITS_NUMBER_RULE(log)
    INSIDE_UNITS_NUMBER_RULE(log2)
    INSIDE_UNITS_NUMBER_RULE(log10)
    INSIDE_UNITS_NUMBER_RULE(sinh)
    INSIDE_UNITS_NUMBER_RULE(cosh)
    INSIDE_UNITS_NUMBER_RULE(tanh)
    INSIDE_UNITS_NUMBER_RULE(asinh)
    INSIDE_UNITS_NUMBER_RULE(acosh)
    INSIDE_UNITS_NUMBER_RULE(atanh)
    INSIDE_UNITS_NUMBER_RULE(pow)
#undef INSIDE_UNITS_NUMBER_RULE
    template <auto B>
    inline constexpr math_rule rule_of<m::entry::pow_base_fn<B>> = math_rule::number;

    // The output for specs S of a result in reference To (a quantity), or a plain number (Number),
    // whose number is the output of the numbers' kind NK.
    template <class O>
    struct rep_of
    {
      using type = typename O::rep;
    };
    template <class NK, bool Number, reference To, bool PiAngles = false>
    struct output_in
    {
      template <class N>
      using with = std::conditional_t<Number, N, quantity<To, N>>;
      template <class S>
      struct out
      {
        using type = with<typename NK::template output<S>>;
      };
      template <class C>
      struct out<m::named<C>>
      {
        static consteval auto pick()
        {
          if constexpr (insidable<C>)
            return std::type_identity<with<C>>{};
          else
          {
            static_assert(!Number, "ins::math: f<C>(q) - the result is a number: C is an inside");
            static_assert(quantity_like<C>, "ins::math: f<C>(q) - C is a quantity or its number");
            static_assert((C::reference.U == To.U && implicit_v<To, C::reference>)
                            || (PiAngles && pi_angle_output_v<C>),
                          "ins::math: f<C>(q) - C's unit must be exactly the result's (or name only "
                          "the number); asin, acos, atan and atan2 also give an angle in a unit with a factor of pi");
            return std::type_identity<C>{};
          }
        }
        using type = typename decltype(pick())::type;
      };

      // The output's number: O itself, or its rep.
      template <class O>
      using number_t = typename std::conditional_t<Number, std::type_identity<O>, rep_of<O>>::type;
      template <class O, class V>
      static constexpr auto make(V&& v)
      {
        if constexpr (Number)
          return std::forward<V>(v);
        else
          return lift_to<O>(std::forward<V>(v));
      }
    };

    // Each rule: whether it takes the arguments (takes), the kind of their numbers (NK),
    // whether the result is a plain number or else its reference (number, to),
    // and the specs the numbers' kind sees (number_spec).
    template <math_rule Rule, class F, class... Args>
    struct quantity_rule
    {
      static constexpr bool takes = false;
    };
    struct rule_base
    {
      template <class S>
      using number_spec = S;
    };

    template <math_rule Rule, class F, reference R, class V>
      requires(Rule == math_rule::same || Rule == math_rule::root2 || Rule == math_rule::root3)
    struct quantity_rule<Rule, F, quantity<R, V>> : rule_base
    {
      using NK = m::kind<F, 1, V>;
      static constexpr bool      takes  = NK::takes;
      static constexpr bool      number = false;
      static constexpr reference to     = Rule == math_rule::same  ? R
                                        : Rule == math_rule::root2 ? root_reference<R, 2>()
                                        : root_reference<R, 3>();
    };

    // An angle in radians: the number's function;
    // with a factor of π (sin and cos): reduced exactly by quarter turns, the result on the named notch.
    template <math_rule Rule, class F, reference R, class V>
      requires(Rule == math_rule::trig || Rule == math_rule::tan)
    struct quantity_rule<Rule, F, quantity<R, V>>
    {
      static_assert(radians_v<R> || pi_angle_v<R>,
                    "units: sin, cos and tan take an angle in radians, or in a unit with a factor of pi (°, a turn) - "
                    "value_cast<si::radian, Out>(q) first");
      using NK = m::kind<F, 1, V>;
      static constexpr bool      takes  = NK::takes;
      static constexpr bool      number = true;
      static constexpr reference to     = {};
      template <class S>
      struct spec
      {
        using type = S;
      };
      template <m::out_spec S>
      struct spec<m::specs<S>>
      {
        static_assert(radians_v<R> || Rule == math_rule::trig,
                      "units: tan of an angle with a factor of pi (°, a turn) - name the result: tan<C>(q), C a number "
                      "whose range holds the tangent over the angle's grid (else an expected)");
        static_assert(radians_v<R> || S.HasNotch,
                      "units: sin and cos of an angle with a factor of pi (°, a turn) - name the result's notch: "
                      "sin<per<K>>(q)");
        using type = m::specs<S>;
      };
      template <class S>
      using number_spec = typename spec<S>::type;
    };

    // A number: an angle in radians.
    template <class F, class X>
    struct quantity_rule<math_rule::arc, F, X> : rule_base
    {
      using NK = m::kind<F, 1, number_of_t<X>>;
      static constexpr bool      takes  = NK::takes;
      static constexpr bool      number = false;
      static constexpr reference to     = radian_reference;
    };

    // Two quantities that add, in their common unit: an angle in radians (atan2), or in that unit (hypot).
    // (Two that do not add take nothing, and form no common unit: the deleted overloads below say why.)
    template <class F, reference A, class VA, reference B, class VB, bool = common_rescalable<A, VA, B, VB>>
    struct common_rule
    {
      using NK = m::kind<F, 0>;
      static constexpr reference to = {};
    };
    template <class F, reference A, class VA, reference B, class VB>
    struct common_rule<F, A, VA, B, VB, true>
    {
      using NK = m::kind<F, 1, common_number_t<A, VA, B>, common_number_t<B, VB, A>>;
      static constexpr reference to = sum_reference<A, B>();
    };
    template <math_rule Rule, class F, reference A, class VA, reference B, class VB>
      requires(Rule == math_rule::arc2 || Rule == math_rule::common)
    struct quantity_rule<Rule, F, quantity<A, VA>, quantity<B, VB>> : rule_base
    {
      using C  = common_rule<F, A, VA, B, VB>;
      using NK = typename C::NK;
      static constexpr bool      takes  = NK::takes;
      static constexpr bool      number = false;
      static constexpr reference to     = Rule == math_rule::arc2 ? radian_reference : C::to;
    };

    // Numbers (insides or dimensionless quantities): a number.
    template <class F, class... X>
    struct quantity_rule<math_rule::number, F, X...> : rule_base
    {
      using NK = m::kind<F, 1, number_of_t<X>...>;
      static constexpr bool      takes  = NK::takes;
      static constexpr bool      number = true;
      static constexpr reference to     = {};
    };
  } // namespace detail
} // namespace ins

namespace ins::math::detail
{
  // A quantity's rank: 2, or 3 when its unit holds a blade (a part of a multivector, which exp takes:
  // units/detail/geoalg_exp.hpp).
  template <class T>
    requires ins::quantity_like<T>
  inline constexpr int kind_rank<T> = ins::detail::has_blade(dimension_of(T::reference.U)) ? 3 : 2;

  // Quantities: the rule of F.
  template <class F, class... Args>
  struct kind<F, 2, Args...>
  {
    using rule = ins::detail::quantity_rule<ins::detail::rule_of<F>, F, Args...>;
    static constexpr bool takes = rule::takes;
    using io = ins::detail::output_in<typename rule::NK, rule::number, rule::to,
                                      ins::detail::rule_of<F> == ins::detail::math_rule::arc
                                        || ins::detail::rule_of<F> == ins::detail::math_rule::arc2>;

    template <class S>
    using output = typename io::template out<typename rule::template number_spec<S>>::type;

    template <class O>
    [[gnu::always_inline]] static constexpr auto eval(const Args&... x)
    {
      using NO = typename io::template number_t<O>;
      constexpr auto r{ins::detail::rule_of<F>};
      if constexpr (r == ins::detail::math_rule::trig || r == ins::detail::math_rule::tan)
      {
        constexpr ins::reference R{Args...[0]::reference};
        if constexpr (ins::detail::radians_v<R>)
          return F::template eval<NO>(ins::detail::value_of(x...[0]));
        else
          return ins::detail::pi_trig<NO, std::same_as<F, entry::cos_fn>>(x...[0]);
      }
      else if constexpr (r == ins::detail::math_rule::arc2 || r == ins::detail::math_rule::common)
      {
        constexpr ins::reference A{Args...[0]::reference}, B{Args...[1]::reference};
        return io::template make<O>(F::template eval<NO>(ins::detail::in_common_unit<A, B>(x...[0]),
                                                         ins::detail::in_common_unit<A, B>(x...[1])));
      }
      else if constexpr (r == ins::detail::math_rule::arc || r == ins::detail::math_rule::number)
        return io::template make<O>(F::template eval<NO>(ins::detail::number_of(x)...));
      else
        return io::template make<O>(F::template eval<NO>(ins::detail::value_of(x)...));
    }

    // A named output f<C>(q): the numbers' kind decides it (C itself, or an expected where the result can miss C),
    // lifted to the quantity.
    template <class O>
    [[gnu::always_inline]] static constexpr auto eval_named(const Args&... x)
    {
      using NO = typename io::template number_t<O>;
      constexpr auto r{ins::detail::rule_of<F>};
      const auto named = [](const auto&... n)
      {
        return detail::eval_named<F, NO, std::remove_cvref_t<decltype(n)>...>(n...);
      };
      if constexpr (r == ins::detail::math_rule::trig || r == ins::detail::math_rule::tan)
      {
        constexpr ins::reference R{Args...[0]::reference};
        if constexpr (ins::detail::radians_v<R>)
          return named(ins::detail::value_of(x...[0]));
        else if constexpr (r == ins::detail::math_rule::tan) // a degree input: one rounding, an expected where NO can miss
          return ins::detail::pi_tan<NO>(x...[0]);
        else // (a degree input: the pi-scaled engine, into NO as named)
          return ins::detail::pi_trig<NO, std::same_as<F, entry::cos_fn>>(x...[0]);
      }
      else if constexpr (r == ins::detail::math_rule::arc2 || r == ins::detail::math_rule::common)
      {
        constexpr ins::reference A{Args...[0]::reference}, B{Args...[1]::reference};
        if constexpr (r == ins::detail::math_rule::arc2 && ins::detail::pi_angle_output_v<O>)
          return ins::detail::pi_arc<F, O>(ins::detail::in_common_unit<A, B>(x...[0]),
                                           ins::detail::in_common_unit<A, B>(x...[1]));
        else
          return io::template make<O>(
            named(ins::detail::in_common_unit<A, B>(x...[0]), ins::detail::in_common_unit<A, B>(x...[1])));
      }
      else if constexpr (r == ins::detail::math_rule::arc && ins::detail::pi_angle_output_v<O>)
        return ins::detail::pi_arc<F, O>(ins::detail::number_of(x...[0]));
      else if constexpr (r == ins::detail::math_rule::arc || r == ins::detail::math_rule::number)
        return io::template make<O>(named(ins::detail::number_of(x)...));
      else
        return io::template make<O>(named(ins::detail::value_of(x)...));
    }
  };
} // namespace ins::math::detail

namespace ins::math
{
  // atan2, hypot, gcd and lcm of two quantities that do not add (another dimension or kind, no common unit).
  template <auto... Spec, ins::reference A, class VA, ins::reference B, class VB>
    requires(ins::detail::sum_diagnosed<A, B>())
  void atan2(const ins::quantity<A, VA>&, const ins::quantity<B, VB>&) = delete (
    "units: atan2 takes two quantities that add - one dimension and kind, a common unit");
  template <auto... Spec, ins::reference A, class VA, ins::reference B, class VB>
    requires(ins::detail::sum_diagnosed<A, B>())
  void hypot(const ins::quantity<A, VA>&, const ins::quantity<B, VB>&) = delete (
    "units: hypot takes two quantities that add - one dimension and kind, a common unit");
  template <auto... Spec, ins::reference A, class VA, ins::reference B, class VB>
    requires(ins::detail::sum_diagnosed<A, B>())
  void gcd(const ins::quantity<A, VA>&, const ins::quantity<B, VB>&) = delete (
    "units: gcd takes two quantities that add - one dimension and kind, a common unit");
  template <auto... Spec, ins::reference A, class VA, ins::reference B, class VB>
    requires(ins::detail::sum_diagnosed<A, B>())
  void lcm(const ins::quantity<A, VA>&, const ins::quantity<B, VB>&) = delete (
    "units: lcm takes two quantities that add - one dimension and kind, a common unit");
} // namespace ins::math

// exp of multivectors (units/detail/geoalg_exp.hpp).
#include <inside/units/detail/geoalg_exp.hpp>
// log of multivectors (units/detail/geoalg_log.hpp).
#include <inside/units/detail/geoalg_log.hpp>

#endif // INSIDE_UNITS_MATH_HPP
