// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Integer / Q-format division and modulo honour the rounding-mode policy.
//
// Historically the native div/mod paths gated only on the `snap` bit and
// always truncated toward zero, silently discarding the round mode (a
// `round_nearest` grid divided like C++ `/`). These pin the corrected
// behaviour: each mode rounds the quotient accordingly, the remainder stays
// consistent (`(a/b)*b + a%b == a`), and the result lands inside its grid.

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

#include <cstdlib>

#include <gtest/gtest.h>

using namespace beman::inside;
using namespace beman::inside::detail;

namespace {
// Reference: round the exact quotient a/b per each mode (integer math).
imax ref_trunc(imax a, imax b) { return a / b; }
imax ref_floor(imax a, imax b) {
    imax t = a / b, r = a % b;
    return (r != 0 && ((a < 0) != (b < 0))) ? t - 1 : t;
}
imax ref_ceil(imax a, imax b) {
    imax t = a / b, r = a % b;
    return (r != 0 && ((a < 0) == (b < 0))) ? t + 1 : t;
}
imax ref_near(imax a, imax b) {
    imax t = a / b, r = a % b;
    if (r == 0)
        return t;
    bool      neg = (a < 0) != (b < 0);
    long long ar = std::llabs(r), ab = std::llabs(b);
    return (2 * ar >= ab) ? (neg ? t - 1 : t + 1) : t;
}
imax ref_heven(imax a, imax b) {
    imax t = a / b, r = a % b;
    if (r == 0)
        return t;
    bool      neg = (a < 0) != (b < 0);
    long long ar = std::llabs(r), ab = std::llabs(b);
    if (2 * ar < ab)
        return t;
    if (2 * ar > ab)
        return neg ? t - 1 : t + 1;
    imax away = neg ? t - 1 : t + 1;
    return (t & 1) == 0 ? t : away;
}
} // namespace

// integer division honours each rounding mode
TEST(DivRoundTest, integer_division_honours_each_rounding_mode) {
    using Nn  = inside<{-100, 100}, round_nearest>;
    using Nnd = inside<{1, 10}, round_nearest>;
    ASSERT_TRUE(static_cast<rational>(Nn{-8} / Nnd{3}) == -3); // round to nearest, not -2
    ASSERT_TRUE(static_cast<rational>(Nn{8} / Nnd{3}) == 3);
    ASSERT_TRUE(static_cast<rational>(Nn{7} / Nnd{2}) == 4); // 3.5 → 4 (half away)
    ASSERT_TRUE(static_cast<rational>(Nn{-7} / Nnd{2}) == -4);

    using Ff  = inside<{-100, 100}, round_floor>;
    using Ffd = inside<{1, 10}, round_floor>;
    ASSERT_TRUE(static_cast<rational>(Ff{-8} / Ffd{3}) == -3); // floor(-2.667)
    ASSERT_TRUE(static_cast<rational>(Ff{8} / Ffd{3}) == 2);   // floor(2.667)

    using Cc  = inside<{-100, 100}, round_ceil>;
    using Ccd = inside<{1, 10}, round_ceil>;
    ASSERT_TRUE(static_cast<rational>(Cc{8} / Ccd{3}) == 3);   // ceil(2.667)
    ASSERT_TRUE(static_cast<rational>(Cc{-8} / Ccd{3}) == -2); // ceil(-2.667)

    using Tt  = inside<{-100, 100}, snap>;
    using Ttd = inside<{1, 10}, snap>;
    ASSERT_TRUE(static_cast<rational>(Tt{-8} / Ttd{3}) == -2); // bare snap = truncate
    ASSERT_TRUE(static_cast<rational>(Tt{8} / Ttd{3}) == 2);
}

// half-even division breaks ties to even
TEST(DivRoundTest, half_even_division_breaks_ties_to_even) {
    using H  = inside<{-100, 100}, round_half_even>;
    using Hd = inside<{1, 10}, round_half_even>;
    ASSERT_TRUE(static_cast<rational>(H{5} / Hd{2}) == 2);   // 2.5 → 2 (even)
    ASSERT_TRUE(static_cast<rational>(H{7} / Hd{2}) == 4);   // 3.5 → 4 (even)
    ASSERT_TRUE(static_cast<rational>(H{-5} / Hd{2}) == -2); // -2.5 → -2
    ASSERT_TRUE(static_cast<rational>(H{9} / Hd{2}) == 4);   // 4.5 → 4
    ASSERT_TRUE(static_cast<rational>(H{3} / Hd{2}) == 2);   // 1.5 → 2
}

