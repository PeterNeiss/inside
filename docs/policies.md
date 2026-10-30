# Policies

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

The second template parameter of `inside<G, P>` says what happens when a value
does not fit: when it is out of range, or off the grid's notches, or when a
division meets 0. A policy is a set of **flag bits**, combined with `|`.

```cpp
using safe    = inside<{0, 100}>;                           // the default: checked
safe s = 150;                                               // throws beman::inside::inside_error

using clamped = inside<{0, 100}, clamp>;                    // saturate
clamped c = 150;                                            // 100
clamped d = -5;                                             // 0

using angle   = inside<{0, 360, bounds::half_open}, wrap>;  // modular: [0, 360), period 360
angle a = 370;                                              // 10
angle b = -10;                                              // 350

using index   = inside<{0, 9}>;
auto i = index::try_make(10);                               // no throw: !i, i.error() == errc::overflow
```

Out of range is only possible when a value is **stored** into a type:
construction, `=`, compound assignment, a cast. Arithmetic widens its result
grid instead (see [arithmetic.md](arithmetic.md)), so it cannot leave its range.

## Policy flags

| Flag | Effect |
|---|---|
| `checked` | report an out-of-range or off-notch store, or a division by 0. On for every policy without `unsafe` |
| `clamp` | saturate to the nearer limit |
| `wrap` | fold back modulo the period, Upper − Lower + Notch |
| `snap` | round an off-notch value onto the grid instead of rejecting it; alone, toward zero |
| `round_toward_zero` | `snap` alone, named |
| `round_nearest` | to the nearest notch, ties away from zero (includes `snap`) |
| `round_floor`, `round_ceil` | toward −∞, toward +∞ (include `snap`) |
| `round_half_even` | to the nearest notch, ties to the even point (includes `snap`) |
| `ignore_zero` | no division-by-zero check: `a / 0` is undefined, `/= 0` does nothing |
| `ignore_range` | no runtime range check |
| `unsafe` | `ignore_range | snap | ignore_zero`: no runtime checks at all; a wrong value is undefined behaviour |

- `clamp` and `wrap` exclude each other (a `static_assert`).
- Every policy without `unsafe` checks: `inside<G>`, `inside<G, round_nearest>`
  and `inside<G, ignore_zero>` all report a value outside the range. `clamp`
  and `wrap` handle the range, the rounding modes handle the notch.
