// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// F: to<float>() and to<double>(), each rounded once to nearest, ties to
//    even. Half the samples sit within a slot of a float midpoint, where a
//    rounding through double would break the tie.
//    F value float_bits double_hex

#include "dump.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace diff {
namespace {
template <class T>
void floats(rng_t& g, int n) {
    constexpr std::uint64_t mx = beman::inside::detail::max_index_of<T>;
    for (int i = 0; i < n; ++i) {
        T x = pick<T>(g);
        if (i % 2 == 1) {
            // A float midpoint near x, onto T's lattice, then a slot either way.
            const float  f   = static_cast<float>(*x.template to<double>());
            const double mid = (static_cast<double>(f) + static_cast<double>(std::nextafter(f, 1e30f))) / 2;
            if (const auto y = T::template try_make<round_floor>(mid)) {
                std::uint64_t k  = static_cast<std::uint64_t>(y->raw()) -
                                   static_cast<std::uint64_t>(beman::inside::detail::raw_lo<T>);
                const auto    dk = static_cast<std::int64_t>(g() % 5) - 2;
                if (!(dk < 0 && k < static_cast<std::uint64_t>(-dk)))
                    k += static_cast<std::uint64_t>(dk);
                if (k > mx)
                    k = mx;
                x = T::from_raw(beman::inside::detail::raw_from_offset<T>(k));
            }
        }
        const float   f = *x.template to<float>();
        const double  d = *x.template to<double>();
        std::uint32_t bits;
        std::memcpy(&bits, &f, sizeof bits);
        char hex[64];
        std::snprintf(hex, sizeof hex, "%a", d);
        std::cout << "F " << x << ' ' << bits << ' ' << hex << '\n';
    }
}
} // namespace

void dump_float(rng_t& g, int n) {
    floats<inside<{{0, 2}, per<(1ll << 62)>}>>(g, n);
    floats<inside<{{-1, 1}, per<(1ll << 62)>}>>(g, n);
    floats<inside<{{-1'000'000, 1'000'000}, per<1'000'003>}>>(g, n);
    floats<inside<{0, 1ll << 62}>>(g, n);
    floats<inside<{{0, rational{1, 1 << 20}}, per<(1ll << 62)>}>>(g, n);
}
} // namespace diff
