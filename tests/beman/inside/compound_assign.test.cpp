// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

#include <gtest/gtest.h>

using namespace beman::inside;
using namespace beman::inside::detail;

// Raw int/float/double are no longer arithmetic-mutation operands: `b += 1`,
// `b *= 2.0` are ill-formed (guidance static_assert in arithmetic.hpp). The only
// non-inside operand a compound assign accepts is a `rational`. As with the binary
// operators, the guidance overloads are SFINAE-transparent (the static_assert
// fires only on a call), so the ill-formedness can't be probed with
// `requires` — only the sanctioned spellings are positively testable.
// compound assignment: sanctioned RHS compiles
TEST(CompoundAssignTest, compound_assignment_sanctioned_rhs_compiles) {
    using u100 = inside<{0, 100}>;
    static_assert(requires(u100 b) { b += 1_ins; });       // an inside
    static_assert(requires(u100 b) { b += rational{1}; }); // a rational
}

// compound assignment: insidable RHS
TEST(CompoundAssignTest, compound_assignment_insidable_rhs) {
    using u100 = inside<{0, 100}>;
    static_assert([] {
        u100 a{50}, d{20};
        a -= d;
        return a.as<imax>();
    }() == 30);

    using ui = inside<{0, 100}, snap>;
    static_assert([] {
        ui d{50}, two{2};
        d *= two;
        return d.as<imax>();
    }() == 100);
    static_assert([] {
        ui e{60}, three{3};
        e /= three;
        return e.as<imax>();
    }() == 20);
    static_assert([] {
        ui f{17}, five{5};
        f %= five;
        return f.as<imax>();
    }() == 2);
}

// compound /= and %= by a zero inside report by default
TEST(CompoundAssignTest, compound_and_by_a_zero_inside_report_by_default) {
    using u100 = inside<{0, 100}>;
    u100 a{50};
    ASSERT_THROW((void)(([&] { a /= u100{0}; }())), beman::inside::inside_error);

    using ui = inside<{0, 100}, snap>;
    ui s{50};
    ASSERT_THROW((void)(([&] { s %= ui{0}; }())), beman::inside::inside_error);
}

// compound assignment: rational RHS
TEST(CompoundAssignTest, compound_assignment_rational_rhs) {
    // round_nearest required so the inside's rational assignment path is available.
    using rn = inside<{{0, 100}, per<100>}, round_nearest>;

    rn a{0.5_r};
    a += 0.25_r; // 0.50 + 0.25 = 0.75
    ASSERT_EQ(a, rn{0.75_r});

    a -= 0.25_r; // 0.75 − 0.25 = 0.50
    ASSERT_EQ(a, rn{0.5_r});

    a *= 4_r; // 0.50 × 4 = 2
    ASSERT_EQ(a, rn{2_r});

    a /= 2_r; // 2 / 2 = 1
    ASSERT_EQ(a, rn{1_r});
}

// compound assignment: fractional RHS via rational
TEST(CompoundAssignTest, compound_assignment_fractional_rhs_via_rational) {
    using rn = inside<{{-100, 100}, per<16>}, round_nearest>;

    rn a{1.0}; // construction from a double is unchanged
    a += 2.5_r;
    ASSERT_EQ(a, rn{3.5});

    a -= 1.5_r;
    ASSERT_EQ(a, rn{2.0});

    a *= 1.5_r;
    ASSERT_EQ(a, rn{3.0});

    a /= 2_r;
    ASSERT_EQ(a, rn{1.5});
}

// compound /= 0_r (rational zero) reports error
TEST(CompoundAssignTest, compound_0_r_rational_zero_reports_error) {
    using rn = inside<{{0, 100}, per<100>}, round_nearest>;
    rn a{0.5_r};
    ASSERT_THROW((void)(([&] { a /= 0_r; }())), beman::inside::inside_error);
}

// increment / decrement
TEST(CompoundAssignTest, increment_decrement) {
    using u10 = inside<{0, 10}>;
    // The ++/-- happens inside each lambda body, fully sequenced before the lambda
    // returns; the `== N` compares the lambda's *result*, not a mutated operand.
    static_assert([] {
        u10 a{5};
        ++a;
        return a.as<imax>();
    }() == 6);
    static_assert([] {
        u10 a{5};
        a++;
        return a.as<imax>();
    }() == 6);
    static_assert([] {
        u10 a{5};
        --a;
        return a.as<imax>();
    }() == 4);
    static_assert([] {
        u10 a{5};
        a--;
        return a.as<imax>();
    }() == 4);

    // post-inc/dec returns the old value
    static_assert([] {
        u10  a{5};
        auto r = a++;
        return r.as<imax>();
    }() == 5);
    static_assert([] {
        u10  a{5};
        auto r = a--;
        return r.as<imax>();
    }() == 5);
}

