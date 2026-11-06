// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// test::exact — the exact fraction the tests compute run-time oracles with.
#ifndef BEMAN_INSIDE_TESTS_EXACT_HPP
#define BEMAN_INSIDE_TESTS_EXACT_HPP

#include <beman/inside/inside.hpp>
#include <beman/inside/inside/io.hpp>

#include <compare>
#include <concepts>
#include <ostream>

namespace beman::inside::test {
// An exact fraction for run-time oracles. A rational's arithmetic is compile
// time only, so a test that computes an expected value at run time computes
// it here: fractions of 2048-bit integers, reduced once they pass 768 bits,
// so a few steps on 64-bit values never overflow; compared exactly.
struct exact {
    static constexpr std::size_t limbs = 32;
    using wide                         = detail::wide_rational<limbs>;
    wide V;

    constexpr exact() = default;
    constexpr exact(const wide& v) : V{tidy(v)} {}
    constexpr exact(rational r) : V{detail::exact_of<limbs>(r)} {}
    template <std::integral T>
    constexpr exact(T i) : V{detail::exact_of<limbs>(i)} {}
    template <insidable B>
        requires(detail::exact_limbs<B> < limbs)
    constexpr exact(const B& b) : V{wide{detail::exact_of(b)}} {}

    // The 64-bit rational, which must hold the value.
    explicit constexpr operator rational() const { return detail::try_rational(V).value(); }
    explicit constexpr operator double() const { return static_cast<double>(static_cast<rational>(*this)); }

    friend constexpr exact operator+(const exact& a, const exact& b) { return exact{a.V + b.V}; }
    friend constexpr exact operator-(const exact& a, const exact& b) { return exact{a.V + -b.V}; }
    friend constexpr exact operator*(const exact& a, const exact& b) { return exact{a.V * b.V}; }
    friend constexpr exact operator/(const exact& a, const exact& b) { return exact{a.V / b.V}; } // b != 0
    friend constexpr exact operator-(const exact& a) { return exact{-a.V}; }

    friend constexpr bool                 operator==(const exact& a, const exact& b) { return a.V == b.V; }
    friend constexpr std::strong_ordering operator<=>(const exact& a, const exact& b) { return a.V <=> b.V; }

    // Toward −∞ and toward zero, as integers.
    friend constexpr exact floor(const exact& a) {
        return exact{wide{detail::floor_divmod(a.V.Num, a.V.Den).Quot, detail::wide_sint<limbs>{1}}};
    }
    friend constexpr exact trunc(const exact& a) {
        return exact{wide{detail::trunc(a.V), detail::wide_sint<limbs>{1}}};
    }

    friend std::ostream& operator<<(std::ostream& os, const exact& a) { return os << detail::exact_to_string(a.V); }

  private:
    static constexpr wide tidy(const wide& v) {
        constexpr int cap = 768;
        return bit_width_of(abs(v.Num)) > cap || bit_width_of(v.Den) > cap ? detail::reduced(v)
                                                                           : v; // (ADL: the wide_int friend)
    }
};
} // namespace beman::inside::test

#endif // BEMAN_INSIDE_TESTS_EXACT_HPP
