# inside for fixed-point users

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

If you think in Qm.n, inside will feel familiar: it *is* fixed point, with the
scale and the range moved into the type so that the compiler tracks them. This
page maps the Qm.n habits onto grids, says which grids are fast, and shows the
same formats with physical units.

## A grid is a fixed-point format

An inside's type is a **grid** `{interval, notch}`:

- the **notch** is the resolution, the value of one LSB: `per<256>` is 2⁻⁸,
  that is **8 fractional bits**;
- the **interval** `[Lower, Upper]` is the range;
- a value is Lower + index · notch. On a whole-number grid the raw *is* the
  value wherever that costs no width, like a plain integer register.

An unsigned Q8.8 register (8 integer bits, 8 fractional bits, every one of the
65536 bit patterns) is:

```cpp
using q8_8 = inside<{{0, rational{65535, 256}}, per<256>}, round_nearest>;   // == beman::inside::q8_8
```

`<beman/inside/inside/formats.hpp>` has the common ones ([storage.md](storage.md#predefined-native-formats)):

| Alias | Format | Raw |
|---|---|---|
| `q4_4`, `q8_8`, `q16_16` | unsigned Q4.4, Q8.8, Q16.16, `round_nearest` | `uint8_t`, `uint16_t`, `uint32_t` |
| `unorm8`, `unorm16`, `unorm32` | [0, 1] in steps of 1/(2ᴺ − 1), so 1.0 is exact, `round_nearest` | `uint8_t`, `uint16_t`, `uint32_t` |
| `uint8` … `uint64`, `int8` … `int64` | the full integer ranges (`int64` from −(2⁶³ − 1)) | the same width |

Any other format is one line: a signed Q1.14 sample is
`inside<{{-1, 1}, per<16384>}, round_nearest>`, a 0.5 °C step
`inside<{{-40, 60}, 0.5}, round_nearest>`.

## What inside adds over a raw Qm.n integer

- **The scale is in the type.** `q8_8{1.5}` stores 384; you never track the
  radix point or the shifts.
- **Result grids widen by themselves.** `a * b` has a new grid, computed at
  compile time: Q8.8 × Q8.8 is Q16.16-shaped, exactly, with no headroom
  analysis. Scaling by a constant point (`x * just<c>`) keeps integer storage
  on a scaled lattice.
- **Storing back is a policy.** Onto a coarser grid a store truncates
  (`snap`), rounds (`round_nearest`, …), saturates (`clamp`), wraps, or
  reports (checked), chosen per type or per store.
- **Round once.** `mul_into<Out>(a, b)` rounds the exact product straight onto
  `Out`, the fixed-point "multiply, then shift with rounding" in one step,
  without a wide intermediate type.
- **Exact by default.** A notched grid holds exact integer indices; only a
  continuous grid (an exact quotient) needs a fraction.

## What storage costs

| Raw | Holds | Cost | For |
|---|---|---|---|
| the value (whole-number grids) | the value as a plain integer | one integer | integer ranges, interop: `raw()` is the wire value |
| an index (every other notched grid) | the 0-based notch index | one integer, plus an offset or shift to read the value | Q formats, dense serialization |
| an exact fraction (continuous grids) | a `rational` | a gcd per operation | exact quotients |

Integer raws are cheap. **The fraction is the slow one**: keep continuous grids
out of hot loops ([storage.md](storage.md#storage-selection)).

## Which grids are fast

1. **Whole-number grids** (`inside<{0, N}>`, `inside<{a, b}>`): `+ − ×` run on
   the raw integers, a product is one `imul`, and a division under `snap` is a
   native `a / b`. Byte-wide loops without checks vectorize at the native lane
   count.
2. **Q-format grids** (notch 1/N, Lower 0): a power-of-two N makes scaling a
   shift. Two same-notch operands divide as `(a·N) / b`, a shift and a divide
   for a power-of-two N ([arithmetic.md](arithmetic.md#division)).
3. **Continuous grids** (notch 0): avoid in hot loops.

For a bulk sum use `sum<Target>`, which checks the total once and keeps the
loop vectorizable; a per-element checked `+=` pays a compare and branch per
element.

## Performance

[performance.md](performance.md) has the measured tables for Q8.8 and Q16.16
construct, add, sub, mul, div, store and accumulate, and for the math
functions, each against a native baseline with hardware counters (built by the
`beman.inside.perf_report` target). On the reference machine Q8.8 construction
and accumulation run faster than the hand-written native code, division
matches it, and a product or an add costs a few instructions more (about
70–90% of native speed). A checked store adds a compare and branch; `unsafe`
or `sum` removes it from a proven-safe inner loop.

## Q formats with units

A unit puts a meaning on the same raw at no cost (see [units.md](units.md)):

```cpp
#include <beman/inside/units.hpp>
using namespace beman::inside::units;

using celsius = inside<{{-40, 60}, 0.5}, round_nearest>;     // a 0.5 °C step, a uint8_t
auto room = point<si::degree_Celsius>(celsius{21.4});        // 21.5 ℃: rounded onto the step
auto body = point<si::degree_Celsius>(celsius{37});
auto diff = body - room;                                     // 15.5 ℃, a temperature difference

constexpr auto km = si::kilo(si::metre);
quantity<km, inside<{0, 10}>> d{7};
auto mm = d.in(si::milli(si::metre));                        // 7000000 mm: the same uint8_t raw
```

- A unit conversion rescales the grid and keeps the raw: km → mm compiles to
  one `mov`.
- `mul_into<Out>` on quantities folds the unit factor into the same single
  rounding: `mul_into<usd>(subtotal, just<8> * percent)`.
- A sensor count can be a dimension of its own: an ADC reading in `cts`
  never adds to a voltage, and a calibrated gain in cts/V turns it into volts
  with one exact division ([`adc_calibration.cpp`](../examples/adc_calibration.cpp)).

[`fixed_point.cpp`](../examples/fixed_point.cpp) shows the formats table, exact
products, `mul_into` onto Q8.8, and the 0.5 °C grid as ℃ points.

## Choosing a grid

- **Hot integer or fixed-point math:** whole-number or Q-format grids;
  `unsafe` in the proven-safe inner loop, then store the result into a checked
  type.
- **Transcendentals:** any grid; integer outputs are the fast ones
  ([math.md](math.md#storage)).
- **No rounding allowed:** choose a notch that holds every value (thirds:
  `per<3>`); only a continuous grid pays for fractions.
- **SIMD byte and halfword loops:** keep the range within the native type (the
  `formats.hpp` aliases use the full range) so the raw stays native width.

## Where to go next

| You want | Read |
|---|---|
| the full storage rules | [storage.md](storage.md) |
| the result grid of each operation | [arithmetic.md](arithmetic.md) |
| policies and errors | [policies.md](policies.md) |
| reproducibility | [determinism.md](determinism.md) |
| sin, cos, sqrt, … | [math.md](math.md) |
| units on your formats | [units.md](units.md) |
