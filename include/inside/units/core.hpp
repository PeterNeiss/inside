// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// The units machinery without the catalog:
// dimensions, magnitudes, kinds, units, references, quantity and quantity_point,
// and the tools for units of your own (base_dimension, kind_def, unit_def, origin_def).
// <inside/units.hpp> adds the catalog (units/systems.hpp).
#ifndef INSIDE_UNITS_CORE_HPP
#define INSIDE_UNITS_CORE_HPP

#include <inside/inside/detail/config.hpp>

#include <inside/units/detail/algebra.hpp>
#include <inside/units/detail/quantity.hpp>
#include <inside/units/detail/point.hpp>
#include <inside/units/detail/define.hpp>

#undef INSIDE_UNITS_WHY_PI
#undef INSIDE_UNITS_WHY_GEOALG

#endif // INSIDE_UNITS_CORE_HPP