- Spelling `checked` next to `unsafe` turns the checks back on.
- A rounding mode rounds **before** the range check. With
  `inside<{0, 9}, round_floor>`, 9.5 stores 9 and 10.0 is out of range. Without
  a rounding mode nothing rounds, so 9.5 into `inside<{0, 9}>` is out of range.
  How each mode rounds, on every kind of grid, is in
  [arithmetic.md](arithmetic.md#rounding).

No flag picks the raw storage: it follows from the grid alone
([storage.md](storage.md#why-no-representation-flags)).

> **At an API boundary**, put `clamp | round_nearest` on the target type and
> write `T{value}`: it saturates and rounds in one store, with no explicit
> cast ([conversions.md](conversions.md#api-boundary-clamp--round_nearest)).

## Per-store override

A type without a clamp, wrap or rounding policy can still get one for a
single store:

```cpp
inside<{0, 100}> x{50};
x.with_clamp() = 150;                   // 100
x.with_wrap()  = 103;                   // 2

inside<{{0, 10}, 2}> g{0};
g.with_snap<round_floor>()     = 3.0;   // 2
g.with_snap<round_ceil>()      = 3.0;   // 4
g.with_snap<round_half_even>() = 5.0;   // 4: the tie goes to the even point
```

`x.policy<F>()` adds any flags `F` the same way.

## Named policies

The free arithmetic functions (`add`, `sub`, `mul`, `div`, `mod`, `divmod`)
take a policy object as a last argument:

| Name | Flag |
|---|---|
| `rounded_toward_zero` | `round_toward_zero` |
| `rounded_nearest`, `rounded_floor`, `rounded_ceil`, `rounded_half_even` | the rounding mode of that name |
| `clamped`, `wrapped` | `clamp`, `wrap` |

```cpp
auto q = div(a, b, rounded_floor);      // the same as div(a, b, make_policy<round_floor>())
```

## Hooks: `on_wrap`, `on_clamp`, `on_error`, `on_overflow`

A hook runs your code when a policy acts. `x.on_wrap(f)` returns a handle; its
`=`, `+=`, `-=`, `*=`, `/=` and `%=` store into `x` with `f` wired in. A hook
also adds the flag it implies: `on_clamp` adds `clamp`, `on_wrap` adds `wrap`.
A hook you do not use costs nothing.

| Hook | Fires when | Called as |
|---|---|---|
| `on_clamp(f)` | a stored value leaves the range and is saturated | `f(x, overshoot)` |
| `on_wrap(f)` | a stored value leaves the range and is folded back | `f(x, carry)`; the carry is an inside |
| `on_error(f)` | a checked store fails (`overflow`, `rounding_error`); replaces the throw | `f(x, errc, const char* message)` |
| `on_overflow(f)` | an arithmetic result does not fit its storage | `f(x, errc)` |

The first three act on the store. `on_overflow` acts on the arithmetic before
it: the free `add`/`sub`/`mul`/`div`/`mod` and the operators inside a compound
assignment. `f` gets `x` by reference, so it can change the stored value. A
division by zero is no overflow: the free functions return it as
`unexpected(errc::division_by_zero)`, as `/` does, and a compound `/=` or `%=`
reports it through `on_error` (or skips it under `ignore_zero`).

```cpp
using sec = inside<{0, 59}, wrap>;
using min = inside<{0, 59}>;
sec seconds{0};
min minutes{0};

seconds.on_wrap([&](auto&, auto carry) { minutes += carry; }) = 125;
// seconds == 5, minutes == 2
```

The carry is an inside, so it adds straight into another inside. Its grid holds
every carry the source can produce: for `125_ins`, `[2, 2]`; for an `int`,
that type's limits divided by the period. A hook that takes `imax` instead of
`auto` works too.

The right-hand side of a hook's `+=` is an inside, a quantity or a `rational`.
A raw `int` or `double` is rejected, as for plain compound assignment: give it
a grid (`5_ins`, `just<5>`, or an inside over its range).

```cpp
using pos = inside<{{0, 64}, per<16>}, wrap | round_nearest>;
pos p{0};
int wraps = 0;
p.on_wrap([&](auto&, auto) { ++wraps; }) += 65.5_ins;
// p == 1.4375 (65.5 − 1025/16: the period is the span plus one notch), wraps == 1
```

The free functions take `on_overflow` too, for a result that does not fit;
a zero divisor stays in the `expected`:

```cpp
auto q = div(d, z, on_overflow([&](auto& result, errc e) { log(e); }));
if (!q) // q.error() == errc::division_by_zero
    ...
```

### Several hooks at once: `with(...)`

`x.with(hooks...)` packs hooks into one store. Each fires only on its own path,
so a compound `+=` can carry an `on_overflow` for its arithmetic and an
`on_clamp` for its store. Exclusive pairs (`on_clamp` with `on_wrap`) do not
compile.

```cpp
inside<{0, 100}> acc{50};
inside<{0, 1000}> big{900};
acc.with(on_overflow([&](auto& self, errc) { self = 0; }),
         on_clamp([&](auto&, auto over) { log_overshoot(over); })) += big;
// acc == 100, overshoot 850
```

### Hooks on quantities

A quantity (see [units.md](units.md)) has the same four hooks. The handle's
`=`, `+=` and `-=` take a quantity in any unit of the same kind and convert it
exactly first. The hook gets the **quantity**, then the inside's carry,
overshoot or errc:

```cpp
quantity<si::second, inside<{0, 60, bounds::half_open}, wrap>> s{50};
quantity<si::minute, inside<{0, 60, bounds::half_open}, wrap>> m{0};
s.on_wrap([&](auto&, auto carry) { m += carry * si::minute; }) += just<2> * si::minute;
// s == 50 s (170 s fold to 50 s), m == 2 min
```

[`clock.cpp`](../examples/clock.cpp) builds a whole s → min → h → day cascade
this way. [`game_hp.cpp`](../examples/game_hp.cpp) uses `on_clamp` on hit
points, and [`torus_map.cpp`](../examples/torus_map.cpp) uses `on_wrap` on pixel
positions.

## Error codes instead of exceptions

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

`errc{}` (0) means no error, and `errc_message(e)` gives a short text.

A store or a free function reports through an `errc&`:

```cpp
errc ec{};
inside<{0, 100}> y{50};
y.policy(ec) = 200;             // ec == errc::overflow; y keeps 50
auto sum = add(y, y, ec);       // an arithmetic failure lands in ec too

inside<{{0, 10}, 2}> coarse{0};
inside<{{0, 10}, 1}> fine{3};
coarse.policy<snap>(ec) = fine; // snap rounds; ec only sees a range error
```

The first error is kept; later ones do not overwrite it. A failed store leaves
the previous value.

## Construction that can fail: `try_make`

`try_make` returns `std::expected<inside, errc>` instead of throwing:

```cpp
auto maybe = inside<{0, 100}>::try_make(150);   // !maybe, maybe.error() == errc::overflow
auto sure  = inside<{0, 100}, clamp>::try_make(150);  // 100: a clamp or wrap policy always succeeds
```

`try_make<F>(v)` adds the flags `F` to the policy for that one store. Given a
`std::expected`, `try_make` passes its error on, so one fallible step feeds the
next without unwrapping:

```cpp
using volts = inside<{{-1, 1}, per<1'000'000>}, round_nearest>;
auto v = volts::try_make((x - offset) / gain);  // the division's error, or the store's
```

For a quantity, `Q::try_make<F>(v)` takes a number in Q's unit, and
`try_value_cast<Q, F>(q)` takes another quantity:

```cpp
using metres = quantity<si::metre, inside<{0, 1000}>>;
const quantity<si::kilo(si::metre), inside<{0, 5}>> leg{2};
auto a = metres::try_make(1200);        // !a: overflow
auto b = try_value_cast<metres>(leg);   // !b: 2000 m does not fit
```

## Replacing the throw handler

Every checked failure that is not caught by a hook or an `errc&` goes through
one handler. By default it throws `beman::inside::inside_error`; under
`-fno-exceptions` it traps. `set_error_handler` installs your own (see
[freestanding.md](freestanding.md#1-exceptions-off-install-a-handler)).
