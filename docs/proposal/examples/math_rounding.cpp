// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Tour: the output's rounding mode picks the point; exact values are exact;
// compile time and run time agree.
#include <beman/inside/math.hpp>

using namespace beman::inside;

int main() {
    // [listing]
    using x_t  = inside<{{1, 16}, per<64>}, round_nearest>;
    using down = inside<{{0, 16}, per<1024>}, round_floor>;
    using up   = inside<{{0, 16}, per<1024>}, round_ceil>;

    auto lo = math::sqrt_into<down>(x_t{2});         // 1.4140625    = 1448/1024 ≤ √2
    auto hi = math::sqrt_into<up>(x_t{2});           // 1.4150390625 = 1449/1024 ≥ √2
    static_assert(math::sqrt_into<down>(x_t{2.25}) == math::sqrt_into<up>(x_t{2.25})); // 1.5 exactly
    static_assert(math::log2_into<down>(x_t{8}) == 3 && math::log2_into<up>(x_t{8}) == 3);

    using plain = inside<{{1, 16}, per<64>}>;           // no rounding in the type
    auto e = math::exp<round_floor>(plain{1.5});     // the mode named on the call
    constexpr auto at_compile_time = math::exp(x_t{1.5});
    auto           at_run_time     = math::exp(x_t{1.5});
    // at_compile_time == at_run_time, on every platform, with or without an FPU
    // [/listing]
    if (lo != 1.4140625_ins || hi != 1.4150390625_ins || at_compile_time != at_run_time || e > at_run_time)
        return 1;
    return 0;
}
