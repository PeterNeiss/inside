// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_DETAIL_DIAG_TEXT_HPP
#define BEMAN_INSIDE_DETAIL_DIAG_TEXT_HPP

#include <beman/inside/detail/config.hpp>

#include <beman/inside/detail/grid_rational.hpp>

#include <cstddef>
#include <string_view>

//---------------------------------------------------------------------------
// Compile-time diagnostics with the values in them. A static_assert
// message can be any constant object with size() and data()
// (P2741), so an error can say "notch 3, Upper − Lower 10". diag_text is
// that object: text and grid numbers appended with <<, in a fixed buffer.
//
//   static_assert(cond, diag_text{} << "grid: the notch must …" << " (notch " << n << ")");
//---------------------------------------------------------------------------
namespace beman::inside::detail {
struct diag_text {
    static constexpr std::size_t capacity = 480;
    char                         Buf[capacity]{};
    std::size_t                  Size{0};

    [[nodiscard]] constexpr std::size_t size() const noexcept { return Size; }
    [[nodiscard]] constexpr const char* data() const noexcept { return Buf; }

    constexpr void push(char c) noexcept {
        if (Size + 1 < capacity)
            Buf[Size++] = c;
    }
    constexpr diag_text& operator<<(std::string_view s) noexcept {
        for (char c : s)
            push(c);
        return *this;
    }
    // A grid number: an integer, or N/D.
    constexpr diag_text& operator<<(const grid_rational& r) {
        append(wide_numerator(r));
        if (const grid_int d = wide_denominator(r); !(d == grid_int{1})) {
            push('/');
            append(d);
        }
        return *this;
    }

    // a − b, in the formatter's wide integers: the message must stay a
    // constant expression even where the grid numbers' own a − b overflows
    // (Clang evaluates a static_assert message when the assertion holds too).
    struct difference {
        grid_rational A;
        grid_rational B;
    };
    constexpr diag_text& operator<<(const difference& d) {
        const grid_int an = wide_numerator(d.A), ad = wide_denominator(d.A);
        const grid_int bn = wide_numerator(d.B), bd = wide_denominator(d.B);
        grid_int       num = an * bd - bn * ad, den = ad * bd;
        const grid_int g = gcd(num, den);
        if (!(g == grid_int{0}) && !(g == grid_int{1})) {
            num = num / g;
            den = den / g;
        }
        append(num);
        if (!(den == grid_int{1})) {
            push('/');
            append(den);
        }
        return *this;
    }

  private:
    constexpr void append(grid_int v) {
        if (v < grid_int{0}) {
            push('-');
            v = -v;
        }
        char        digits[160]{};
        std::size_t n = 0;
        do {
            const grid_int q = v / grid_int{10};
            digits[n++]      = static_cast<char>('0' + static_cast<int>(v - q * grid_int{10}));
            v                = q;
        } while (!(v == grid_int{0}) && n < sizeof digits);
        while (n > 0)
            push(digits[--n]);
    }
};
} // namespace beman::inside::detail

#endif // BEMAN_INSIDE_DETAIL_DIAG_TEXT_HPP
