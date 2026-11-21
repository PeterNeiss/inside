// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Internal: the logarithm of a multivector (math::log), included by units/math.hpp after exp
// (units/detail/geoalg_exp.hpp, whose domain facts and output rules it shares).
//
// The closed form on exp's domain: x = s + y, every two parts of y anticommute, so y·y = −z is a scalar, and
//   log x = ½·log(s² + z) + y·f,
//   f = atan2(√z, s)/√z     for z > 0 (complex numbers, quaternions, rotors),
//       atanh(√−z/s)/√−z    for z < 0 (boosts, split-complex; √−z < s),
//       1/s                 for z = 0 (dual numbers, light-like y).
// Every part is correctly rounded onto its own grid by its own mode, on the exact engine (the integer path).
// The domain is s² + z > 0, and s > 0 where z ≤ 0, but for the principal value of a negative complex number,
// log(−r) = log r + π i (one blade, squaring to −1). Where the argument's type proves the domain the result is plain,
// else a std::expected with error::domain_error.
#ifndef INSIDE_UNITS_DETAIL_GEOALG_LOG_HPP
#define INSIDE_UNITS_DETAIL_GEOALG_LOG_HPP

#include <inside/inside/detail/config.hpp>

#include <inside/units/detail/geoalg_exp.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <utility>

namespace ins::detail::gaexp
{
  // v·2^-S as a double, from v's top bits, for any S.
  template <std::size_t K>
  constexpr double scaled_double(const wide_sint<K>& v, int S) noexcept
  {
    const bool         neg{v.negative()};
    const wide_sint<K> a{neg ? -v : v};
    const int          b{bit_width_of(a)}; // the hidden friend (ADL), not big_int's
    const int          sh{b > 62 ? b - 62 : 0};
    const double       r{::ins::detail::ldexp(static_cast<double>((a >> sh).Word[0]), sh - S)}; // below 2^62
    return neg ? -r : r;
  }

  // The error in units of t = div_q(x, y, S), x within ex units and y within ey units (|y| > ey): the derivatives
  // 1/y and t/y in real terms, plus the truncation. Past 2^62 units it saturates (undecided: more bits).
  template <int S, std::size_t K>
  constexpr std::uint64_t quotient_error(const wide_sint<K>& y, double ex, double ey, const wide_sint<K>& t) noexcept
  {
    const double yl{scaled_double(y.negative() ? -y : y, S) - ::ins::detail::ldexp(ey, -S)};
    const double tr{scaled_double(t.negative() ? -t : t, S)};
    if (!(yl > 0))
      return ~std::uint64_t{0};
    const double e{(ex + tr * ey) / yl * (1 + 0x1p-40) + 2};
    return e < 0x1p62 ? static_cast<std::uint64_t>(e) : ~std::uint64_t{0};
  }

  // One part of log x on the integer path: the scalar part (Scalar), or y's part Part, with Sign the sign of
  // CoDomain's part against its blade. A is s, Y[j] y's part j with its own blade's sign, exactly.
  // G guard bits cover the quotients: by |s| and √|z| where they are not 0 (at least 2^-Rs and 2^-Rz/2 on the
  // grids) and by s − √−z (at least (s² + z)/2s), and the factor |bⱼ| < 2^Mb.
  template <std::size_t E, std::size_t N, std::array<signed char, N> Sq, int G, int Mb, bool Principal>
  struct ga_log_core
  {
    static constexpr std::size_t EZ = 2 * N * E + 1;
    static constexpr std::size_t EQ = EZ + 2 * E + 1;
    static constexpr std::size_t EX = 2 * E + 1;
    wide_rational<E>                A;
    std::array<wide_rational<E>, N> Y;
    bool                            Scalar;
    std::size_t                     Part;
    int                             Sign;

    // z = Σ -sqⱼ·yⱼ², and s² + z, exactly.
    constexpr wide_rational<EZ> z_exact() const noexcept
    {
      using ZI = wide_sint<EZ>;
      ZI num{0}, den{1};
      for (std::size_t j = 0; j < N; ++j)
      {
        const ZI n{Y[j].Num}, d{Y[j].Den};
        num = num * d * d + ZI{-Sq[j]} * n * n * den;
        den = den * d * d;
      }
      return {num, den};
    }
    constexpr wide_rational<EQ> q_exact() const noexcept
    {
      using QI = wide_sint<EQ>;
      const wide_rational<EZ> z{z_exact()};
      const QI                an{A.Num}, ad{A.Den};
      return {an * an * QI{z.Den} + QI{z.Num} * ad * ad, ad * ad * QI{z.Den}};
    }

