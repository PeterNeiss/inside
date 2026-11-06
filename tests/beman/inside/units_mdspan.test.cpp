// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// std::mdspan with quantities (units/mdspan.hpp): the value and raw accessors
// over plain buffers, view_in (the same raws read in another unit), and sum,
// dot and the matrix products in the elements' and products' units.

#include <beman/inside/inside.hpp>
#include <beman/inside/units/io.hpp>
#include <beman/inside/units/mdspan.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <type_traits>

using namespace beman::inside;
using namespace beman::inside::units;

namespace {
using metres = quantity<si::metre, inside<{0, 100}>>;
using km     = quantity<si::kilo(si::metre), inside<{0, 10}>>;
using kelvin = quantity<si::kelvin, inside<{{200, 400}, per<10>}>>;
} // namespace

TEST(UnitsMdspan, accessors) {
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
    m[1] = metres{42};
    EXPECT_EQ(raw[1], 42);
}

TEST(UnitsMdspan, view_in_keeps_the_raw) {
    std::array<km, 2> k{km{1}, km{3}};
    const auto        mm = view_in(std::mdspan{k.data(), std::extents<std::size_t, 2>{}}, si::milli(si::metre));
    EXPECT_EQ(to_string(mm[0].get()), "1000000 mm");
    static_assert(sizeof(km) == 1);
    mm[1] = mm[0];
    EXPECT_EQ(to_string(k[1]), "1 km");
    // A read-only view gives quantities by value.
    const std::array<km, 2> ck{km{2}, km{5}};
    const auto              cm = view_in(std::mdspan{ck.data(), std::extents<std::size_t, 2>{}}, si::metre);
    EXPECT_EQ(to_string(cm[1]), "5000 m");
}

TEST(UnitsMdspan, sums_and_products_carry_units) {
    std::array<metres, 4> a{metres{1}, metres{2}, metres{3}, metres{4}};
    const std::mdspan     v{a.data(), std::extents<std::size_t, 4>{}};
    EXPECT_EQ(to_string(sum(v)), "10 m");
    // A target in another unit: the exact total in metres, converted once.
    EXPECT_EQ(to_string(sum<quantity<si::kilo(si::metre), inside<{{0, 1}, per<1000>}>>>(v)), "0.010 km");
    EXPECT_EQ(to_string(dot(v, v)), "30 m²");
    // A matrix of lengths times itself: areas.
    const std::mdspan                                                   m{a.data(), std::extents<std::size_t, 2, 2>{}};
    std::array<quantity<si::metre * si::metre, inside<{0, 100000}>>, 4> c{};
    matrix_product(m, m, std::mdspan{c.data(), std::extents<std::size_t, 2, 2>{}});
    EXPECT_EQ(to_string(c[0]), "7 m²");
    EXPECT_EQ(to_string(c[3]), "22 m²");
    // Lengths times plain numbers stay lengths.
    std::array<inside<{0, 10}>, 2> n{inside<{0, 10}>{2}, inside<{0, 10}>{5}};
    std::array<metres, 2>          w{metres{3}, metres{4}};
    EXPECT_EQ(to_string(dot(std::mdspan{n.data(), std::extents<std::size_t, 2>{}},
                            std::mdspan{w.data(), std::extents<std::size_t, 2>{}})),
              "26 m");
    // Elementwise: m → m², each stored by the output's policy.
    std::array<quantity<si::metre * si::metre, inside<{0, 100}, clamp>>, 4> sq{};
    transform_into(v, std::mdspan{sq.data(), std::extents<std::size_t, 4>{}}, [](metres x) { return x * x; });
    EXPECT_EQ(to_string(sq[3]), "16 m²");
}
