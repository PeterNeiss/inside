// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INSIDE_INSIDE_DETAIL_DIVISION_HPP
#define INSIDE_INSIDE_DETAIL_DIVISION_HPP

#include <inside/inside/detail/config.hpp>

#include <inside/inside/detail/op_result.hpp>
#include <inside/inside/generic.hpp>
#include <inside/inside/detail/wide_value.hpp>
#include <inside/inside/grid.hpp>
#include <inside/inside/detail/call_policy.hpp>

#include <limits>
#include <type_traits> // std::type_identity
#include <numeric>
#include <utility>

// Division and modulo.
// division::div returns expected<result, error> exactly when the divisor's grid holds zero
// (a quotient never overflows).
// Its paths, in order: Q-format (same notch, snap), native and wide integer (integer-aligned grids, snap),
// a point quotient, a point scale (a nonzero point divisor keeps the dividend's lattice), exact wide fractions,
// an integer quotient that fits, and the unchecked 64-bit rational.
// modulo::mod needs notched grids: a non-integer remainder is not defined on a fractional notch.
namespace ins
{
  // divmod's result, the rounded quotient and its remainder: auto [q, r] = divmod(a, b).
  // Of two quantities, the remainder is a quantity.
  template <class Q, class R>
  struct divmod_result
  {
    Q Quotient;
    R Remainder;
  };
} // namespace ins

namespace ins::detail
{
  // Both operands are plain integer grids and the call names a rounding mode
  // (F: div(a, b, round_floor), a compound assignment's type), the prerequisite for native integer div and mod.
  // The operands' own modes play no part: a / b is the exact quotient whatever their types round.
  template <insidable L, insidable R, policy_flag F>
  inline constexpr bool integer_ops = (F & snap) && integer_lattice<L> && integer_lattice<R>;

  // …and every value fits int64, so the builtin integer division applies.
  template <insidable L, insidable R, policy_flag F>
  inline constexpr bool integer_native_ops = integer_ops<L, R, F> && values_fit_int64<L> && values_fit_int64<R>;

  // Integer operands past int64 (a uint64 grid, the 2^63 of INT64_MIN / -1, a big grid):
  // the rounded quotient in exact wide integers, on a grid of the corner quotients rounded the same way.
  template <insidable L, insidable R, policy_flag F>
  inline constexpr bool integer_wide_ops = integer_ops<L, R, F> && !integer_native_ops<L, R, F>;

  // Whether l / r, l % r and divmod have a result under the flags F,
  // as the constraints of div, mod, divmod and their operators:
  // the quotient grid exists (no zero divisor grid), and the remainder needs integer-valued grids.
  template <insidable L, insidable R, policy_flag F>
  inline constexpr bool div_defined = (integer_native_ops<L, R, F> || integer_wide_ops<L, R, F>)
                                        ? lower_of<R> != 0 || upper_of<R> != 0
                                        : grid_quotient_fits(grid_of<L>, grid_of<R>);

  // The remainder on notched grids that are not both integer:
  // every value of either is a whole number of g, the gcd of their value units,
  // so % and divmod take the integer rules in units of g, and the remainder lies on g·ℤ.
  // (A continuous grid has no lattice and stays rejected.)
  template <insidable L, insidable R>
  inline constexpr bool lattice_mod_ops = notched<L> && notched<R> && !(integer_lattice<L> && integer_lattice<R>);

  template <insidable L, insidable R>
  inline constexpr grid_number mod_unit =
    lattice_mod_ops<L, R> ? gcd(grid_of<L>.value_unit(), grid_of<R>.value_unit()) : grid_number{1};

  // A zero divisor grid has no remainder grid: |r| < |b| holds no value.
  template <insidable L, insidable R, policy_flag F>
  inline constexpr bool mod_defined =
    (integer_ops<L, R, F | snap> || lattice_mod_ops<L, R>) && (lower_of<R> != 0 || upper_of<R> != 0);

  template <insidable L, insidable R, policy_flag F>
  inline constexpr bool divmod_defined = integer_ops<L, R, F | snap>
                                           ? div_defined<L, R, F | snap>
                                           : lattice_mod_ops<L, R> && (lower_of<R> != 0 || upper_of<R> != 0);

  // The native div and mod paths (they need snap) take their mode from the combined flags by rounding_of
  // (policy_flag.hpp), the one precedence every rounding path shares; snap alone truncates toward zero.
  // The run-time quotient and the compile-time grid endpoints must agree on the mode,
  // or a result could escape its own grid.

  // The signed exact quotient a/b (b != 0) rounded to an integer per m, and the remainder a − q·b that goes with it,
  // computed without forming q·b (which can pass T when q rounds away from zero).
  template <std::signed_integral T>
  struct rounded_division
  {
    T Quotient, Remainder;
  };

