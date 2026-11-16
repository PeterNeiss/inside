// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// The integer fast paths against the exact paths they shortcut: every store,
// conversion and quotient below is computed once by the library's public
// operation (which takes the fast path) and once by the exact wide store or
// exact arithmetic (detail::assign_exact, exact_of), over every value of a
// small grid or a sweep past its limits, under every policy.
#include <beman/inside/inside.hpp>
#include <beman/inside/inside/io.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <cstdio>
#include <cstdint>
#include <expected>
#include <limits>
#include <string>

namespace fast_path_tests {
using namespace beman::inside;
namespace bd = beman::inside::detail;

// The exact store of v into L under flags F: the value, or the error.
template <class L, policy_flag F, class V>
std::expected<L, error> exact_store(const V& v) {
    error ec{};
    L     l{};
    if constexpr (std::floating_point<V>)
        bd::assign_exact<V>(l, bd::exact_of_double(v), make_policy<F>(ec), bd::no_action{});
    else
        bd::assign_exact<V>(l, bd::exact_of(v), make_policy<F>(ec), bd::no_action{});
    if (ec != error{})
        return std::unexpected{ec};
    return l;
}
// The store under test: try_make<F> (checked) or the flag constructor.
template <class L, policy_flag F, class V>
std::expected<L, error> fast_store(const V& v) {
    return L::template try_make<F>(v);
}
template <class V>
std::string text_of(const V& v) {
    if constexpr (std::floating_point<V>) {
        char buf[64];
        std::snprintf(buf, sizeof buf, "%.17g", v);
        return buf;
    } else
        return to_string(bd::as_rational(v));
}
template <class L, policy_flag F, class V>
void expect_same(const V& v, const char* what) {
    const auto f = fast_store<L, F>(v);
    const auto e = exact_store<L, F>(v);
    ASSERT_EQ(f.has_value(), e.has_value()) << what << " v=" << text_of(v);
    if (f) {
        EXPECT_EQ(f->raw(), e->raw()) << what << " v=" << text_of(v);
    } else {
        EXPECT_EQ(f.error(), e.error()) << what;
    }
}

template <policy_flag F>
void integer_sweep() {
    using n60  = inside<{{-600, 600}, 60}>;
    using n72  = inside<{{-7, 14}, just<7, 2>}>;
    using un   = inside<{{1, 601}, 60}>;
    using half = inside<{{just<1, 2>, just<201, 2>}, 1}>;
    using q88  = inside<{{0, 255}, per<256>}>;
    for (std::int64_t n = -1000; n <= 1000; ++n) {
        expect_same<n60, F>(n, "n60");
        expect_same<n72, F>(n, "n72");
        expect_same<un, F>(n, "un");
        expect_same<half, F>(n, "half");
        expect_same<q88, F>(n, "q88");
    }
}

TEST(FastPathTest, integer_stores_match_the_exact_store) {
    integer_sweep<checked>();
    integer_sweep<round_floor>();
    integer_sweep<round_ceil>();
    integer_sweep<round_toward_zero>();
    integer_sweep<round_half_away>();
    integer_sweep<round_half_even>();
    integer_sweep<clamp>();
    integer_sweep<clamp | round_floor>();
    integer_sweep<wrap>();
    integer_sweep<wrap | round_half_even>();
}

template <class L, class R, policy_flag F>
void inside_sweep(const char* what) {
    for (const R r : values<R>)
        expect_same<L, F>(r, what);
}
template <policy_flag F>
void cross_grid() {
    using dec2  = inside<{{-50, 50}, per<100>}>;
    using q88   = inside<{{0, 20}, per<256>}>;
    using n72   = inside<{{-7, 14}, just<7, 2>}>;
    using s32   = inside<{-2000, 2000}>;
    using n60   = inside<{{-600, 600}, 60}>;
    using dec3  = inside<{{-5, 5}, per<1000>}>;
    using q114  = inside<{{-1, 1}, per<16384>}>;
    using small = inside<{-10, 10}>;
    inside_sweep<q88, dec2, F>("dec2->q88");
    inside_sweep<n72, q88, F>("q88->n72");
    inside_sweep<n60, s32, F>("s32->n60");
    inside_sweep<dec3, dec2, F>("dec2->dec3");
    inside_sweep<small, s32, F>("s32->small");
    inside_sweep<q114, q88, F>("q88->q114");
}

TEST(FastPathTest, cross_grid_stores_match_the_exact_store) {
    cross_grid<round_floor>();
    cross_grid<round_ceil>();
    cross_grid<round_half_away>();
    cross_grid<round_half_even>();
    cross_grid<clamp | round_floor>();
    cross_grid<clamp | round_half_even>();
    cross_grid<wrap | round_half_even>();
    cross_grid<wrap | round_toward_zero>();
}

template <policy_flag F>
void double_sweep() {
    using dec2         = inside<{{-1000, 1000}, per<100>}>;
    using dec3         = inside<{{-50, 50}, per<1000>}>;
    using n60          = inside<{{-600, 600}, 60}>;
    using n72          = inside<{{-7, 14}, just<7, 2>}>;
    using un           = inside<{{1, 601}, 60}>;
    using half         = inside<{{just<1, 2>, just<201, 2>}, 1}>;
    using s64          = inside<{-4000000000000000000, 4000000000000000000}>;
    std::uint64_t x    = 0x9e3779b97f4a7c15u;
    auto          next = [&] { return x = x * 6364136223846793005u + 1442695040888963407u; };
    auto          each = [&](double v) {
        expect_same<dec2, F>(v, "dec2");
        expect_same<dec3, F>(v, "dec3");
        expect_same<n60, F>(v, "n60");
        expect_same<n72, F>(v, "n72");
        expect_same<un, F>(v, "un");
        expect_same<half, F>(v, "half");
        expect_same<s64, F>(v, "s64");
    };
    for (int k = -2000; k <= 2000; k += 7) {
        // grid points, ties and their neighbours in double
        for (const double base : {k / 100.0,
                                  (k + 0.5) / 100.0,
                                  k / 1000.0,
                                  (k + 0.5) / 1000.0,
                                  k * 0.5,
                                  k * 7.0 / 4.0,
                                  k * 30.0,
                                  k * 60.0})
            for (const double v : {base, std::nextafter(base, -1e300), std::nextafter(base, 1e300), -base})
                each(v);
    }
    for (int i = 0; i < 4000; ++i) {
        const double u = static_cast<double>(next() >> 11) * 0x1p-53;
        each((u - 0.5) * 2400.0);
        each((u - 0.5) * 9.0e18);
    }
}

TEST(FastPathTest, double_stores_match_the_exact_store) {
    double_sweep<round_floor>();
    double_sweep<round_ceil>();
    double_sweep<round_toward_zero>();
    double_sweep<round_half_away>();
    double_sweep<round_half_even>();
    double_sweep<checked>();
    double_sweep<clamp | round_half_even>();
}

TEST(FastPathTest, a_full_int64_source_clamps_and_wraps) {
    using full  = inside<{std::numeric_limits<std::int64_t>::min(), std::numeric_limits<std::int64_t>::max()}>;
    using small = inside<{-10, 10}, clamp>;
    using ring  = inside<{-10, 10}, wrap>;
    for (const std::int64_t v : {std::numeric_limits<std::int64_t>::min(),
                                 std::int64_t{-11},
                                 std::int64_t{-10},
                                 std::int64_t{0},
                                 std::int64_t{10},
                                 std::int64_t{11},
                                 std::int64_t{12345},
                                 std::numeric_limits<std::int64_t>::max()}) {
        const full f = full::from_raw(v, unsafe);
        EXPECT_EQ((small{f}.raw()), (exact_store<small, clamp>(f)->raw())) << v;
        EXPECT_EQ((ring{f}.raw()), (exact_store<ring, wrap>(f)->raw())) << v;
    }
}

TEST(FastPathTest, the_clamp_hook_hears_the_exact_overshoot) {
    using T = inside<{{0, 100}, per<100>}, clamp>;
    for (const auto a : values<inside<{{0, 100}, per<4>}>>)
        for (const auto b : values<inside<{{0, 100}, per<4>}>>) {
            T                  x{a, clamp};
            const bd::rational sum   = *bd::try_add(bd::as_rational(a), bd::as_rational(b));
            bool               fired = false;
            x.on_clamp([&](auto&, const auto& over) {
                fired = true;
                EXPECT_EQ(double(over), static_cast<double>(*bd::try_sub(sum, bd::rational{100}))); // exact: quarters
            }) += T{b, clamp};
            EXPECT_EQ(fired, sum > bd::rational{100});
        }
}

TEST(FastPathTest, a_wide_product_clamps_to_the_limit) {
    using s64 = inside<{-4000000000000000000, 4000000000000000000}>;
    using c64 = inside<{-4000000000000000000, 4000000000000000000}, clamp>;
    for (const std::int64_t a : {std::int64_t{-3000000000},
                                 std::int64_t{-7},
                                 std::int64_t{0},
                                 std::int64_t{9},
                                 std::int64_t{2000000000},
                                 std::int64_t{3999999999}})
        for (const std::int64_t b :
             {std::int64_t{-4000000000}, std::int64_t{-1}, std::int64_t{3}, std::int64_t{2000000001}}) {
            c64 x = c64::from_raw(a, unsafe);
            x *= c64::from_raw(b, unsafe);
            const auto exact = exact_store<c64, clamp>(s64::from_raw(a, unsafe) * s64::from_raw(b, unsafe));
            EXPECT_EQ(x.raw(), exact->raw()) << a << " * " << b;
        }
}

template <class X>
long long plain(const X& x) {
    if constexpr (requires { *x; })
        return *x;
    else
        return x;
}
template <class T>
void steps_and_values() {
    for (std::int64_t i = -3; i <= static_cast<std::int64_t>(bd::max_index_of<T>) + 3; ++i) {
        const auto f = T::from_step(i);
        if (i < 0 || i > static_cast<std::int64_t>(bd::max_index_of<T>)) {
            EXPECT_EQ(f.error(), error::overflow);
            continue;
        }
        ASSERT_TRUE(f) << i;
        EXPECT_EQ(static_cast<std::int64_t>(f->step()), i);
        const bd::rational v = bd::as_rational(*f);
        EXPECT_EQ(double(*f), static_cast<double>(v));
        EXPECT_EQ(f->template to<float>(), static_cast<float>(bd::nearest_float<double>(bd::exact_of(*f))));
        EXPECT_EQ(bd::rational(bd::to_value(f->numerator()), bd::to_value(f->denominator())), v);
        EXPECT_EQ((plain(to<long long, round_floor>(*f))), bd::floor(v));
        EXPECT_EQ((plain(to<long long, round_ceil>(*f))), bd::ceil(v));
        EXPECT_EQ((plain(to<long long, round_half_away>(*f))), bd::round(v));
        EXPECT_EQ((plain(to<long long, round_toward_zero>(*f))), bd::trunc(v));
    }
}

TEST(FastPathTest, steps_and_conversions_out_of_lattices) {
    steps_and_values<inside<{{-7, 14}, just<7, 2>}>>();
    steps_and_values<inside<{{just<1, 2>, just<201, 2>}, 1}>>();
    steps_and_values<inside<{{1, 601}, 60}>>();
    steps_and_values<inside<{{-2, 2}, per<1000>}>>();
    steps_and_values<inside<{{0, 3}, per<65536>}>>();
}

template <class A, class B>
void divmod_and_onto() {
    using Out = inside<{{-1000, 1000}, per<100>}, round_half_even>;
    for (const A a : values<A>)
        for (const B b : values<B>) {
            const bd::rational ra = bd::as_rational(a), rb = bd::as_rational(b);
            const auto [q, r]     = divmod(a, b, round_floor);
            const std::int64_t qe = bd::floor(*bd::try_div(ra, rb));
            EXPECT_EQ(bd::to_value(q), qe);
            EXPECT_EQ(bd::as_rational(r), *bd::try_sub(ra, *bd::try_mul(bd::rational{qe}, rb)));
            const std::expected<Out, error> p  = mul<Out>(a, b);
            const auto                      pe = exact_store<Out, round_half_even>(a * b);
            ASSERT_EQ(bool(p), bool(pe));
            if (p) {
                EXPECT_EQ(p->raw(), pe->raw());
            }
            const std::expected<Out, error> d  = div<Out>(a, b);
            const auto                      de = exact_store<Out, round_half_even>(a / b);
            ASSERT_EQ(bool(d), bool(de));
            if (d) {
                EXPECT_EQ(d->raw(), de->raw());
            }
        }
}

TEST(FastPathTest, divmod_and_rounded_quotients_on_lattices) {
    divmod_and_onto<inside<{{-3, 3}, per<16>}>, inside<{{just<1, 4>, 2}, per<4>}>>();
    divmod_and_onto<inside<{{-1, 1}, per<100>}>, inside<{1, 9}>>();
    divmod_and_onto<inside<{{-70, 70}, 7}>, inside<{{just<1, 2>, 3}, per<2>}>>();
}

TEST(FastPathTest, exact_cast_and_the_predicates_match_the_exact_store) {
    using n72 = inside<{{-7, 14}, just<7, 2>}>;
    using q88 = inside<{{0, 255}, per<256>}>;
    for (std::int64_t n = -300; n <= 300; ++n) {
        const auto e = exact_store<n72, checked>(n);
        const auto c = exact_cast<n72>(n);
        ASSERT_EQ(bool(c), bool(e)) << n;
        if (!c) {
            EXPECT_EQ(c.error(), e.error()) << n;
        }
        EXPECT_EQ(conversion_overflows<n72>(n), !e && e.error() == error::overflow) << n;
        EXPECT_EQ(conversion_rounds<n72>(n), !e && e.error() == error::rounding_error) << n;
        EXPECT_EQ(conversion_overflows<q88>(n), n < 0 || n > 255) << n;
        EXPECT_FALSE(conversion_rounds<q88>(n)) << n;
    }
}
} // namespace fast_path_tests
