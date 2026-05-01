//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDliftHPP
#define BNDliftHPP

#include "slim/optional.hpp"

#include <type_traits>
#include <utility>

namespace bnd
{
  //---------------------------------------------------------------------------
  // is_slim_optional
  //---------------------------------------------------------------------------
  template <class T> struct is_slim_optional : std::false_type {};
  template <class T> struct is_slim_optional<slim::optional<T>> : std::true_type {};

  template <class T>
  inline constexpr bool is_slim_optional_v =
      is_slim_optional<std::remove_cvref_t<T>>::value;

  namespace detail
  {
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
    using Ret = std::conditional_t<is_slim_optional_v<R>, R, slim::optional<R>>;

    if (!(detail::lift_engaged(args) && ...))
      return Ret{slim::nullopt};

    if constexpr (is_slim_optional_v<R>)
      return op(detail::lift_unwrap(std::forward<Args>(args))...);
    else
      return Ret{op(detail::lift_unwrap(std::forward<Args>(args))...)};
  }

} // namespace bnd

#endif // BNDliftHPP
