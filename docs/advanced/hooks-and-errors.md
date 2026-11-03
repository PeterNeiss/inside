# Hooks and errors

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

What runs when a store clamps, wraps or fails, every way an error is
reported, and how to replace the throw. [Policies](../guide/policies.md) has
the everyday view.

## Hooks

A hook runs your code when a policy acts. `x.on_wrap(f)` returns a handle;
its `=`, `+=`, `-=`, `*=`, `/=` and `%=` store into `x` with `f` wired in. A
hook also adds the flag it implies: `on_clamp` adds `clamp`, `on_wrap` adds
`wrap`. A hook you do not use costs nothing.

| Hook | Fires when | Called as |
|---|---|---|
| `on_clamp(f)` | a stored value leaves the range and is saturated | `f(x, overshoot)`; the overshoot is an inside |
| `on_wrap(f)` | a stored value leaves the range and is folded back | `f(x, carry)`; the carry is an inside |
| `on_error(f)` | a checked store fails (`overflow`, `rounding_error`), or a compound op does (a zero divisor in `/=` or `%=`); replaces the throw | `f(x, errc, const char* message)` |

`f` gets `x` by reference, so it can change the stored value. A store takes
one hook: clamping, wrapping and failing are different outcomes of the same
store. The free functions `add` … `divmod` take no hooks; they return a zero
divisor as `unexpected(errc::division_by_zero)`, as `/` does.

### The carry

The carry is an inside, so it adds straight into another inside, and a hook
reads it as a number with `as<imax>(carry)`. Its grid holds every carry the
source can produce: for `125_ins` into a period of 60, `[2, 2]`; for an `int`,
that type's limits divided by the period.

```cpp
using pos = inside<{{0, 64}, per<16>}, wrap | round_nearest>;
pos p{0};
int wraps = 0;
p.on_wrap([&](auto&, auto) { ++wraps; }) += 65.5_ins;
// p == 1.4375 (65.5 − 1025/16: the period is the span plus one notch), wraps == 1
```

### The overshoot

The overshoot is the source minus the limit it was clamped to, exactly, as an
inside. Its grid holds every value of the source kind minus either limit: an
integer type's range, an inside's grid, a `rational`'s 64-bit values, or a
`double`'s whole range down to its least subnormal (an inside of about 2100
bits, built only when the hook runs). 150.7 into `{0, 100}` reports the
double 150.7 minus 100, not 50. A `long double` is exact below 2¹⁰²⁴, the
range the library stores exactly; ±∞ reports the end of the overshoot's grid.

On a compound assignment the hook acts on the store back:

```cpp
inside<{0, 100}> acc{50};
inside<{0, 1000}> big{900};
acc.on_clamp([&](auto&, auto over) { /* over == 850 */ }) += big;   // acc == 100
```

### The right-hand side

The right-hand side of a hook's `+=` is an inside, a quantity or a
`rational`. A raw `int` or `double` is rejected, as for plain compound
assignment: give it a grid (`5_ins`, `just<5>`, or an inside over its range).

### Hooks on quantities

A quantity has the same three hooks. The handle's `=`, `+=` and `-=` take a
quantity in any unit of the same kind and convert it exactly first; `*=` and
`/=` take numbers. The hook gets the **quantity**, then the carry (a count of
periods), the overshoot (a quantity in the same unit) or the errc:

```cpp
quantity<si::second, inside<{0, 60, bounds::half_open}, wrap>> s{50};
quantity<si::minute, inside<{0, 60, bounds::half_open}, wrap>> m{0};
s.on_wrap([&](auto&, auto carry) { m += carry * si::minute; }) += just<2> * si::minute;
// s == 50 s (170 s fold to 50 s), m == 2 min
```

[`clock.cpp`](../../examples/clock.cpp) builds a whole s → min → h → day
cascade this way, [`game_hp.cpp`](../../examples/game_hp.cpp) uses `on_clamp`
on hit points, and [`torus_map.cpp`](../../examples/torus_map.cpp) `on_wrap`
on pixel positions.

## A flag as a call's policy

A flag is also the policy of one call. The free arithmetic functions take one
as a last argument, and so does a constructor's second argument:

```cpp
inside<{0, 100}> a{7}, b{2};
auto q = div(a, b, round_floor);                     // std::expected: 3
auto r = mod(a, b, round_nearest);                   // std::expected
inside<{0, 10}> h{3.5, round_floor};                 // 3
errc ec{};
auto e = div(a, b, make_policy<round_floor>(ec));    // a flag and an errc&
```