// modulo remainder stays consistent with the rounded quotient
TEST(DivRoundTest, modulo_remainder_stays_consistent_with_the_rounded_quotient) {
    // For every mode, (a/b)*b + a%b == a.
    using N  = inside<{-100, 100}, round_nearest>;
    using Nd = inside<{1, 10}, round_nearest>;
    ASSERT_TRUE(static_cast<rational>(N{-8} % Nd{3}) == 1); // -8 = (-3)*3 + 1
    ASSERT_TRUE(static_cast<rational>(N{8} % Nd{3}) == -1); //  8 =   3 *3 - 1

    using F  = inside<{-100, 100}, round_floor>;
    using Fd = inside<{1, 10}, round_floor>;
    ASSERT_TRUE(static_cast<rational>(F{-8} % Fd{3}) == 1); // floored remainder ≥ 0 for b>0
    ASSERT_TRUE(static_cast<rational>(F{8} % Fd{3}) == 2);

    using T  = inside<{-100, 100}, snap>;
    using Td = inside<{1, 10}, snap>;
    ASSERT_TRUE(static_cast<rational>(T{-8} % Td{3}) == -2); // truncated remainder (sign of dividend)
}

// Q-format division honours the rounding mode
TEST(DivRoundTest, q_format_division_honours_the_rounding_mode) {
    using Qn = inside<{{0, 255}, per<256>}, round_nearest>;
    using Qt = inside<{{0, 255}, per<256>}, snap>;
    // 200/3 = 66.6667 → on the 1/256 grid: nearest = 17067/256, truncate = 17066/256.
    ASSERT_TRUE((static_cast<rational>(*(Qn{200.0} / Qn{3.0})) == rational{17067u, 256}));
    ASSERT_TRUE((static_cast<rational>(*(Qt{200.0} / Qt{3.0})) == rational{17066u, 256}));

    // floor / ceil / half_even also honoured (round_uquotient — Lower == 0, so the
    // quotient is non-negative; these arms were previously untested).
    using Qf = inside<{{0, 255}, per<256>}, round_floor>;
    using Qc = inside<{{0, 255}, per<256>}, round_ceil>;
    using Qe = inside<{{0, 255}, per<256>}, round_half_even>;
    ASSERT_TRUE((static_cast<rational>(*(Qf{200.0} / Qf{3.0})) == rational{17066u, 256})); // floor
    ASSERT_TRUE((static_cast<rational>(*(Qc{200.0} / Qc{3.0})) == rational{17067u, 256})); // ceil
    // exact ties on the 1/256 grid: 1.5/256 → 2 (even), 2.5/256 → 2 (even).
    ASSERT_TRUE((static_cast<rational>(*(Qe{rational{3, 256}} / Qe{2.0})) == rational{2u, 256}));
    ASSERT_TRUE((static_cast<rational>(*(Qe{rational{5, 256}} / Qe{2.0})) == rational{2u, 256}));
    ASSERT_TRUE((static_cast<rational>(*(Qn{rational{5, 256}} / Qn{2.0})) == rational{3u, 256})); // nearest tie
}

namespace {
template <class A, class B, imax (*Ref)(imax, imax)>
void sweep_div_mod() {
    for (imax a = lower_imax<A>; a <= upper_imax<A>; ++a)
        for (imax b = lower_imax<B>; b <= upper_imax<B>; ++b) {
            if (b == 0)
                continue;
            A          ba{a};
            B          bb{b};
            const imax q = Ref(a, b);
            SCOPED_TRACE(::testing::Message() << "a=" << a << " b=" << b << " expected q=" << q);
            ASSERT_TRUE(static_cast<rational>(ba / bb) == q);         // quotient per mode
            ASSERT_TRUE(static_cast<rational>(ba % bb) == a - q * b); // remainder a − q·b
        }
}
} // namespace

// div/mod property sweep vs reference, every mode
TEST(DivRoundTest, div_mod_property_sweep_vs_reference_every_mode) {
    // Divisor grids exclude zero so results are plain (non-expected).
    using NA   = inside<{-40, 40}, round_nearest>;
    using ND   = inside<{-7, -1}, round_nearest>; // negative divisors too
    using NPos = inside<{1, 7}, round_nearest>;
    sweep_div_mod<NA, NPos, ref_near>();
    sweep_div_mod<NA, ND, ref_near>();

    using FA  = inside<{-40, 40}, round_floor>;
    using FD  = inside<{1, 7}, round_floor>;
    using FDn = inside<{-7, -1}, round_floor>;
    sweep_div_mod<FA, FD, ref_floor>();
    sweep_div_mod<FA, FDn, ref_floor>();

    using CA = inside<{-40, 40}, round_ceil>;
    using CD = inside<{1, 7}, round_ceil>;
    sweep_div_mod<CA, CD, ref_ceil>();

    using HA = inside<{-40, 40}, round_half_even>;
    using HD = inside<{1, 7}, round_half_even>;
    sweep_div_mod<HA, HD, ref_heven>();

    using TA  = inside<{-40, 40}, snap>;
    using TD  = inside<{1, 7}, snap>;
    using TDn = inside<{-7, -1}, snap>;
    sweep_div_mod<TA, TD, ref_trunc>();
    sweep_div_mod<TA, TDn, ref_trunc>();
}
