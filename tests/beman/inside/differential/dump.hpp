// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// The differential oracle's dumpers: each family prints random cases as
// text lines (a tag, the operands, the results), and check.py recomputes
// every result exactly (Python Fraction, π to 200 digits). Values print
// through inside's own operator<<, which is exact (a decimal or N/D).

#ifndef BEMAN_INSIDE_TESTS_DIFFERENTIAL_DUMP_HPP
#define BEMAN_INSIDE_TESTS_DIFFERENTIAL_DUMP_HPP

#include <beman/inside/inside.hpp>
#include <beman/inside/inside/io.hpp>

#include <cstdint>
#include <iostream>
#include <random>

namespace diff {
using namespace beman::inside;

using rng_t = std::mt19937_64;

// A raw slot of T: uniform, or (one in four) within three slots of either end.
template <class T>
T pick(rng_t& g) {
    constexpr std::uint64_t mx = beman::inside::detail::max_index_of<T>;
    std::uint64_t           k  = mx == ~std::uint64_t{0} ? g() : g() % (mx + 1);
    if (g() % 4 == 0)
        k = (g() % 2) ? mx - g() % 3 : g() % 3;
    if (k > mx)
        k = mx;
    return T::from_raw(beman::inside::detail::raw_from_offset<T>(k));
}

// A value or an expected: the value, or E<code>; an operation the types do
// not offer prints N.
template <class E>
void out(const E& e) {
    if constexpr (requires { e.has_value(); }) {
        if (e)
            std::cout << *e;
        else
            std::cout << 'E' << static_cast<int>(e.error());
    } else
        std::cout << e;
}

void dump_arith(rng_t& g, int n);
void dump_round(rng_t& g, int n);
void dump_bounded(rng_t& g, int n);
void dump_float(rng_t& g, int n);
void dump_pi(rng_t& g, int n);
} // namespace diff

#endif // BEMAN_INSIDE_TESTS_DIFFERENTIAL_DUMP_HPP
