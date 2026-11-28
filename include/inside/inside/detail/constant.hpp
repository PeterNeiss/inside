// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INSIDE_INSIDE_DETAIL_CONSTANT_HPP
#define INSIDE_INSIDE_DETAIL_CONSTANT_HPP

#include <inside/inside/detail/config.hpp>

#include <inside/inside/grid.hpp> // grid
#include <inside/inside/key.hpp>  // key, id

#include <concepts>
#include <type_traits> // std::constant_wrapper

// A value the code computes while compiling, std::cw<V> (std::constant_wrapper), is the point V:
// std::cw<N> in a template, std::cw<upper_of<T>>, std::cw<grid_number{1} / 3>. (A literal is _q's: 3_q, "2/3"_q.)
// The operators, comparisons, compound assignments and constructors read it as id<key{grid{V}}>.
// Two wrappers combine by their own rules, as std defines them: std::cw<1> / std::cw<3> is std::cw<0>.
namespace ins::detail
{
  template <class T>
  inline constexpr bool is_constant_wrapper_v = false;
  template <auto X, class T>
  inline constexpr bool is_constant_wrapper_v<std::constant_wrapper<X, T>> = true;

  template <class C>
  using constant_type_t = typename std::remove_cvref_t<C>::value_type;

  // A wrapper of an exact number: an integer (not a bool), a grid number or a rational.
  template <class C>
  concept constant_number = is_constant_wrapper_v<std::remove_cvref_t<C>>
                        && !std::same_as<constant_type_t<C>, bool> && !std::floating_point<constant_type_t<C>>
                        && requires { grid{std::remove_cvref_t<C>::value}; };

  // A wrapper of a double is its binary value, not the decimal written (std::cw<0.1>):
  // the raw-scalar overloads reject it, as they reject 0.1.
  template <class C>
  concept floating_constant =
    is_constant_wrapper_v<std::remove_cvref_t<C>> && std::floating_point<constant_type_t<C>>;

  // The point a wrapper names.
  template <constant_number C>
  using point_of_t = id<key{grid{std::remove_cvref_t<C>::value}}>;

  // An operand as the operators read it: a wrapper's point, anything else itself.
  template <class T>
  constexpr decltype(auto) operand(const T& x)
  {
    if constexpr (constant_number<T>)
      return point_of_t<T>{};
    else
      return (x);
  }

  // Exactly one operand a wrapper: two follow their own rules.
  template <class L, class R>
  concept constant_operands = constant_number<L> != constant_number<R>;
} // namespace ins::detail

#endif // INSIDE_INSIDE_DETAIL_CONSTANT_HPP