//---------------------------------------------------------------------------
// operator-= raw fast path (perf): with equal notches `-=` subtracts raws
// directly (index rhs debits a compile-time Lower/Notch bias) instead of
// delegating to `+= (-rhs)`. These cases pin value equality with the binary
// route and the policy tail at both raw edges. [perf-paths]
//---------------------------------------------------------------------------
// compound -=: raw fast path agrees with binary subtraction
TEST(CompoundAssignTest, compound_raw_fast_path_agrees_with_binary_subtraction) {
    // Q8.8 — index storage both sides, bias 0.
    using q88 = inside<{{0, 255}, per<256>}, snap>;
    for (int whole : {0, 1, 100, 255})
        for (int sub : {0, 1, 55, 100}) {
            if (whole - sub < 0)
                continue;
            q88 lhs{whole};
            lhs -= q88{sub};
            ASSERT_EQ(rational{lhs}, rational{whole - sub});
        }

    // fractional raws on the same grid
    q88 frac_lhs{rational{771, 256}};    // 3 + 3/256
    frac_lhs -= q88{rational{515, 256}}; // 2 + 3/256
    ASSERT_EQ(rational{frac_lhs}, rational{1});

    // offset index grid (negative Lower): bias = Lower/Notch = -8.
    using off = inside<{{-2, 2}, per<4>}, snap>;
    off offset_lhs{rational{3, 4}};
    offset_lhs -= off{rational{-1, 2}}; // 3/4 − (−1/2) = 5/4
    ASSERT_EQ(rational{offset_lhs}, (rational{5, 4}));

    // mixed storages on the same unit notch: [1000,2000] stores an unsigned
    // index (Lower-relative raw), [-3000,3000] stores the value directly.
    using hi   = inside<{1000, 2000}, snap>;
    using lo   = inside<{0, 500}, snap>;
    using wide = inside<{-3000, 3000}, snap>;
    hi index_lhs{1500}; // index-raw lhs, value-raw rhs
    index_lhs -= lo{300};
    ASSERT_EQ(rational{index_lhs}, rational{1200});
    wide value_lhs{1500};  // value-raw lhs, index-raw rhs:
    value_lhs -= hi{1200}; // bias = lower_of<R>/Notch = 1000
    ASSERT_EQ(rational{value_lhs}, rational{300});
}

// compound -=: policy tail at the raw edges
TEST(CompoundAssignTest, compound_policy_tail_at_the_raw_edges) {
    using q_clamp = inside<{{0, 255}, per<256>}, clamp | snap>;
    q_clamp clamped{1};
    clamped -= q_clamp{100}; // 1 − 100 < 0 -> clamps to Lower
    ASSERT_EQ(rational{clamped}, rational{0});

    using q_wrap = inside<{{0, 3}, per<4>}, wrap | snap>;
    q_wrap wrapped{0};
    wrapped -= q_wrap{rational{1, 4}}; // 0 − 1/4 wraps to Upper − ... = 3
    ASSERT_EQ(rational{wrapped}, rational{3});

    using q_checked = inside<{{0, 255}, per<256>}, checked>;
    q_checked reported{1};
    ASSERT_THROW((void)(([&] { reported -= q_checked{100}; }())), beman::inside::inside_error);

    // in-range edge stays exact under every policy
    q_clamp exact_edge{255};
    exact_edge -= q_clamp{255};
    ASSERT_EQ(rational{exact_edge}, rational{0});
}

// compound -=: non-fast storages still route through += (-rhs)
TEST(CompoundAssignTest, compound_non_fast_storages_still_route_through_plus_rhs) {
    // rational raw falls back and stays exact
    using ex = inside<{{0, 4}, per<3>}, exact | round_nearest>;
    ex exact_lhs{rational{7, 3}};
    exact_lhs -= ex{rational{2, 3}};
    ASSERT_EQ(rational{exact_lhs}, (rational{5, 3}));

    // a signed dyadic grid
    using rl = inside<{{-4, 4}, per<256>}, round_nearest>;
    rl dy_lhs{rational{3, 2}};
    dy_lhs -= rl{rational{1, 4}};
    ASSERT_EQ(rational{dy_lhs}, (rational{5, 4}));

    // cross-notch operands take the binary route
    using tenths   = inside<{{0, 10}, per<10>}, round_nearest>;
    using quarters = inside<{{0, 10}, per<4>}, round_nearest>;
    tenths cross{rational{5, 2}};      // 2.5 on the 1/10 grid
    cross -= quarters{rational{1, 2}}; // 2.0 — exact on both grids
    ASSERT_EQ(rational{cross}, rational{2});
}
