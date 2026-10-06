// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_DETAIL_INT_FOR_BITS_HPP
#define BEMAN_INSIDE_DETAIL_INT_FOR_BITS_HPP

#include <beman/inside/detail/wide_int.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <type_traits>

//---------------------------------------------------------------------------
// int_for_bits — one rule for every integer the library stores or computes
// with: the smallest builtin integer of at least Bits value bits, else a
// wide_int of enough 64-bit limbs. Raws and intermediates both size by it, so
// a grid's bit count fully decides its integer types.
//---------------------------------------------------------------------------
namespace beman::inside::detail {
constexpr std::size_t limbs_for_bits(int bits) noexcept { return static_cast<std::size_t>((bits + 63) / 64); }

// Bits counts value bits; a signed type spends one more on the sign.
template <int Bits, bool Signed>
struct int_for_bits {
    static constexpr int total = Bits + (Signed ? 1 : 0);
    using type                 = std::conditional_t<
        (total <= 8),
        std::conditional_t<Signed, std::int8_t, std::uint8_t>,
        std::conditional_t<
            (total <= 16),
            std::conditional_t<Signed, std::int16_t, std::uint16_t>,
            std::conditional_t<(total <= 32),
                               std::conditional_t<Signed, std::int32_t, std::uint32_t>,
                               std::conditional_t<(total <= 64),
                                                  std::conditional_t<Signed, std::int64_t, std::uint64_t>,
                                                  wide_int<limbs_for_bits(total), Signed>>>>>;
};

template <int Bits, bool Signed>
using int_for_bits_t = typename int_for_bits<Bits, Signed>::type;

// An integer the library may hold in a raw or intermediate: a builtin
// integer or a wide_int. (std::integral cannot be extended to wide_int.)
template <typename T>
concept raw_integer = std::integral<T> || is_wide_int_v<T>;

// Signedness of a raw_integer (std::is_signed_v is false for class types).
template <raw_integer T>
inline constexpr bool raw_signed = std::numeric_limits<T>::is_signed;
} // namespace beman::inside::detail

#endif // BEMAN_INSIDE_DETAIL_INT_FOR_BITS_HPP
