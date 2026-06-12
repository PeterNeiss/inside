//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDliftHPP
#define BNDliftHPP

#include "slim/optional.hpp"
#include "slim/expected.hpp"

#include <concepts>
#include <type_traits>
#include <utility>

//---------------------------------------------------------------------------
// lift — monadic composition for `slim::optional`.
//
// `lift(op, args...)` unwraps each `slim::optional` arg, calls `op`, and
// re-wraps the result. If any arg is `nullopt`, returns `nullopt` without
// invoking `op`. If `op` already returns `slim::optional<R>`, the result is
// forwarded as-is (no double-wrap). Used pervasively by interval/grid/bound
// arithmetic and by the optional-forwarding operators on `rational`.
//---------------------------------------------------------------------------
namespace bnd
{
  namespace detail
  {
    template <class T> struct is_slim_optional : std::false_type {};
    template <class T> struct is_slim_optional<slim::optional<T>> : std::true_type {};

    template <class T>
    inline constexpr bool is_slim_optional_v =
        is_slim_optional<std::remove_cvref_t<T>>::value;

    // strip slim::optional<X> down to X, leave non-optional unchanged
    template <class T> struct unwrap { using type = T; };
    template <class T> struct unwrap<slim::optional<T>> { using type = T; };
    template <class T> using unwrap_t = typename unwrap<std::remove_cvref_t<T>>::type;

    template <class T>
    constexpr decltype(auto) lift_unwrap(T&& v)
    {
      if constexpr (is_slim_optional_v<T>)
        return *std::forward<T>(v);
      else
        return std::forward<T>(v);
    }

    template <class T>
    constexpr bool lift_engaged(T const& v)
    {
      if constexpr (is_slim_optional_v<T>)
        return v.has_value();
      else
        return true;
    }

    // expected-like: anything with the std::expected access surface — covers
    // both the slim backport and std::expected (which slim::expected aliases
    // on C++23). The error_type requirement keeps bound (has value()) and
    // optional (has has_value()) out.
    template <typename T>
    concept expected_like = requires(std::remove_cvref_t<T> const& e) {
      typename std::remove_cvref_t<T>::value_type;
      typename std::remove_cvref_t<T>::error_type;
      { e.has_value() } -> std::convertible_to<bool>;
      e.error();
      *e;
    };

    // strip expected<X, E> down to X, leave non-expected unchanged
    template <typename T>
    struct expected_value { using type = std::remove_cvref_t<T>; };
    template <typename T> requires expected_like<T>
    struct expected_value<T>
    { using type = typename std::remove_cvref_t<T>::value_type; };
    template <typename T>
    using expected_value_t = typename expected_value<T>::type;

    template <class T>
    constexpr decltype(auto) expected_unwrap(T const& v)
    {
      if constexpr (expected_like<T>) return *v;
      else                            return v;
    }
  }

  //---------------------------------------------------------------------------
  // ok(expected) — deliberately drop the error cause and enter the zero-cost
  // optional world (sentinel encoding, auto-chaining lift operators):
  //     ok(math::tan(x)) * gain + offset      // optional<bound> chain
  // Works for both the C++20 backport and std::expected.
  //---------------------------------------------------------------------------
  template <typename T, typename E>
  [[nodiscard]] constexpr slim::optional<T> ok(slim::expected<T, E> const& e)
  { return e.has_value() ? slim::optional<T>{*e} : slim::optional<T>{slim::nullopt}; }

  //---------------------------------------------------------------------------
  // lift_expected(op, map_nullopt, lhs, rhs)
  //---------------------------------------------------------------------------
  // Binary lift over expected operands: the first (left) error short-circuits;
  // otherwise `op` runs on the unwrapped values. If `op` itself returns an
  // optional (a division chain), its nullopt is mapped to `map_nullopt` —
  // the operator's single documented cause.
  //---------------------------------------------------------------------------
  template <class Op, class E, class L, class R>
  constexpr auto lift_expected(Op op, E map_nullopt, L const& lhs, R const& rhs)
  {
    using Raw = std::remove_cvref_t<
        decltype(op(detail::expected_unwrap(lhs), detail::expected_unwrap(rhs)))>;
    using Out = detail::unwrap_t<Raw>;
    using Ret = slim::expected<Out, E>;

    if constexpr (detail::expected_like<L>)
      if (!lhs.has_value()) return Ret{slim::unexpected{lhs.error()}};
    if constexpr (detail::expected_like<R>)
      if (!rhs.has_value()) return Ret{slim::unexpected{rhs.error()}};

    auto r = op(detail::expected_unwrap(lhs), detail::expected_unwrap(rhs));
    if constexpr (detail::is_slim_optional_v<Raw>)
      return r.has_value() ? Ret{*r} : Ret{slim::unexpected{map_nullopt}};
    else
      return Ret{r};
  }

  //---------------------------------------------------------------------------
  // lift(op, args...)
  //---------------------------------------------------------------------------
  // Calls op on the unwrapped args and returns slim::optional<result>.
  // Returns nullopt if any optional arg is empty. If op already returns a
  // slim::optional<R>, it is forwarded as-is (no double-wrap).
  //---------------------------------------------------------------------------
  template <class Op, class... Args>
  constexpr auto lift(Op op, Args&&... args)
  {
    using R = std::remove_cvref_t<
        decltype(op(detail::lift_unwrap(std::forward<Args>(args))...))>;
    using Ret = std::conditional_t<detail::is_slim_optional_v<R>, R, slim::optional<R>>;

    if (!(detail::lift_engaged(args) && ...))
      return Ret{slim::nullopt};

    if constexpr (detail::is_slim_optional_v<R>)
      return op(detail::lift_unwrap(std::forward<Args>(args))...);
    else
      return Ret{op(detail::lift_unwrap(std::forward<Args>(args))...)};
  }

} // namespace bnd

#endif // BNDliftHPP
