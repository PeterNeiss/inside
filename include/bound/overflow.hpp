//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
// This is derived from Peter Sommerlads odins.h, allowed by MIT license
//---------------------------------------------------------------------------
#ifndef BNDoverflowHPP
#define BNDoverflowHPP

#include <cstdint>
#include <type_traits>
#include <limits>
#include <climits>
//#include <compare>
#include <concepts>

#ifdef __has_builtin
  #if __has_builtin(__builtin_add_overflow)
    #define BOUND_HAVE_BUILDIN
  #endif
#endif

#if defined(__clang__)
  #define BOUND_HAVE_BUILDIN
#endif

namespace bnd
{
  template<std::integral T>
  [[nodiscard]] constexpr bool non_builtin_add_overflow(T l, T r, T* result) noexcept
  {
    if constexpr (std::numeric_limits<T>::is_signed)
    {
      if constexpr(sizeof(T) == sizeof(std::int64_t))
      {
        *result = static_cast<T>(static_cast<uint64_t>(l) + static_cast<uint64_t>(r));
        if (l < 0)
          return (r<0) && (*result > l);
        else
          return (r >= 0) && (*result < l);
      }
      else
      {
        std::int64_t res {l};
        res += r;
        *result = static_cast<T>(res);
        return res != *result;
      }
    }
    else
    { // unsigned
      if constexpr(sizeof(T) == sizeof(std::uint64_t))
      {
        *result = l + r;
        return *result < l; // wrapped when true
      }
      else
      {
        std::uint64_t res {l};
        res += r;
        *result = static_cast<T>(res);
        return res != *result;
      }
    }
    return true;
  }

#ifdef BOUND_HAVE_BUILDIN
  template<std::integral T>
  [[nodiscard]]
  constexpr bool add_overflow(T l, T r, T* result) noexcept
  { return __builtin_add_overflow(l,r,result); }
#else // DIY
  template<sized_integer T>
  [[nodiscard]]
  constexpr bool add_overflow(T l, T r, T* result) noexcept
  { return non_builtin::non_builtin_add_overflow(l,r,result); }
#endif
} // namespace bnd

#endif // BNDoverflowHPP
