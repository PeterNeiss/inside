# Conversions, casts and text

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

Everything that crosses the boundary of an inside: native numbers in and out,
the named casts, the predicates that test a value before a store, text in and
out, and the same for quantities. How to *write* a constant (`1_ins`,
`just<N, D>`, `0.1_r`) is in the [tutorial](tutorial.md#idea-1--the-grid).

`beman::inside::rational` appears below as an exact 64-bit fraction: the value
of `0.1_r`, the raw of a continuous inside, and a value you can construct
from or compare with. Its own arithmetic returns `std::expected`, because a
fraction can outgrow 64 bits, so compute with insides and read exact values
out with `numerator()` and `denominator()`.

## Implicit conversions

| Conversion | When | For |
|---|---|---|
| `operator rational()` (implicit) | the values fit a 64-bit fraction | exact use as a `rational` |
| `operator double()` (explicit) | the policy has a rounding mode (`round_*` or `snap`) | floating point, `printf` |

There is no conversion to a built-in integer. An implicit one would narrow
silently, `int i = b;` on a grid past `int`, and `unsigned char c = b;` on a
grid below 0. Read an integer with `as<T>()` or `to<T>()`, which name `T` and
check what `T` cannot hold. Where `T` holds the whole grid no check is left,
so an index costs nothing:

```cpp
inside<{0, 100}> b{42};
if (b == 42) { /* … */ }      // compares with a scalar, exactly
imax v = b.as<imax>();        // every value fits: no check
std::vector<int> vec(101);
vec[b.as<std::size_t>()] = 0; // an index, named

inside<{{0, 1}, per<1024>}, round_nearest> g{0.25};
double e = double(g);         // explicit: g's policy rounds
```

## Named extraction: `to<T>()` and `as<T>()`

`b.to<T>()` returns `std::expected<T, errc>`: `errc::overflow` when the value
is outside `T`'s range, `errc::domain_error` for a negative value into an
unsigned `T`. A fractional value into an integer `T` needs permission to
round, as a store does: it rounds by the call's mode
(`b.to<int, round_floor>()`), else by the type's (`round_nearest` & co.), and
with neither it is `errc::rounding_error`. `b.as<T>()` returns `T`, for a value you know converts: a
failure is reported like a failed checked store (`inside_error` with the
same code, or the installed handler).

```cpp
inside<{0, 1000}> b{42};
auto r = b.to<std::uint8_t>();    // expected: 42
auto v = b.as<std::int16_t>();    // 42; throws if out of range
using wide = inside<{0, std::numeric_limits<std::uint64_t>::max()}>;
auto w = wide{18'000'000'000'000'000'000u}.to<std::uint64_t>();   // past int64: to<T> works
```

Both exist as free functions too, `to<T>(b)` and `as<T>(b)`, found by ADL. In
generic code they avoid the `b.template as<imax>()` spelling.

`as<double>()` needs a rounding mode, like `operator double`. `to<double>()`
does not: it is the explicit way for a grid without one.
`to<long double>()` rounds the exact value once to the `long double`'s own
precision, never through a `double`.

## Exact read-out: `numerator()` and `denominator()`

The exact value in lowest terms: the numerator carries the sign, the
denominator is positive (1 on an integer grid). They return `imax` when every
value of the grid fits, and a wide integer otherwise.

```cpp
inside<{{-4, 4}, per<16>}, round_nearest> g{0.1875};
g.numerator();     // 3
g.denominator();   // 16: exactly 3/16
```

These are the only *exact* read-out. For an approximate number use
`to<double>()` or a rounded `to<intN>()`.

## Free-function casts

Five casts name what happens at the boundary. They read well inside a
`std::ranges::transform` lambda, where `B{v}` would hide the intent:

| Cast | Does |
|---|---|
| `clamp_cast<B>(v)` | saturate to B's range, whatever B's policy |
| `clamp_cast<B, Round>(v)` | the same, rounding by `Round` (`round_floor`, `round_ceil`, `round_nearest`, `round_half_even`) |
| `wrap_cast<B>(v)` | fold v into B's range modulo its period |
| `checked_cast<B>(v)` | report through the checked policy (throw by default): `overflow` or `rounding_error` |
| `unchecked_cast<B>(v)` | trust the caller; out of range is undefined |

```cpp
using pct = inside<{0, 100}>;
clamp_cast<pct>(150);        // 100
wrap_cast<pct>(105);         // 4: the period of [0, 100] is 101
checked_cast<pct>(42);       // 42
unchecked_cast<pct>(42);     // 42

using coarse = inside<{{0, 10}, 2}>;
clamp_cast<coarse, round_floor>(3.0);     // 2
clamp_cast<coarse, round_ceil>(3.0);      // 4
clamp_cast<coarse, round_nearest>(3.0);   // 4
clamp_cast<coarse, round_floor>(15.0);    // 10: clamped

using small = inside<{{0, 10}, per<10>}, clamp>;
small a = 2.5;
clamp_cast<small, round_nearest>(a * a);  // 6.3: 6.25 rounded onto 1/10
```

The source can be a native number or an inside on any grid; the rounding
mode relaxes the notch check, so a finer value rounds onto the target. The
fluent forms do the same: `b.with_clamp() = r`, `(a * a).with_snap<round_nearest>()`
([arithmetic.md](arithmetic.md#rounding)).

### API boundary: `clamp | round_nearest`

For a type at an API boundary, such as an actuator command or a fused sensor
output, put `clamp | round_nearest` on the type and write `T{value}`:

```cpp
using output_t = inside<{-100, 100}, clamp | round_nearest>;
return output_t{raw};   // saturates and rounds; no clamp_cast<output_t, round_nearest>(raw)
```

`clamp_cast<B, Round>` remains the tool when one call site must override the
type's policy. [`pid_controller.cpp`](../examples/pid_controller.cpp) (a % command)
and [`sensor_fusion.cpp`](../examples/sensor_fusion.cpp) (a ℃ reading) use this
form on quantities.

## Conversion predicates

Test a value before a store, without storing:

```cpp
conversion_overflows<pct>(150);   // true: outside [0, 100]
conversion_rounds<pct>(3.5);      // true: not on notch 1
conversion_loses<pct>(150);       // true: either of the two
```

They are `noexcept` and never raise. A NaN or an infinity counts as overflow
(and does not round), and a continuous grid never rounds. They follow the
store's order: a rounding mode rounds first, so 9.5 does not overflow
`inside<{0, 9}, round_floor>`. [`histogram.cpp`](../examples/histogram.cpp)
uses them as an outlier filter for latencies in ms.

## Writing text: `to_string`, `operator<<` and `std::format`

`to_string(x)`, `std::cout << x` and `std::format("{}", x)` print the same
text: the **exact** value, never rounded. Its form follows the grid:

| Grid | Prints | Examples |
|---|---|---|
| integer notch | the integer | `42`, `-7` |
| decimal notch (a denominator 2^a·5^b with a 5, like `per<100>` or `0.05_r`) | that notch's decimals, for every value | `19.90`, `2.00` on a cents grid |
| any other notch, or continuous | the shortest exact form: a decimal when there is one, else `N/D` | `0.5`, `0.0000152587890625` on `per<65536>`; `7/3` |

A column of prices lines up (`19.90`, not `19.9`), a binary fixed-point value
is not padded to 16 decimals, and a value with no finite decimal stays exact.
Every form reads back through `parse`.

- Wide and big values print every digit.
- A `rational` prints its shortest exact form, an `interval` `[lo..hi]`, a
  grid `{[lo..hi], notch}`.
- `to_string_debug(x)` adds the raw, its type and the grid:
  `0.5 {128[uint16_t Max:512] {[0..2], 0.00390625}}`.

### Format specs

A non-empty spec formats the value:

| Type | Takes |
|---|---|
| an integer grid whose values fit `int64` | the integer specs: `{:>4}`, `{:#x}`, `{:b}` |
| every other inside, and `rational` | `f`, `e`, `g` with fill, align, sign, `#`, `0`, width and precision |

The digits are **rounded once from the exact value by the type's own
rounding mode**, the digits a store at that precision would give. A type
without a rounding mode, and `rational`, round to nearest with ties to even,
as `printf` does with a `double`. A width alone keeps the exact text.

| 0.125 and −0.129 with `{:.2f}` | no mode | `round_nearest` | `round_floor` | `round_ceil` | `snap` |
|---|---|---|---|---|---|
| prints | `0.12`, `-0.13` | `0.13`, `-0.13` | `0.12`, `-0.13` | `0.13`, `-0.12` | `0.12`, `-0.12` |

```cpp
#include <beman/inside/inside/io.hpp>
#include <print>

inside<{0, 100}> hp{42};
std::println("HP = {}", hp);               // HP = 42
std::println("HP = {:>5}", hp);            // HP =    42
std::println("HP = {:#04x}", hp);          // HP = 0x2a

using cents = inside<{{0, 1'000'000}, per<100>}>;
cents price{19.9_r};
std::println("{} {:.1f} {:>8}", price, price, price);   // 19.90 19.9    19.90

inside<{{0, 1}, per<1024>}> g{0.125};
std::println("{} {:.2f} {:e}", g, g, g);   // 0.125 0.12 1.250000e-01
```

## Reading text: `parse<B>` and `operator>>`

`parse<B>(text)` reads one number exactly, with no `double` in between, and
returns `std::expected<B, errc>`. The text is an optional sign, then a literal
(`1'000`, `1.25`, `1.5e2`, `0xff`, `0b1010`, `0x1.8p3`) or a fraction `N/D`:
the forms `to_string` prints. Long decimals past 64 bits are read exactly too.
The value then goes through `B::try_make`, so B's policy applies:

```cpp
using pct = inside<{0, 100}>;
parse<pct>("42");                          // 42
parse<pct>("0.1");                         // errc::rounding_error: not on the integer grid
parse<pct>("150");                         // errc::overflow
parse<pct>("4x");                          // errc::invalid_format
parse<inside<{{0, 1}, per<10>}>>("0.1");   // exactly 1/10

inside<{0, 100}> a{0};
std::cin >> a;                             // one token; failbit on error, a unchanged
```

Flags after the type add to B's policy for the call. `parse_exact<B>` accepts
only a value B holds exactly, whatever B's policy:

```cpp
using cents = inside<{{0, 1'000'000}, per<100>}>;
parse<cents>("1.005");                     // errc::rounding_error
parse<cents, round_nearest>("1.005");      // 1.01: the tie goes away from zero

using price = inside<{{0, 1'000'000}, per<100>}, round_nearest | clamp>;
parse<price>("1.005");                     // 1.01: price rounds
parse_exact<price>("1.005");               // errc::rounding_error
parse_exact<price>("2000000");             // errc::overflow: not clamped
```

The core header has the `constexpr` pointer-pair form, `parse<B>(first, last)`;
`io.hpp` adds the `std::string_view` form and `operator>>`.
[`json_io.cpp`](../examples/json_io.cpp) reads and writes a JSON document byte
for byte this way.

## Quantities

A quantity converts like its number, with its unit spelled out
([units.md](units.md)):

| From a quantity | Spelling |
|---|---|
| the same quantity in another unit, exact | `q.in(u)` |
| its number | `q.numerical_value()`, `q.numerical_value_in(u)` |
| another number type, rounded by it | `value_cast<Rep2>(q)`, `value_cast<u, Rep2>(q)`, `value_cast<Q>(q)` |
| the same, as an `expected` | `try_value_cast<Q, F>(q)`, `try_value_cast<Rep2, F>(q)` |
| another kind of the same dimension | `quantity_cast<isq::height>(q)` |
| a plain number, when dimensionless | implicit: (3 km) / (2 m) is 1500 |

| Into a quantity | Spelling |
|---|---|
| a number in Q's unit | `Q{v}`, `Q::try_make<F>(v)`, `just<5> * si::metre` |
| a cast | `clamp_cast<Q, Round>(x)`, `wrap_cast<Q>(x)`: `x` a number in Q's unit, or a quantity converted to it first |
| a test | `conversion_overflows<Q>(v)`, `conversion_rounds<Q>(v)`, `conversion_loses<Q>(v)` |
| text | `parse<Q, F>(text)`, `parse_exact<Q>(text)`: the number, read in Q's unit |

```cpp
using metres = quantity<si::metre, inside<{0, 1000}>>;
wrap_cast<quantity<si::second, inside<{0, 60, bounds::half_open}, wrap>>>(just<3> * si::minute + just<5> * si::second);   // 5 s
*parse<metres>("12");             // 12 m
conversion_overflows<metres>(2000.0);   // true
```

`to_string(q)`, `<<` and `std::format` write the number and then the unit's
symbol (`5 km`, `10.5 m/s`, `20 ℃`, `90°`). A format spec applies to the
number, and `unit_symbol(u, text_encoding::ascii)` gives the ASCII spelling.
A JSON or wire format holds the bare number: write
`q.numerical_value_in(USD / item)`, which also documents the unit.

## Comparing

An inside compares with native numbers and with other insides directly,
exactly, and without narrowing either side:

```cpp
inside<{0, 100}> a{42}, b{58};
a == 42;          // true
a < 50.5;         // true: the double is compared at full precision
a + b == 100;     // true
```

Two quantities compare in their common unit (`just<1> * si::kilo(si::metre) == just<1000> * si::metre`);
a length and a duration do not compare.
