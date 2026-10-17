// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// S: a store B::try_make(v) of an exact rational or a double, rounded by
//    B's mode onto its lattice, range checked (or clamped) after rounding;
//    also mul_into<B> / div_into<B> of two grid values (v their exact result).
//    S <mode> <clamp> <lower> <upper> <notch> <v> <result>
//    v is a rational, or a double in hex (0x1.8p+1).

#include "dump.hpp"

#include <cstdio>

namespace diff {
namespace {
template <class B>
void store(rng_t& g, int n, const char* mode, bool clamped, bool ties) {
    // Denominators that put values on midpoints (ties) or never (primes).
    constexpr imax tie_dens[]   = {1, 2, 3, 6, 8, 16, 7919};
    constexpr imax prime_dens[] = {7919, 10007, 65521};
    for (int i = 0; i < n; ++i) {
        const imax d = ties ? tie_dens[g() % 7] : prime_dens[g() % 3];
        const imax k = static_cast<imax>(g() % static_cast<std::uint64_t>(28 * d + 1)) - 14 * d;
        const auto v = rational{k, d};
        std::cout << "S " << mode << ' ' << clamped << ' ' << lower_of<B> << ' ' << upper_of<B> << ' '
                  << notch_of<B> << ' ' << v << ' ';
        out(B::try_make(v));
        std::cout << '\n';
        // The same through a double: its exact binary value.
        const double x = static_cast<double>(static_cast<std::int64_t>(g() >> 11)) * 0x1p-48 - 16.0;
        char         hex[64];
        std::snprintf(hex, sizeof hex, "%a", x);
        std::cout << "S " << mode << ' ' << clamped << ' ' << lower_of<B> << ' ' << upper_of<B> << ' '
                  << notch_of<B> << ' ' << hex << ' ';
        out(B::try_make(x));
        std::cout << '\n';
        // The same through mul_into<B> / div_into<B>: the exact product or
        // quotient of two grid values. mul_into reports a failed store
        // through B's policy (a throw); div_into returns it. Anchored grids
        // only: the products hit ties, which an unanchored grid rounds by slot.
        if (!ties)
            continue;
        using A   = inside<{{-4, 4}, per<4>}>;
        using D   = inside<{{-3, 3}, per<3>}>;
        const A a = pick<A>(g);
        const D e = pick<D>(g);
        std::cout << "S " << mode << ' ' << clamped << ' ' << lower_of<B> << ' ' << upper_of<B> << ' '
                  << notch_of<B> << ' ' << rational{a} * rational{e} << ' ';
        try {
            std::cout << mul_into<B>(a, e);
        } catch (const inside_error& err) {
            std::cout << 'E' << static_cast<int>(err.Code);
        }
        std::cout << '\n';
        if (e != 0) {
            std::cout << "S " << mode << ' ' << clamped << ' ' << lower_of<B> << ' ' << upper_of<B> << ' '
                      << notch_of<B> << ' ' << rational{a} / rational{e} << ' ';
            out(div_into<B>(a, e));
            std::cout << '\n';
        }
    }
}

template <grid G, bool Ties>
void modes(rng_t& g, int n) {
    store<inside<G, round_nearest>>(g, n, "nearest", false, Ties);
    store<inside<G, round_floor>>(g, n, "floor", false, Ties);
    store<inside<G, round_ceil>>(g, n, "ceil", false, Ties);
    store<inside<G, round_half_even>>(g, n, "even", false, Ties);
    store<inside<G, snap>>(g, n, "trunc", false, Ties);
    store<inside<G, round_nearest | clamp>>(g, n, "nearest", true, Ties);
    store<inside<G, round_floor | clamp>>(g, n, "floor", true, Ties);
}
} // namespace

void dump_round(rng_t& g, int n) {
    using r = rational;
    modes<grid{{-10, 10}, per<8>}, true>(g, n);
    modes<grid{{-10, 10}, 1}, true>(g, n);
    modes<grid{{-10, 10}, r{1, 3}}, true>(g, n);
    // Unanchored: Lower off the notch's multiples; prime denominators only
    // (a tie there rounds by the slot, not by the value).
    modes<grid{{r{1, 3}, r{31, 3}}, 1}, false>(g, n);
    modes<grid{{r{-7, 5}, r{53, 5}}, r{1, 2}}, false>(g, n);
}
} // namespace diff
