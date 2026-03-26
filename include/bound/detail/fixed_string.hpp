//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDfixed_stringHPP
#define BNDfixed_stringHPP

#include <cstddef>
#include <string_view>
#include <algorithm>
#include <type_traits>

namespace bnd
{
  //---------------------------------------------------------------------------
  // fixed_string 
  //---------------------------------------------------------------------------
  // N must be the cound of valid character, NOT null terminated
  //---------------------------------------------------------------------------
  template <std::size_t N>
  struct fixed_string 
  {
    char Data[N + 1]{};

    constexpr fixed_string() = default;
    constexpr fixed_string(const char (&s)[N + 1]) 
    {
      // TODO use std::copy
      for (std::size_t i = 0; i <= N; ++i)
          Data[i] = s[i];
    }

    [[nodiscard]] static constexpr std::size_t size() noexcept { return N; }
    [[nodiscard]] static constexpr bool empty() noexcept { return N == 0; }

    [[nodiscard]] constexpr const char* data() const noexcept { return Data; }
    [[nodiscard]] constexpr const char* c_str() const noexcept { return Data; }

    [[nodiscard]] constexpr char  operator[](std::size_t i) const { return Data[i]; }
    [[nodiscard]] constexpr char& operator[](std::size_t i)       { return Data[i]; }

    [[nodiscard]] constexpr const char* begin() const noexcept { return Data; }
    [[nodiscard]] constexpr const char* end()   const noexcept { return Data + N; }

    [[nodiscard]] constexpr operator std::string_view() const noexcept { return {Data, N}; }
    [[nodiscard]] constexpr std::string_view sv() const noexcept { return {Data, N}; }

    template <std::size_t M>
    [[nodiscard]] constexpr bool operator==(const fixed_string<M>& other) const noexcept 
    {
      // TODO remove size check, compare value only
      if constexpr (N != M) 
        return false;
      else 
      {
        // TODO use algorithm
        for (std::size_t i = 0; i < N; ++i)
          if (Data[i] != other.Data[i]) 
            return false;

        return true;
      }
    }

    template <std::size_t M>
    [[nodiscard]] constexpr bool operator!=(const fixed_string<M>& other) const noexcept 
    { return !(*this == other); }

    [[nodiscard]] constexpr bool operator==(std::string_view sv) const noexcept 
    { return std::string_view{Data, N} == sv; }

    [[nodiscard]] constexpr bool operator!=(std::string_view sv) const noexcept 
    { return std::string_view{Data, N} != sv; }
  };

  template <std::size_t N>
  fixed_string(const char (&)[N]) -> fixed_string<N - 1>;

  //---------------------------------------------------------------------------
  // concat 
  //---------------------------------------------------------------------------
  namespace detail 
  {
    template <std::size_t... Ns>
    constexpr auto concat_impl(const fixed_string<Ns>&... parts) 
    {
      constexpr std::size_t total = (Ns + ...);
      fixed_string<total> result;
      std::size_t pos = 0;
      auto append = [&](const auto& part) 
      {
        for (std::size_t i = 0; i < part.size(); ++i)
            result[pos++] = part[i];
      };
      (append(parts), ...);
      result[total] = '\0';
      return result;
    }
  } // namespace detail

  template <std::size_t... Ns>
  [[nodiscard]] constexpr auto concat(const fixed_string<Ns>&... parts) 
  { return detail::concat_impl(parts...); }

  template <std::size_t A, std::size_t B>
  [[nodiscard]] constexpr auto operator+(const fixed_string<A>& lhs, const fixed_string<B>& rhs) 
  { return detail::concat_impl(lhs, rhs); }

// ============================================================================
//  Integer to fixed_string
// ============================================================================

  //---------------------------------------------------------------------------
  // to_fixed_string 
  //---------------------------------------------------------------------------
  namespace detail
  {
    template <auto V>
    constexpr std::size_t digit_count() 
    {
      using T = decltype(V);
      if constexpr (std::is_signed_v<T>) 
      {
        if (V < 0) {
          // +1 for '-' sign; negate as unsigned to avoid UB on INT_MIN
          using U = std::make_unsigned_t<T>;
          U abs = static_cast<U>(0) - static_cast<U>(V);
          std::size_t n = 1; // the '-'
          while (abs > 0) { ++n; abs /= 10; }
          return n;
        }
      }
      if (V == 0) return 1;
      auto tmp = V;
      std::size_t n = 0;
      while (tmp > 0) { ++n; tmp /= 10; }
      return n;
    }
  } // namespace detail

  template <auto V>
      requires std::is_integral_v<decltype(V)>
  [[nodiscard]] constexpr auto to_fixed_string() {
      constexpr std::size_t N = detail::digit_count<V>();
      fixed_string<N> result;

      using T = decltype(V);

      if constexpr (V == 0) {
          result[0] = '0';
      } else if constexpr (std::is_signed_v<T> && V < 0) {
          // negate as unsigned to handle INT_MIN
          using U = std::make_unsigned_t<T>;
          U abs = static_cast<U>(0) - static_cast<U>(V);
          std::size_t i = N;
          while (abs > 0) {
              result[--i] = '0' + static_cast<char>(abs % 10);
              abs /= 10;
          }
          result[0] = '-';
      } else {
          auto val = V;
          std::size_t i = N;
          while (val > 0) {
              result[--i] = '0' + static_cast<char>(val % 10);
              val /= 10;
          }
      }

      result[N] = '\0';
      return result;
  }


  // Storage wrapper: each unique NTTP gets its own static constexpr array.
  //  Since GCC 12 doesn't allow `static constexpr` inside constexpr functions,
  //  we use a struct template to hold the storage.
  template <auto V>
      requires std::is_integral_v<decltype(V)>
  struct int_string_storage {
      static constexpr auto value = to_fixed_string<V>();
  };

  /// Get a string_view for an integral NTTP. Safe — points to static storage.
  template <auto V>
      requires std::is_integral_v<decltype(V)>
  [[nodiscard]] constexpr std::string_view int_to_sv() {
      return int_string_storage<V>::value;
}

} // namespace bnd

#endif // BNDfixed_stringHPP
