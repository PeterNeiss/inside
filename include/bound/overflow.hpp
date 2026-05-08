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

#if defined(__has_builtin)
  #if __has_builtin(__builtin_add_overflow) \
   && __has_builtin(__builtin_sub_overflow) \
   && __has_builtin(__builtin_mul_overflow)
    #define BOUND_HAVE_BUILTIN 1
  #endif
#endif

// Older GCC predates __has_builtin but ships these from 5.0.
#if !defined(BOUND_HAVE_BUILTIN) && defined(__GNUC__) && __GNUC__ >= 5
  #define BOUND_HAVE_BUILTIN 1
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
  }

  template<std::integral T>
  [[nodiscard]] constexpr bool non_builtin_mul_overflow(T l, T r, T* result) noexcept
  {
    if constexpr (std::numeric_limits<T>::is_signed)
    {
      if constexpr(sizeof(T) == sizeof(std::int64_t))
      {
        bool resultnegative { (l < 0) != (r < 0) };
        uint64_t res{};
        auto abs64 { [](int64_t value) -> uint64_t { return value < 0? 1ULL + ~static_cast<uint64_t>(value):static_cast<uint64_t>(value);} };
        if (not non_builtin_mul_overflow(abs64(l), abs64(r), &res))
        {
          if (resultnegative)
          {
            if (res <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) + 1ull)
            {
              *result = static_cast<T>(1ULL + ~res); // two's complement
              return false;
            }
          }
          else
          {
            if (res <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))
            {
              *result = static_cast<T>(res);
              return false;
            }
          }
        }
        return true; // overflow
      }
      else
      {
        std::int64_t res {l};
        res *= r;
        *result = static_cast<T>(res);
        return res != *result; // detect overflow bits
      }
    }
    else
    { // unsigned
      if constexpr(sizeof(T) == sizeof(std::uint64_t))
      {
        // compute high-parts and low-parts
        uint64_t lhigh { l >> 32 };
        uint64_t llow { l & 0xffff'ffffULL} ;
        uint64_t rhigh { r >> 32 };
        uint64_t rlow { r & 0xffff'ffffULL} ;
        if (lhigh > 0 && rhigh > 0) return true;
        uint64_t high_low{ lhigh>0 ? lhigh*rlow : rhigh*llow };
        if (high_low >> 32) return true; // overflow
        uint64_t low_low { llow * rlow } ;
        *result = (high_low << 32) + low_low;

        return *result < low_low; // detect overflow
      }
      else
      {
        std::uint64_t res {l};
        res *= r;
        *result = static_cast<T>(res);
        return res != *result;
      }
    }
  }

#ifdef BOUND_HAVE_BUILTIN
  template<std::integral T>
  [[nodiscard]]
  constexpr bool add_overflow(T l, T r, T* result) noexcept
  { return __builtin_add_overflow(l,r,result); }

  template<std::integral T>
  [[nodiscard]]
  constexpr bool sub_overflow(T l, T r, T* result) noexcept
  { return __builtin_sub_overflow(l,r,result); }

  template<std::integral T>
  [[nodiscard]]
  constexpr bool mul_overflow(T l, T r, T* result) noexcept
  { return __builtin_mul_overflow(l,r,result); }
#else // DIY
  template<std::integral T>
  [[nodiscard]]
  constexpr bool add_overflow(T l, T r, T* result) noexcept
  { return non_builtin_add_overflow(l,r,result); }

  template<std::integral T>
  [[nodiscard]]
  constexpr bool sub_overflow(T l, T r, T* result) noexcept
  {
    if constexpr (std::numeric_limits<T>::is_signed)
    {
      if (r == std::numeric_limits<T>::min())
      { // -r would overflow; only safe when l >= 0
        if (l >= 0) return true;
        *result = l - r;
        return false;
      }
      return non_builtin_add_overflow(l, static_cast<T>(-r), result);
    }
    else
    {
      *result = l - r;
      return r > l;
    }
  }

  template<std::integral T>
  [[nodiscard]]
  constexpr bool mul_overflow(T l, T r, T* result) noexcept
  { return non_builtin_mul_overflow(l,r,result); }
#endif
} // namespace bnd

#endif // BNDoverflowHPP
