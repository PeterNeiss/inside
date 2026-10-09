# Conversions

This page covers the conversion surface between `inside` and scalar
arithmetic types — the exact integer-pair read-out, the named casts, the
conversion predicates, the implicit conversion operators, and the idioms for
writing literal values into insides.

> **Note.** `beman::inside::rational` is an exact 64-bit fraction: the value
> of `0.1_r`, the raw of a continuous inside, and a runtime exact value you
> can construct an inside from or compare with. Its arithmetic returns
> `std::expected` (a fraction can outgrow 64 bits); stay in inside-space for
> arithmetic — that is where the no-overflow guarantee lives — and read exact
> values out with `numerator()` / `denominator()`.

## Implicit operator conversions on `inside`

| Conversion | When it applies | Purpose |
|---|---|---|
| `operator imax()` (implicit) | integer-notch grid (notch denom = 1) with Lower ≥ imax_min and Upper ≤ imax_max | drop-in for integer contexts: accumulators, comparisons, and indexing (`vec[b]` converts imax → size_t) |
| `operator double()` (**implicit** for an `f64`-policy inside, explicit otherwise) | `f64`: always (the double-exact grid makes every value exact in `double`). Others: grid carries a rounding policy (`round_*` or `snap`) | floating-point arithmetic / printf |

`operator imax()` is deliberately the **only** implicit integer conversion —
a second one (e.g. `size_t`) would make built-in mixed arithmetic like
`imax_var += b` ambiguous. Indexing goes through imax and the standard
imax → size_t conversion; if your build enables `-Wsign-conversion`, index
sites will surface that conversion as a warning.

```cpp
inside<{0, 100}> b{42};
if (b == 42) { ... }      // arithmetic compare, no conversion needed
if (b < 50)  { ... }      // works on insides and scalars

imax v = b;                // implicit (integer-shape grid)
std::vector<int> vec(101);
vec[b] = 0;                // no .as<>() — imax, then imax → size_t

double e = double(b);      // explicit (rounding-gated operator double())

using gain = inside<{{0, 4}, per<65536>}, round_nearest | f64>;
double d = gain{0.5};      // implicit — an f64 inside's value is exact in double (double-exact grid)
```

For wide grids (Upper > imax_max) the implicit operators are SFINAE-disabled
— use the typed-error `to<T>()` instead:

```cpp
using wide = inside<{0, std::numeric_limits<std::uint64_t>::max()}>;
auto r = wide{huge}.to<std::uint64_t>();   // std::expected<uint64_t, errc>
```

## Named extraction: `to<T>()` and `as<T>()`

`inside::to<T>()` returns `std::expected<T, errc>` — each failure surfaces as a
distinct typed error: out of T's range → `errc::overflow`,
negative-into-unsigned → `errc::domain_error`:

```cpp
auto r = b.to<std::uint16_t>();
if (!r) { /* r.error() is errc::overflow or errc::domain_error */ }
```

`as<T>()` is the non-expected sibling — calls `to<T>().value()`. Use it when
the value is known in-range and you want to fail loud on a logic bug:

```cpp
narrow b{42};
auto v = b.as<std::int16_t>();   // 42; throws on out-of-range
```

Both also exist as **free functions** — `to<T>(b)` / `as<T>(b)` (found by
ADL). In generic code the free form avoids the `template` disambiguator a
dependent member call would need (`b.template as<imax>()`):

```cpp
template <insidable B>
imax oracle(B a, B b) { return as<imax>(a) % as<imax>(b); }
```

> **Floating-point gate.** `as<double>()` (member and free) shares
> `operator double()`'s policy gate: a strict inside — one without a rounding
> flag — rejects both at compile time. `to<double>()` stays ungated; it is
> the explicit opt-in for a strict inside.

## Exact read-out: `numerator()` / `denominator()`

Every inside holds an exact value. To read it back out exactly — without
naming the internal representation — use the integer-pair accessors: the value
in lowest terms, the numerator carrying the sign, the denominator positive (1
on an integer notch). They return `imax` when every value of the grid fits it,
and otherwise a wide integer that holds every value (a wide grid, a grid
reaching past `int64`).

```cpp
inside<{{-4, 4}, per<16>}, round_nearest> g{0.1875};
g.numerator();     //  3
g.denominator();   // 16        →  exactly 3/16

inside<{0, 100}> hp{42};
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
| `checked_cast<B>(v)`   | report through the checked policy (by default, throw `beman::inside::inside_error`): `errc::overflow` when out of range, `errc::rounding_error` when off-notch |
| `unchecked_cast<B>(v)` | trust the caller — UB if out of range |
| `clamp_floor<B>(v)`    | clamp + round toward −∞ |
| `clamp_ceil<B>(v)`     | clamp + round toward +∞ |
| `clamp_round<B>(v)`    | clamp + round to nearest |

```cpp
using pct = inside<{0, 100}>;

