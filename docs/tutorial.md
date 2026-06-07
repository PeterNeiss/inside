# bound — a mental model

This is the top-down tour: what `bound` *is*, the three ideas it rests on, and
how a value flows through a program. For per-topic depth see the other docs;
for the *why* behind the design see [internals.md](internals.md).

## The one-sentence model

A `bound` is **a number that carries its own grid** — a lower bound, an upper
bound, and a step size — all as exact fractions, checked at compile time. The
type *is* the contract; the runtime value can never leave it.

## Idea 1 — the grid

```cpp
using pct = bound<{0, 100}>;                         // integers 0..100
using sample = bound<{{-1, 1}, notch<1, 16384>}>;    // -1..1 in 1/16384 steps
```

A grid is `{ {Lower, Upper}, Notch }`. Three exact fractions, one invariant:
`(Upper − Lower) / Notch` is a whole number — there is an integer count of
notches across the interval. You write the fractions with integer/dyadic
literals, `notch<N, D>`, and `frac<N, D>`; the underlying exact-fraction type
is internal and you never name it.

Because the grid is part of the *type*, two differently-shaped bounds are
different types — the compiler tracks range and precision for you.

## Idea 2 — arithmetic widens, it never overflows

```cpp
pct a = 70, b = 58;
auto s = a + b;          // type is bound<{0, 200}> — computed at compile time
```

`+ - * /` compute the **result grid** at compile time to contain every value
the operation could produce. So `a + b` cannot overflow: its type is already
wide enough. The result is a *new* bound type; nothing is checked at runtime on
the happy path, and integer-aligned grids hit native-integer fast paths.

Scalars need a grid too — write `a + 1_b`, not `a + 1` (a raw `int` has no
grid). Bound-space `dot` / `cross` / `lerp` follow the same widening rule.

See [arithmetic.md](arithmetic.md).

## Idea 3 — policy governs the narrowing

Widening is free; the interesting decisions happen when a value is **assigned
or converted into a narrower grid**. That is the only place a value can fail to
fit, and a *policy* decides what happens:

```cpp
bound<{0, 100}, clamp> p = 150;     // p == 100  (saturate)
bound<{0, 359}, wrap>  deg = 370;   // deg == 10 (modular)
bound<{0, 100}>        q = 150;     // throws (default: checked)
```

Policies: `clamp`, `wrap`, `sentinel`, the `round_*` family, plus per-operation
callbacks (`on_clamp`, `on_wrap`, …) and a throw-free `std::error_code` mode.
See [policies.md](policies.md).

## How a value flows

```cpp
using mix = bound<{{-4, 4}, notch<1, 16384>}, round_nearest | clamp>;
sample a = read_input();          // 1. enter the bounded world once
auto    m = a * gain + offset;    // 2. widen through arithmetic (no overflow)
mix     out{m};                   // 3. narrow back via a policy at the sink
float   f = double(out);          // 4. leave to float only at the boundary
```

The pattern is: **convert in once, stay in bound-space, narrow at the sink.**
Exact-rational guarantees hold throughout steps 1–3; step 4 (`double(out)`) is
explicit precisely because it drops the guarantee.

## Reading values out

- `to<T>()` — typed, fallible conversion to a native integer.
- `numerator()` / `denominator()` — exact read-out, no rounding.
- implicit `operator imax()` / `operator std::size_t()` for integer-aligned
  grids (so a bound indexes an array directly).
- explicit `double(b)` — opt in to floating point.

See [conversions.md](conversions.md).

## Math on bounds

`bnd::math` is a `<cmath>`-shaped, `constexpr`, bit-exact function set over
bounds — `sin`/`cos`/`sqrt`/`exp`/`log`/`atan2`/… Angles are radians; output
grids auto-deduce from the input; runtime-conditional failures (a `tan` pole, a
negative `sqrt`) surface as `slim::expected`. See [math.md](math.md).

## Where to go next

| You want to… | Read |
|---|---|
| handle out-of-range / errors | [policies.md](policies.md) |
| understand result-grid rules | [arithmetic.md](arithmetic.md) |
| convert / cast / read out | [conversions.md](conversions.md) |
| iterate, store, use with the STL | [storage.md](storage.md) |
| call sin/cos/sqrt/… | [math.md](math.md) |
| know *why* it's shaped this way | [internals.md](internals.md) |
