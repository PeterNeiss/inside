// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Every member of the library's class templates either compiles or is
// constrained away. An explicit instantiation compiles each member whose
// constraints hold, as some compilers do when a program reflects on the class
// (units_parity.test.cpp): a member whose body is ill-formed for some
// arguments (a static_assert body, a missing operation) fails here.

#include <beman/inside/inside.hpp>
#include <beman/inside/units.hpp>

#include <gtest/gtest.h>

using namespace beman::inside;
using namespace beman::inside::units;

namespace {
// A cursor's own policy: flags_of drops the cursor marker.
template <class>
struct policy_of;
template <grid G, policy_flag P>
struct policy_of<inside<G, P>> {
    static constexpr policy_flag value = P;
};
using cur = cursor<inside<{0, 60}, round_floor>, 5>;
using pt  = decltype(point<si::degree_Celsius>(inside<{-50, 50}>{0}));
template <class T>
concept steps = requires(T x) { ++x; };
} // namespace

// Insides: an integer grid, a Q format with a rounding and clamp, a bounded
// continuous grid, a grid past 64 bits, a point, a cursor.
template class beman::inside::inside<grid{0, 100}>;
template class beman::inside::inside<grid{{0, 1}, per<4>}, round_nearest | clamp>;
template class beman::inside::inside<grid{{-1, 1}, max_denominator{8}}, wrap>;
template class beman::inside::inside<grid{{0, 0x1p70}, per<4>}>;
template class beman::inside::inside<grid{7}>;
template class beman::inside::inside<grid_of<cur>, policy_of<cur>::value>;

// Quantities and a point.
template class beman::inside::units::quantity<si::metre, inside<{0, 100}>>;
template class beman::inside::units::quantity<percent, inside<{{0, 100}, per<4>}, round_nearest>>;
template class beman::inside::units::quantity<si::second, cur>;
template class beman::inside::units::quantity_point<pt::reference, pt::origin, pt::rep>;

TEST(ExplicitInstantiationTest, every_member_compiles_or_is_constrained) {
    // The instantiations above are the test; a continuous grid has no ++.
    static_assert(!steps<inside<{{-1, 1}, max_denominator{8}}>>);
    static_assert(steps<inside<{0, 100}>>);
    SUCCEED();
}
