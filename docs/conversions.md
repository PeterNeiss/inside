# Conversions

This page covers the conversion surface between `bound` and scalar
arithmetic types — the exact integer-pair read-out, the named casts, the
conversion predicates, the implicit conversion operators, and the idioms for
writing literal values into bounds.

> **Note.** The exact fractional representation type is an internal
> implementation detail (`bnd::detail::rational`) and is **not** part of the
> public surface. You never name it, receive it, or operate on it: scalars
> enter a bound through construction or `_b` / `just<>` / `frac<N,D>`, and
> exact values come back out through `numerator()` / `denominator()`. Stay in
> bound-space for arithmetic — that is where the no-overflow guarantee lives.

## Implicit operator conversions on `bound`

| Conversion | When it applies | Purpose |
|---|---|---|
| `operator imax()` (implicit) | integer-notch grid (notch denom = 1) with Lower ≥ imax_min and Upper ≤ imax_max | drop-in for integer contexts: accumulators, comparisons, and indexing (`vec[b]` converts imax → size_t) |
| `operator double()` (**implicit** for `real`-policy bounds, explicit otherwise) | `real`: always (the dyadic grid makes every value exact in `double`). Others: grid carries a rounding policy (`round_*` or `ignore_round`) | floating-point arithmetic / printf |

`operator imax()` is deliberately the **only** implicit integer conversion —
a second one (e.g. `size_t`) would make built-in mixed arithmetic like
`imax_var += b` ambiguous. Indexing goes through imax and the standard
imax → size_t conversion; if your build enables `-Wsign-conversion`, index
sites will surface that conversion as a warning.

```cpp
bound<{0, 100}> b{42};
if (b == 42) { ... }      // arithmetic compare, no conversion needed
if (b < 50)  { ... }      // works on bounds and scalars

imax v = b;                // implicit (integer-shape grid)
std::vector<int> vec(101);
vec[b] = 0;                // no .as<>() — imax, then imax → size_t

double e = double(b);      // explicit (rounding-gated operator double())

using gain = bound<{{0, 4}, notch<1, 65536>}, round_nearest | real>;
double d = gain{0.5};      // implicit — a real bound's value is exact in double
```

For wide grids (Upper > imax_max) the implicit operators are SFINAE-disabled
— use the typed-error `to<T>()` instead:

```cpp
using wide = bound<{0, std::numeric_limits<std::uint64_t>::max()}>;
auto r = wide{huge}.to<std::uint64_t>();   // slim::expected<uint64_t, errc>
```

## Named extraction: `to<T>()` and `as<T>()`

`bound::to<T>()` returns `slim::expected<T, errc>` — sentinel-state, out of
T's range, and negative-into-unsigned each surface as a typed error:

```cpp
auto r = b.to<std::uint16_t>();
if (!r) { /* r.error() is errc::overflow or errc::domain_error */ }
```

`as<T>()` is the non-expected sibling — calls `to<T>().value()`. Use it when
the value is known in-range and you want to fail loud on a logic bug:

```cpp
narrow b{42};
auto v = b.as<std::int16_t>();   // 42; throws on sentinel-state / out-of-range
```

Both also exist as **free functions** — `to<T>(b)` / `as<T>(b)` (found by
ADL). In generic code the free form avoids the `template` disambiguator a
dependent member call would need (`b.template as<imax>()`):

```cpp
template <boundable B>
imax oracle(B a, B b) { return as<imax>(a) % as<imax>(b); }
```

> **Floating-point gate.** `as<double>()` (member and free) shares
> `operator double()`'s policy gate: a strict bound — one without a rounding
> flag — rejects both at compile time. `to<double>()` stays ungated; it is
> the explicit opt-in for strict bounds.

## Exact read-out: `numerator()` / `denominator()`

A fractional (Q-format) bound holds an exact value. To read it back out
exactly — without naming the internal representation — use the integer-pair
accessors. The numerator carries the sign; the denominator is positive; an
integer-notch bound reports a denominator of 1.

```cpp
bound<{{-4, 4}, notch<1, 16>}, round_nearest> g{0.1875};
g.numerator();     //  3
g.denominator();   // 16        →  exactly 3/16

bound<{0, 100}> hp{42};
hp.numerator();    // 42
hp.denominator();  //  1
```

For an *approximate* scalar, `to<double>()` / `as<double>()` (when the grid
carries a rounding policy) and the rounded `to<intN>()` are the right tools;
`numerator()`/`denominator()` are the only **exact** read-out.

## Free-function casts

Seven named casts complement the constructors. They make the intent
(clamp, wrap, throw, trust, or compose with a rounding mode) visible at the
call site — particularly useful inside `std::transform` lambdas.

| Cast | Behaviour |
|---|---|
| `clamp_cast<B>(v)`     | clamp to `[Lower, Upper]`, never throw |
| `wrap_cast<B>(v)`      | modular reduction into the target interval |
| `checked_cast<B>(v)`   | throw `std::system_error` on overflow or off-notch |
| `unchecked_cast<B>(v)` | trust the caller — UB if out of range |
| `clamp_floor<B>(v)`    | clamp + round toward −∞ |
| `clamp_ceil<B>(v)`     | clamp + round toward +∞ |
| `clamp_round<B>(v)`    | clamp + round to nearest |

```cpp
using pct = bound<{0, 100}>;

clamp_cast    <pct>(150);   // 100  (clamps regardless of B's declared policy)
wrap_cast     <pct>(105);   // 4    (modular reduction into [0, 100])
checked_cast  <pct>(42);    // 42   (throws on out-of-range or off-notch)
unchecked_cast<pct>(42);    // 42   (skips runtime checks)
```

