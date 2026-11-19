// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INSIDE_INSIDE_DETAIL_LIFT_HPP
#define INSIDE_INSIDE_DETAIL_LIFT_HPP

#include <inside/inside/detail/config.hpp>

#include <inside/inside/detail/error.hpp> // error

#include <concepts>
#include <expected>
#include <type_traits>
#include <utility>

// Monadic composition for std::expected<T, error>:
// lift(op, args...) unwraps each expected argument, calls op and wraps the result again;
// the first argument (left to right) holding an error short-circuits with it.
// An op that returns an expected already is forwarded as is, so the inner operation reports its own cause.
// Internal: it backs grid division and the expected-lift operators, which user code chains through.
// A std::expected is larger than its value (a flag and an error),
// so it only travels as a parameter or return value, never as stored state.
namespace ins::detail
{
  template <class T>
  struct is_expected : std::false_type {};
  template <class T, class E>
  struct is_expected<std::expected<T, E>> : std::true_type {};

  template <class T>
  inline constexpr bool is_expected_v = is_expected<std::remove_cvref_t<T>>::value;

  // Any std::expected specialization (without cv and ref).
  template <typename T>
  concept expected_like = is_expected_v<T>;

  // expected<X, E> stripped to X; anything else unchanged.
  template <class T>
  struct unwrap
  {
    using type = T;
  };
  template <class T, class E>
  struct unwrap<std::expected<T, E>>
  {
    using type = T;
  };
  template <class T>
  using unwrap_t = typename unwrap<std::remove_cvref_t<T>>::type;

  template <class T>
  constexpr decltype(auto) lift_unwrap(T&& v)
  {
    if constexpr (expected_like<T>)
      return *std::forward<T>(v);
    else
      return std::forward<T>(v);
  }

  // Copies an argument's error into e; true if it holds one.
  template <class T>
  constexpr bool lift_take_error([[maybe_unused]] const T& v, [[maybe_unused]] error& e)
  {
    if constexpr (expected_like<T>)
      if (!v.has_value())
      {
        e = v.error();
        return true;
      }
    return false;
  }

  template <class Op, class... Args>
  [[nodiscard]] constexpr auto lift(Op op, Args&&... args)
  {
    using R   = std::remove_cvref_t<decltype(op(lift_unwrap(std::forward<Args>(args))...))>;
    using Ret = std::conditional_t<is_expected_v<R>, R, std::expected<R, error>>;

    error e{};
    if ((lift_take_error(args, e) || ...))
      return Ret{std::unexpected{e}};

    if constexpr (expected_like<R>)
      return op(lift_unwrap(std::forward<Args>(args))...);
    else
      return Ret{op(lift_unwrap(std::forward<Args>(args))...)};
  }
} // namespace ins::detail

#endif // INSIDE_INSIDE_DETAIL_LIFT_HPP