  template <std::signed_integral T>
  constexpr rounded_division<T> div_rem_rounded(T a, T b, round_mode m) noexcept
  {
    using U = std::make_unsigned_t<T>;
    if (b == -1) // exact; a / -1 and a % -1 are UB for T::min (−T::min wraps: no caller's grid holds it)
      return {T(U(0) - U(a)), T(0)};
    const T    t   = a / b;                       // C++ truncation toward zero
    const T    r   = a % b;                       // the sign of a, |r| < |b|
    const bool neg = (a < 0) != (b < 0);          // the exact quotient is negative
    const U    ar  = r < 0 ? U(~U(r) + 1u) : U(r); // |r| and |b| in U (safe for T::min)
    const U    ab  = b < 0 ? U(~U(b) + 1u) : U(b);
    if (!rounds_away(m, neg, classify_remainder(m, ar, ab), (t & 1) != 0))
      return {t, r};
    // One step away from zero;
    // r and b differ in sign (neg) or agree, so the adjusted remainder stays below |b|.
    return neg ? rounded_division<T>{T(t - 1), T(r + b)} : rounded_division<T>{T(t + 1), T(r - b)};
  }

  template <std::signed_integral T>
  constexpr T div_rounded(T a, T b, round_mode m) noexcept
  {
    return div_rem_rounded(a, b, m).Quotient;
  }

  // The native quotient q of a / b stored into Q.
  // Q's grid reaches 2^63 when a can be INT64_MIN and b −1, past from_value's int64 and where div_rem_rounded's q
  // wraps, so such a Q stores through its exact index.
  template <insidable Q, std::signed_integral T>
  constexpr Q native_quotient(T a, T b, T q) noexcept
  {
    Q res;
    if constexpr (values_fit_int64<Q>)
      from_value(res, std::int64_t{q});
    else
    {
      constexpr std::size_t K{exact_limbs<Q>};
      using I = wide_sint<K>;
      const I v{a == std::numeric_limits<T>::min() && b == T{-1} ? -I{a} : I{q}};
      res = exact_result<Q>(wide_rational<K>{v, I{1}});
    }
    return res;
  }

  // The narrowest signed type in which native div and mod of L by R are exact:
  // int32 when both value ranges fit (INT32_MIN excluded, so a / -1 cannot overflow), else int64.
  // A 32-bit divide is markedly cheaper.
  template <insidable L, insidable R>
  using native_div_t = std::conditional_t<(lower_int64<L> > std::numeric_limits<std::int32_t>::min()
                                           && upper_int64<L> <= std::numeric_limits<std::int32_t>::max()
                                           && lower_int64<R> > std::numeric_limits<std::int32_t>::min()
                                           && upper_int64<R> <= std::numeric_limits<std::int32_t>::max()),
                                          std::int32_t, std::int64_t>;

  // The zero-divisor check is skipped when R's grid excludes zero or unsafe appears
  // (a zero divisor is then UB, matching the no-op `/= 0`).
  template <insidable L, insidable R, policy_flag F, policy_flag G>
  inline constexpr bool divisor_unchecked =
    divisor_excludes_zero<R> || !is_checked(G | F | policy_of<L> | policy_of<R>);

  // A quotient interval's endpoint rounded to an integer index at compile time.
  // Under half_even the endpoint is bracketed by [floor, ceil] (the Upper by ceil) instead of reproducing the
  // parity rule.
  constexpr rational round_rat(rational q, round_mode m, bool upper) noexcept
  {
    if (m == round_mode::half_even)
      m = upper ? round_mode::ceil : round_mode::floor;
    return round_to_integral(q, m); // exact: INT64_MIN / -1 reaches 2^63
  }

  template <insidable L, insidable R = L, policy_flag F = none>
  struct division
  {
    // The native integer division, in two flavours, both with snap:
    //   native_div_integer  both operands integer-aligned: a / b;
    //   native_div_fixed    both on one notch 1/K (K > 1), anchored, any sign: the quotient's index on that notch is
    //                       round(ja·K / jb) of the value indices (the native (a << log2 K)/b for a power of two).
    // Otherwise the exact rational path returns a continuous quotient.
    static constexpr bool native_div_integer = integer_native_ops<L, R, F>;

    // (if constexpr: naming a 64-bit view instantiates it even where && would skip it,
    // so an exact-valued operand returns before any is named.)
    static constexpr bool native_div_fixed = []
    {
      if constexpr (wide_grid_numbers<L> || wide_grid_numbers<R> || !notched<L> || notch_of<L> != notch_of<R>
                    || !anchored<L> || !anchored<R> || grid_numerator(notch_of<L>) != grid_int{1}
                    || !(grid_int{1} < grid_denominator(notch_of<L>)) || !(grid_of<L> / grid_of<R>).has_value())
        return false;
      else
        return (F & snap) != 0;
    }();

