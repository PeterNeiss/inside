# Getting started

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

A short tour of the ideas behind `beman.inside`, each with one example. It
takes about ten minutes; the [guides](README.md#guides) take each idea further.

## Your first program

The library needs GCC 16 in C++26 mode with static reflection. With the
[single header](../single_include/beman/inside/inside.hpp) on the include path:

```cpp
#include <beman/inside/inside.hpp>
#include <iostream>
using namespace beman::inside;

int main() {
    inside<{0, 100}> a{70}, b{58};
    auto sum = a + b;           // a new type, inside<{0, 200}>
    std::cout << sum << '\n';   // 128
}
```

```bash
g++-16 -std=c++26 -freflection -I single_include first.cpp && ./a.out
```

With CMake, link the `beman::inside` target instead ([README](../README.md#getting-it)).
The single header holds everything; with the separate headers, printing needs
`<beman/inside/inside/io.hpp>`.

## A number with a grid

An `inside` is a number that carries its **grid**: a lower limit, an upper
limit and a step, the *notch*. All three are exact and known at compile time,
and the value can never leave the grid.

```cpp
using pct    = inside<{0, 100}>;                    // the integers 0 … 100
using halves = inside<{0.5, 10}>;                   // 0.5 … 10 in steps of 1/2
using sample = inside<{{-1, 1}, per<(1 << 14)>}>;   // −1 … 1 in steps of 1/16384
```

Both limits belong to the grid. With two limits the notch is the coarsest one
that keeps every integer and both limits on the grid; `per<D>` names a step of
1/D. An `inside<{0, 100}>` is stored in one byte, because 101 values fit in a
`uint8_t`.

## Arithmetic widens

`+`, `-` and `*` compute a **result type** that holds every value the
operation can produce, so the result cannot overflow:

```cpp
pct a{70}, b{58};
auto s = a + b;        // inside<{0, 200}>: 128
auto d = a - b;        // inside<{-100, 100}>: 12
auto p = a * b;        // inside<{0, 10000}>: 4060
auto t = a + 1_ins;    // a plain 1 has no grid: write 1_ins
```

`a + 1` does not compile, because a plain `int` says nothing about its range.
`1_ins` is the number 1 as an inside of one point; `just<1>` is the same.

Division is exact. When the divisor's grid cannot hold 0, the result is a plain
value; when it can, the result is a `std::expected` that you check:

```cpp
auto q = a / inside<{1, 10}>{4};   // 17.5, exactly
auto r = a / b;                    // std::expected: b's grid holds 0
if (r) { /* *r is 35/29, exactly */ }
```

## The policy decides at the store

A value can only fail to fit when it is **stored** into a type with a smaller
grid: on construction, `=`, `+=` and the like. The type's **policy**, its
second template argument, decides what happens then:

```cpp
inside<{0, 100}, clamp> y{150};   // 100: saturates
inside<{0, 359}, wrap>  z{370};   // 10: wraps around
inside<{0, 100}>        x{50};    // a constant: checked while compiling (150 would not compile)
int  n = 150;                     // read at run time
auto m = inside<{0, 100}>::try_make(n);   // a std::expected: errc::overflow
```

Nothing throws. A store that the grids cannot prove to fit names what happens
(`try_make`, `clamp`, `wrap`, an `errc&`, an `on_error` hook) or does not
compile.

A value between two notches needs permission to round:

```cpp
using cents = inside<{{0, 1000}, per<100>}, round_nearest>;
cents price{19.999};              // 20.00
```

The policies are `clamp`, `wrap`, the rounding modes (`round_nearest`,
`round_floor`, `round_ceil`, `round_half_even`, `round_toward_zero`) and
`unsafe`, which drops the checks. Combine them with `|`:
`inside<{0, 100}, clamp | round_nearest>`.
[Policies](guide/policies.md) has the details, including every way a store
reports.

## Reading values out

An inside does not turn into an `int` by itself. Name the type you want:

| Write | Gives |
|---|---|
| `x.as<int>()` | an `int`; no check where `int` holds the whole grid |
| `x.to<std::uint8_t>()` | a `std::uint8_t` when every value of the grid fits one, else a `std::expected<std::uint8_t, errc>` that fails if the value does not |
| `double(x)` | a `double`, on a type with a rounding mode |
| `x.numerator()`, `x.denominator()` | the exact value as a fraction, two insides |
| `std::cout << x`, `std::format("{}", x)` | the exact value as text |

An index is `v[x.as<std::size_t>()]`.

## Units

`<beman/inside/units.hpp>` adds physical quantities. A quantity is an inside
with a unit:

```cpp
using namespace beman::inside::units;

constexpr auto km = si::kilo(si::metre);
quantity<km, inside<{{0, 500}, per<1000>}>> leg{12.5, km};   // 12.5 km, to the metre
auto total = leg + just<800> * si::metre;                    // 13300 m, exact
total.in(km);                                                // 13.300 km
// leg + just<1> * si::second;                               // does not compile
// quantity<km, inside<{0, 500}>> bad{12};                   // does not compile: name the unit
```

A unit conversion only rescales the grid, so it is exact and costs nothing at
run time. Temperatures are points on a scale:

```cpp
auto oven = point<international::degree_Fahrenheit>(inside<{0, 600}>{350});
oven.point_in(si::degree_Celsius);                       // 530/3 ℃, exactly
```

You can add units and dimensions of your own, such as a currency or hit
points; see [Units](guide/units.md).

## Math

`<beman/inside/math.hpp>` has `sin`, `cos`, `sqrt`, `exp`, `log` and the rest
of `<cmath>`, on insides. Each result is the **correctly rounded** point of
its output grid, and it is the same on every platform, at compile time and
without an FPU. A result is rounded, so the input or the output must allow it:

```cpp
using angle = inside<{{-4, 4}, per<(1 << 10)>}, round_nearest>;
math::sin(angle{1});                                // 0.841796875: the output grid follows the input
using out = inside<{{0, 2}, per<100>}, round_nearest>;
math::sqrt<out>(inside<{0, 4}>{2});            // 1.41: you name the output grid
```

With `<beman/inside/units/math.hpp>`, angles can be in degrees, and the
reduction is exact: the sine of 30° is exactly 1/2.

```cpp
math::sin<per<1000>>(just<30> * si::degree);   // 0.500
```

## How a value flows

```cpp
using mix = inside<{{-4, 4}, per<(1 << 14)>}, round_nearest | clamp>;
// gain and bias are insides too
sample s = read_input();     // 1. enter the bounded world once
auto   m = s * gain + bias;  // 2. compute: the types widen, nothing overflows
mix    out{m};               // 3. store at the end: the policy rounds and clamps
double d = double(out);    // 4. leave for floating point, explicitly
```

Convert in once, compute exactly, and narrow once at the end. Steps 1 to 3
are exact; step 4 is explicit because it gives that up.

## Where to go next

| You want to… | Read |
|---|---|
| write grids, use the predefined formats | [Grids](guide/grids.md) |
| choose what happens at the edges, handle errors | [Policies](guide/policies.md) |
| divide, round, sum a range | [Arithmetic](guide/arithmetic.md) |
| convert, print, parse | [Conversions and text](guide/conversions.md) |
| loop over a grid, use the STL | [Ranges and the STL](guide/ranges.md) |
| work with metres, ℃, currencies, your own units | [Units](guide/units.md) |
| call sin, sqrt, exp | [Math](guide/math.md) |
| know more | the [advanced topics](README.md#advanced-topics) |
