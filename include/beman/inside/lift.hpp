// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_LIFT_HPP
#define BEMAN_INSIDE_LIFT_HPP

#include <beman/inside/detail/debug.hpp>   // errc

#include <concepts>
#include <expected>
#include <type_traits>
#include <utility>

//---------------------------------------------------------------------------
// lift — monadic composition for `std::expected<T, errc>`. `lift(op, args...)`
// unwraps each expected arg, calls `op`, and re-wraps; the first arg holding an
// error short-circuits with that error. An `op` already returning expected<R>
// is forwarded as-is, so the inner operation reports its own cause. Used by
// interval/grid/inside arithmetic and rational's operators.
//
// std::expected is larger than its value (flag + errc), so it only ever
// travels as a parameter or return value — never as stored state.
//---------------------------------------------------------------------------
namespace beman::inside
{
  namespace detail
  {
    template <class T> struct is_expected : std::false_type {};
    template <class T, class E> struct is_expected<std::expected<T, E>> : std::true_type {};

    template <class T>
    inline constexpr bool is_expected_v = is_expected<std::remove_cvref_t<T>>::value;

    // Any std::expected specialization (cv/ref-stripped).
    template <typename T>
    concept expected_like = is_expected_v<T>;

    // strip expected<X, E> down to X, leave non-expected unchanged
    template <class T> struct unwrap { using type = T; };
    template <class T, class E> struct unwrap<std::expected<T, E>> { using type = T; };
    template <class T> using unwrap_t = typename unwrap<std::remove_cvref_t<T>>::type;

    template <class T>
    constexpr decltype(auto) lift_unwrap(T&& v)
    {
      if constexpr (is_expected_v<T>)
        return *std::forward<T>(v);
      else
        return std::forward<T>(v);
    }

    // Copy an arg's error into `e`; true if the arg holds one.
    template <class T>
    constexpr bool lift_take_error([[maybe_unused]] T const& v, [[maybe_unused]] errc& e)
    {
      if constexpr (is_expected_v<T>)
        if (!v.has_value()) { e = v.error(); return true; }
      return false;
    }
  }

  //---------------------------------------------------------------------------
  // lift(op, args...) — call op on the unwrapped args → expected<result, errc>;
  // the first erroneous arg (left to right) short-circuits with its error. An op
  // already returning expected<R, errc> passes through.
  //---------------------------------------------------------------------------
  template <class Op, class... Args>
  [[nodiscard]] constexpr auto lift(Op op, Args&&... args)
  {
    using R = std::remove_cvref_t<
        decltype(op(detail::lift_unwrap(std::forward<Args>(args))...))>;
    using Ret = std::conditional_t<detail::is_expected_v<R>, R, std::expected<R, errc>>;

    errc e{};
    if ((detail::lift_take_error(args, e) || ...))
      return Ret{std::unexpected{e}};

    if constexpr (detail::is_expected_v<R>)
      return op(detail::lift_unwrap(std::forward<Args>(args))...);
    else
      return Ret{op(detail::lift_unwrap(std::forward<Args>(args))...)};
  }

} // namespace beman::inside

#endif // BEMAN_INSIDE_LIFT_HPP