    static constexpr bool wide_div_integer = integer_wide_ops<L, R, F>;

    static constexpr bool native_div = native_div_integer || native_div_fixed || wide_div_integer;

    // Any other dividend on a lattice, with a mode: the quotient rounded once onto the multiples of the dividend's
    // notch (as native_div_fixed does on a shared 1/K notch).
    // Without a mode, or for a continuous dividend, the quotient is exact.
    static constexpr bool notch_div = []
    {
      if constexpr (native_div || wide_grid_numbers<L> || wide_grid_numbers<R> || !notched<L> || (F & snap) == 0)
        return false;
      else
        return (grid_of<L> / grid_of<R>).has_value();
    }();

    // The rounding mode of the native paths, the grid's and the run time's.
    static constexpr round_mode rmode = rounding_of(F);

    // A clear diagnostic when there is no result grid, instead of a cryptic failure of the expected-deref below.
    // The native integer path forms no exact quotient grid.
    static_assert(div_defined<L, R, F>,
                  "division: no result type - the divisor grid is the point 0, or the exact quotient's fractions "
                  "pass 1024 bits");

    // The native integer quotient's range: the corner quotients rounded by the run-time mode
    // (half_even: the low end by floor, the high by ceil, as round_rat).
    // a/b is monotonic in each operand on either side of 0, and so is its rounding,
    // so these are the rounded endpoints of the exact quotient grid, which is not formed:
    // its endpoints can pass the 64-bit rational (7/2^63 for a divisor reaching INT64_MIN).
    // Rounded in uint64, sign apart, so limits past int64 and INT64_MIN / −1 = 2^63 need no special case
    // (|a / b| ≤ |a| for an integer |b| ≥ 1).
    // Grid numbers past 64 bits (a big grid) round the same way in grid integers.
    static constexpr interval integer_quotient_range()
    {
      const round_mode lo_mode{rmode == round_mode::half_even ? round_mode::floor : rmode};
      const round_mode hi_mode{rmode == round_mode::half_even ? round_mode::ceil : rmode};
      constexpr bool   big{wide_grid_numbers<L> || wide_grid_numbers<R>};
      using M = std::conditional_t<big, grid_int, std::uint64_t>;
      using V = std::conditional_t<big, grid_number, rational>;
      struct signed_mag
      {
        M    Mag;
        bool Neg;
      };
      auto of = [](const grid_number& g)
      {
        if constexpr (big)
          return signed_mag{abs(grid_numerator(g)), grid_numerator(g).negative()};
        else
        {
          const rational r{to_rational(g)};
          return signed_mag{r.Num, r.Den < 0 && r.Num != 0};
        }
      };
      auto quotient = [](const signed_mag& a, const signed_mag& b, round_mode m)
      {
        const M    q{a.Mag / b.Mag}, r{a.Mag % b.Mag};
        const bool neg{a.Neg != b.Neg && a.Mag != M{0}};
        const M    mag{rounds_away(m, neg, classify_remainder(m, r, b.Mag), q % M{2} != M{0}) ? q + M{1} : q};
        return neg ? -V{mag} : V{mag};
      };
      const signed_mag lo{of(lower_of<R>)}, hi{of(upper_of<R>)};
      const M          step{of(grid_of<R>.value_unit()).Mag};
      signed_mag       divisors[4];
      int              n{0};
      if ((!lo.Neg && lo.Mag != M{0}) || hi.Neg) // zero-free: the limits
      {
        divisors[n++] = lo;
        divisors[n++] = hi;
      }
      else
      {
        if (hi.Mag != M{0})
        {
          divisors[n++] = signed_mag{step, false};
          divisors[n++] = hi;
        }
        if (lo.Neg)
        {
          divisors[n++] = lo;
          divisors[n++] = signed_mag{step, true};
        }
      }
      if (n == 0) // a zero divisor grid (the static_assert above)
        return interval{0, 0};
      const signed_mag a0{of(lower_of<L>)};
      V                ql{quotient(a0, divisors[0], lo_mode)}, qh{quotient(a0, divisors[0], hi_mode)};
      for (const signed_mag& a : {a0, of(upper_of<L>)})
        for (int i = 0; i < n; ++i)
        {
          const V l{quotient(a, divisors[i], lo_mode)}, h{quotient(a, divisors[i], hi_mode)};
          if (l < ql)
            ql = l;
          if (qh < h)
            qh = h;
        }
      return interval{ql, qh};
    }

