// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_DETAIL_INT_FOR_BITS_HPP
#define BEMAN_INSIDE_DETAIL_INT_FOR_BITS_HPP

#include <beman/inside/detail/config.hpp>

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

template <std::size_t I, class... Ts>
using nth_type_t = Ts...[I];

// Bits counts value bits; a signed type spends one more on the sign.
template <int Bits, bool Signed>
struct int_for_bits {
    static constexpr int         total = Bits + (Signed ? 1 : 0);
    static constexpr std::size_t i     = total <= 8 ? 0 : total <= 16 ? 1 : total <= 32 ? 2 : total <= 64 ? 3 : 4;
    using wide                         = wide_int<limbs_for_bits(total), Signed>;
    using type = std::conditional_t<Signed,
                                    nth_type_t<i, std::int8_t, std::int16_t, std::int32_t, std::int64_t, wide>,
                                    nth_type_t<i, std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t, wide>>;
};

template <int Bits, bool Signed>
using int_for_bits_t = typename int_for_bits<Bits, Signed>::type;
} // namespace beman::inside::detail

#endif // BEMAN_INSIDE_DETAIL_INT_FOR_BITS_HPP