clamp_cast    <pct>(150);   // 100  (clamps regardless of B's declared policy)
wrap_cast     <pct>(105);   // 4    (modular reduction into [0, 100])
checked_cast  <pct>(42);    // 42   (throws on out-of-range or off-notch)
unchecked_cast<pct>(42);    // 42   (skips runtime checks)
```

The cast's policy applies to every target and source shape: an `f64`
(double-backed) target is clamped or wrapped like any other, and an `f64`
source is read by value (`clamp_round<pct>(r)` with `r == 2.5` gives 3). The
same holds for the fluent forms — `b.with_clamp() = r`, `(r * k).with_snap()`.

For `double → bounded` pipelines (audio / graphics / DSP), the `clamp_*`
family composes clamping with a rounding mode:

```cpp
using coarse = inside<{{0, 10}, 2}>;
clamp_floor<coarse>(3.0);   // 2   (clamp + floor)
clamp_ceil <coarse>(3.0);   // 4   (clamp + ceil)
clamp_round<coarse>(3.0);   // 4   (clamp + round to nearest)
clamp_floor<coarse>(15.0);  // 10  (out-of-range clamps to upper)
```

The source may also be an **inside** on a finer or incompatible grid — the one-shot
rounding relaxes the notch check, so a fine value rounds onto the target:

```cpp
using small = inside<{{0, 10}, per<10>}, clamp>;
small a = 2.5;
clamp_round<small>(a * a);  // 6.3  (6.25 rounded onto the 1/10 grid, then clamped)
```

The fluent equivalent is the value form of `with_snap()` —
`(a * a).with_snap<round_nearest>()` — see
[arithmetic](arithmetic.md#rounding).

### API boundary: `clamp | round_nearest`

For typed API boundaries — actuator commands, fused sensor outputs, anything
that takes "saturate and snap to my grid" semantics — the idiom is to put
`clamp | round_nearest` on the target inside's policy and write `T{value}`:

```cpp
// Before — explicit clamp_round at the boundary:
using output_t = inside<{-100, 100}, clamp>;
return clamp_round<output_t>(raw);

// After — policy carries the intent, `T{raw}` snaps automatically:
using output_t = inside<{-100, 100}, clamp | round_nearest>;
return output_t{raw};
```

The `clamp_*` free functions are still the right choice for one-off conversions
where the call site needs to override the inside's declared policy.
[examples/pid_controller.cpp](../examples/pid_controller.cpp) and
[examples/sensor_fusion.cpp](../examples/sensor_fusion.cpp) demonstrate the
boundary-policy form.

## Conversion predicates

Inspect a value *before* attempting an unsafe construction:

```cpp
conversion_overflows<pct>(150);   // true  — out of [0, 100]
conversion_rounds   <pct>(3.5);   // true  — doesn't land on notch 1
conversion_is_lossy <pct>(150);   // true  — overflow OR rounding
```

All three are pure inspection — none performs the conversion or has
side effects. They are `noexcept` and never raise: a NaN or infinity counts as
overflow (`conversion_overflows` is true, `conversion_rounds` false), and a
continuous grid (notch 0) never truncates. See
[examples/histogram.cpp](../examples/histogram.cpp) for these as outlier
filters around a sample-collection loop.

## Idiom: writing literal values into insides

Pick the shape that matches the context. All stay in inside-space — none
names the internal representation.

| Shape | When to use |
|---|---|
| Bare literal (`0`, `0.5`, `100`) | Constructing or assigning an inside (`pct{42}`, `gain{0.5}`, `b = 7`) and comparisons (`b == 5`, `b < 50`). Not arithmetic or compound assignment — `b + 1` and `b += 1` are ill-formed. Dyadic decimals (`0.5`, `0.25`, `0x1p-8`) are binary-exact and fine as grid endpoints. |
| `_ins` literal (`1_ins`, `0.5_ins`, `0xff_ins`) | An inside operand for arithmetic and compound assignment — `a + 1_ins`, `a * 2_ins`, `b += 1_ins`, `b > 0.5_ins`. Gives a scalar a grid so it joins inside arithmetic; the result stays an inside. The parse is exact (no double round-trip). |
| `just<V>` | A compile-time point-inside from any structural NTTP value — `just<2>`, `just<math::pi>`. Same role as `_ins` for non-literal constants. |
| `zero` / `one` | Built-in point insides for 0 / 1. Assign into any grid that can exactly represent the value (compile-time checked — out of range or off-notch is an error); also stand in for the value in comparison/arithmetic — `b == zero`, `b + one`. |
| `per<D>` | The grid **step** 1/D: `per<16384>`. Other steps are `frac<N, D>` (`frac<360, 4096>`) or a plain integer (`{{0, 100}, 5}`). A step is always positive (`per<0>` and negative steps are errors; a continuous grid is spelled `0`). Optional with two limits: `{0.5, 10}` derives the step 1/2. |
| `frac<N, D>` | An exact **ratio** no literal can spell — `frac<1, 3>`, `frac<-6, 5>` for −1.2. Signed numerator, positive denominator. |
| `_r` literal (`0.1_r`, `1.25_r`, `0x1p-12_r`) | An exact **decimal or hex** value: `0.1_r` is exactly 1/10, unlike the double `0.1`. Use it for grid limits that are decimal fractions (`{0.1_r, 1}`), where a plain `0.1` is rejected. |

Examples:

```cpp
// Construct + compare — bare / _ins literals.
using pct = inside<{0, 100}>;
pct x = 42;
auto y = x + 1_ins;                 // inside + inside, stays bounded
if (x > 50) { ... }              // bare scalar compare is fine (exact, also for 50.5)