    // The rational results: the scalar part at s² + z = 1 (0), a blade part whose coefficient is 0, and y/s
    // where y·y = 0. Elsewhere log is transcendental (Lindemann–Weierstrass), π included.
    constexpr eng::maybe_exact<EX> exact() const
    {
      using R  = wide_rational<EX>;
      using RI = wide_sint<EX>;
      if (Scalar)
        return eng::is_one(q_exact()) ? eng::maybe_exact<EX>{R{RI{0}, RI{1}}} : std::nullopt;
      const wide_rational<E>& b{Y[Part]};
      const bool              z0{eng::is_zero(z_exact())};
      if (z0 && A.Num.negative()) // the principal value π of a negative complex number
        return std::nullopt;
      if (eng::is_zero(b))
        return R{RI{0}, RI{1}};
      if (z0)
      {
        const RI n{RI{b.Num} * RI{A.Den}};
        return R{Sign < 0 ? -n : n, RI{b.Den} * RI{A.Num}};
      }
      return std::nullopt;
    }

    template <int W>
    constexpr auto run() const
    {
      constexpr int T{W + 12 + G};
      using I = eng::fixed_t<T + 2 * G + 16>;
      constexpr std::size_t K{eng::limbs_of<I>};
      using eng::approx;
      if (Scalar) // ½·log(s² + z): log at scale T is the half at scale T + 1
      {
        const eng::fixed_approx<K> l{eng::log_exact<T, K, eng::fraction_bits<EQ>>(q_exact())};
        return approx<K>{l.Value, T + 1, l.Error};
      }
      const wide_rational<EZ> z{z_exact()};
      if (eng::is_zero(z)) // the principal value: log(−r) = log r + π·i
      {
        const I p{static_cast<I>(eng::pi_q<T>)};
        return approx<K>{Sign < 0 ? -p : p, T, 1};
      }
      // Sign·bⱼ·f for f at scale Sc within df units.
      const wide_rational<E>& b{Y[Part]};
      const auto              times_b = [&](const I& f, std::uint64_t df, int Sc)
      {
        using J = wide_sint<K + E + 1>;
        const I      v{static_cast<I>(eng::nearest_div(J{f} * J{b.Num}, J{b.Den}))};
        const double e{static_cast<double>(df) * ::ins::detail::ldexp(1.0, Mb) * (1 + 0x1p-40) + 2};
        return approx<K>{Sign < 0 ? -v : v, Sc, e < 0x1p62 ? static_cast<std::uint64_t>(e) : ~std::uint64_t{0}};
      };
      const I s{eng::to_q<T, K>(A)}; // within ½ unit
      if (!z.Num.negative())         // z > 0: f = atan2(√z, s)/√z
      {
        const I r {static_cast<I>(eng::sqrt_exact_q<T, K, eng::fraction_bits<EZ>>(z))}; // within 1 unit
        const I as{s.negative() ? -s : s};
        eng::fixed_approx<K> th; // θ = atan2(√z, s) in (0, π]
        if (!(as < r))           // |s| ≥ √z: atan(√z/|s|), or π − it for s < 0
        {
          const I                    t{eng::div_q(r, as, T)};
          const eng::fixed_approx<K> a{eng::atan_fixed<T>(t, quotient_error<T>(as, 1.0, 1.0, t))};
          th = s.negative() ? eng::fixed_approx<K>{static_cast<I>(eng::pi_q<T>) - a.Value, a.Error + 1} : a;
        }
        else // π/2 − atan(s/√z)
        {
          const I                    t{eng::div_q(s, r, T)};
          const eng::fixed_approx<K> a{eng::atan_fixed<T>(t, quotient_error<T>(r, 1.0, 1.0, t))};
          th = eng::fixed_approx<K>{static_cast<I>(eng::pi_q<T - 1>) - a.Value, a.Error + 1};
        }
        const I f{eng::div_q(th.Value, r, T)};
        return times_b(f, quotient_error<T>(r, static_cast<double>(th.Error), 1.0, f), T);
      }
      // z < 0: f = atanh(λ/s)/λ = log((s + λ)/(s − λ))/(2λ), λ = √−z < s.
      const wide_rational<EZ> mz{-z.Num, z.Den};
      const I                 l  {static_cast<I>(eng::sqrt_exact_q<T, K, eng::fraction_bits<EZ>>(mz))};
      const I                 den{s - l};
      if (!(den > I{2}))
        return approx<K>::unresolved(T);
      const I                    a {eng::div_q(s + l, den, T)}; // ≥ 1
      const eng::fixed_approx<K> lg{eng::log_fixed<T>(a, quotient_error<T>(den, 2.0, 2.0, a))};
      const I                    f2{eng::div_q(lg.Value, l, T)}; // 2f at scale T, f at scale T + 1
      return times_b(f2, quotient_error<T>(l, static_cast<double>(lg.Error), 1.0, f2), T + 1);
    }
  };

