# Getting started

A short tour of the ideas behind `inside`, each with one example. It
takes about ten minutes; the [guides](README.md#guides) take each idea further.

## Your first program

The library needs GCC 16 in C++26 mode with static reflection. With the
[single header](../single_include/inside/inside.hpp) on the include path:

```cpp
#include <inside/inside.hpp>
#include <iostream>
using namespace ins;

int main() {
    inside<{0, 100}> a{70}, b{58};
    auto sum = a + b;           // a new type, inside<{0, 200}>
    std::cout << sum << '\n';   // 128
}
```

```bash
g++-16 -std=c++26 -freflection -I single_include first.cpp && ./a.out
```

With CMake, link the `inside::inside` target instead ([README](../README.md#getting-it)).
The single header holds everything; with the separate headers, printing needs
`<inside/inside/io.hpp>`.

## A number with a grid

An `inside` is a number that carries its **grid**: a lower limit, an upper
limit and a step, the *notch*. All three are exact and known at compile time,
and the value can never leave the grid.

```cpp
using integers = inside<{0, 100}>;                    // the integers 0 … 100
using halves   = inside<{0.5, 10}>;                   // 0.5 … 10 in steps of 1/2
using q14      = inside<{{-1, 1}, per<(1 << 14)>}>;   // −1 … 1 in steps of 1/16384
```

Both limits belong to the grid. With two limits the notch is the coarsest one
that keeps every integer and both limits on the grid; `per<D>` names a step of
1/D. An `inside<{0, 100}>` is stored in one byte, because 101 values fit in a
`uint8_t`.

## Arithmetic widens

`+`, `-` and `*` compute a **result type** that holds every value the
operation can produce, so the result cannot overflow:

```cpp
integers a{70}, b{58};
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
auto m = inside<{0, 100}>::try_make(n);   // a std::expected: error::overflow
```

Nothing throws. A store that the grids cannot prove to fit names what happens
(`try_make`, `clamp`, `wrap`, an `error&`, an `on_error` hook) or does not
compile.

A value between two notches needs permission to round:

```cpp
using hundredths = inside<{{0, 1000}, per<100>}, round_half_away>;
hundredths x{19.999};             // 20.00
```

The policies are `clamp`, `wrap`, the rounding modes (`round_half_away`,
`round_floor`, `round_ceil`, `round_half_even`, `round_toward_zero`) and
`unsafe`, which drops the checks. Combine them with `|`:
`inside<{0, 100}, clamp | round_half_away>`.
[Policies](guide/policies.md) has the details, including every way a store
reports.

## Reading values out

An inside does not turn into an `int` by itself. Name the type you want:

| Write | Gives |
|---|---|
| `x.to<int>()` | an `int` when every value of the grid fits one (nothing is checked), else a `std::expected<int, error>` that fails if the value does not |
| `x.to<double>()` | a `double`, rounded once |
| `x.numerator()`, `x.denominator()` | the exact value as a fraction, two insides |
| `std::cout << x`, `std::format("{}", x)` | the exact value as text |

An inside on non-negative integers is an index, and a container takes it as
it is: `v[x]` ([Conversions](guide/conversions.md#indices)).

## Units

`<inside/units.hpp>` adds physical quantities. A quantity is an inside
with a unit:

```cpp
using namespace ins;

constexpr auto km = si::kilo(si::metre);
quantity<km, inside<{{0, 500}, per<1000>}>> leg{12.5, km};   // 12.5 km, to the metre
auto total = leg + just<800> * si::metre;                    // 13300 m, exact
total.in(km);                                                // 13.300 km
// leg + si::second;                                         // does not compile
// quantity<km, inside<{0, 500}>> bad{12};                   // does not compile: name the unit
```

A unit conversion only rescales the grid, so it is exact and costs nothing at
run time. A quantity compares only with a quantity of its kind
(`total > just<10> * km`), never with a bare number. Temperatures are points
on a scale:

```cpp
auto oven = point<international::degree_Fahrenheit>(inside<{0, 600}>{350});
oven.point_in(si::degree_Celsius);                       // 530/3 ℃, exactly
```

You can add units and dimensions of your own, such as a currency or hit
points; see [Units](guide/units.md).

## Math

`<inside/math.hpp>` has `sin`, `cos`, `sqrt`, `exp`, `log` and the rest
of `<cmath>`, on insides. Each result is the **correctly rounded** point of
its output grid, and it is the same on every platform, at compile time and
without an FPU. A result is rounded, so the input or the output must allow it:

```cpp
using arg = inside<{{-4, 4}, per<(1 << 10)>}, round_half_away>;
math::sin(arg{1});                                  // 0.841796875: the output grid follows the input
using out = inside<{{0, 2}, per<100>}, round_half_away>;
math::sqrt<out>(inside<{0, 4}>{2});            // 1.41: you name the output grid
```

With `<inside/units/math.hpp>`, angles can be in degrees, and the
reduction is exact: the sine of 30° is exactly 1/2.

```cpp
math::sin<per<1000>>(just<30> * si::degree);   // 0.500
```

## How a value flows

An audio sample, in full scale (FS), through a gain (a ratio) and a bias (a sample):

```cpp
inline constexpr auto FS = base_unit<"FS", "amplitude">;
using sample = quantity<FS, inside<{{-1, 1}, per<(1 << 14)>}>>;
using mix    = quantity<FS, inside<{{-4, 4}, per<(1 << 14)>}, round_half_away | clamp>>;
sample s = read_input();                         // 1. enter the bounded world once
auto   m = s * gain + bias;                      // 2. compute: the types widen, nothing overflows
mix    out{m};                                   // 3. store at the end: the policy rounds and clamps
double d = out.numerical_value_in(FS).to<double>();   // 4. leave for floating point, explicitly
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
