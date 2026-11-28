# Policies

The second template argument of `id<key{G, P}>` is its **policy**: what
happens when a value does not fit the grid, because it is out of range or
between two notches.

```cpp
id<"[0..100] clamp">              b{150};      // 100: saturates
id<"[0..359] wrap">               c{370};      // 10: wraps around
id<"[0..10] 2 round_half_away">  d{3.2};   // 4: rounds to the nearest notch
id<"[0..100]">                     a{50};    // 50: a constant, checked while compiling
// id<"[0..100]">                  e{150};   // ❌ does not compile: 150 is outside
```

A value can only fail to fit when it is **stored**: on construction, `=`,
`+=` and the other compound assignments, and a cast. Arithmetic never fails
this way, because its result type is wide enough
([Arithmetic](arithmetic.md)).

Nothing throws. A store either clamps, wraps, rounds, takes an alternative
value, reports, or does not compile: where the grids cannot prove that a
stored value fits, the store names what happens, before it runs.

## The flags

| Flag | When a value does not fit |
|---|---|
| (none) | the default, also spelled `checked`: a store the grids cannot prove names a channel (below) |
| `clamp` | out of range: store the nearer limit |
| `wrap` | out of range: fold it back by the period, Upper − Lower + Notch |
| `round_half_away` | between notches: the nearest notch, a tie away from zero |
| `round_half_even` | between notches: the nearest notch, a tie to the even one |
| `round_floor`, `round_ceil` | between notches: toward −∞, toward +∞ |
| `round_toward_zero` | between notches: toward zero |
| `unsafe` | no checks at all: out of range is undefined behaviour, and a value between notches truncates |

Combine a range flag and a rounding flag with `|`:
`id<"[0..100] clamp|round_half_away">` saturates and rounds in one store.
That is the usual type for an API boundary, such as an actuator command.

- Every policy without `unsafe` still checks what it does not handle:
  `id<"[0..100] clamp">` rounds nothing, so a store of 3.5 needs a
  channel (and the constant 3.5 does not compile).
- `clamp` and `wrap` exclude each other, and a policy has at most one
  rounding mode.
- Rounding comes before the range check: 9.5 into
  `id<"[0..9] round_floor">` stores 9.

## One store under other flags

A type's policy applies to every store. For one store, name the flags there:

```cpp
using bounded = id<"[0..100]">;
bounded p{150, clamp};             // 100: a new value under clamp
bounded q{3.7, round_half_away};   // 4

bounded x{50};
x.with<clamp>() = 150;             // 100: one assignment under clamp
x.with<wrap>()  = 103;             // 2
```

The flags of the call replace the type's own of the same kind: a rounding
mode replaces the type's rounding mode, `clamp` or `wrap` its range handling.

## Reporting

Three channels report a failed store as a value:

```cpp
int  n = 200;                     // read at run time
auto m = bounded::try_make(n);    // a std::expected: !m, m.error() == error::overflow

error ec{};
bounded y{50};
y.on_error(ec) = n;               // ec == error::overflow; y keeps 50
```

- `T::try_make(v)` returns `std::expected<T, error>`. Under `clamp` or `wrap`
  it always succeeds (a NaN aside).
- `x.on_error(ec) = v` reports into an `error&`. The first error is kept, and a
  failed store leaves the old value.
- `x.on_error(f) = v` calls `f(x, error, message)`, which may store an
  alternative value.

The error codes are `overflow` (does not fit), `rounding_error` (between
notches), `division_by_zero`, `domain_error` (outside a function's domain),
`not_finite` (a NaN or ∞ from a `double`) and `invalid_format` (text that is
not a number). [Hooks and errors](../advanced/hooks-and-errors.md) has the
full picture, and the error handler that preconditions reach.

## Hooks

A hook runs your code when a store clamps, wraps or fails. `x.on_wrap(f)`
returns a handle; assign through it. On a clock's seconds and minutes:

```cpp
id<"[0..59] [s] wrap">  seconds{0, si::second};
id<"[0..59] [min] clamp"> minutes{0, si::minute};   // (+= carry names its range handling)
seconds.on_wrap([&](auto&, auto carry) { minutes += carry * si::minute; }) = 125_q * si::second;
// seconds == 5 s, minutes == 2 min
```

| Hook | Runs when | Called as |
|---|---|---|
| `x.on_clamp(f)` | a value is saturated | `f(x, overshoot)` |
| `x.on_wrap(f)` | a value is folded back | `f(x, carry)`: the number of periods |
| `x.on_error(f)` | a checked store fails; its channel | `f(x, error, message)` |

The carry is a number of periods and the overshoot is in the value's unit, so
they add straight into another value (a carry times its unit into a quantity). A
hook you do not call costs nothing.
[`clock.cpp`](../../examples/clock.cpp) builds an s → min → h → day cascade
this way.

## Policies of results

A result keeps the policy its operands share, and a constant such as `2_q`
counts as neutral. The policy acts when the result is stored: arithmetic
itself is exact. With `id<"[0..10] round_floor"> a{7}`, `a / 2_q` is
3.5, and storing it into `a`'s type gives 3.

## A store the grids cannot prove

A store is **proven** when every value of the source fits the target: an
`id<"[0..50]">` into an `id<"[0..100]">`, an `int8_t` into
`id<"[-128..127]">`, anything under `clamp` or `wrap` (a `double` aside:
NaN fits no range). Every other store names what happens, before it runs:

```cpp
using bounded   = id<"[0..100]">;
int        n    = 150;                          // read at run time
const auto step = 1_q;
bounded x{50};                                  // a constant: checked while compiling
auto    y = bounded::try_make(n);               // a run-time int: std::expected<bounded, error>
bounded z{n, clamp};                            // or clamp, wrap, a rounding mode
error ec{};
x.on_error(ec) += step;                         // an error&: the first error is kept
x.on_error([](auto& s, error, auto) { s = 0_q; }) += step;   // an alternative value
x = bounded::try_make(n).value_or(bounded{0});  // the same, as an expected
bounded u{n, unsafe};                           // the fit is your precondition
// x += step;                           // ❌ does not compile: x + step may pass 100
```

A loop that accumulates names the channel once: `acc.on_error(ec) += x` with
one `error` checked after the loop, or an accumulator type with `clamp`. A
`double` from outside needs a report channel (`try_make`, `on_error(ec)`,
`on_error`) or `unsafe`, since NaN fits no clamp.

## Which channel reports a failure

| Operation | A failure goes to |
|---|---|
| a store: construction, `=`, `+=`, …, `++` | the channel it names: an `error&`, `on_error`, `try_make`'s expected; with none, a store the grids cannot prove does not compile |
| `try_make`, `to<T>()` where it can fail, `parse` | the returned `std::expected` |
| `/` and `%` by a grid that holds 0 | the returned `std::expected` |
| a named output that can miss: `mul<Out>`, `sum<T>`, `exact_cast<T>`, `f<Out>(x)`, `value_cast` | the returned `std::expected` |
| a precondition: a cursor past its end, a raw read off the grid | the error handler, which prints and aborts by default |

An inside never stores a `std::expected`: check it and store `*e`, or pass it
to `T::try_make(e)`, which passes the error on.

## Details

- [Hooks and errors](../advanced/hooks-and-errors.md): the carry and overshoot
  grids, hooks on quantities, the error handler, every channel.
- [Exact arithmetic](../advanced/exact-arithmetic.md#rounding): how each
  rounding mode rounds, on every kind of grid.