    // The native paths' endpoints rounded by the run-time quotient's mode, so round_ceil cannot escape the grid:
    // onto the integers, or onto the operands' notch 1/K.
    static constexpr grid result_grid = []
    {
      if constexpr (native_div_integer || wide_div_integer)
        return grid{integer_quotient_range(), rational{1}};
      else if constexpr (native_div_fixed)
      {
        constexpr interval q{(*(grid_of<L> / grid_of<R>)).interval()};
        // An endpoint on the notch stays; one off it rounds onto it.
        constexpr rational n{notch64<L>}, k{rational{1} / n};
        auto on = [&](const grid_number& x, bool up)
        {
          return grid_divides_evenly(x, notch_of<L>) ? to_rational(x)
                                                     : round_rat(to_rational(x) * k, rmode, up) * n;
        };
        return grid{interval{on(q.Lower, false), on(q.Upper, true)}, n};
      }
      else if constexpr (notch_div)
      {
        // The exact quotient's limits rounded the same way (rounding is monotone), onto multiples of n.
        constexpr interval    q{(*(grid_of<L> / grid_of<R>)).interval()};
        constexpr grid_number n{notch_of<L>};
        return grid{interval{grid_round<rmode>(q.Lower / n) * n, grid_round<rmode>(q.Upper / n) * n}, n};
      }
      else
        return *(grid_of<L> / grid_of<R>);
    }();

    using result = inside<result_grid, result_flags<L, R>>; // the operands' shared policy

    // The exact quotient of two anchored integer raws as one 64-bit fraction:
    // |J_L|·p_L·q_R and |J_R|·p_R·q_L stay below 2^62 for every value.
    static constexpr bool int_quotient_fits = []
    {
      if constexpr (!rational_storage<result> || !notched<L> || !notched<R> || !anchored<L> || !anchored<R>
                    || wide_valued<L> || wide_valued<R>)
        return false;
      else
      {
        constexpr grid_int lim{grid_int{1} << 62};
        auto max_j = []<insidable B>()
        {
          const grid_int lo{exact_quotient(lower_of<B>, notch_of<B>)}, hi{exact_quotient(upper_of<B>, notch_of<B>)};
          const grid_int a{abs(lo)}, b{abs(hi)};
          return a < b ? b : a;
        };
        const grid_int a{grid_numerator(notch_of<L>) * grid_denominator(notch_of<R>)};
        const grid_int b{grid_numerator(notch_of<R>) * grid_denominator(notch_of<L>)};
        return max_j.template operator()<L>() * a < lim && max_j.template operator()<R>() * b < lim;
      }
    }();

    // x / just<c> (c != 0) off the native paths: the result keeps x's lattice scaled by 1/|c| (grid operator/),
    // so it is x's offset (op_result.hpp), or past 64 bits the exact quotient on that lattice.
    // Exact and in range by construction: it cannot fail.
    static constexpr bool point_divisor =
      !native_div && !notch_div && point_grid<R> && lower_of<R> != 0 && notched<result>;

    // notch_div in integers: with A and B the operands' values in their own units uL and uR,
    // the slot is round(A·cp / (B·cd)), cp/cd = uL/(uR·n), and both products fit int64.
    static constexpr bool notch_div_int = []
    {
      if constexpr (!notch_div || !notched<R> || !values_fit_int64<L> || !values_fit_int64<R>
                    || !values_fit_int64<result>)
        return false;
      else
      {
        constexpr grid_number c{unit_of<L> / (unit_of<R> * notch_of<L>)};
        constexpr grid_int    lim{grid_int{1} << 63};
        auto max_abs = [](const grid_int& a, const grid_int& b) { return abs(a) < abs(b) ? abs(b) : abs(a); };
        return max_abs(units_lo<L, unit_of<L>>, units_hi<L, unit_of<L>>) * grid_numerator(c) < lim
            && max_abs(units_lo<R, unit_of<R>>, units_hi<R, unit_of<R>>) * grid_denominator(c) < lim;
      }
    }();

    // A quotient never overflows: the native paths round onto a grid of the rounded corner quotients,
    // a point divisor scales the lattice, and any other quotient is a point or continuous with a denominator bound
    // that sizes its raw (exact_fits).
    // The only failure is a zero divisor; a grid that excludes it gives a plain result.
    using return_t = std::conditional_t<divisor_excludes_zero<R>, result, std::expected<result, error>>;

    template <policy_flag G = F, typename E = empty_ref>
    static constexpr return_t div(L, R, call_policy<G, E> = {});
  };

