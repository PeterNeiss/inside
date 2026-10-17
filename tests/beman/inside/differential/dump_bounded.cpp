// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// B: bounded continuous grids. Exact + − × ÷ (A in [-5, 5] with
//    denominators ≤ 7, D in [1, 3] with denominators ≤ 10), then a store of
//    a·d / (d + d²) into [-40, 40] with denominators ≤ 1000 under each mode
//    (the Farey neighbours), and of a double.
//    B a d sum difference product quotient reversed_quotient
//      store_nearest store_floor store_ceil store_trunc store_exact
//      double double_nearest double_floor double_ceil

#include "dump.hpp"

#include <cstdio>

namespace diff {
namespace {
using A  = inside<{{-5, 5}, max_denominator{7}}>;
using D  = inside<{{1, 3}, max_denominator{10}}>;
using Cn = inside<{{-40, 40}, max_denominator{1000}}, round_nearest>;
using Cf = inside<{{-40, 40}, max_denominator{1000}}, round_floor>;
using Cc = inside<{{-40, 40}, max_denominator{1000}}, round_ceil>;
using Cs = inside<{{-40, 40}, max_denominator{1000}}, snap>;
using Ce = inside<{{-40, 40}, max_denominator{1000}}>;

template <class T>
T pick_fraction(rng_t& g, imax lo, imax hi, imax max_den) {
    for (;;) {
        const imax d = 1 + static_cast<imax>(g() % static_cast<std::uint64_t>(max_den));
        const imax k = lo * d + static_cast<imax>(g() % static_cast<std::uint64_t>((hi - lo) * d + 1));
        if (const auto t = T::try_make(rational{k, d}))
            return *t;
    }
}

template <class T, class V>
void store(const V& v) {
    std::cout << ' ';
    out(T::try_make(v));
}
} // namespace

void dump_bounded(rng_t& g, int n) {
    for (int i = 0; i < n; ++i) {
        const A a = pick_fraction<A>(g, -5, 5, 7);
        const D d = pick_fraction<D>(g, 1, 3, 10);
        std::cout << "B " << a << ' ' << d << ' ';
        out(a + d);
        std::cout << ' ';
        out(a - d);
        std::cout << ' ';
        out(a * d);
        std::cout << ' ';
        out(a / d);
        std::cout << ' ';
        out(d / a);
        const auto v = (a * d) / (d + d * d);
        store<Cn>(v);
        store<Cf>(v);
        store<Cc>(v);
        store<Cs>(v);
        store<Ce>(v);
        const double x = static_cast<double>(static_cast<std::int64_t>(g() >> 11)) * 0x1p-48 - 16.0;
        char         hex[64];
        std::snprintf(hex, sizeof hex, "%a", x);
        std::cout << ' ' << hex;
        store<Cn>(x);
        store<Cf>(x);
        store<Cc>(x);
        std::cout << '\n';
    }
}
} // namespace diff
