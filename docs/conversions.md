# Conversions

This page covers the conversion surface between `bound`, `rational`, and
scalar arithmetic types — the named casts, the conversion predicates, the
implicit conversion operators, and the idioms for writing literal values
into bounds.

## Implicit operator conversions on `bound`

| Conversion | When it applies | Purpose |
|---|---|---|
| `operator rational()` (implicit) | always | exact value extraction — preferred at API boundaries that take `arithmetic` |
| `operator imax()` (implicit) | integer-notch grid (notch denom = 1) with Lower ≥ imax_min and Upper ≤ imax_max | drop-in for `int64_t` contexts (accumulators, indexing into signed-size containers) |
| `operator std::size_t()` (implicit) | integer-notch grid with Lower ≥ 0 and Upper ≤ imax_max | drop-in for `vec[bound_idx]` without `-Wsign-conversion` noise |
| `operator double()` (explicit) | grid carries a rounding policy (`round_*` or `ignore_round`) | floating-point arithmetic / printf |

```cpp
bound<{0, 100}> b{42};
if (b == 42) { ... }      // arithmetic compare, no conversion needed
if (b < 50)  { ... }      // works on bounds and scalars

imax v       = b;          // implicit (integer-shape grid)
std::size_t i = b;          // implicit (index-shape grid)
std::vector<int> v(101);
v[b] = 0;                  // no .as<>() — implicit size_t

double d     = double(b);  // explicit (operator double() is explicit)
```

For wide grids (Upper > imax_max) the implicit operators are SFINAE-disabled
— use the typed-error `to<T>()` instead:

```cpp
using wide = bound<{0, std::numeric_limits<std::uint64_t>::max()}>;
auto r = wide{huge}.to<std::uint64_t>();   // std::expected<uint64_t, errc>
```

## Named extraction: `to<T>()` and `as<T>()`

`bound::to<T>()` returns `std::expected<T, errc>` — sentinel-state, out of
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

For `rational`, prefer the named reductions over `static_cast<int>(r)`:

```cpp
rational r{7u, 2};      // 3.5
r.trunc();              //  3   (toward zero)
r.floor();              //  3   (toward −∞)
r.round();              //  4   (half away from zero)
```

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

## `rational` helpers

`rational` exposes a few standalone helpers in addition to the arithmetic
operators:

```cpp
rational::inv(rational{3u, 4});         // slim::optional<rational> -> 4/3
rational::inv(0_r);                     // nullopt (zero numerator)

divides_evenly(rational{6u}, rational{2u});   // true
divides_evenly(rational{7u}, rational{2u});   // false (7/2 is non-integer)
```

`inv` returns `nullopt` when the result would put the original numerator
into the denominator slot but it doesn't fit in `imax`. `divides_evenly`
treats division by zero as `true` (convention: everything divides 0 evenly).

## Idiom: `_r` literal vs `rational{N}` vs bare literal

Three shapes for writing a literal rational; pick the one that matches the
context.

| Shape | When to use |
|---|---|
| Bare literal (`0`, `0.5`, `100`) | The receiving API accepts `arithmetic` — comparisons against rational, bound construction with a round policy, scalar arguments to `add` / `clamp_round` / etc. The lightest form. |
| `_r` literal (`0_r`, `0.5_r`, `100_r`) | A `rational` value is *required* — initializing a `rational` variable, NTTPs, the LHS/RHS of `rational::add_unchecked`, etc. Drop-in for `rational{N}` and `rational{0.5}`. |
| `rational{N, D}` constructor | The fraction isn't a binary-exact dyadic — `rational{8, 100}`, `rational{1, 3}`, `rational{1, 16384}`. There's no single-literal form because `0.08` is not exactly 8/100 in double. |

Examples:

```cpp
// Comparison against rational — bare literal is enough.
static_assert(Lower<my_grid> == 0);
static_assert(Notch<my_grid> == 1);

// Need an actual rational object — use the literal.
rational two_thirds = 2_r / 3;
auto half = 0.5_r;

// Non-dyadic fraction — two-arg constructor.
constexpr rational tax_rate{8, 100};   // exact 2/25
constexpr rational q14{1, 16384};
```

`0.5_r` is exactly `rational{1, 2}` because `0.5` is binary-exact. Avoid
writing `0.08_r` if you mean 8/100 — the `long double` literal `0.08`
isn't exactly 2/25, so `0.08_r` is a rational with a 50-bit-ish denominator.

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