  // What the types tell about log over Domain: the guard bits, the domain proof, and bounds on each output part.
  template <class Domain>
  struct log_facts : domain_facts<Domain>
  {
    using B = domain_facts<Domain>;
    using B::N, B::sq, B::has_scalar, B::scalar_index, B::yidx;
    using typename B::DP;
    template <std::size_t J>
    using ypart = typename B::template ypart<J>;

    template <class V>
    static consteval int recip_bits() // a nonzero |v| on V's grid is at least 2^-recip_bits
    {
      static_assert(notch_of<V> != 0 && ::ins::detail::anchored<V>,
                    "math::log: the argument's parts are on notched grids through 0");
      return ::ins::detail::grid_denominator(notch_of<V>).bit_width();
    }
    static constexpr int Rs = []
    {
      if constexpr (has_scalar)
        return recip_bits<num_t<std::tuple_element_t<scalar_index, DP>>>();
      else
        return 0;
    }();
    // A nonzero z = Σ ±(kⱼ·nⱼ)² is at least 1 over the product of the notches' squared denominators.
    static constexpr int Rz = []<std::size_t... J>(std::index_sequence<J...>)
    {
      return (0 + ... + (2 * recip_bits<num_t<ypart<J>>>()));
    }(std::make_index_sequence<N>{});
    static constexpr int Ms = []
    {
      if constexpr (has_scalar)
        return eng::in_mag<num_t<std::tuple_element_t<scalar_index, DP>>>;
      else
        return 1;
    }();
    static constexpr int Mb = []<std::size_t... J>(std::index_sequence<J...>)
    {
      return std::max({1, eng::in_mag<num_t<ypart<J>>>...});
    }(std::make_index_sequence<N>{});
    static constexpr int Rq = 2 * Rs + Rz;
    static constexpr int G{Mb + Rs + Rz + Rq + 2 * Ms + 24};

    static constexpr bool any_neg = []
    {
      for (auto s : sq)
        if (s < 0)
          return true;
      return false;
    }();
    static constexpr bool any_pos = []
    {
      for (auto s : sq)
        if (s > 0)
          return true;
      return false;
    }();
    // log(−r) = log r + π i: one blade, squaring to −1.
    static constexpr bool principal = N == 1 && sq[0] < 0;

    template <class V>
    static consteval grid_number max_abs_g()
    {
      return -lower_of<V> < upper_of<V> ? upper_of<V> : -lower_of<V>;
    }