// Exact non-dyadic grid endpoints — frac<N,D> (1.2 and 0.6 are not dyadic):
using db_div20 = inside<{{frac<-6, 5>, frac<3, 5>}, per<40>}, round_nearest>;

// A runtime fraction n/16 without leaving inside-space: divide by a grid'd 16.
vel_t v{ inside<{-12, 12}>{n} / just<16> };
```

Dyadic decimals are exact as plain literals: `0.5` is exactly 1/2, `0x1p-8`
is exactly 1/256. A decimal that is *not* a binary fraction (0.1, 8/100, −1.2)
needs an exact spelling: `0.1_r`, `0.08_r`, `-1.2_r`, or `frac<N, D>` for a
ratio with no finite decimal (1/3). The two-limit form `{lo, hi}` derives its
notch from a plain floating-point limit only down to 1/1024, so
`inside<{0.1, 1}>` is a compile error that points to `0.1_r`.

## Reading text: `from_chars<B>` and `operator>>`

`from_chars<B>(text)` parses a number exactly — no `double` round-trip — and
returns `std::expected<B, errc>`. The text is one number: an optional sign, then
the literal grammar (`1'000`, `1.25`, `1.5e2`, `0xff`, `0b1010`, `0x1.8p3`) or a
fraction `N/D` — the forms `to_string` prints, so printed values read back.
A number past the 64-bit rational (a long decimal) is parsed exactly too.
The value then goes through `B::try_make`, so `B`'s policy applies:

```cpp
using pct = inside<{0, 100}>;
from_chars<pct>("42");      // 42
from_chars<pct>("0.1");     // errc::rounding_error — 1/10 is not on the integer grid
from_chars<pct>("150");     // errc::overflow
from_chars<pct>("4x");      // errc::invalid_format
from_chars<inside<{{0, 1}, per<10>}>>("0.1");   // exactly 1/10

inside<{0, 100}> a{0};
std::cin >> a;              // io.hpp: one token; failbit on error, a unchanged
```

Flags after the type add to `B`'s policy for that call, as `x.policy<F>()`
does; `from_chars_exact<B>` instead accepts only a value `B` holds exactly,
whatever `B`'s policy:

```cpp
using cents = inside<{{0, 1'000'000}, per<100>}>;
from_chars<cents>("1.005");                 // errc::rounding_error
from_chars<cents, round_nearest>("1.005");  // 1.01 (ties away from zero)

using price = inside<{{0, 1'000'000}, per<100>}, round_nearest | clamp>;
from_chars<price>("1.005");                 // 1.01: price rounds
from_chars_exact<price>("1.005");           // errc::rounding_error
from_chars_exact<price>("2000000");         // errc::overflow, not clamped
```

The core header takes a pointer pair, `from_chars<B>(first, last)`, and is
`constexpr`; `io.hpp` adds the `std::string_view` overload and `operator>>`.

## Comparing insides

`inside` compares directly with arithmetic types and other insides — no
`static_cast` needed:

```cpp
inside<{0, 100}> a{42}, b{58};
a == 42;          // true
a < 50;           // true
a + b == 100;     // true
```

Mixed-type comparisons (`inside<G1> < inside<G2>`) compute on a common
representation chosen at compile time — no implicit narrowing.

## `std::print` / `std::format` integration

`beman/inside/io.hpp` ships a `std::formatter` specialization for
`inside<G, P>`. Empty `{}` matches `operator<<` (the exact value — a whole
number, an exact decimal such as `0.625` however many digits, or a fraction
such as `7/3`); non-empty specs route by storage shape — integer grids
go through `std::formatter<imax>` (`{:>4}`, `{:#x}`, `{:b}`, …), fractional
grids through `std::formatter<double>` (`{:.2f}`, `{:e}`).

```cpp
#include <beman/inside/io.hpp>
#include <print>

inside<{0, 100}> hp{42};
std::println("HP = {}",       hp);       // 42
std::println("HP = {:>5}",    hp);       //    42
std::println("HP = {:#04x}",  hp);       // 0x2a

inside<{{0, 1}, per<16>}, round_nearest> g{0.625};
std::println("gain = {}",    g);          // 0.625 (exact)
std::println("gain = {:.3f}", g);          // 0.625
```

See `examples/decibels.cpp` for the same pattern in a real Q-format
conversion routine.
