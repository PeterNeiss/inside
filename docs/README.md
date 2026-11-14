# Documentation

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

The pages come in three levels. Read [Getting started](getting-started.md)
first; it introduces every idea the other pages build on.

## Guides

Everyday use, one task per page.

| Page | Covers |
|---|---|
| [Grids](guide/grids.md) | writing a grid and its values, half-open ranges, the predefined formats |
| [Policies](guide/policies.md) | what happens when a value does not fit: checked, `clamp`, `wrap`, rounding, error codes |
| [Arithmetic](guide/arithmetic.md) | result types, scalars, division, rounding once, `%`, sums |
| [Conversions and text](guide/conversions.md) | native numbers in and out, casts, comparisons, printing, parsing |
| [Ranges and the STL](guide/ranges.md) | looping over a grid, stepping, algorithms, `min`/`max`, hashing, random draws |
| [Units](guide/units.md) | quantities, conversions, temperatures, units and dimensions of your own |
| [Math](guide/math.md) | `sin`, `sqrt`, `exp`, …: rounding, output grids, angles, constants |

## Advanced topics

The details behind the guides, and topics most programs never need.

| Page | Covers |
|---|---|
| [Exact arithmetic](advanced/exact-arithmetic.md) | how result grids and policies are formed, the division paths, bounded continuous grids, rounding on every kind of grid |
| [Storage](advanced/storage.md) | which raw integer a grid gets, offset grids, wide raws, grids past 64 bits, raw access |
| [Hooks and errors](advanced/hooks-and-errors.md) | `on_wrap`, `on_clamp`, `on_error`, the error codes, the handler, which channel reports what |
| [Text in detail](advanced/text.md) | the printed forms, format specs, the parse grammar |
| [The units model](advanced/units-model.md) | dimensions, magnitudes, kinds, origins, the catalog, inside's algorithms on quantities |
| [Geometric algebra](advanced/geometric-algebra.md) | basis vectors as units, the signs of their products, multivectors, wedge, contractions, inverses, duals, exp |
| [The math engine](advanced/math-engine.md) | how results are rounded correctly, what is guaranteed, speed, angles over many turns |
| [Constants](advanced/constants.md) | the mathematical and physical constants |
| [For fixed-point users](advanced/fixed-point.md) | Qm.n formats as grids, which grids are fast |
| [mdspan](advanced/mdspan.md) | insides as indices of `std::mdspan`, views of raw buffers, exact sums and matrix products, quantities |
| [Determinism](advanced/determinism.md) | which results are bit-identical across platforms, and the build rules |
| [Certifying math](advanced/certify.md) | proving the math of your own grids correct by exhaustion |
| [Freestanding and bare metal](advanced/freestanding.md) | no exceptions, no FPU, no text |
| [Reading compiler errors](advanced/diagnostics.md) | decoding the types in a message, the named reasons, probing operations |
| [The single header](advanced/single-header.md) | one file for everything, Compiler Explorer |

## Background

| Page | Covers |
|---|---|
| [Internals](internals.md) | why the library is shaped this way, for contributors |
| [Roadmap](roadmap.md) | what is not implemented, and why |
| [Resources](resources.md) | prior art, a feature comparison, talks |
| [Adopting safe numbers](adoption.md) | why safer number types are hard to adopt |
| [*int considered harmful*](int-considered-harmful.pdf) | the paper behind the library |
| [Proposals](proposal/) | three draft WG21 papers: the number, quantities, math |

Generated reports: [performance](performance.md), [math accuracy](accuracy.md)
and the [certified math pairs](certified.md).

## Headers

| Header | Holds |
|---|---|
| `<beman/inside/inside.hpp>` | the number: `inside`, grids, policies, arithmetic, casts, ranges, `cursor`, `std::hash` and `std::numeric_limits` |
| `<beman/inside/inside/io.hpp>` | text: `to_string`, `<<`, `>>`, `std::format`, `parse` of a `string_view` |
| `<beman/inside/inside/formats.hpp>` | the native formats `uint8` … `int64`, `unorm8` …, `q8_8` …, and the counters |
| `<beman/inside/inside/random.hpp>` | `uniform<B>(rng)` |
| `<beman/inside/inside/mdspan.hpp>` | `std::mdspan` support: `extents_of`, `index_for`, `indices`, the accessors, `sum`, `dot`, the matrix products |
| `<beman/inside/math.hpp>` | `beman::inside::math`: the functions and the constants |
| `<beman/inside/math/constants.hpp>` | the constants alone (`math::pi`, …), with the engine that computes them |
| `<beman/inside/units.hpp>` | `beman::inside::units`: the machinery and the SI and ISQ catalog |
| `<beman/inside/units/core.hpp>` | the machinery alone, enough for units of your own |
| `<beman/inside/units/io.hpp>` | text of quantities: `to_string`, `<<`, `std::format`, `parse<Q>` |
| `<beman/inside/units/math.hpp>` | math on quantities: roots of units, angles in degrees, `value_cast` across π |
| `<beman/inside/units/random.hpp>` | `uniform<Q>(rng)` |
| `<beman/inside/units/mdspan.hpp>` | `std::mdspan` of quantities, `units::view_in` |
| `<beman/inside/units/constants.hpp>` | the physical constants as quantities |
| `<beman/inside/units/geoalg.hpp>` | geometric algebra: `multivector`, `wedge`, the contractions, `reverse`, `grade<K>`, `inverse`, `dual`, `regressive` |

