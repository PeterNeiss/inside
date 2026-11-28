// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Polynomials: sums of parts in the powers of variables of your own (basis_vector<"x", polynomial>),
// and their calculus, exact:
//   - substitute(p, x, a): p with x replaced by a (a number, a quantity, or a polynomial: p∘a);
//   - derivative(p, x), antiderivative(p, x);
//   - divmod(p, d): the quotient and the remainder of p / d, in one variable.
// +, -, * and the division by one term come from units/geoalg.hpp. Each coefficient keeps its own inside,
// so every coefficient's range is proved.
#ifndef INSIDE_UNITS_POLYNOMIAL_HPP
#define INSIDE_UNITS_POLYNOMIAL_HPP

#include <inside/inside/detail/config.hpp>

#include <inside/units/geoalg.hpp>

#include <algorithm>
#include <cstdint>
#include <expected>
#include <type_traits>

namespace ins
{
  namespace detail
  {
    // The variable X names: x of basis_vector<"x", polynomial>.
    template <unit_value X>
    consteval const base_dimension_def* variable_of()
    {
      const dimension d{dimension_of(X)};
      if (d.Size != 1 || d.Exp[0] != 1 || d.Base[0]->Rule.Product != basis_product::polynomial
          || !magnitude_of(X).is_one())
        ins::detail::constexpr_error<"units: the variable is a polynomial basis vector - basis_vector<\"x\", "
                                     "polynomial>">();
      return d.Base[0];
    }
    // The power of v in d.
    consteval int power_in(const dimension& d, const base_dimension_def* v)
    {
      for (std::size_t i = 0; i < d.Size; ++i)
        if (d.Base[i]->Name == v->Name && d.Base[i]->Rule == v->Rule)
          return d.Exp[i];
      return 0;
    }
    template <unit_value X, class P>
    consteval int power_of()
    {
      return power_in(part_dim<P>(), variable_of<X>());
    }
    // The highest and the lowest power of x in p's parts, 0 included.
    template <unit_value X, class P>
    consteval int max_power()
    {
      int m{0};
      for (const dimension& d : dims_of<parts_t<P>>())
        m = std::max(m, power_in(d, variable_of<X>()));
      return m;
    }
    template <unit_value X, class P>
    consteval int min_power()
    {
      int m{0};
      for (const dimension& d : dims_of<parts_t<P>>())
        m = std::min(m, power_in(d, variable_of<X>()));
      return m;
    }
    template <class P>
    consteval bool has_polynomial_parts()
    {
      return std::ranges::any_of(dims_of<parts_t<P>>(), has_polynomial);
    }

    // p times x^E.
    template <unit_value X, int E, class P>
    constexpr auto times_power(const P& p)
    {
      return each_part(
        p,
        [](const auto& q)
        {
          using Q = std::remove_cvref_t<decltype(q)>;
          static constexpr unit_value u = []
          {
            unit_value t{unit_of_part<Q>()};
            unit_add(t, unit_atom{variable_of<X>()->Vector, 0, static_cast<std::int8_t>(E)});
            return t;
          }();
          return as_part(make_quantity<reference_of(u)>(number_of_part(q)));
        });
    }
    // The coefficient of x^K in p: its parts with x to the power K, x taken out.
    template <unit_value X, int K, class P>
    constexpr auto coefficient(const P& p)
    {
      return each_part(
        p,
        [](const auto& q)
        {
          if constexpr (power_of<X, std::remove_cvref_t<decltype(q)>>() == K)
            return times_power<X, -K>(q);
          else
            return multivector<>{};
        });
    }
    // p's parts below x^K.
    template <unit_value X, int K, class P>
    constexpr auto below_power(const P& p)
    {
      return each_part(
        p,
        [](const auto& q)
        {
          if constexpr (power_of<X, std::remove_cvref_t<decltype(q)>>() < K)
            return q;
          else
            return multivector<>{};
        });
    }

    // Σ c_k a^(k - K) over k from K to Max, by Horner's rule.
    template <unit_value X, int K, int Max, class P, class A>
    constexpr auto horner(const P& p, const A& a)
    {
      if constexpr (K == Max)
        return coefficient<X, K>(p);
      else
        return coefficient<X, K>(p) + horner<X, K + 1, Max>(p, a) * a;
    }

