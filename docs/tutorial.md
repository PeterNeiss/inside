# inside — a mental model

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

This is the top-down tour: what an `inside` *is*, the four ideas it rests on,
and how a value flows through a program. Each idea ends with a link to the
guide that covers it in depth; [internals.md](internals.md) explains *why* the
library is shaped this way.

## The one-sentence model

An `inside` is **a number that carries its own grid**: a lower bound, an upper
bound and a step size, all exact fractions, all known at compile time. The type
is the contract, and the value can never leave it.

## Idea 1 — the grid

```cpp
using pct    = inside<{0, 100}>;                  // integers 0 … 100
using halves = inside<{0.5, 10}>;                 // 0.5 … 10 in steps of 1/2 (derived)
using sample = inside<{{-1, 1}, per<16384>}>;     // −1 … 1 in steps of 1/16384
```

A grid is `{{Lower, Upper}, Notch}`: three exact fractions. The one rule is
that `(Upper − Lower) / Notch` is a whole number, so the interval holds a whole
count of notches.

With only two limits, `{Lower, Upper}`, the notch is derived. It is the
coarsest step 1/k that keeps every integer and both limits on the grid: 1 for
integer limits, 1/2 for `{0.5, 10}`, 1/80 for `{just<3, 10>, just<13, 16>}`.

The fractions are written with:

| Spelling | Means |
|---|---|
| `42`, `0.5`, `0x1p-4` | an integer or a dyadic literal (exact in binary) |
| `per<D>` | the step 1/D |
| `just<N, D>` | the exact ratio N/D (also a value: a one-point inside) |
| `0.1_r` | the exact decimal 1/10 |
| `1_ins` | the one-point inside holding 1 |

Both limits belong to the grid: `{0, 100}` holds 0 and 100. For a range you
think of as half-open, such as an index `[0, N)` or a period `[0, 360)`, say so
with a third argument. The open limit moves in by one notch, so the grid stays
closed and the type is the same:

```cpp
inside<{0, 360, bounds::half_open}>                  // == inside<{0, 359}>
inside<{{0, 64}, per<16>, bounds::half_open}>        // == {{0, 63.9375}, per<16>}
inside<{0, 10, bounds::open}>                        // == inside<{1, 9}>
```

An open limit needs a notch: a continuous grid (notch 0) has no next point.

Because the grid is part of the type, two differently shaped insides are
different types. The compiler tracks range and precision for you.

## Idea 2 — arithmetic widens, it never overflows

```cpp
pct a = 70, b = 58;
auto s = a + b;          // inside<{0, 200}>: 128, computed at compile time
```

`+ - * /` compute the **result grid** at compile time so that it holds every
value the operation could produce. `a + b` cannot overflow, because its type is
already wide enough. Nothing is checked at run time on this path, and grids on
whole numbers use native integer instructions.

A plain `int` has no grid, so write `a + 1_ins`, not `a + 1`. Division by a
grid that holds 0 is the one operation that can fail; its result is a
`std::expected`. See [arithmetic.md](arithmetic.md).

## Idea 3 — the policy governs the narrowing

Widening is free. The decisions happen when a value is **stored into a
narrower grid**: that is the only place a value can fail to fit, and the
type's *policy* decides what happens:

```cpp
inside<{0, 100}, clamp> p = 150;     // 100 (saturate)
inside<{0, 359}, wrap>  deg = 370;   // 10 (modular)
inside<{0, 100}>        q = 150;     // throws: the default policy is checked
```

The flags are `clamp`, `wrap`, the rounding modes (`round_nearest`,
`round_floor`, …) and `unsafe`. On top come per-store hooks
(`x.on_wrap(f) += d` calls `f` with the carry) and a non-throwing mode with
`errc` codes. See [policies.md](policies.md).

## Idea 4 — the unit is part of the type too

`<beman/inside/units.hpp>` puts a unit, a dimension and a quantity kind next to
the grid. A quantity is an inside plus a unit:

```cpp
constexpr auto km = si::kilo(si::metre);
quantity<km, inside<{{0, 500}, per<1000>}>> leg{12.5};   // 12.5 km, to the metre
auto total = leg + just<800> * si::metre;                // 13300 m
auto room  = point<si::degree_Celsius>(inside<{{-50, 50}, per<10>}>{21.5});
room.point_in(international::degree_Fahrenheit);         // 70.70 ℉, exact
```

A unit conversion only rescales the grid: km → m multiplies the interval and
the notch by 1000 and keeps the raw number. So it is exact, it cannot fail, and
it costs nothing. A sum of two units runs in their common unit, exactly. A
length never adds to a duration, and temperatures are points on a scale, not
amounts.

You can add your own units, and your own dimensions: a currency, hit points,
pixels, ADC counts. Policies, hooks, `mul_into`, `sum`, `parse` and the math
functions all work on quantities. See [units.md](units.md).

## How a value flows

```cpp
using mix = inside<{{-4, 4}, per<16384>}, round_nearest | clamp>;
sample a = read_input();          // 1. enter the bounded world once
auto   m = a * gain + offset;     // 2. widen through arithmetic (no overflow)
mix    out{m};                    // 3. narrow back through a policy at the sink
float  f = double(out);           // 4. leave for floating point at the boundary
```

**Convert in once, stay inside, narrow at the sink.** Steps 1–3 are exact.
Step 4 is explicit because it gives that up. With units the shape is the same:
a sensor value enters as a quantity, the arithmetic carries the unit, and the
sink's type names the unit it wants.

## Reading values out

| From an inside | Gives |
|---|---|
| `x.to<T>()` | a native `T`, as `std::expected<T, errc>` (fails if it does not fit) |
| `x.numerator()`, `x.denominator()` | the exact value as a fraction |
| implicit `imax` | on whole-number grids, or a grid with a rounding mode: `arr[i]` works |
| `double(x)` | explicit, on a grid with a rounding mode |

| From a quantity | Gives |
|---|---|
| `q.in(u)` | the same quantity in unit `u`, exact |
| `q.numerical_value()`, `q.numerical_value_in(u)` | the inside |
| `value_cast<Rep2>(q)`, `value_cast<u, Rep2>(q)` | another number type, rounded by Rep2 |
| an implicit conversion to an inside | for a dimensionless quantity: (3 km) / (2 m) is 1500 |

See [conversions.md](conversions.md).

## Math on insides

`beman::inside::math` is a `<cmath>`-shaped set of functions over insides:
`sin`, `cos`, `sqrt`, `exp`, `log`, `atan2`, …. Each result is the correctly
rounded point of its output grid, so it is the same on every platform, at
compile time and without an FPU.

- The input or the output needs a rounding mode (`inside<G, round_nearest>`).
- Angles are radians; with units, also degrees or turns, reduced exactly.
- A failure that depends on the value (a `tan` pole, the root of a negative
  number) comes back as `std::expected`.

See [math.md](math.md).

## Where to go next

| You want to… | Read |
|---|---|
| choose what happens at the edges, handle errors | [policies.md](policies.md) |
| know the result grid of an operation, round once | [arithmetic.md](arithmetic.md) |
| convert, cast, print, parse | [conversions.md](conversions.md) |
| iterate, store, use the STL | [storage.md](storage.md) |
| work with metres, ℃, currencies, your own units | [units.md](units.md) |
| call sin, cos, sqrt, … | [math.md](math.md) |
| use π, e or the SI constants | [constants.md](constants.md) |
| map your Qm.n fixed-point habits | [fixed-point.md](fixed-point.md) |
| replay bit-identically (lockstep, fuzzing) | [determinism.md](determinism.md) |
| build for bare metal or without exceptions | [freestanding.md](freestanding.md) |
| read a long compiler error | [diagnostics.md](diagnostics.md) |
| use the one-file header (Compiler Explorer) | [single-header.md](single-header.md) |
| know *why* it is shaped this way | [internals.md](internals.md) |
| compare with other libraries | [resources.md](resources.md) |