  template <insidable L, insidable R, policy_flag F>
  template <policy_flag G, typename E>
  constexpr auto division<L, R, F>::div(L lhs, R rhs, call_policy<G, E> policy) -> return_t
  {
    // A zero divisor, reported through the policy's error& (if any) and returned as the error.
    // Reachable only where return_t is an expected: the divisor grid holds zero and no unsafe appears.
    [[maybe_unused]] auto zero = [&] { return report_or_unexpected<result>(policy, error::division_by_zero); };
    [[maybe_unused]] constexpr bool zero_unchecked{divisor_unchecked<L, R, F, G>};

    if constexpr (native_div_fixed)
    {
      // ja·K, jb and the quotient in the narrowest signed type that holds them:
      // a 32-bit divide where it fits (Q8.8, Q15, …), else 64 bits, else a wide_int.
      // The quotient's magnitude is at most |ja·K|; −ja·K is listed for the one that can pass the type, min / −1.
      constexpr grid_int K{grid_denominator(notch_of<L>)};
      constexpr grid_int ja_lo{exact_quotient(lower_of<L>, notch_of<L>) * K};
      constexpr int      bits{signed_value_bits({ja_lo, -ja_lo, exact_quotient(upper_of<L>, notch_of<L>) * K,
                                                 exact_quotient(lower_of<R>, notch_of<R>),
                                                 exact_quotient(upper_of<R>, notch_of<R>)})};
      using W = std::conditional_t<(bits <= 31), std::int32_t, work_int_t<bits>>; // bits: without the sign
      const W jb{value_index<W>(rhs)};
      if constexpr (!zero_unchecked)
        if (jb == W{0})
          return zero();
      const W ja{value_index<W>(lhs) * static_cast<W>(K)};
      if constexpr (std::is_integral_v<W>)
        return from_value_index<result>(div_rounded(ja, jb, rmode));
      else
        return from_value_index<result>(rounded_div<rmode>(ja, jb));
    }
    else if constexpr (native_div_integer)
    {
      using T = native_div_t<L, R>;
      const T rhs_val{static_cast<T>(to_value(rhs))};
      if constexpr (!zero_unchecked)
        if (rhs_val == 0)
          return zero();
      const T lhs_val{static_cast<T>(to_value(lhs))};
      return native_quotient<result>(lhs_val, rhs_val, div_rounded(lhs_val, rhs_val, rmode));
    }
    else if constexpr (wide_div_integer)
    {
      // Integer values past int64: the rounded quotient in exact wide integers, as modulo's remainder.
      constexpr std::size_t K{exact_limbs<L, R, result>};
      using I = wide_sint<K>;
      const I b{value_in_units<I, grid_number{1}>(rhs)}; // integer values: no division
      if constexpr (!zero_unchecked)
        if (b.is_zero())
          return zero();
      const I a{value_in_units<I, grid_number{1}>(lhs)};
      return from_value_index<result>(rounded_div<rmode>(a, b)); // notch 1: the value is the index
    }
    else if constexpr (notch_div_int)
    {
      constexpr grid_number  c{unit_of<L> / (unit_of<R> * notch_of<L>)};
      constexpr std::int64_t cp{static_cast<std::int64_t>(grid_numerator(c))},
                             cd{static_cast<std::int64_t>(grid_denominator(c))};
      const std::int64_t b{value_in_units<std::int64_t, unit_of<R>>(rhs)};
      if constexpr (!zero_unchecked)
        if (b == 0)
          return zero();
      const std::int64_t a{value_in_units<std::int64_t, unit_of<L>>(lhs)};
      return from_value_index<result>(div_rounded(a * cp, b * cd, rmode));
    }
    else if constexpr (notch_div)
    {
      // The exact quotient, rounded once onto result's notch (in range by the construction of its grid).
      const auto q{division<L, R>::div(lhs, rhs, policy)};
      if constexpr (std::same_as<std::remove_cvref_t<decltype(q)>, typename division<L, R>::result>)
        return result{q, make_policy<F | unsafe>()};
      else
      {
        if (!q)
          return std::unexpected{q.error()};
        return result{*q, make_policy<F | unsafe>()};
      }
    }
    else if constexpr (point_storage<result>)
    {
      // A point quotient (point / point, or 0 / x): its one value is the type.
      if constexpr (!zero_unchecked)
        if (rhs == 0)
          return zero();
      return result::from_raw(raw_t<result>{}, unsafe);
    }
    else if constexpr (point_divisor)
    {
      if constexpr (point_scale<R, L, result>)
        return scale_by_point<result, R>(lhs);
      else
        return exact_result<result>(exact_of(lhs) / exact_of(rhs));
    }
    else if constexpr (wide_valued<L> || wide_valued<R> || wide_valued<result>)
    {
      // A wide-index or big-grid operand: the exact quotient, in the result's raw.
      static_assert(exact_fits<exact_op::div, result, L, R>, "a continuous quotient's bound sizes its raw");
      const auto d{exact_of(rhs)};
      if constexpr (!zero_unchecked)
        if (d.Num.is_zero())
          return zero();
      // (A wide operand puts the quotient's numerator or denominator bound past 64 bits: a fraction raw.)
      static_assert(fraction_storage<result>);
      return result::from_raw(fraction_raw_of<raw_t<result>>(exact_of(lhs) / d), unsafe);
    }
    else if constexpr (int_quotient_fits)
    {
      // Two integer raws: J_L·Notch_L / (J_R·Notch_R) as one fraction of value indices, reduced once,
      // with no operand decoded and no checked divide.
      static_assert(notch_int64<L>.Fits && notch_int64<R>.Fits);
      constexpr std::int64_t a{notch_int64<L>.P * notch_int64<R>.Q};
      constexpr std::int64_t b{notch_int64<R>.P * notch_int64<L>.Q};
      const std::int64_t d{value_index<std::int64_t>(rhs) * b};
      if constexpr (!zero_unchecked)
        if (d == 0)
          return zero();
      const std::int64_t n{value_index<std::int64_t>(lhs) * a};
      return result::from_raw(d < 0 ? rational{-n, -d} : rational{n, d}, unsafe);
    }
    else
    {
      static_assert(exact_fits<exact_op::div, result, L, R>, "a continuous quotient's bound sizes its raw");
      rational rhs_r = rhs;
      if constexpr (!zero_unchecked)
        if (rhs_r.Num == 0)
          return zero();
      return result::from_raw(div_unchecked(as_rational(lhs), rhs_r), unsafe);
    }
  }

