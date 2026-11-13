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

// std::mdspan with insides (inside/mdspan.hpp): extents from index types,
// indices, inside_mdspan's indexing by type and by policy, the value and raw
// accessors, exact sum / dot / matrix products, and transform_into.
namespace mdspan_tests {
using namespace beman::inside;

namespace {
template <class E, std::size_t R>
concept has_extent_index = requires { typename extent_index_t<E, R>; };

int at_run_time(int v) {
    volatile int x = v;
    return x;
}

using pixel = inside<{0, 255}>;
using row   = inside<{0, 3}>;
using col   = inside<{0, 4}>;
using img_t = inside_mdspan<pixel, extents_of<row, col>>;

std::array<pixel, 20> ramp() {
    std::array<pixel, 20> a{};
    for (std::size_t k = 0; k < a.size(); ++k)
        a[k] = test::store<pixel>(static_cast<int>(k) * 10);
    return a;
}
} // namespace

TEST(MdspanTest, extents_and_index_types) {
    static_assert(std::is_same_v<extents_of<row, col>, std::extents<std::size_t, 4, 5>>);
    static_assert(std::is_same_v<extents_of<inside<{0, 0}>>, std::extents<std::size_t, 1>>);
    using E = std::extents<std::size_t, 4, std::dynamic_extent, 1>;
    static_assert(std::is_same_v<extent_index_t<E, 0>, row>);
    static_assert(std::is_same_v<extent_index_t<E, 2>, inside<{0, 0}>>);
    static_assert(!has_extent_index<E, 1>); // dynamic
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
        EXPECT_EQ((dyn[static_cast<std::size_t>(i.as<int>()), j]), m++);
    }
    EXPECT_EQ(m, 6);
}

TEST(MdspanTest, indexing_by_type_and_policy) {
    std::array<pixel, 20> a = ramp();
    const img_t           img{a.data()};
    // Proven: the grids lie within the extents.
    EXPECT_TRUE((img[test::store<row>(at_run_time(2)), test::store<col>(at_run_time(3))]) == 130);
    // Builtin integers keep std::mdspan's indexing; they may mix with insides.
    EXPECT_TRUE((img[std::size_t{2}, std::size_t{3}]) == 130);
    EXPECT_TRUE((img[row{2}, std::size_t{3}]) == 130);
    // clamp pads at the edges, wrap is periodic, checked reports.
    using crow = inside<{-5, 10}, clamp>;
    using wcol = inside<{-10, 10}, wrap>;
    EXPECT_TRUE((img[crow{at_run_time(-1)}, col{0}]) == 0);
    EXPECT_TRUE((img[crow{at_run_time(9)}, col{1}]) == 160);
    EXPECT_TRUE((img[row{1}, wcol{at_run_time(-1)}]) == 90);
    EXPECT_TRUE((img[row{1}, wcol{at_run_time(7)}]) == 70);
    using wide_row = inside<{0, 10}>;
    // (an index past the extent is a precondition, reported to the handler)
    EXPECT_DEATH(((void)img[test::store<wide_row>(at_run_time(4)), col{0}]), "beman::inside");
    EXPECT_TRUE((img[test::store<wide_row>(at_run_time(3)), col{0}]) == 150);
    // An offset keeps the clamp policy: r + dr stays on a clamp grid.
    const crow r = 0;
    EXPECT_TRUE((img[r - 1_ins, col{2}]) == 20);
    // Dynamic extents hold every inside index to the run-time extent.
    std::array<int, 6> d{0, 1, 2, 3, 4, 5};
    inside_mdspan      dyn{d.data(), 2, 3};
    static_assert(std::is_same_v<decltype(dyn)::extents_type, std::dextents<std::size_t, 2>>);
    EXPECT_EQ((dyn[inside<{0, 1}>{1}, inside<{0, 2}>{2}]), 5);
    EXPECT_EQ((dyn[inside<{0, 9}, clamp>{at_run_time(7)}, inside<{0, 9}, clamp>{at_run_time(7)}]), 5);
    EXPECT_DEATH(((void)dyn[test::store<inside<{0, 9}>>(at_run_time(2)), inside<{0, 2}>{0}]), "beman::inside");
}

TEST(MdspanTest, wrapper_is_an_mdspan) {
    std::array<pixel, 20>                           a = ramp();
    const img_t                                     img{a.data()};
    const std::mdspan<pixel, extents_of<row, col>>& base = img;
    EXPECT_TRUE((base[std::size_t{3}, std::size_t{4}]) == 190);
    const inside_mdspan from_std{base};
    static_assert(std::is_same_v<decltype(from_std), const img_t>);
    // submdspan of the wrapper: a std::mdspan, wrapped again for inside indices.
    const inside_mdspan sub{std::submdspan(img, 1, std::full_extent)};
    EXPECT_TRUE((sub[inside<{0, 4}>{2}] == 70));
    EXPECT_EQ(img.size(), 20u);
}