For `double → bounded` pipelines (audio / graphics / DSP), the `clamp_*`
family composes clamping with a rounding mode:

```cpp
using coarse = bound<{{0, 10}, 2}>;
clamp_floor<coarse>(3.0);   // 2   (clamp + floor)
clamp_ceil <coarse>(3.0);   // 4   (clamp + ceil)
clamp_round<coarse>(3.0);   // 4   (clamp + round to nearest)
clamp_floor<coarse>(15.0);  // 10  (out-of-range clamps to upper)
```

### API boundary: `clamp | round_nearest`

For typed API boundaries — actuator commands, fused sensor outputs, anything
that takes "saturate and snap to my grid" semantics — the idiom is to put
`clamp | round_nearest` on the target bound's policy and write `T{value}`:

```cpp
// Before — explicit clamp_round at the boundary:
using output_t = bound<{-100, 100}, clamp>;
return clamp_round<output_t>(raw);

// After — policy carries the intent, `T{raw}` snaps automatically:
using output_t = bound<{-100, 100}, clamp | round_nearest>;
return output_t{raw};
```

The `clamp_*` free functions are still the right choice for one-off conversions
where the call site needs to override the bound's declared policy.
[examples/pid_controller.cpp](../examples/pid_controller.cpp) and
[examples/sensor_fusion.cpp](../examples/sensor_fusion.cpp) demonstrate the
boundary-policy form.

## Conversion predicates

Inspect a value *before* attempting an unsafe construction:

```cpp
will_conversion_overflow<pct>(150);    // true  — out of [0, 100]
will_conversion_truncate<pct>(3.5);    // true  — doesn't land on notch 1
is_conversion_lossy     <pct>(150);    // true  — overflow OR truncation
```

All three are pure inspection — none performs the conversion or has
side effects. See
[examples/histogram.cpp](../examples/histogram.cpp) for these as outlier
filters around a sample-collection loop.

## Idiom: writing literal values into bounds

Pick the shape that matches the context. All stay in bound-space — none
names the internal representation.

| Shape | When to use |
|---|---|
| Bare literal (`0`, `0.5`, `100`) | Constructing a bound (`pct{42}`, `gain{0.5}`), comparisons (`b == 5`, `b < 50`), or compound assignment (`b += 1`). Dyadic decimals (`0.5`, `0.25`, `0x1p-8`) are binary-exact and fine as grid endpoints. |
| `_b` literal (`0`, `0.5_b`, `0xff_b`) | A *bound* operand for arithmetic — `a + 1_b`, `a * 2_b`, `b > 0.5_b`. Gives a scalar a grid so it joins bound arithmetic; the result stays a bound. The parse is exact (no double round-trip). |
| `just<V>` | A compile-time point-bound from any structural NTTP value — `just<2>`, `just<math::pi>`. Same role as `_b` for non-literal constants. |
| `zero` / `one` | Built-in point-bounds for 0 / 1. Assign into any grid that can exactly represent the value (compile-time checked — out of range or off-notch is an error); also stand in for the value in comparison/arithmetic — `b == zero`, `b + one`. |
| `notch<N, D>` | The grid **step** in a `bound<{...}>` spec — `notch<1, 16384>`. |
| `frac<N, D>` | An exact **non-dyadic** grid endpoint that no floating literal can spell — `frac<-6, 5>` for −1.2, `frac<3, 5>` for 0.6. Signed numerator. |

Examples:

```cpp
// Construct + compare — bare / _b literals.
using pct = bound<{0, 100}>;
pct x = 42;
auto y = x + 1_b;                 // bound + bound, stays bounded
if (x > 50) { ... }              // bare scalar compare is fine

// Exact non-dyadic grid endpoints — frac<N,D> (1.2 and 0.6 are not dyadic):
using db_div20 = bound<{{frac<-6, 5>, frac<3, 5>}, notch<1, 40>}, round_nearest>;

// A runtime fraction n/16 without leaving bound-space: divide by a grid'd 16.
vel_t v{ bound<{-12, 12}>{n} / just<16> };
```

Dyadic decimals are exact as plain literals: `0.5` is exactly 1/2, `0x1p-8`
is exactly 1/256. Reach for `frac<N, D>` only when the value is *not* a
binary fraction (e.g. 1/3, 8/100, −6/5).

## Comparing bounds

`bound` compares directly with arithmetic types and other bounds — no
`static_cast` needed:

```cpp
bound<{0, 100}> a{42}, b{58};
REQUIRE(a == 42);
REQUIRE(a < 50);
REQUIRE(a + b == 100);
```

Mixed-type comparisons (`bound<G1> < bound<G2>`) compute on a common
representation chosen at compile time — no implicit narrowing.

## `std::print` / `std::format` integration

`bound/formatter.hpp` ships a `std::formatter` specialization for
`bound<G, P>`. Empty `{}` matches `operator<<` (exact value — a whole number
or an `N/D` fraction); non-empty specs route by storage shape — integer grids
go through `std::formatter<imax>` (`{:>4}`, `{:#x}`, `{:b}`, …), fractional
grids through `std::formatter<double>` (`{:.2f}`, `{:e}`).

```cpp
#include "bound/formatter.hpp"
#include <print>

bound<{0, 100}> hp{42};
std::println("HP = {}",       hp);       // 42
std::println("HP = {:>5}",    hp);       //    42
std::println("HP = {:#04x}",  hp);       // 0x2a

bound<{{0, 1}, notch<1, 16>}, round_nearest> g{0.625};
std::println("gain = {}",    g);          // 5/8
std::println("gain = {:.3f}", g);          // 0.625
```

See `examples/decibels.cpp` for the same pattern in a real Q-format
conversion routine.