  // modulo: notched grids only, integer-valued or a lattice in units of mod_unit.
  template <insidable L, insidable R, policy_flag F = none>
  struct modulo
  {
    // a mod b is the remainder of an integer quotient, so it always rounds:
    // truncation (C++ %) unless the policy names a mode, with or without snap (which only decides whether / rounds).
    static constexpr policy_flag FS = F | snap;

    // A hard requirement, not a fallback: a mod b needs a lattice on both sides.
    static_assert(integer_ops<L, R, FS> || lattice_mod_ops<L, R>, "modulo requires notched grids");

    // The builtin division when every value fits int64, else exact wide integers.
    static constexpr bool native_mod = integer_native_ops<L, R, FS>;

    // The values' unit: 1 on integer grids, else the gcd of the value units.
    static constexpr grid_number unit_value = mod_unit<L, R>;
    // A lattice remainder in builtin integers, when both operands' unit counts fit int64.
    static constexpr bool lattice_native =
      lattice_mod_ops<L, R> && signed_value_bits({units_lo<L, unit_value>, units_hi<L, unit_value>}) < 63
      && signed_value_bits({units_lo<R, unit_value>, units_hi<R, unit_value>}) < 63;

    static constexpr round_mode rmode = rounding_of(F);

    // The remainder that goes with the rounded quotient, r = a − round(a/b)·b,
    // with the tightest bounds every mode allows:
    //   trunc    the dividend's sign, |r| ≤ min(|b| − 1, |a|)
    //   floor    the divisor's sign, |r| ≤ |b| − 1, and ≤ |a| when a and b agree
    //   ceil     the divisor's opposite sign, the same bounds
    //   nearest  either sign (also half_even), |r| ≤ min(⌊|b|/2⌋, |a|)
    // (|r| ≤ |a| holds wherever round(a/b) = 0 is the only way to keep r = a.)
    // In units of `unit` (1 on integer grids): b_max − 1 is b_max − unit, and ⌊|b|/2⌋ counts units.
    static constexpr grid result_grid = []
    {
      using G = grid_number;
      const G one{unit_value}, zero{0};
      const G a_lo{lower_of<L>}, a_hi{upper_of<L>}, b_lo{lower_of<R>}, b_hi{upper_of<R>};
      const G b_max{abs(b_lo) > abs(b_hi) ? abs(b_lo) : abs(b_hi)};
      const G a_max{abs(a_lo) > abs(a_hi) ? abs(a_lo) : abs(a_hi)};
      auto lesser = [](const G& x, const G& y) { return x < y ? x : y; };
      G lo{zero}, hi{zero};
      switch (rmode)
      {
        case round_mode::trunc:
          if (zero < a_hi)
            hi = lesser(b_max - one, a_hi);
          if (a_lo < zero)
            lo = -lesser(b_max - one, -a_lo);
          break;
        case round_mode::floor:
        case round_mode::ceil:
        {
          // floor: r follows b's sign; ceil: the opposite.
          // A divisor part of one sign bounds |r| by its magnitude − 1,
          // and by |a| where the dividend's sign is r's (round(a/b) is then 0 or a's own multiple).
          const bool floor_mode{rmode == round_mode::floor};
          if (zero < b_hi) // positive divisors: r ≥ 0 (floor) or r ≤ 0 (ceil)
          {
            const G m{b_hi - one};
            if (floor_mode)
              hi = zero <= a_lo ? lesser(m, a_hi) : m;
            else
              lo = a_hi <= zero ? -lesser(m, -a_lo) : -m;
          }
          if (b_lo < zero) // negative divisors: the mirror
          {
            const G m{-b_lo - one};
            if (floor_mode)
              lo = lesser(lo, a_hi <= zero ? -lesser(m, -a_lo) : -m);
            else
            {
              const G h{zero <= a_lo ? lesser(m, a_hi) : m};
              hi = hi < h ? h : hi;
            }
          }
          break;
        }
        default: // nearest, half_even: |r| ≤ |b|/2, either sign
        {
          const G half{G{grid_numerator(b_max / one) / grid_int{2}} * one}; // b_max: whole units ≥ 0
          hi = lesser(half, a_max);
          lo = -hi;
          break;
        }
      }
      if constexpr (lattice_mod_ops<L, R>)
        return grid{{lo, hi}, one};
      else
        return grid{lo, hi};
    }();

