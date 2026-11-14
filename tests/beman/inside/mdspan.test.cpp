// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// std::mdspan with insides and quantities (inside/mdspan.hpp,
// units/mdspan.hpp).

#include <beman/inside/inside.hpp>
#include <beman/inside/inside/formats.hpp>
#include <beman/inside/inside/io.hpp>
#include <beman/inside/inside/mdspan.hpp>
#include <beman/inside/math.hpp>
#include <beman/inside/units/io.hpp>
#include <beman/inside/units/mdspan.hpp>

#include "support.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <numeric>
#include <type_traits>
#include <vector>

// std::mdspan with insides (inside/mdspan.hpp): insides on non-negative
// integers as indices of std::mdspan, the std containers and builtin arrays, extents from index types,
// index_for, indices, the value and raw accessors, exact sum / dot / matrix
// products, and transform_into.
namespace mdspan_tests {
using namespace beman::inside;

namespace {
template <class E, std::size_t R>
concept has_extent_index = requires { typename extent_index_t<E, R>; };
template <class A, class I>
concept subscripts = requires(A a, I i) { a[i]; };
template <class M, class I, class J>
concept subscripts2 = requires(M m, I i, J j) { m[i, j]; };
// Whether a builtin operator takes I next to a builtin number: arithmetic,
// compound assignment into the builtin, bit operations, logic. (Subscripts
// and pointer arithmetic are what an index is for.)
template <class I>
concept builtin_operand =
    requires(I i, int n) { i + n; } || requires(I i, int n) { n + i; } || requires(I i, int n) { n - i; } ||
    requires(I i, int n) { i * n; } || requires(I i, int n) { n / i; } || requires(I i, int n) { n % i; } ||
    requires(I i, std::size_t z) { i + z; } || requires(I i, double d) { d + i; } ||
    requires(I i, int n) { n += i; } || requires(I i, int n) { n -= i; } || requires(I i, int n) { n *= i; } ||
    requires(I i, int n) { n /= i; } || requires(I i, int n) { n %= i; } || requires(I i, std::size_t z) { z += i; } ||
    requires(I i, std::size_t z) { z -= i; } || requires(I i, double d) { d += i; } || requires(I i) { i << 2; } ||
    requires(I i, int n) { n << i; } || requires(I i) { i >> 1; } || requires(I i) { i & 1; } ||
    requires(I i) { i | 1; } || requires(I i) { i ^ 1; } || requires(I i) { ~i; } ||
    requires(I i, int n) { n &= i; } || requires(I i, int n) { n |= i; } || requires(I i, int n) { n ^= i; } ||
    requires(I i, int n) { n <<= i; } || requires(I i, int n) { n >>= i; } || requires(I i) { !i; } ||
    requires(I i, bool b) { i && b; } || requires(I i, double d) { i < d; };
template <class T, class I>
concept from_raw_takes = requires(I i) { T::from_raw(i); };
template <class T, class I>
concept assigns_from = requires(T t, I i) { t = i; };
template <class X, class A>
concept steps = requires(X x, A a) { x += a; } && requires(X x, A a) { x -= a; } && requires(X x, A a) { x *= a; };
template <class X, class A>
concept divides = requires(X x, A a) { x /= a; } || requires(X x, A a) { x %= a; };

int at_run_time(int v) {
    volatile int x = v;
    return x;
}

using pixel = inside<{0, 255}>;
using row   = inside<{0, 3}>;
using col   = inside<{0, 4}>;
using img_t = std::mdspan<pixel, extents_of<row, col>>;

std::array<pixel, 20> ramp() {
    std::array<pixel, 20> a{};
    for (std::size_t k = 0; k < a.size(); ++k)
        a[k] = test::store<pixel>(static_cast<int>(k) * 10);
    return a;
}
} // namespace

TEST(MdspanTest, an_index_converts_to_the_integers_that_hold_it) {
    using wide = inside<{0, 300}>;
    static_assert(std::is_convertible_v<row, std::size_t> && std::is_convertible_v<row, int> &&
                  std::is_convertible_v<row, signed char> && std::is_convertible_v<row, std::ptrdiff_t>);
    static_assert(std::is_nothrow_constructible_v<std::size_t, row>);
    // Never narrowing, never to bool, a character or a floating type.
    static_assert(!std::is_convertible_v<wide, unsigned char> && std::is_convertible_v<wide, short>);
    static_assert(!std::is_convertible_v<row, bool> && !std::is_convertible_v<row, char> &&
                  !std::is_convertible_v<row, double> && !std::is_convertible_v<row, float>);
    static_assert(!assigns_from<double, row> && !assigns_from<bool, row> && !assigns_from<char, row>);
    static_assert(assigns_from<int, row> && assigns_from<std::size_t, row>);
    // An index is an inside on non-negative integers, under any policy; a
    // result is one where its grid is.
    static_assert(std::is_convertible_v<inside<{3, 11}, clamp>, int> &&
                  std::is_convertible_v<inside<{0, 9}, unsafe>, int>);
    static_assert(std::is_convertible_v<decltype(row{} + 1_ins), std::size_t>);
    static_assert(std::is_convertible_v<decltype(row{} * row{}), std::size_t>);
    // No index: a grid with a negative value, a fraction, or past int64; a
    // cursor, whose end is one past the last index.
    static_assert(!std::is_convertible_v<inside<{-1, 3}>, int> && !std::is_convertible_v<decltype(-row{}), int>);
    static_assert(!std::is_convertible_v<inside<{{0, 3}, per<2>}>, int>);
    static_assert(!std::is_convertible_v<inside<{{0, 3}, per<2>}, round_nearest>, int>);
    static_assert(!std::is_convertible_v<cursor<row>, std::size_t>);
    // No builtin operator takes an index, but the subscript and the pointer
    // arithmetic behind it.
    static_assert(!builtin_operand<row> && !builtin_operand<inside<{3, 11}, clamp>>);
    static_assert(subscripts<int*, row> && subscripts<int[4], row>);
    // An index is a value, never a raw: from_raw does not take it.
    static_assert(!from_raw_takes<inside<{{0, 255}, per<256>}>, row> && !from_raw_takes<inside<{100, 107}>, row> &&
                  !from_raw_takes<row, row>);
    EXPECT_EQ(static_cast<int>(test::store<row>(at_run_time(3))), 3);
    EXPECT_EQ(std::size_t{wide{300}}, 300u);
    // Builtin arrays and pointers.
    const int  raw[4]{10, 11, 12, 13};
    const row  r = test::store<row>(at_run_time(2));
    const int* p = raw;
    EXPECT_TRUE(raw[r] == 12 && p[r] == 12 && *(p + r) == 12);
}

TEST(MdspanTest, compound_assignment_takes_builtin_integers) {
    using C = inside<{0, 100}, clamp>;
    using W = inside<{0, 359}, wrap>;
    using T = inside<{0, 100}>;
    using F = inside<{{0, 10}, per<10>}, clamp>;
    static_assert(steps<C, int> && steps<W, int> && steps<F, int> && steps<C, long> && steps<C, unsigned> &&
                  steps<C, short>);
    // A checked type names a channel; no double, bool or character; no division.
    static_assert(!steps<T, int> && !steps<C, double> && !steps<C, bool> && !steps<C, char>);
    static_assert(!divides<C, int>);
    int n = at_run_time(7);
    C   c = 50;
    c += 1;
    c *= 2; // 102: clamped
    c -= 3;
    EXPECT_TRUE(c == 97);
    c += n;
    EXPECT_TRUE(c == 100);
    c -= 1000;
    EXPECT_TRUE(c == 0);
    W w = 350;
    w += 20;
    EXPECT_TRUE(w == 10);
    w -= 11;
    EXPECT_TRUE(w == 359);
    w *= 2;
    EXPECT_TRUE(w == 358);
    F f = 0.5_ins;
    f += 1;
    EXPECT_TRUE(f == 1.5_ins);
    f *= 100;
    EXPECT_TRUE(f == 10);
    T    t = 5;
    errc ec{};
    t.policy(ec) += 3;
    EXPECT_TRUE(t == 8 && ec == errc{});
    t.policy(ec) += n * 100;
    EXPECT_TRUE(t == 8 && ec == errc::overflow);
    t.policy<clamp>() *= 1000;
    EXPECT_TRUE(t == 100);
    T u = 1;
    u.on_error([&](auto&, errc, auto) { n = -1; }) -= 5;
    EXPECT_TRUE(u == 1 && n == -1);
}

TEST(MdspanTest, an_index_need_not_start_at_zero) {
    // 3 … 11: positions of a larger array, the value is the position.
    using win = inside<{3, 11}>;
    std::array<int, 12> a{};
    std::iota(a.begin(), a.end(), 0);
    const win              w = test::store<win>(at_run_time(5));
    const std::vector<int> v(a.begin(), a.end());
    const std::span<int>   s{a};
    EXPECT_TRUE(a[w] == 5 && v[w] == 5 && s[w] == 5);
    EXPECT_TRUE(a[win{3}] == 3 && a[win{11}] == 11);
    static_assert(std::is_same_v<extents_of<win>, std::extents<std::size_t, 12>>);
    static_assert(index_for<win, std::extents<std::size_t, 12>, 0>);
    static_assert(!index_for<win, std::extents<std::size_t, 11>, 0>);
    // Even indices only.
    using even = inside<{{0, 10}, 2}>;
    EXPECT_EQ(a[even{4}], 4);
}

TEST(MdspanTest, extents_and_index_types) {
    static_assert(std::is_same_v<extents_of<row, col>, std::extents<std::size_t, 4, 5>>);
    static_assert(std::is_same_v<extents_of<inside<{0, 0}>>, std::extents<std::size_t, 1>>);
    using E = std::extents<std::size_t, 4, std::dynamic_extent, 1>;
    static_assert(std::is_same_v<extent_index_t<E, 0>, row>);
    static_assert(std::is_same_v<extent_index_t<E, 2>, inside<{0, 0}>>);
    static_assert(!has_extent_index<E, 1>); // dynamic
    // index_for: the index type proves the bounds of a static extent.
    using IE = img_t::extents_type;
    static_assert(index_for<row, IE, 0> && index_for<col, IE, 1>);
    static_assert(index_for<row, IE, 1>);                     // 0 … 3 within 5
    static_assert(!index_for<col, IE, 0>);                    // 0 … 4 past 4
    static_assert(!index_for<inside<{-1, 3}>, IE, 0>);        // no index
    static_assert(!index_for<row, IE, 2>);                    // no such rank
    static_assert(!index_for<row, std::dextents<int, 1>, 0>); // dynamic: nothing proven
    static_assert(index_for<extent_index_t<E, 0>, E, 0> && index_for<extent_index_t<E, 2>, E, 2>);
    // Extents from index values.
    const std::dextents<std::size_t, 2> e{row{2}, col{3}};
    EXPECT_TRUE(e.extent(0) == 2 && e.extent(1) == 3);
}

TEST(MdspanTest, indices_order_and_types) {
    std::array<pixel, 20> a = ramp();
    const img_t           img{a.data()};
    int                   n = 0;
    for (auto [r, c] : indices(img)) {
        static_assert(std::is_same_v<decltype(r), row> && std::is_same_v<decltype(c), col>);
        EXPECT_TRUE((img[r, c] == n * 10)); // last index fastest
        ++n;
    }
    EXPECT_EQ(n, 20);
    // A dynamic rank yields plain integers.
    std::vector<int> v(6);
    std::iota(v.begin(), v.end(), 0);
    const std::mdspan dyn{v.data(), std::extents<std::size_t, 2, std::dynamic_extent>{3}};
    int               m = 0;
    for (auto [i, j] : indices(dyn)) {
        static_assert(std::is_same_v<decltype(i), extent_index_t<decltype(dyn)::extents_type, 0>>);
        static_assert(std::is_same_v<decltype(j), std::size_t>);
        EXPECT_EQ((dyn[i, j]), m++);
    }
    EXPECT_EQ(m, 6);
}

TEST(MdspanTest, indexing_std_mdspan) {
    std::array<pixel, 20> a = ramp();
    const img_t           img{a.data()};
    EXPECT_TRUE((img[test::store<row>(at_run_time(2)), test::store<col>(at_run_time(3))]) == 130);
    // Builtin integers mix with indices; arrays and spans of indices work too.
    EXPECT_TRUE((img[std::size_t{2}, std::size_t{3}]) == 130);
    EXPECT_TRUE((img[row{2}, std::size_t{3}]) == 130);
    EXPECT_TRUE((img[std::array{row{2}, row{3}}]) == 130);
    // An inside that is no index is no argument.
    static_assert(subscripts2<img_t, row, col>);
    static_assert(!subscripts2<img_t, inside<{-1, 3}>, col>);
    static_assert(!subscripts2<img_t, inside<{{0, 3}, per<2>}>, col>);
    // A wider value is stored into the index type first, by its policy:
    // clamp pads at the edges, wrap is periodic.
    using crow = inside<{0, 3}, clamp>;
    using wcol = inside<{0, 4}, wrap>;
    EXPECT_TRUE((img[crow{at_run_time(-1)}, col{0}]) == 0);
    EXPECT_TRUE((img[crow{at_run_time(9)}, col{1}]) == 160);
    EXPECT_TRUE((img[row{1}, wcol{at_run_time(-1)}]) == 90);
    EXPECT_TRUE((img[row{1}, wcol{at_run_time(7)}]) == 70);
    const crow r = 0;
    EXPECT_TRUE((img[crow{r - 1_ins}, col{2}]) == 20);
    // An interior index plus an offset is proven within: no policy needed.
    using inner             = inside<{1, 2}>;
    const inner           i = test::store<inner>(at_run_time(2));
    const inside<{-1, 1}> d = test::store<inside<{-1, 1}>>(at_run_time(1));
    EXPECT_TRUE((img[row{i + d}, col{0}]) == 150);
    // Dynamic extents: the index's value, bounds as std::mdspan states them.
    std::array<int, 6> dd{0, 1, 2, 3, 4, 5};
    const std::mdspan  dyn{dd.data(), 2, 3};
    EXPECT_EQ((dyn[inside<{0, 1}>{1}, inside<{0, 2}>{2}]), 5);
#ifdef _GLIBCXX_ASSERTIONS
    // An index type wider than the extent: libstdc++'s own bounds check.
    using wide_row = inside<{0, 10}>;
    EXPECT_DEATH(((void)img[test::store<wide_row>(at_run_time(4)), col{0}]), "");
#endif
}

TEST(MdspanTest, submdspan_takes_indices) {
    std::array<pixel, 20> a = ramp();
    const img_t           img{a.data()};
    const auto            line = std::submdspan(img, row{1}, std::full_extent);
    EXPECT_TRUE((line[col{2}] == 70));
    const auto part = std::submdspan(img, std::full_extent, std::pair{col{1}, col{3}});
    EXPECT_TRUE((part[row{3}, std::size_t{1}] == 170));
    EXPECT_EQ(img.size(), 20u);
}

TEST(MdspanTest, value_accessor) {
    using tenth = inside<{{-10, 10}, per<10>}, round_nearest>;
    const std::array<double, 4> d{0.44, 1.56, 2.5, -0.7};
    const std::mdspan<tenth, std::extents<std::size_t, 4>, std::layout_right, value_accessor<tenth, const double>> v{
        d.data()};
    static_assert(std::is_same_v<decltype(v)::reference, tenth>); // read-only: by value
    EXPECT_TRUE((v[0] == rational{4, 10} && v[1] == rational{16, 10} && v[3] == rational{-7, 10}));
    // A value past the grid is reported by the element's policy.
    using small = inside<{-100, 100}>;
    const std::array<int, 2> big{5, 500};
    const std::mdspan<small, std::extents<std::size_t, 2>, std::layout_right, value_accessor<small, const int>> s{
        big.data()};
    EXPECT_TRUE((s[0] == 5));
    EXPECT_DEATH(((void)small{s[1]}), "beman::inside"); // raw memory read: a precondition
    // Writable: a proxy that reads and writes through the element.
    std::array<std::int16_t, 2> st{0, 0};
    const std::mdspan<small, std::extents<std::size_t, 2>, std::layout_right, value_accessor<small, std::int16_t>> w{
        st.data()};
    w[0] = small{42};
    w[1] = 7;
    EXPECT_EQ(st[0], 42);
    EXPECT_EQ(st[1], 7);
    EXPECT_TRUE((w[0].get() == 42 && small{w[1]} == 7));
    w[1] = w[0];
    EXPECT_EQ(st[1], 42);
}

TEST(MdspanTest, raw_accessor) {
    using adc = inside<{0, 4095}>; // 12 bits in a uint16_t: spare raws
    const std::array<std::uint16_t, 3> raw{100, 4095, 5000};
    const std::mdspan<adc, std::extents<std::size_t, 3>, std::layout_right, raw_accessor<adc, const std::uint16_t>>
        checked_view{raw.data()};
    EXPECT_TRUE((checked_view[0] == 100 && checked_view[1] == 4095));
    EXPECT_DEATH(((void)adc{checked_view[2]}), "beman::inside"); // raw memory read: a precondition
    using adc_clamp = inside<{0, 4095}, clamp>;
    const std::mdspan<adc_clamp,
                      std::extents<std::size_t, 3>,
                      std::layout_right,
                      raw_accessor<adc_clamp, const std::uint16_t>>
        clamped{raw.data()};
    EXPECT_TRUE((clamped[2] == 4095));
    using adc_wrap = inside<{0, 4095}, wrap>;
    const std::
        mdspan<adc_wrap, std::extents<std::size_t, 3>, std::layout_right, raw_accessor<adc_wrap, const std::uint16_t>>
            wrapped{raw.data()};
    EXPECT_TRUE((wrapped[2] == 5000 - 4096));
    // Fixed point: Q8.8 words.
    std::array<std::uint16_t, 2>                                                                 q{384, 0};
    const std::mdspan<q8_8, std::extents<std::size_t, 2>, std::layout_right, raw_accessor<q8_8>> words{q.data()};
    EXPECT_TRUE((words[0].get() == 1.5_r));
    words[1] = q8_8{2.25};
    EXPECT_EQ(q[1], 576);
}

TEST(MdspanTest, sum) {
    std::array<pixel, 20> a = ramp();
    const img_t           img{a.data()};
    const auto            s = sum(img);
    static_assert(grid_of<decltype(s)> == grid{0, 20 * 255});
    EXPECT_TRUE((s == 1900));
    EXPECT_TRUE((sum<inside<{0, 2000}>>(img) == 1900));
    // A strided view sums through the mapping.
    const std::layout_stride::mapping<std::extents<std::size_t, 4>> every_fifth{std::extents<std::size_t, 4>{},
                                                                                std::array<std::size_t, 1>{5}};
    const std::mdspan                                               column0{a.data(), every_fifth};
    EXPECT_TRUE((sum(column0) == 0 + 50 + 100 + 150));
    // An unanchored lattice: 4 values of {0.5, 1.5, …} sum on {2, 3, …}.
    using half = inside<{{0.5, 9.5}, 1}>;
    std::array<half, 4> h{half{0.5}, half{1.5}, half{2.5}, half{9.5}};
    const auto          hs = sum(std::mdspan{h.data(), std::extents<std::size_t, 4>{}});
    static_assert(grid_of<decltype(hs)> == grid{{2, 38}, 1});
    EXPECT_TRUE((hs == 14));
    // Dynamic extents need a target.
    EXPECT_TRUE((sum<inside<{0, 2000}>>(std::mdspan{a.data(), 4, 5}) == 1900));
}

TEST(MdspanTest, dot_and_matrix_products) {
    using v_t             = inside<{-10, 10}>;
    std::array<v_t, 3> x  = {1, 2, 3};
    std::array<v_t, 3> y  = {4, -5, 6};
    const std::mdspan  vx = {x.data(), std::extents<std::size_t, 3>{}};
    const std::mdspan  vy = {y.data(), std::extents<std::size_t, 3>{}};
    const auto         d  = dot(vx, vy);
    static_assert(grid_of<decltype(d)> == grid{-300, 300});
    EXPECT_TRUE((d == 12));
    EXPECT_TRUE((dot<inside<{-1000, 1000}>>(std::mdspan{x.data(), 3}, std::mdspan{y.data(), 3}) == 12));
    EXPECT_DEATH(((void)dot<inside<{-1000, 1000}>>(std::mdspan{x.data(), 3}, std::mdspan{y.data(), 2})),
                 "beman::inside: dot: the extents differ");

    // A = [[1, 2, 3], [4, 5, 6]]: A·x and A·Aᵀ against loops.
    std::array<v_t, 6>                 a{1, 2, 3, 4, 5, 6};
    const std::mdspan                  ma{a.data(), std::extents<std::size_t, 2, 3>{}};
    std::array<inside<{-500, 500}>, 2> ax{};
    matrix_vector_product(ma, vx, std::mdspan{ax.data(), std::extents<std::size_t, 2>{}});
    EXPECT_TRUE((ax[0] == 14 && ax[1] == 32));
    std::array<v_t, 6>                 at{1, 4, 2, 5, 3, 6};
    std::array<inside<{-500, 500}>, 4> c{};
    matrix_product(ma,
                   std::mdspan{at.data(), std::extents<std::size_t, 3, 2>{}},
                   std::mdspan{c.data(), std::extents<std::size_t, 2, 2>{}});
    EXPECT_TRUE((c[0] == 14 && c[1] == 32 && c[2] == 32 && c[3] == 77));
    // The output rounds by its own policy: a sum stored once onto a coarser grid.
    std::array<inside<{{-500, 500}, 10}, round_nearest>, 2> coarse{};
    matrix_vector_product(ma, vx, std::mdspan{coarse.data(), std::extents<std::size_t, 2>{}});
    EXPECT_TRUE((coarse[0] == 10 && coarse[1] == 30));
    // Exact past 64 bits: products of 2^40 accumulate in wide integers.
    using big             = inside<{0, 1ll << 40}>;
    std::array<big, 3> p  = {big{1ll << 40}, big{1ll << 40}, big{1ll << 40}};
    const auto         dp = dot(std::mdspan{p.data(), std::extents<std::size_t, 3>{}},
                                std::mdspan{p.data(), std::extents<std::size_t, 3>{}});
    EXPECT_EQ(to_string(dp), "3626777458843887524118528"); // 3·2^80
}

TEST(MdspanTest, transform_into) {
    using in_t  = inside<{0, 100}>;
    using out_t = inside<{{0, 10}, per<1000>}, round_nearest>;
    std::array<in_t, 4>  in{1, 4, 9, 2};
    std::array<out_t, 4> out{};
    transform_into(std::mdspan{in.data(), 4}, std::mdspan{out.data(), 4}, [](in_t v) { return math::sqrt<out_t>(v); });
    EXPECT_TRUE((out[0] == 1 && out[1] == 2 && out[2] == 3 && out[3] == rational{1414, 1000}));
    // Two inputs, stored by the output's policy (clamp saturates).
    std::array<inside<{0, 150}, clamp>, 4> s{};
    transform_into(std::mdspan{in.data(), 4}, std::mdspan{in.data(), 4}, std::mdspan{s.data(), 4}, [](in_t u, in_t v) {
        return u * v;
    });
    EXPECT_TRUE((s[0] == 1 && s[1] == 16 && s[2] == 81 && s[3] == 4));
    std::array<in_t, 4> big{100, 50, 9, 2};
    transform_into(std::mdspan{big.data(), 4},
                   std::mdspan{big.data(), 4},
                   std::mdspan{s.data(), 4},
                   [](in_t u, in_t v) { return u * v; });
    EXPECT_TRUE((s[0] == 150 && s[1] == 150 && s[2] == 81));
    EXPECT_DEATH((void)transform_into(std::mdspan{in.data(), 4}, std::mdspan{out.data(), 3}, [](in_t v) { return v; }),
                 "beman::inside: transform_into: the extents differ");
}
} // namespace mdspan_tests

