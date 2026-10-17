// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// This is derived from Peter Sommerlads odins.h, allowed by MIT license
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_DETAIL_OVERFLOW_HPP
#define BEMAN_INSIDE_DETAIL_OVERFLOW_HPP

#include <beman/inside/detail/config.hpp>
BEMAN_INSIDE_WARNINGS_PUSH

#include <concepts>
#include <limits>
#include <type_traits>

//---------------------------------------------------------------------------
// overflow — overflow-detecting add/sub/mul for integers: the GCC/Clang
// __builtin_*_overflow intrinsics. Return true on overflow; *result holds the
// wrapped value either way. Used by rational::*_impl and every checked path.
//---------------------------------------------------------------------------
namespace beman::inside {
#if BEMAN_INSIDE_HAS_OVERFLOW_BUILTINS
template <std::integral T>
[[nodiscard]] constexpr bool add_overflow(T l, T r, T* result) noexcept {
    return __builtin_add_overflow(l, r, result);
}

template <std::integral T>
[[nodiscard]] constexpr bool sub_overflow(T l, T r, T* result) noexcept {
    return __builtin_sub_overflow(l, r, result);
}

template <std::integral T>
[[nodiscard]] constexpr bool mul_overflow(T l, T r, T* result) noexcept {
    return __builtin_mul_overflow(l, r, result);
}
#else
// Portable forms (MSVC): the wrapped value in the unsigned type of at least
// unsigned int's width (no promotion to int), the overflow by sign or bound.
namespace detail {
template <std::integral T>
using overflow_word_t = std::conditional_t<(sizeof(T) < sizeof(unsigned)), unsigned, std::make_unsigned_t<T> >;
}

template <std::integral T>
[[nodiscard]] constexpr bool add_overflow(T l, T r, T* result) noexcept {
    using U = detail::overflow_word_t<T>;
    *result = static_cast<T>(static_cast<U>(static_cast<U>(l) + static_cast<U>(r)));
    if constexpr (std::is_unsigned_v<T>)
        return *result < l;
    else
        return (l < 0) == (r < 0) && (*result < 0) != (l < 0);
}

template <std::integral T>
[[nodiscard]] constexpr bool sub_overflow(T l, T r, T* result) noexcept {
    using U = detail::overflow_word_t<T>;
    *result = static_cast<T>(static_cast<U>(static_cast<U>(l) - static_cast<U>(r)));
    if constexpr (std::is_unsigned_v<T>)
        return l < r;
    else
        return (l < 0) != (r < 0) && (*result < 0) != (l < 0);
}

template <std::integral T>
[[nodiscard]] constexpr bool mul_overflow(T l, T r, T* result) noexcept {
    using U  = detail::overflow_word_t<T>;
    using UT = std::make_unsigned_t<T>;
    *result  = static_cast<T>(static_cast<U>(static_cast<U>(l) * static_cast<U>(r)));
    if (l == 0 || r == 0)
        return false;
    if constexpr (std::is_unsigned_v<T>)
        return l > std::numeric_limits<T>::max() / r;
    else {
        // |l|·|r| against the bound of the result's sign (max, or max + 1).
        const UT   al  = l < 0 ? static_cast<UT>(UT{0} - static_cast<UT>(l)) : static_cast<UT>(l);
        const UT   ar  = r < 0 ? static_cast<UT>(UT{0} - static_cast<UT>(r)) : static_cast<UT>(r);
        const bool neg = (l < 0) != (r < 0);
        const UT   lim = static_cast<UT>(static_cast<UT>(std::numeric_limits<T>::max()) + (neg ? 1 : 0));
        return al > lim / ar;
    }
}
#endif
} // namespace beman::inside

BEMAN_INSIDE_WARNINGS_POP

#endif // BEMAN_INSIDE_DETAIL_OVERFLOW_HPP
