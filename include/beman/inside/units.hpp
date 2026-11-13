// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Opt-in: physical units and quantity kinds on inside — the main features of
// mp-units with an inside as the only representation.
//
//   quantity<si::metre, inside<{0, 10}>> d{7, si::metre};   // 7 m: a number and its unit
//   auto k = just<5> * si::kilo(si::metre);                 // 5 km, a point grid
//   auto s = k + d;                                         // 5007 m, exact
//   auto v = d.in(si::milli(si::metre));                    // 7000 mm, the same raw
//   d.numerical_value_in(si::metre);                        // 7: read in a named unit
//   quantity<isq::height[si::metre], H> h{…};               // a quantity kind
//
// A unit conversion is a grid rescale: km → m multiplies the interval and
// notch by 1000 and keeps the raw, so it is exact, total and free. A sum of
// two units runs in their common unit (the gcd of the magnitudes), so it is
// exact too. Products and quotients take inside's result grids.
//
// Compile time: dimensions, magnitudes, units, quantity specs and references
// are structural values combined by consteval functions — no type lists, no
// type-level sorting. The names a user writes (si::metre, isq::height) are
// empty tag objects whose type carries the value.
//
// Magnitudes are exact rationals times a power of π. A π magnitude (degree ↔
// radian) has no exact rescale: it rounds once, through value_cast with a
// target representation and a rounding flag (units/math.hpp).
//
// Also here:
//   quantity_point, point<U>(v), the origins     ℃ and ℉ points, exact shifts
//   quantity_cast, value_cast, try_value_cast    kinds and representations
//   named_unit, quantity_spec, point_origin,     units, kinds, origins and
//   base_dimension, unit_c / spec_c / origin_c   dimensions of your own
//   basis_vector                                 a basis vector of a geometric
//                                                algebra: you state its square
//   q.on_wrap / on_clamp / on_error, ++q, Q::try_make(v, u)
//   mul<C>, div<C>, add_all, mul_all, sum<Q>, divmod,
//   conversion_overflows / rounds / loses<Q>(v, u), Q{v, u, clamp} / Q{q, wrap},
//   dot, cross, lerp                             inside's algorithms
//   percent, per_mille                           dimensionless units
// The catalog: isq, si, si2019, international. The derived physical
// constants are in units/constants.hpp; text (to_string, <<, std::format,
// parse<Q>) in units/io.hpp; multivectors and the products of geometric
// algebra (3 + 4 i, wedge, reverse, inverse, dual) in units/geoalg.hpp.
//
// This umbrella is units/core.hpp (the machinery: units/detail/algebra.hpp,
// quantity.hpp, point.hpp) plus units/systems.hpp (the catalog). For units of
// your own without the SI catalog, include units/core.hpp.
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_UNITS_HPP
#define BEMAN_INSIDE_UNITS_HPP

#include <beman/inside/inside/detail/config.hpp>

#include <beman/inside/units/core.hpp>    // the machinery
#include <beman/inside/units/systems.hpp> // the catalog

#endif // BEMAN_INSIDE_UNITS_HPP