Each flag is a constant of its own type that converts to `policy_flag`, so the
same name is a template argument (`inside<G, round_floor>`), part of a
combination (`clamp | round_floor`) and a call's policy.

## Error codes

Errors can be reported as a `beman::inside::errc` instead of a throw. The
library does not use `std::error_code` or `<system_error>`.

| Code | Means |
|---|---|
| `overflow` | the value does not fit its destination: an inside's range, a native type (`to<T>()`), a 64-bit rational |
| `rounding_error` | the value is between two notches, and the policy does not round |
| `division_by_zero` | the divisor is 0 |
| `domain_error` | the argument is outside the function's mathematical domain: `sqrt` of a negative, `pow` of a negative base, a negative value into an unsigned `to<T>()`, bad `grid::try_make` parameters |
| `not_finite` | a NaN or an infinity came in from a `double` |
| `invalid_format` | text that is not a number (`parse`, `operator>>`) |

`errc{}` (0) means no error, and `errc_message(e)` gives a short static text.

A store or a free function reports through an `errc&`:

```cpp
errc ec{};
inside<{0, 100}> y{50};
y.policy(ec) = 200;               // ec == errc::overflow; y keeps 50

inside<{{0, 10}, 2}> coarse{0};
inside<{{0, 10}, 1}> fine{3};
coarse.policy<snap>(ec) = fine;   // snap rounds; ec only sees a range error
```

The first error is kept; later ones do not overwrite it. A failed store
leaves the previous value.

## Construction that can fail: `try_make`

`T::try_make(v)` returns `std::expected<T, errc>` instead of throwing, and
`try_make<F>(v)` adds the flags `F` for that one store. Given a
`std::expected`, it passes the error on, so one fallible step feeds the next
without unwrapping:

```cpp
using volts = inside<{{-1, 1}, per<1'000'000>}, round_nearest>;
inside<{-1000, 1000}> x{500}, offset{100};
inside<{0, 1000}> gain{400};
auto v = volts::try_make((x - offset) / gain);   // the division's error, or the store's: 1
```

For a quantity, `Q::try_make<F>(v)` takes a number in Q's unit, and
`try_value_cast<Q, F>(q)` takes another quantity:

```cpp
using metres = quantity<si::metre, inside<{0, 1000}>>;
const quantity<si::kilo(si::metre), inside<{0, 5}>> leg{2};
auto a = metres::try_make(1200);        // !a: overflow
auto b = try_value_cast<metres>(leg);   // !b: 2000 m does not fit
```

## Which channel reports a failure

Each operation reports through one channel:

| Operation | A failure goes to |
|---|---|
| a store: construction, `=`, `+=` and the other compound ops, `as<T>()`, `checked_cast` | the hook (`on_error`, or `on_clamp` / `on_wrap` where they apply), else the store's `errc&`, else the handler |
| `try_make`, `parse`, `to<T>()`, `grid::try_make` | the returned `std::expected` |
| `/` and `%` by a grid that holds 0, `rational` arithmetic | the returned `std::expected` |
| `rational r = p / q;` (a `rational` from an `expected`) | the handler, like a failed store |

An inside never stores a `std::expected`: `T x = a / b;`, `x = a / b;`,
`x += a / b;` and the policy handles' `=` are deleted. Check the `expected`
and store `*e`, or call `T::try_make(e)`, which passes its error on. No path
throws `std::bad_expected_access`; only calling `.value()` on an `expected`
yourself does.

## The error handler

Every failure that no hook or `errc&` takes goes through one handler. By
default it throws `beman::inside::inside_error` (a `std::runtime_error` whose
`Code` member is the `errc`); under `-fno-exceptions` it traps.
`set_error_handler` installs your own:

```cpp
[[noreturn]] void on_inside_error(beman::inside::errc code, const char* what) noexcept;

beman::inside::set_error_handler(&on_inside_error);   // returns the previous handler
// get_error_handler() reads it; set_error_handler(nullptr) restores the default
```

The handler must not return: log, reset, `longjmp`, or terminate. `what` is a
static string, so no allocation happens. There is one handler per program. It
is atomic, so one thread may install a handler while others raise, and it has
default visibility, so shared libraries built with `-fvisibility=hidden` see
the same one. [Freestanding](freestanding.md) shows it on bare metal.