// std::mdspan with quantities (units/mdspan.hpp): the value and raw accessors
// over plain buffers, view_in (the same raws read in another unit), and sum,
// dot and the matrix products in the elements' and products' units.
namespace units_mdspan_tests {
using namespace beman::inside;
using namespace beman::inside::units;

namespace {
using metres = quantity<si::metre, inside<{0, 100}>>;
using km     = quantity<si::kilo(si::metre), inside<{0, 10}>>;
using kelvin = quantity<si::kelvin, inside<{{200, 400}, per<10>}>>;
} // namespace

TEST(UnitsMdspanTest, accessors) {
    // Deci-kelvin codes from a sensor, read as kelvin: raw 731 is 273.1 K.
    const std::array<std::uint16_t, 3> codes{0, 731, 2000};
    const std::
        mdspan<kelvin, std::extents<std::size_t, 3>, std::layout_right, raw_accessor<kelvin, const std::uint16_t>>
            t{codes.data()};
    EXPECT_EQ(to_string(t[1]), "273.1 K");
    const auto total = sum(t);
    static_assert(
        std::is_same_v<std::remove_const_t<decltype(total)>, quantity<si::kelvin, inside<{{600, 1200}, per<10>}>>>);
    EXPECT_EQ(to_string(total), "873.1 K");
    // Plain metres from an int array, written back through the quantity.
    std::array<int, 2>                                                                                      raw{3, 0};
    const std::mdspan<metres, std::extents<std::size_t, 2>, std::layout_right, value_accessor<metres, int>> m{
        raw.data()};
    EXPECT_EQ(to_string(m[0].get()), "3 m");
    m[1] = metres{42, si::metre};
    EXPECT_EQ(raw[1], 42);
}

TEST(UnitsMdspanTest, view_in_keeps_the_raw) {
    std::array<km, 2> k{km{1, si::kilo(si::metre)}, km{3, si::kilo(si::metre)}};
    const auto        mm = view_in(std::mdspan{k.data(), std::extents<std::size_t, 2>{}}, si::milli(si::metre));
    EXPECT_EQ(to_string(mm[0].get()), "1000000 mm");
    static_assert(sizeof(km) == 1);
    mm[1] = mm[0];
    EXPECT_EQ(to_string(k[1]), "1 km");
    // A read-only view gives quantities by value.
    const std::array<km, 2> ck{km{2, si::kilo(si::metre)}, km{5, si::kilo(si::metre)}};
    const auto              cm = view_in(std::mdspan{ck.data(), std::extents<std::size_t, 2>{}}, si::metre);
    EXPECT_EQ(to_string(cm[1]), "5000 m");
}

TEST(UnitsMdspanTest, sums_and_products_carry_units) {
    std::array<metres, 4> a{metres{1, si::metre}, metres{2, si::metre}, metres{3, si::metre}, metres{4, si::metre}};
    const std::mdspan     v{a.data(), std::extents<std::size_t, 4>{}};
    EXPECT_EQ(to_string(sum(v)), "10 m");
    // A target in another unit: the exact total in metres, converted once.
    EXPECT_EQ(to_string(*sum<quantity<si::kilo(si::metre), inside<{{0, 1}, per<1000>}>>>(v)), "0.010 km");
    EXPECT_EQ(to_string(dot(v, v)), "30 m²");
    // A matrix of lengths times itself: areas.
    const std::mdspan                                                   m{a.data(), std::extents<std::size_t, 2, 2>{}};
    std::array<quantity<si::metre * si::metre, inside<{0, 100000}>>, 4> c{};
    matrix_product(m, m, std::mdspan{c.data(), std::extents<std::size_t, 2, 2>{}});
    EXPECT_EQ(to_string(c[0]), "7 m²");
    EXPECT_EQ(to_string(c[3]), "22 m²");
    // Lengths times plain numbers stay lengths.
    std::array<inside<{0, 10}>, 2> n{inside<{0, 10}>{2}, inside<{0, 10}>{5}};
    std::array<metres, 2>          w{metres{3, si::metre}, metres{4, si::metre}};
    EXPECT_EQ(to_string(dot(std::mdspan{n.data(), std::extents<std::size_t, 2>{}},
                            std::mdspan{w.data(), std::extents<std::size_t, 2>{}})),
              "26 m");
    // Elementwise: m → m², each stored by the output's policy.
    std::array<quantity<si::metre * si::metre, inside<{0, 100}, clamp>>, 4> sq{};
    transform_into(v, std::mdspan{sq.data(), std::extents<std::size_t, 4>{}}, [](metres x) { return x * x; });
    EXPECT_EQ(to_string(sq[3]), "16 m²");
}
} // namespace units_mdspan_tests