The [single header](advanced/single-header.md) holds all of them.

## Examples

Each of the 44 programs in [`examples/`](../examples/) checks its own output
and runs as a test (`ctest --preset gcc-debug -L example`).

| Theme | Programs |
|---|---|
| first steps | [`basics`](../examples/basics.cpp) (clamp, wrap, bounded loops), [`errors`](../examples/errors.cpp) (every way to handle an error), [`casts`](../examples/casts.cpp) (one-shot stores), [`division`](../examples/division.cpp) (exact and integer division) |
| the standard library | [`algorithms`](../examples/algorithms.cpp), [`ranges`](../examples/ranges.cpp), [`id_pool`](../examples/id_pool.cpp) (an `unordered_set` of insides), [`storage`](../examples/storage.cpp) (which raw a grid gets) |
| units | [`units`](../examples/units.cpp) (a tour), [`kitchen`](../examples/kitchen.cpp) (lb and g, oven temperatures), [`circuit`](../examples/circuit.cpp) (Ohm's law, an RC filter), [`moon_rocket`](../examples/moon_rocket.cpp) (a flight to the Moon) |
| units of your own | [`own_units`](../examples/own_units.cpp) (fathoms, °R, EUR, bytes), [`money`](../examples/money.cpp) (USD and ¢, a tax), [`wei`](../examples/wei.cpp) (ETH to the wei), [`adc_calibration`](../examples/adc_calibration.cpp) (ADC counts), [`json_io`](../examples/json_io.cpp) (exact JSON numbers) |
| wrap, clamp and hooks | [`clock`](../examples/clock.cpp) (s → min → h → months), [`game_hp`](../examples/game_hp.cpp) (hit points), [`rifle`](../examples/rifle.cpp) (rounds), [`torus_map`](../examples/torus_map.cpp) (pixels), [`sequence_number`](../examples/sequence_number.cpp) and [`jitter_buffer`](../examples/jitter_buffer.cpp) (packets) |
| signal and control | [`audio_mixer`](../examples/audio_mixer.cpp), [`oscillator`](../examples/oscillator.cpp) (a phase counter), [`decibels`](../examples/decibels.cpp), [`pid_controller`](../examples/pid_controller.cpp), [`sensor_fusion`](../examples/sensor_fusion.cpp) (a mean of ℃ points), [`histogram`](../examples/histogram.cpp), [`fixed_point`](../examples/fixed_point.cpp) (Q formats) |
| math | [`navigation`](../examples/navigation.cpp) (range and bearing), [`audio_math`](../examples/audio_math.cpp), [`huge_angles`](../examples/huge_angles.cpp) (the sine of 10¹⁸ rad), [`geometry`](../examples/geometry.cpp) (`dot`, `cross`, `lerp`), [`expected_pipeline`](../examples/expected_pipeline.cpp) (errors with their cause) |
| geometric algebra | [`complex`](../examples/complex.cpp) (z·w, z / w, an impedance, e^(iπ)), [`quaternions`](../examples/quaternions.cpp) (Hamilton's rules, an exact rotation, a rotor from its angle), [`geometric_algebra`](../examples/geometric_algebra.cpp) (wedge, reflection, cross product, dual numbers) |
| arrays | [`image`](../examples/image.cpp) (a blur with indices proven in bounds, a heat map from raw codes), [`matrix`](../examples/matrix.cpp) (exact rotations in Q1.14, Fibonacci by matrix powers) |
| big numbers | [`solar_system`](../examples/solar_system.cpp) (nanometres across ±10¹³ m), [`planck_to_cosmos`](../examples/planck_to_cosmos.cpp) (10⁻⁴¹ m to 10²⁷ m), [`combinatorics`](../examples/combinatorics.cpp) (factorials, binomials, odds as exact fractions), [`rsa`](../examples/rsa.cpp) (RSA-1024 on residues) |