    using result = inside<result_grid, result_flags<L, R>>; // the operands' shared policy

    // Modulo never overflows (the remainder fits result_grid):
    // the only failure is a zero divisor, and a grid that excludes it gives a plain result.
    using return_t = std::conditional_t<divisor_excludes_zero<R>, result, std::expected<result, error>>;

    template <policy_flag G = F, typename E = empty_ref>
    static constexpr return_t mod(L, R, call_policy<G, E> = {});
  };

  template <insidable L, insidable R, policy_flag F>
  template <policy_flag G, typename E>
  constexpr auto modulo<L, R, F>::mod(L lhs, R rhs, call_policy<G, E> policy) -> return_t
  {
    constexpr bool zero_unchecked{divisor_unchecked<L, R, F, G>};
    if constexpr (lattice_native)
    {
      // A lattice: the integer remainder of the unit counts, scaled back.
      const std::int64_t b{value_in_units<std::int64_t, unit_value>(rhs)};
      if constexpr (!zero_unchecked)
        if (b == 0)
          return report_or_unexpected<result>(policy, error::division_by_zero);
      return from_value_in_units<result, unit_value>(
        div_rem_rounded(value_in_units<std::int64_t, unit_value>(lhs), b, rmode).Remainder);
    }
    else if constexpr (lattice_mod_ops<L, R>)
    {
      // A lattice past int64 units: the same in exact wide integers.
      constexpr std::size_t K{exact_limbs<L, R, result>};
      using I = wide_sint<K>;
      const I b{value_in_units<I, unit_value>(rhs)};
      if constexpr (!zero_unchecked)
        if (b.is_zero())
          return report_or_unexpected<result>(policy, error::division_by_zero);
      const I a{value_in_units<I, unit_value>(lhs)};
      return exact_result<result>(
        wide_rational<K>{(a - rounded_div<rmode>(a, b) * b) * static_cast<I>(grid_numerator(unit_value)),
                         static_cast<I>(grid_denominator(unit_value))});
    }
    else if constexpr (!native_mod)
    {
      // Integer values past int64: r = a − round(a/b)·b in exact wide integers.
      using I = wide_sint<exact_limbs<L, R, result>>;
      const I b{value_in_units<I, grid_number{1}>(rhs)}; // integer values: no division
      if constexpr (!zero_unchecked)
        if (b.is_zero())
          return report_or_unexpected<result>(policy, error::division_by_zero);
      const I a{value_in_units<I, grid_number{1}>(lhs)};
      return exact_result<result>(wide_rational<exact_limbs<L, R, result>>{a - rounded_div<rmode>(a, b) * b, I{1}});
    }
    else
    {
      using T = native_div_t<L, R>;
      const T rhs_val{static_cast<T>(to_value(rhs))};
      if constexpr (!zero_unchecked)
        if (rhs_val == 0)
          return report_or_unexpected<result>(policy, error::division_by_zero);
      result res;
      // The remainder that goes with the rounded quotient (trunc: C++ %).
      const T lhs_val{static_cast<T>(to_value(lhs))};
      from_value(res, std::int64_t{div_rem_rounded(lhs_val, rhs_val, rmode).Remainder});
      return res;
    }
  }

  // divmod, what % requires: the rounded quotient and the remainder r = a − q·b that goes with it, from one division
  // (native within int64, else exact wide integers). The halves have the grids of a / b and a % b.
  template <insidable L, insidable R, policy_flag F = none>
  struct quotient_remainder
  {
    // Rounds like %, so like % it needs no snap.
    static constexpr policy_flag FS = F | snap;

    static_assert(integer_ops<L, R, FS> || lattice_mod_ops<L, R>, "divmod requires notched grids");

    static constexpr round_mode rmode = rounding_of(FS);

