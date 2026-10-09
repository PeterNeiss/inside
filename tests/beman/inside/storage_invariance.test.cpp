// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// f64 and f32 are storage only: every result — value, rounding, error code,
// plain-or-expected return, ordering type, printing, limits, hash, math — is
// the one the same type gives without them. Each case records everything a
// type does into a trace and requires the three traces to be equal. (Under
// BEMAN_INSIDE_MATH_NO_FP the flags fall back to integer storage; the same
// expectations hold.)

#include <beman/inside/inside.hpp>
#include <beman/inside/cmath.hpp>
#include <beman/inside/io.hpp>
#include <beman/inside/numeric_limits.hpp>

#include <gtest/gtest.h>

#include <compare>
#include <expected>
#include <string>
#include <type_traits>
#include <vector>

#include <version> // __cpp_lib_format
#ifdef __cpp_lib_format
    #include <format>
#endif

using namespace beman::inside;

namespace {
template <class X>
std::string describe(const X& x) {
    if constexpr (detail::is_expected_v<X>)
        return x ? "ok " + describe(*x) : "err " + std::to_string(static_cast<int>(x.error()));
    else if constexpr (insidable<X>) {
        std::string s = to_string(x);
#ifdef __cpp_lib_format
        if constexpr (!(detail::integer_lattice<X> && notch_of<X> != 0 && detail::values_fit_imax<X>))
            s += std::format(" [{:.3f}|{:e}]", x, x);
#endif
        return s;
    } else if constexpr (std::is_same_v<X, std::strong_ordering>)
        return x < 0 ? "lt" : x > 0 ? "gt" : "eq";
    else if constexpr (std::is_same_v<X, bool>)
        return x ? "true" : "false";
    else
        return std::to_string(x);
}

template <class F>
std::string attempt(F f) {
    try {
        return describe(f());
    } catch (const inside_error& e) {
        return "throw " + std::to_string(static_cast<int>(e.Code));
    }
}

template <class R>
std::string shape() {
    return detail::is_expected_v<R> ? "expected" : "plain";
}

const rational    kValues[]  = {rational{0},
                                rational{1},
                                rational{-1},
                                rational{1, 4},
                                rational{3, 8},
                                rational{1, 3},
                                rational{5, 2},
                                rational{-7, 4},
                                rational{4},
                                rational{9, 2},
                                rational{-17, 4},
                                rational{1, 1024},
                                rational{1023, 256},
                                rational{100},
                                rational{101}};
const char* const kTexts[]   = {"0.25", "0.1", "2.5", "-1.75", "7/3", "4", "4.5", "1e30", "x"};
const double      kDoubles[] = {0.25, 0.1, 2.5, -1.75, 1e30, 4.0};

// Everything T does. FP is the storage flag under test (none, f64 or f32),
// given so `_into` outputs can carry it too.
template <grid G, policy_flag P, policy_flag FP>
std::vector<std::string> trace() {
    using T = inside<G, P | FP>;
    std::vector<std::string> log;
    auto                     add = [&](std::string what, std::string out) { log.push_back(what + ": " + out); };

    std::vector<T> on_grid;
    for (const rational& v : kValues) {
        add("try_make", describe(T::try_make(v)));
        add("ctor", attempt([&] { return T{v}; }));
        add("overflows", describe(conversion_overflows<T>(v)));
        add("rounds", describe(conversion_rounds<T>(v)));
        add("checked_cast", attempt([&] { return checked_cast<T>(v); }));
        if (const auto t = T::template try_make<snap | clamp>(v))
            on_grid.push_back(*t);
    }
    for (const double d : kDoubles)
        add("double", describe(T::try_make(d)));
    for (const char* text : kTexts) {
        add("from_chars", describe(from_chars<T>(text)));
        add("from_chars_exact", describe(from_chars_exact<T>(text)));
        add("from_chars<round_floor>", describe(from_chars<T, round_floor>(text)));
    }

    for (const T& a : on_grid) {
        add("hash", describe(std::hash<T>{}(a)));
        add("neg", describe(-a));
        for (const T& b : on_grid) {
            add("+", describe(a + b));
            add("-", describe(a - b));
            add("*", describe(a * b));
            add("/ shape", shape<decltype(a / b)>());
            add("/", attempt([&] { return a / b; }));
            // (% and the deduced math reject a type at compile time by the same
            // rules with or without the flag: integer grids with snap, and a
            // rounding policy.)
            if constexpr (has_flag(P, snap) && detail::integer_lattice<T>)
                add("%", attempt([&] { return a % b; }));
            add("<=>", describe(a <=> b));
            add("<=> int", describe(a <=> 2));
            static_assert(std::is_same_v<decltype(a <=> b), std::strong_ordering>);
            static_assert(std::is_same_v<decltype(a <=> 2.5), std::partial_ordering>);
            add("==", describe(a == b));
            add("+=", attempt([&] {
                    T x = a;
                    x += b;
                    return x;
                }));
            add("-=", attempt([&] {
                    T x = a;
                    x -= b;
                    return x;
                }));
            if constexpr (has_flag(P, snap)) // the product's finer notch needs rounding
                add("*=", attempt([&] {
                        T x = a;
                        x *= b;
                        return x;
                    }));
        }
    }
    // Mixed with the same type without the flag, both ways.
    using T0 = inside<G, P>;
    for (const T& a : on_grid) {
        const T0 p = T0::from_raw(T0::template try_make<snap | clamp>(rational{a}).value().raw());
        add("mixed +", describe(a + p));
        add("mixed /", attempt([&] { return p / a; }));
        add("mixed <=>", describe(a <=> p));
        add("assign from plain", attempt([&] { return T{p}; }));
        add("assign to plain", attempt([&] { return T0{a}; }));
    }
    add("sum", attempt([&] { return beman::inside::sum<T>(on_grid); }));
    using big = inside<{{-1'000'000, 1'000'000}, notch_of<T>}, P | FP>;
    add("sum big", attempt([&] { return beman::inside::sum<big>(on_grid); }));

    using L = std::numeric_limits<T>;
    add("limits",
        describe(L::digits) + " " + describe(L::digits10) + " " + describe(L::is_exact) + " " +
            describe(static_cast<int>(L::round_style)) + " " + describe(L::is_integer) + " " + describe(L::traps));
    add("double conversion", describe(requires(const T& t) { static_cast<double>(t); }));
    add("implicit double", describe(std::is_convertible_v<T, double>));

    // Math: deduced outputs (they must compile alike) and named outputs.
    using O = inside<{{-4, 4}, per<1024>}, round_nearest | FP>;
    for (const T& a : on_grid) {
        add("sin_into", attempt([&] { return math::sin_into<O>(a); }));
        add("atan_into", attempt([&] { return math::atan_into<O>(a); }));
        add("abs", attempt([&] { return math::abs(a); }));
        if constexpr (has_flag(P, snap)) {
            add("sin", attempt([&] { return math::sin(a); }));
            add("sqrt", attempt([&] { return math::sqrt(a); }));
            add("floor", attempt([&] { return math::floor(a); }));
            for (const T& b : on_grid) {
                add("atan2", attempt([&] { return math::atan2(a, b); }));
                add("hypot", attempt([&] { return math::hypot(a, b); }));
                add("fmod", attempt([&] { return math::fmod(a, b); }));
            }
        }
    }
    return log;
}

template <grid G, policy_flag P>
void check() {
    const auto plain  = trace<G, P, none>();
    const auto with64 = trace<G, P, f64>();
    const auto with32 = trace<G, P, f32>();
    ASSERT_EQ(plain.size(), with64.size());
    ASSERT_EQ(plain.size(), with32.size());
    for (std::size_t i = 0; i < plain.size(); ++i) {
        EXPECT_EQ(plain[i], with64[i]) << "f64, entry " << i;
        EXPECT_EQ(plain[i], with32[i]) << "f32, entry " << i;
    }
}

inline constexpr grid quarters{{-4, 4}, per<4>};
inline constexpr grid fine{{0, 16}, per<256>};
inline constexpr grid whole{-100, 100};
} // namespace

#define BEMAN_INSIDE_INVARIANCE(name, g, p) \
    TEST(StorageInvarianceTest, name) { check<g, p>(); }

BEMAN_INSIDE_INVARIANCE(quarters_checked, quarters, checked)
BEMAN_INSIDE_INVARIANCE(quarters_nearest, quarters, round_nearest)
BEMAN_INSIDE_INVARIANCE(quarters_floor, quarters, round_floor)
BEMAN_INSIDE_INVARIANCE(quarters_half_even_clamp, quarters, round_half_even | clamp)
BEMAN_INSIDE_INVARIANCE(quarters_wrap_nearest, quarters, round_nearest | wrap)
BEMAN_INSIDE_INVARIANCE(quarters_snap, quarters, snap)
BEMAN_INSIDE_INVARIANCE(fine_checked, fine, checked)
BEMAN_INSIDE_INVARIANCE(fine_ceil, fine, round_ceil)
BEMAN_INSIDE_INVARIANCE(whole_checked, whole, checked)
BEMAN_INSIDE_INVARIANCE(whole_nearest_clamp, whole, round_nearest | clamp)
BEMAN_INSIDE_INVARIANCE(whole_snap, whole, snap)

#undef BEMAN_INSIDE_INVARIANCE