// Arms no other test reaches, found by the reachability audit (gcov and an
// `if constexpr` probe over the suite).
namespace reached_arms_tests {
using namespace beman::inside;
using detail::rational;

// A writable value accessor over doubles writes each value rounded once.
TEST(MdspanArmTest, value_accessor_writes_doubles) {
    using tenth = inside<{{-10, 10}, per<10>}, round_nearest>;
    std::array<double, 2>                                                                                    d{0, 0};
    const std::mdspan<tenth, std::extents<std::size_t, 2>, std::layout_right, value_accessor<tenth, double>> w{
        d.data()};
    w[1] = tenth{rational{-7, 10}};
    EXPECT_EQ(d[1], -0.7);
}

// unsafe passes an out-of-grid raw through unchecked (the caller's promise).
TEST(MdspanArmTest, unsafe_raws_pass_through) {
    using adc = inside<{0, 4095}, unsafe>;
    const std::array<std::uint16_t, 1> raw{5000};
    const std::mdspan<adc, std::extents<std::size_t, 1>, std::layout_right, raw_accessor<adc, const std::uint16_t>> v{
        raw.data()};
    EXPECT_EQ(adc{v[0]}.raw(), 5000);
}

// Extents of different ranks are not compatible; a non-index type has no
// static extent.
TEST(MdspanArmTest, extent_traits) {
    static_assert(!detail::extents_compatible<std::extents<std::size_t, 2>, std::extents<std::size_t, 2, 2>>);
    static_assert(detail::extent_of_index<int> == 0);
}
} // namespace reached_arms_tests

