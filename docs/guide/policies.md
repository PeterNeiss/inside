# Policies

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

The second template argument of `inside<G, P>` is its **policy**: what
happens when a value does not fit the grid, because it is out of range or
between two notches.

```cpp
inside<{0, 100}>                     a{150};   // throws: the default policy reports it
inside<{0, 100}, clamp>              b{150};   // 100: saturates
inside<{0, 359}, wrap>               c{370};   // 10: wraps around
inside<{{0, 10}, 2}, round_nearest>  d{3.2};   // 4: rounds to the nearest notch
```

A value can only fail to fit when it is **stored**: on construction, `=`,
`+=` and the other compound assignments, and a cast. Arithmetic never fails
this way, because its result type is wide enough
([Arithmetic](arithmetic.md)).

## The flags

| Flag | When a value does not fit |
|---|---|
| (none) | report it: the default, also spelled `checked` |
| `clamp` | out of range: store the nearer limit |
| `wrap` | out of range: fold it back by the period, Upper − Lower + Notch |
| `round_nearest` | between notches: the nearest notch, a tie away from zero |
| `round_half_even` | between notches: the nearest notch, a tie to the even one |
| `round_floor`, `round_ceil` | between notches: toward −∞, toward +∞ |
| `round_toward_zero` | between notches: toward zero (also spelled `snap`) |
| `unsafe` | no checks at all: out of range is undefined behaviour, and a value between notches truncates |

Combine a range flag and a rounding flag with `|`:
`inside<{0, 100}, clamp | round_nearest>` saturates and rounds in one store.
That is the usual type for an API boundary, such as an actuator command.

- Every policy without `unsafe` still reports what it does not handle:
  `inside<{0, 100}, clamp>` rounds nothing, so a store of 3.5 is an error.
- `clamp` and `wrap` exclude each other, and a policy has at most one
  rounding mode.
- Rounding comes before the range check: 9.5 into
  `inside<{0, 9}, round_floor>` stores 9.

## One store under other flags

A type's policy applies to every store. For one store, name the flags there:

```cpp
using pct = inside<{0, 100}>;
pct p{150, clamp};                 // 100: a new value under clamp
pct q{3.7, round_nearest};         // 4

pct x{50};
x.policy<clamp>() = 150;           // 100: one assignment under clamp
x.policy<wrap>()  = 103;           // 2
```

The flags of the call replace the type's own of the same kind: a rounding
mode replaces the type's rounding mode, `clamp` or `wrap` its range handling.

## Without exceptions

A failed store throws `beman::inside::inside_error` by default. Three ways
report instead:

```cpp
auto m = pct::try_make(150);       // a std::expected: !m, m.error() == errc::overflow

errc ec{};
pct y{50};
y.policy(ec) = 200;                // ec == errc::overflow; y keeps 50
```

- `T::try_make(v)` returns `std::expected<T, errc>`. Under `clamp` or `wrap`
  it always succeeds.
- `x.policy(ec) = v` reports into an `errc&`. The first error is kept, and a
  failed store leaves the old value.
- A `clamp` or `wrap` policy has no range errors to report.

The error codes are `overflow` (does not fit), `rounding_error` (between
notches), `division_by_zero`, `domain_error` (outside a function's domain),
`not_finite` (a NaN or ∞ from a `double`) and `invalid_format` (text that is
not a number). With exceptions off, a failure goes to a handler you install;
[Hooks and errors](../advanced/hooks-and-errors.md) has the full picture.

## Hooks

A hook runs your code when a store clamps, wraps or fails. `x.on_wrap(f)`
returns a handle; assign through it:

```cpp
inside<{0, 59}, wrap> seconds{0};
inside<{0, 59}>       minutes{0};
seconds.on_wrap([&](auto&, auto carry) { minutes += carry; }) = 125;
// seconds == 5, minutes == 2
```

| Hook | Runs when | Called as |
|---|---|---|
| `x.on_clamp(f)` | a value is saturated | `f(x, overshoot)` |
| `x.on_wrap(f)` | a value is folded back | `f(x, carry)`: the number of periods |
| `x.on_error(f)` | a checked store fails; replaces the throw | `f(x, errc, message)` |

The carry and the overshoot are insides, so they add straight into another
inside. A hook you do not call costs nothing.
[`clock.cpp`](../../examples/clock.cpp) builds an s → min → h → day cascade
this way.

## Policies of results

A result keeps the policy its operands share. With
`inside<{0, 10}, round_floor> a{7}`, `a / 2_ins` is 3: the division rounds
down, because `a` does. A constant such as `2_ins` counts as neutral.

## Which channel reports a failure

| Operation | A failure goes to |
|---|---|
| a store: construction, `=`, `+=`, …, `as<T>()`, `checked_cast` | the store's hook or `errc&` if it has one, else the throw |
| `try_make`, `to<T>()`, `parse` | the returned `std::expected` |
| `/` and `%` by a grid that holds 0 | the returned `std::expected` |

An inside never stores a `std::expected`: check it and store `*e`, or pass it
to `T::try_make(e)`, which passes the error on.

## Details

- [Hooks and errors](../advanced/hooks-and-errors.md): the carry and overshoot
  grids, hooks on quantities, the error handler, every channel.
- [Exact arithmetic](../advanced/exact-arithmetic.md#rounding): how each
  rounding mode rounds, on every kind of grid.