    // The domain from the types: s > 0 and s² above every positive part of y·y (then s² + z > 0 and s > √−z);
    // or every square −1 and a part of y never 0 (z > 0); or one blade squaring to −1 and either part never 0.
    template <class V>
    static consteval bool never_zero()
    {
      return grid_number{0} < lower_of<V> || upper_of<V> < grid_number{0};
    }
    static constexpr bool domain_proven = []
    {
      bool s_pos{false};
      if constexpr (has_scalar)
      {
        using S = num_t<std::tuple_element_t<scalar_index, DP>>;
        if (grid_number{0} < lower_of<S>)
        {
          grid_number pos{0};
          [&]<std::size_t... J>(std::index_sequence<J...>)
          {
            ((sq[J] > 0 ? (pos = pos + max_abs_g<num_t<ypart<J>>>() * max_abs_g<num_t<ypart<J>>>(), 0) : 0), ...);
          }(std::make_index_sequence<N>{});
          s_pos = pos < lower_of<S> * lower_of<S>;
        }
      }
      if (s_pos)
        return true;
      const bool y_never_zero = []<std::size_t... J>(std::index_sequence<J...>)
      {
        return (false || ... || never_zero<num_t<ypart<J>>>());
      }(std::make_index_sequence<N>{});
      bool all_neg{true};
      for (auto s : sq)
        all_neg = all_neg && s < 0;
      if (all_neg && y_never_zero)
        return true;
      if constexpr (principal && has_scalar)
        return never_zero<num_t<std::tuple_element_t<scalar_index, DP>>>();
      return false;
    }();
    // Bounds on the result's parts, above: |½ log q| for q in [2^-Rq, s² + Σ yⱼ²]; a blade part at most π
    // (z > 0: |yⱼ|·θ/√z ≤ θ ≤ π), ½·log((s + λ)²/q) (z < 0), and |yⱼ|/|s| (z = 0).
    static constexpr double scalar_bound = []
    {
      double q{0};
      if constexpr (has_scalar)
        q += max_abs<num_t<std::tuple_element_t<scalar_index, DP>>> * max_abs<num_t<std::tuple_element_t<scalar_index, DP>>>;
      [&]<std::size_t... J>(std::index_sequence<J...>)
      {
        ((q += max_abs<num_t<ypart<J>>> * max_abs<num_t<ypart<J>>>), ...);
      }(std::make_index_sequence<N>{});
      // q at least 2^-Rq; at least s² where no square is positive (z ≥ 0) and s is never 0.
      double lo{Rq * 0.6931471805599453};
      if constexpr (has_scalar && !any_pos)
      {
        using Sn = num_t<std::tuple_element_t<scalar_index, DP>>;
        if constexpr (never_zero<Sn>())
        {
          const double m{grid_number{0} < lower_of<Sn> ? static_cast<double>(lower_of<Sn>)
                                                        : -static_cast<double>(upper_of<Sn>)};
          lo = m >= 1 ? 0.0 : -2 * __builtin_log(m) * (1 + 0x1p-30);
        }
      }
      const double hi{q > 1 ? __builtin_log(q) : 0.0};
      return 0.5 * (hi > lo ? hi : lo) * (1 + 0x1p-20) + 0x1p-20;
    }();
    static constexpr double part_bound = []
    {
      double m{principal ? 3.1415926535897932 * (1 + 0x1p-20) : 0.0};
      if (any_neg)
        m = 3.1415926535897932 * (1 + 0x1p-20);
      if (any_pos)
        m = std::max(m, 0.5 * (2 * Ms + 2 + Rq) * 0.6931471805599453 * (1 + 0x1p-20) + 0x1p-20);
      bool zero_square{!any_neg || any_pos}; // z = 0 with y ≠ 0: a null part, or squares of both signs
      for (auto s : sq)
        zero_square = zero_square || s == 0;
      if (zero_square)
        m = std::max(m, ::ins::detail::ldexp(1.0, Mb + Rs));
      return m;
    }();
  };

  // The output of log over Domain: deduced as exp's, on [-L, L] with L the part's bound rounded up onto the notch.
  template <class Domain, ::ins::math::detail::out_spec S>
  struct log_deduced
  {
    using D = log_facts<Domain>;
    static constexpr grid_number notch = S.HasNotch ? S.Notch : parts_notch<Domain>;
    static_assert(notch != 0,
                  "math::log: a deduced output takes the argument's notch - its parts need one, or name it: "
                  "log<per<K>>(x)");
    static constexpr policy_flag flags = ::ins::math::detail::spec_flags(
      ::ins::detail::with_default_rounding(policy_of<num_t<std::tuple_element_t<0, typename D::DP>>>
                                           & ~::ins::detail::cursor_marker),
      S);
    static consteval grid_number limit(double m)
    {
      return ::ins::detail::grid_round<::ins::detail::round_mode::ceil>(grid_number{m} / notch) * notch;
    }
    template <std::size_t J>
    using number = inside<grid{{-limit(J == D::N ? D::scalar_bound : D::part_bound),
                                limit(J == D::N ? D::scalar_bound : D::part_bound)},
                               notch},
                          flags>;
    using type = typename output_parts<Domain, number>::type;
  };

  // Whether part Q of a named output C holds every value (as part_fits does for exp).
  template <class Domain, class CoDomain, std::size_t Q>
  consteval bool log_part_fits()
  {
    using F = facts<Domain, CoDomain>;
    using L = log_facts<Domain>;
    using O = num_t<std::tuple_element_t<Q, typename F::CP>>;
    constexpr policy_flag p{policy_of<O>};
    if constexpr (F::src[Q].Kind == part_source::zero || !is_checked(p) || has_any_flag(p, clamp | wrap))
      return true;
    else
    {
      const grid_number m{F::src[Q].Kind == part_source::scalar ? L::scalar_bound : L::part_bound};
      return m <= upper_of<O> && lower_of<O> <= -m;
    }
  }

