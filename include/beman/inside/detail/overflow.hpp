// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// This is derived from Peter Sommerlads odins.h, allowed by MIT license
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_DETAIL_OVERFLOW_HPP
#define BEMAN_INSIDE_DETAIL_OVERFLOW_HPP

#include <concepts>

//---------------------------------------------------------------------------
// overflow — overflow-detecting add/sub/mul for integers: the GCC/Clang
// __builtin_*_overflow intrinsics. Return true on overflow; *result holds the
// wrapped value either way. Used by rational::*_impl and every checked path.
//---------------------------------------------------------------------------
namespace beman::inside
{
  template <std::integral T>
  [[nodiscard]] constexpr bool add_overflow(T l, T r, T* result) noexcept
  { return __builtin_add_overflow(l, r, result); }

  template <std::integral T>
  [[nodiscard]] constexpr bool sub_overflow(T l, T r, T* result) noexcept
  { return __builtin_sub_overflow(l, r, result); }

  template <std::integral T>
  [[nodiscard]] constexpr bool mul_overflow(T l, T r, T* result) noexcept
  { return __builtin_mul_overflow(l, r, result); }
} // namespace beman::inside

#endif // BEMAN_INSIDE_DETAIL_OVERFLOW_HPP