    // The one variable of p and d, as a unit.
    template <class P, class D>
    consteval unit_value one_variable()
    {
      const base_dimension_def* v{};
      bool                      found{false}; // tested instead of v (no address test in a constant expression)
      const auto                scan = [&](const auto& dims)
      {
        for (const dimension& d : dims)
          for (const monomial_factor& f : monomial_of(d))
          {
            if (found && v->Name != f.Var->Name)
              ins::detail::constexpr_error<"units: divmod - polynomials in one variable">();
            v     = f.Var;
            found = true;
          }
      };
      scan(dims_of<parts_t<P>>());
      scan(dims_of<parts_t<D>>());
      unit_value u{};
      unit_add(u, unit_atom{v->Vector, 0, 1});
      return u;
    }
    // A quotient whose divisor is not 0 (proved by its grid, or checked).
    template <class Q>
    constexpr auto known_quotient(const Q& q)
    {
      if constexpr (is_expected_v<Q>)
        return *q;
      else
        return q;
    }
    // One step per power of the quotient, from the highest, N + K, down: r's term of x^(N+K) over d's leading
    // coefficient lc is the quotient's term of x^K, and that multiple of d comes off r; the term it cancels is 0,
    // and drops out of r's type.
    template <unit_value X, int N, int K, class LC, class Q, class R, class D>
    constexpr auto long_division(const LC& lc, const Q& q, const R& r, const D& d)
    {
      const auto t   {times_power<X, K>(known_quotient(coefficient<X, N + K>(r) / lc))};
      const auto rest{below_power<X, N + K>(r - t * d)};
      if constexpr (K == 0)
        return divmod_result<decltype(q + t), decltype(rest)>{q + t, rest};
      else
        return long_division<X, N, K - 1>(lc, q + t, rest, d);
    }
  } // namespace detail

  // substitute(p, x, a): p with the variable x replaced by a: a number or a quantity gives p's value there, a
  // polynomial the composition p∘a. Horner's rule, exact; x's powers in p are 0 or more.
  template <multivector_like P, unit_value X, multivector_like A>
  [[nodiscard]] constexpr auto substitute(const P& p, unit_t<X>, const A& a)
  {
    static_assert(detail::min_power<X, P>() >= 0, "units: substitute - p has a negative power of the variable");
    return detail::horner<X, 0, detail::max_power<X, P>()>(p, a);
  }

  // derivative(p, x): c·xᵏ to k·c·xᵏ⁻¹, exact.
  template <multivector_like P, unit_value X>
  [[nodiscard]] constexpr auto derivative(const P& p, unit_t<X>)
  {
    return detail::each_part(
      p,
      [](const auto& q)
      {
        constexpr int k{detail::power_of<X, std::remove_cvref_t<decltype(q)>>()};
        if constexpr (k == 0)
          return multivector<>{};
        else
          return detail::times_power<X, -1>(detail::as_part(detail::point_v<k> * q));
      });
  }

  // antiderivative(p, x): the one with constant term 0, c·xᵏ to c/(k+1)·xᵏ⁺¹, exact.
  template <multivector_like P, unit_value X>
  [[nodiscard]] constexpr auto antiderivative(const P& p, unit_t<X>)
  {
    return detail::each_part(
      p,
      [](const auto& q)
      {
        constexpr int k{detail::power_of<X, std::remove_cvref_t<decltype(q)>>()};
        if constexpr (k == -1)
        {
          static_assert(false, "units: antiderivative - the antiderivative of x⁻¹ is a logarithm, no polynomial");
          return multivector<>{};
        }
        else
        {
          constexpr int sign{k + 1 > 0 ? 1 : -1};
          return detail::times_power<X, 1>(detail::as_part(detail::point_v<grid_number{sign} / (sign * (k + 1))> * q));
        }
      });
  }

  // divmod(p, d): the quotient q and the remainder r of polynomials in one variable, p = q d + r with r's degree
  // below d's, exact: auto [q, r] = divmod(p, d). A std::expected when d's leading coefficient's grid holds 0
  // (error::division_by_zero when it is 0).
  template <multivector_like P, multivector_like D>
    requires(detail::has_polynomial_parts<P>() || detail::has_polynomial_parts<D>())
  [[nodiscard]] constexpr auto divmod(const P& p, const D& d)
  {
    constexpr unit_value X{detail::one_variable<P, D>()};
    static_assert(detail::min_power<X, P>() >= 0 && detail::min_power<X, D>() >= 0,
                  "units: divmod - polynomials with no negative powers");
    constexpr int M{detail::max_power<X, P>()}, N{detail::max_power<X, D>()};
    const auto    lc{detail::coefficient<X, N>(d)};
    static_assert(!detail::is_multivector_v<std::remove_cvref_t<decltype(lc)>>,
                  "units: divmod - d's leading coefficient is one number or quantity");
    const auto divide = [&]
    {
      if constexpr (M < N)
        return divmod_result<multivector<>, P>{multivector<>{}, p};
      else
        return detail::long_division<X, N, M - N>(lc, multivector<>{}, p, d);
    };
    if constexpr (!detail::is_expected_v<decltype(lc / lc)>)
      return divide();
    else
    {
      using result = decltype(divide());
      if (detail::number_of_part(lc) == 0)
        return std::expected<result, error>{std::unexpect, error::division_by_zero};
      return std::expected<result, error>{divide()};
    }
  }
} // namespace ins

#endif // INSIDE_UNITS_POLYNOMIAL_HPP