  // log onto CoDomain: CoDomain itself where the domain is proven and (Named) every part holds every value,
  // else std::expected<CoDomain, error>.
  template <class Domain, class CoDomain, bool Named>
  constexpr auto log_run(const Domain& x)
  {
    using F  = facts<Domain, CoDomain>;
    using L  = log_facts<Domain>;
    using DP = typename F::DP;
    using CP = typename F::CP;
    constexpr std::size_t N{F::N};
    static_assert(F::covers, "math::log: CoDomain must hold the scalar part and every blade of the argument");
    constexpr bool fits = []<std::size_t... Q>(std::index_sequence<Q...>)
    {
      return !Named || (log_part_fits<Domain, CoDomain, Q>() && ...);
    }(std::make_index_sequence<F::NC>{});
    constexpr bool plain{L::domain_proven && fits};
    constexpr std::size_t E = []<std::size_t... I>(std::index_sequence<I...>)
    {
      return std::max({std::size_t{1}, eng::input_limbs<num_t<std::tuple_element_t<I, DP>>>...});
    }(std::make_index_sequence<F::ND>{});
    using core = ga_log_core<E, N, F::sq, L::G, L::Mb, L::principal>;

    const DP& px = parts_view(x);
    core      c{};
    if constexpr (F::has_scalar)
      c.A = exact_widened<E>(anum<F>(px), 1);
    else
      c.A = wide_rational<E>{wide_sint<E>{0}, wide_sint<E>{1}};
    for_index<N>([&]<std::size_t J>() { c.Y[J] = exact_widened<E>(ynum<F, J>(px), F::dsign[J]); });

    using result = std::conditional_t<plain, CoDomain, std::expected<CoDomain, error>>;
    if constexpr (!L::domain_proven)
    {
      // s² + z > 0; s > 0 where z ≤ 0, but for the principal value (one blade squaring to −1, y = 0).
      const auto q{c.q_exact()};
      const auto z{c.z_exact()};
      bool       ok{!q.Num.negative() && !eng::is_zero(q)};
      if (ok && !(!z.Num.negative() && !eng::is_zero(z)))
      {
        bool y_zero{true};
        for (const auto& yj : c.Y)
          y_zero = y_zero && eng::is_zero(yj);
        ok = !c.A.Num.negative() || (L::principal && y_zero);
      }
      if (!ok)
        return result{std::unexpect, error::domain_error};
    }
    // Each part from its own core: plain where it holds every value, else checked.
    auto parts = [&]<std::size_t... Q>(std::index_sequence<Q...>)
    {
      return std::tuple{[&]
                        {
                          using O = num_t<std::tuple_element_t<Q, CP>>;
                          core cq{c};
                          cq.Scalar = F::src[Q].Kind == part_source::scalar;
                          cq.Part   = F::src[Q].Blade;
                          cq.Sign   = F::osign[Q];
                          if constexpr (F::src[Q].Kind == part_source::zero)
                            return std::expected<O, error>{O(just<0>)};
                          else if constexpr (!Named || log_part_fits<Domain, CoDomain, Q>())
                            return std::expected<O, error>{eng::evaluate<O, eng::start_bits<O>>(cq)};
                          else
                            return eng::evaluate_checked<O, eng::start_bits<O>>(cq);
                        }()...};
    }(std::make_index_sequence<F::NC>{});
    return [&]<std::size_t... Q>(std::index_sequence<Q...>) -> result
    {
      if constexpr (plain)
        return CoDomain{part_of_number<std::tuple_element_t<Q, CP>>(*std::get<Q>(parts))...};
      else
      {
        error e{};
        const bool ok{((std::get<Q>(parts) || (e = std::get<Q>(parts).error(), false)) && ...)};
        if (!ok)
          return std::unexpected(e);
        return CoDomain{part_of_number<std::tuple_element_t<Q, CP>>(*std::get<Q>(parts))...};
      }
    }(std::make_index_sequence<F::NC>{});
  }
} // namespace ins::detail::gaexp

namespace ins::math::detail
{
  // log of a multivector, or of a quantity of a blade: the closed form, every part of the output correctly rounded.
  template <class Domain>
  struct kind<entry::log_fn, 3, Domain>
  {
    static constexpr bool takes = ins::detail::gaexp::domain_closed<Domain>();

    template <class S>
    struct out;
    template <out_spec S>
    struct out<specs<S>>
    {
      using type = typename ins::detail::gaexp::log_deduced<Domain, S>::type;
    };
    template <class C>
    struct out<named<C>>
    {
      using type = typename ins::detail::gaexp::named<Domain, C>::type;
    };
    template <class S>
    using output = typename out<S>::type;

    template <class O>
    [[nodiscard]] static constexpr auto eval(const Domain& x)
    {
      return ins::detail::gaexp::log_run<Domain, O, false>(x);
    }
    template <class O>
    [[nodiscard]] static constexpr auto eval_named(const Domain& x)
    {
      return ins::detail::gaexp::log_run<Domain, O, true>(x);
    }
  };
} // namespace ins::math::detail

#endif // INSIDE_UNITS_DETAIL_GEOALG_LOG_HPP