    // On a lattice the quotient is the integer round(a/b):
    // its grid holds the quotient interval's limits rounded the same way (rounding is monotone).
    // (A consteval function, not an immediately invoked lambda: GCC 16 takes the address of an immediate lambda's
    // invoker.)
    static consteval grid lattice_quotient_grid_of()
    {
      const interval q{(*(grid_of<L> / grid_of<R>)).interval()};
      return grid{grid_round<rmode>(q.Lower), grid_round<rmode>(q.Upper)};
    }
    static constexpr grid lattice_quotient_grid = lattice_quotient_grid_of();
    static constexpr auto quotient_tag()
    {
      if constexpr (integer_ops<L, R, FS>)
        return std::type_identity<typename division<L, R, FS>::result>{};
      else
        return std::type_identity<inside<lattice_quotient_grid, result_flags<L, R>>>{};
    }
    using quotient = typename decltype(quotient_tag())::type;

    using result = divmod_result<quotient, typename modulo<L, R, FS>::result>;

    using return_t = std::conditional_t<divisor_excludes_zero<R>, result, std::expected<result, error>>;

    // The only failure is a zero divisor.
    template <policy_flag G = F, typename E = empty_ref>
    static constexpr return_t divmod(L lhs, R rhs, call_policy<G, E> policy = {})
    {
      // (Generic: its body is formed only where a zero divisor is possible.)
      auto fail = [&](auto code) -> return_t
      {
        if constexpr (uses_error_ref<std::remove_cvref_t<decltype(policy)>>)
          policy.report(code);
        return std::unexpected{code};
      };
      if constexpr (modulo<L, R, FS>::lattice_native && integer_lattice<quotient> && values_fit_int64<quotient>)
      {
        // A lattice whose unit counts fit int64: one builtin division, as %.
        using M = typename modulo<L, R, FS>::result;
        constexpr grid_number u{mod_unit<L, R>};
        const std::int64_t    b{value_in_units<std::int64_t, u>(rhs)};
        if constexpr (!divisor_unchecked<L, R, F, G>)
          if (b == 0)
            return fail(error::division_by_zero);
        const auto qr{div_rem_rounded(value_in_units<std::int64_t, u>(lhs), b, rmode)};
        result     res{quotient{}, from_value_in_units<M, u>(qr.Remainder)};
        from_value(res.Quotient, qr.Quotient);
        return res;
      }
      else if constexpr (lattice_mod_ops<L, R>)
      {
        // A lattice: q and r = a − q·b on the unit counts, exactly.
        using M = typename modulo<L, R, FS>::result;
        constexpr grid_number u{mod_unit<L, R>};
        constexpr std::size_t K{exact_limbs<L, R, quotient, M>};
        using I = wide_sint<K>;
        const I b{value_in_units<I, u>(rhs)};
        if constexpr (!divisor_unchecked<L, R, F, G>)
          if (b.is_zero())
            return fail(error::division_by_zero);
        const I a{value_in_units<I, u>(lhs)};
        const I q{rounded_div<rmode>(a, b)};
        return result{exact_result<quotient>(wide_rational<K>{q, I{1}}),
                      exact_result<M>(wide_rational<K>{(a - q * b) * static_cast<I>(grid_numerator(u)),
                                                       static_cast<I>(grid_denominator(u))})};
      }
      else if constexpr (!integer_native_ops<L, R, FS>)
      {
        // Integer values past int64: q and r = a − q·b in exact wide integers, as / and % do.
        using Q = typename division<L, R, FS>::result;
        using M = typename modulo<L, R, FS>::result;
        constexpr std::size_t K{exact_limbs<L, R, Q, M>};
        using I = wide_sint<K>;
        const I b{value_in_units<I, grid_number{1}>(rhs)}; // integer values: no division
        if constexpr (!divisor_unchecked<L, R, F, G>)
          if (b.is_zero())
            return fail(error::division_by_zero);
        const I a{value_in_units<I, grid_number{1}>(lhs)};
        const I q{rounded_div<rmode>(a, b)};
        if constexpr (notch_of<Q> == 1 && notch_of<M> == 1 && anchored<Q> && anchored<M>)
          return result{from_value_index<Q>(q), from_value_index<M>(I{a - q * b})};
        else
          return result{exact_result<Q>(wide_rational<K>{q, I{1}}),
                        exact_result<M>(wide_rational<K>{a - q * b, I{1}})};
      }
      else
      {
        using T = native_div_t<L, R>;
        const T rhs_val{static_cast<T>(to_value(rhs))};
        if constexpr (!divisor_unchecked<L, R, F, G>)
          if (rhs_val == 0)
            return fail(error::division_by_zero);
        const T    lhs_val{static_cast<T>(to_value(lhs))};
        const auto qr{div_rem_rounded(lhs_val, rhs_val, rmode)};
        result     res{native_quotient<typename division<L, R, FS>::result>(lhs_val, rhs_val, qr.Quotient), {}};
        from_value(res.Remainder, std::int64_t{qr.Remainder});
        return res;
      }
    }
  };

} // namespace ins::detail

#endif // INSIDE_INSIDE_DETAIL_DIVISION_HPP