// raw_accessor on raws past int64: uint64's top values, and wide raws held
// by the policy like any other.
namespace wide_raw_tests {
using namespace beman::inside;

template <class N, std::size_t K, class R = typename N::raw_type>
using raw_view = std::mdspan<N, std::extents<std::size_t, K>, std::layout_right, raw_accessor<N, R>>;

TEST(MdspanTest, raw_accessor_past_int64) {
    std::array<std::uint64_t, 3> u{0, 7, 18446744073709551615u};
    const raw_view<uint64, 3>    m{u.data()};
    EXPECT_TRUE(m[2].get() == 18446744073709551615u);
    m[0] = uint64{5};
    EXPECT_EQ(u[0], 5u);

    using WS = inside<{-1180591620717411303424_g, 1180591620717411303424_g}>;
    std::array<WS::raw_type, 2> w{WS{-3}.raw(), WS{1180591620717411303424_g}.raw()};
    const raw_view<WS, 2>       mw{w.data()};
    EXPECT_TRUE(mw[0].get() == -3 && mw[1].get() == WS{1180591620717411303424_g});

    // A wide raw past the grid: clamped by the policy, or reported.
    using WC = inside<{0, 1180591620717411303424_g}, clamp>;
    std::array<WC::raw_type, 1> c{static_cast<WC::raw_type>(~WC::raw_type{0})};
    EXPECT_TRUE((raw_view<WC, 1>{c.data()}[0].get() == WC{1180591620717411303424_g}));
    using WE = inside<{0, 1180591620717411303424_g}>;
    std::array<WE::raw_type, 1> e{static_cast<WE::raw_type>(~WE::raw_type{0})};
    EXPECT_DEATH(((void)raw_view<WE, 1>{e.data()}[0].get()), "beman::inside"); // a precondition
}
} // namespace wide_raw_tests
