// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// P: value_cast across a factor of π (units_math.hpp), rounded once onto
//    the output grid by its mode: degrees → radians and back.
//    P <mode> <factor> <out_notch> <in> <out>
//    factor is "deg2rad" (x·π/180) or "rad2deg" (x·180/π).

#include "dump.hpp"

#include <beman/inside/units_math.hpp>

namespace diff {
namespace {
using namespace beman::inside::units;

template <class Out>
void deg_to_rad(rng_t& g, int n, const char* mode) {
    using D = inside<{{-720, 720}, per<1000>}>;
    for (int i = 0; i < n; ++i) {
        const quantity<si::degree, D> q{pick<D>(g)};
        std::cout << "P " << mode << " deg2rad " << notch_of<Out> << ' ' << q.numerical_value() << ' '
                  << value_cast<si::radian, Out>(q).numerical_value() << '\n';
    }
}

template <class Out>
void rad_to_deg(rng_t& g, int n, const char* mode) {
    using R = inside<{{-13, 13}, per<1'000'003>}>;
    for (int i = 0; i < n; ++i) {
        const quantity<si::radian, R> q{pick<R>(g)};
        std::cout << "P " << mode << " rad2deg " << notch_of<Out> << ' ' << q.numerical_value() << ' '
                  << value_cast<si::degree, Out>(q).numerical_value() << '\n';
    }
}
} // namespace

void dump_pi(rng_t& g, int n) {
    deg_to_rad<inside<{{-13, 13}, per<(1ll << 40)>}, round_nearest>>(g, n, "nearest");
    deg_to_rad<inside<{{-13, 13}, per<(1ll << 40)>}, round_floor>>(g, n, "floor");
    deg_to_rad<inside<{{-13, 13}, per<1'000'003>}, round_ceil>>(g, n, "ceil");
    rad_to_deg<inside<{{-800, 800}, per<7>}, round_half_even>>(g, n, "even");
    rad_to_deg<inside<{{-800, 800}, per<1000>}, round_nearest>>(g, n, "nearest");
}
} // namespace diff