TEST(MdspanTest, value_accessor) {
    using tenth = inside<{{-10, 10}, per<10>}, round_nearest>;
    const std::array<double, 4> d{0.44, 1.56, 2.5, -0.7};
    const inside_mdspan<tenth, std::extents<std::size_t, 4>, std::layout_right, value_accessor<tenth, const double>> v{
        d.data()};
    static_assert(std::is_same_v<decltype(v)::reference, tenth>); // read-only: by value
    EXPECT_TRUE((v[0] == rational{4, 10} && v[1] == rational{16, 10} && v[3] == rational{-7, 10}));
    // A value past the grid is reported by the element's policy.
    using small = inside<{-100, 100}>;
    const std::array<int, 2> big{5, 500};
    const inside_mdspan<small, std::extents<std::size_t, 2>, std::layout_right, value_accessor<small, const int>> s{
        big.data()};
    EXPECT_TRUE((s[0] == 5));
    EXPECT_DEATH(((void)small{s[1]}), "beman::inside"); // raw memory read: a precondition
    // Writable: a proxy that reads and writes through the element.
    std::array<std::int16_t, 2> st{0, 0};
    const inside_mdspan<small, std::extents<std::size_t, 2>, std::layout_right, value_accessor<small, std::int16_t>> w{
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
    const inside_mdspan<adc, std::extents<std::size_t, 3>, std::layout_right, raw_accessor<adc, const std::uint16_t>>
        checked_view{raw.data()};
    EXPECT_TRUE((checked_view[0] == 100 && checked_view[1] == 4095));
    EXPECT_DEATH(((void)adc{checked_view[2]}), "beman::inside"); // raw memory read: a precondition
    using adc_clamp = inside<{0, 4095}, clamp>;
    const inside_mdspan<adc_clamp,
                        std::extents<std::size_t, 3>,
                        std::layout_right,
                        raw_accessor<adc_clamp, const std::uint16_t>>
        clamped{raw.data()};
    EXPECT_TRUE((clamped[2] == 4095));
    using adc_wrap = inside<{0, 4095}, wrap>;
    const inside_mdspan<adc_wrap,
                        std::extents<std::size_t, 3>,
                        std::layout_right,
                        raw_accessor<adc_wrap, const std::uint16_t>>
        wrapped{raw.data()};
    EXPECT_TRUE((wrapped[2] == 5000 - 4096));
    // Fixed point: Q8.8 words.
    std::array<std::uint16_t, 2>                                                                   q{384, 0};
    const inside_mdspan<q8_8, std::extents<std::size_t, 2>, std::layout_right, raw_accessor<q8_8>> words{q.data()};
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
    const inside_mdspan<kelvin,
                        std::extents<std::size_t, 3>,
                        std::layout_right,
                        raw_accessor<kelvin, const std::uint16_t>>
        t{codes.data()};
    EXPECT_EQ(to_string(t[1]), "273.1 K");
    const auto total = sum(t);
    static_assert(
        std::is_same_v<std::remove_const_t<decltype(total)>, quantity<si::kelvin, inside<{{600, 1200}, per<10>}>>>);
    EXPECT_EQ(to_string(total), "873.1 K");
    // Plain metres from an int array, written back through the quantity.
    std::array<int, 2> raw{3, 0};
    const inside_mdspan<metres, std::extents<std::size_t, 2>, std::layout_right, value_accessor<metres, int>> m{
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
    std::array<double, 2>                                                                                      d{0, 0};
    const inside_mdspan<tenth, std::extents<std::size_t, 2>, std::layout_right, value_accessor<tenth, double>> w{
        d.data()};
    w[1] = tenth{rational{-7, 10}};
    EXPECT_EQ(d[1], -0.7);
}

// unsafe passes an out-of-grid raw and an out-of-extent index through
// unchecked (the caller's promise).
TEST(MdspanArmTest, unsafe_raws_and_indices_pass_through) {
    using adc = inside<{0, 4095}, unsafe>;
    const std::array<std::uint16_t, 1> raw{5000};
    const inside_mdspan<adc, std::extents<std::size_t, 1>, std::layout_right, raw_accessor<adc, const std::uint16_t>>
        v{raw.data()};
    EXPECT_EQ(adc{v[0]}.raw(), 5000);
    const std::dextents<std::size_t, 1> e{3};
    EXPECT_EQ((detail::extent_index<0>(e, inside<{0, 10}, unsafe>{7})), 7u);
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
using raw_view = inside_mdspan<N, std::extents<std::size_t, K>, std::layout_right, raw_accessor<N, R>>;

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
