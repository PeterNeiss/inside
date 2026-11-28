# Hooks and errors

What runs when a store clamps, wraps or fails, every way an error is
reported, and the handler that preconditions reach. Nothing throws: a store
the grids cannot prove names its channel, or does not compile.
[Policies](../guide/policies.md) has the everyday view.

## Hooks

A hook runs your code when a policy acts. `x.on_wrap(f)` returns a handle;
its `=`, `+=`, `-=`, `*=`, `/=` and `%=` store into `x` with `f` wired in. A
hook also adds the flag it implies: `on_clamp` adds `clamp`, `on_wrap` adds
`wrap`. A hook you do not use costs nothing.

| Hook | Fires when | Called as |
|---|---|---|
| `on_clamp(f)` | a stored value leaves the range and is saturated | `f(x, overshoot)`; the overshoot is an inside |
| `on_wrap(f)` | a stored value leaves the range and is folded back | `f(x, carry)`; the carry is an inside |
| `on_error(f)` | a checked store fails (`overflow`, `rounding_error`, `not_finite`), or a compound op does (a zero divisor in `/=` or `%=`); the store's channel | `f(x, error, const char* message)` |

`f` gets `x` by reference, so it can change the stored value: `on_error`'s
`f` can store an alternative value. A store takes one hook: clamping,
wrapping and failing are different outcomes of the same store. A `double`
through `on_clamp` or `on_wrap` is a constant (stored once while compiling,
then with the hook); a run-time `double` needs a report channel, since NaN
fits no clamp. The free functions `add` … `divmod` take no hooks; they return a zero
divisor as `unexpected(error::division_by_zero)`, as `/` does.

### The carry

The carry is an inside, so it adds straight into another inside, and a hook
reads it as a number with `carry.to<long long>()`. Its grid holds every carry
the source can produce, exactly: for `125_ins` into a period of 60, `[2, 2]`;
for an `int`, that type's limits divided by the period; for a `double`, its
whole range divided by the period (an inside past 64 bits). No carry
saturates.

```cpp
using ring = id<"[0..64] 1/16 wrap|round_half_away">;
ring p{0};
int  wraps = 0;
p.on_wrap([&](auto&, auto) { ++wraps; }) += 65.5_ins;
// p == 1.4375 (65.5 − 1025/16: the period is the span plus one notch), wraps == 1
```

### The overshoot

The overshoot is the source minus the limit it was clamped to, exactly, as an
inside. Its grid holds every value of the source kind minus either limit: an
integer type's range, an inside's grid, or a
`double`'s whole range down to its least subnormal (an inside of about 2100
bits, built only when the hook runs). 150.7 into `{0, 100}` reports the
double 150.7 minus 100, not 50 (a `double` reaches a clamp hook as a
constant). A `long double` is exact below 2¹⁰²⁴, the
range the library stores exactly; ±∞ reports the end of the overshoot's grid.

On a compound assignment the hook acts on the store back:

```cpp
id<"[0..100]"> acc{50};
id<"[0..1000]"> big{900};
acc.on_clamp([&](auto&, auto over) { /* over == 850 */ }) += big;   // acc == 100
```

### The right-hand side

The right-hand side of a hook's `+=` is an inside or a quantity. A raw
`double` is rejected, as for plain compound assignment: give it a grid
(`0.5_ins`, `just<1, 2>`, or an inside over its range).

### Hooks on quantities

A quantity has the same three hooks. The handle's `=`, `+=` and `-=` take a
quantity in any unit of the same kind and convert it exactly first; `*=` and
`/=` take numbers. The hook gets the **quantity**, then the carry (a count of
periods), the overshoot (a quantity in the same unit) or the error:

```cpp
id<"[0..60) [s] wrap"> s{50, si::second};
id<"[0..60) [min] wrap"> m{0, si::minute};
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
id<"[0..100]"> a{7}, b{2};
auto q = div(a, b, round_floor);                     // std::expected: 3
auto r = mod(a, b, round_half_away);                   // std::expected
id<"[0..10]"> h{3.5, round_floor};                 // 3
error ec{};
auto e = div(a, b, make_policy<round_floor>(ec));    // a flag and an error&
```

Each flag is a constant of its own type that converts to `policy_flag`, so the
same name is a template argument (`id<key{G, round_floor}>`), part of a
combination (`clamp | round_floor`) and a call's policy.

## Error codes

Errors are reported as a `ins::error`. The library does not use
`std::error_code` or `<system_error>`, and nothing throws.

| Code | Means |
|---|---|
| `overflow` | the value does not fit its destination: an inside's range, a native type (`to<T>()`) |
| `rounding_error` | the value is between two notches, and the policy does not round |
| `division_by_zero` | the divisor is 0, or a fraction's denominator is (`parse("1/0")`) |
| `domain_error` | the argument is outside the function's mathematical domain: `sqrt` of a negative, `pow` of a negative base |
| `not_finite` | a NaN or an infinity came in from a `double` |
| `invalid_format` | text that is not a number (`parse`, `operator>>`) |
| `missing_unit` | a quantity's text without its unit: `"12.5"` for metres |
| `unknown_unit` | a unit's text that names no unit visible at the call, or does not read as one |
| `incompatible_unit` | a unit that does not convert exactly to the quantity's: another dimension or kind, a factor of π, another absolute origin |

`error{}` (0) means no error, and `error_message(e)` gives a short static text.

A store or a free function reports through an `error&`:

```cpp
error ec{};
id<"[0..100]"> y{50};
y.on_error(ec) = 200;             // ec == error::overflow; y keeps 50

id<"[0..10] 2"> coarse{0};
id<"[0..10] 1"> fine{3};
coarse.on_error<round_toward_zero>(ec) = fine; // rounds; ec only sees a range error
```

The first error is kept; later ones do not overwrite it. A failed store
leaves the previous value.

## Construction that can fail: `try_make`

`T::try_make(v)` returns `std::expected<T, error>`, and
`try_make<F>(v)` adds the flags `F` for that one store. Given a
`std::expected`, it passes the error on, so one fallible step feeds the next
without unwrapping:

```cpp
using result = id<"[-1..1] 1/1'000'000 round_half_away">;
id<"[-1000..1000]"> x{500}, y{100};
id<"[0..1000]">     d{400};
auto r = result::try_make((x - y) / d);   // the division's error, or the store's: 1
```

For a quantity, `Q::try_make<F>(v, u)` takes a number and its unit, and
`value_cast<Q, F>(q)` takes another quantity:

```cpp
using metres = id<"[0..1000] [m]">;
const id<"[0..5] [km]"> leg{2, si::kilo(si::metre)};
auto a = metres::try_make(1200, si::metre);   // !a: overflow
auto b = value_cast<metres>(leg);         // !b: 2000 m does not fit
```

## Which channel reports a failure

Each operation reports through one channel:

| Operation | A failure goes to |
|---|---|
| a store: construction, `=`, `+=` and the other compound ops, `++` | the hook (`on_error`, or `on_clamp` / `on_wrap` where they apply), else the store's `error&`; a store the grids cannot prove, with neither, does not compile (a constant is checked while compiling) |
| `try_make`, `parse`, `to<T>()` where it can fail | the returned `std::expected` |
| `/` and `%` by a grid that holds 0 | the returned `std::expected` |
| a named output that can miss: `mul<Out>`, `div<Out>`, `sum<T>`, `exact_cast<T>`, `value_cast`, `from_step`, `f<Out>(x)`, mdspan `sum`/`dot`/`matrix_product`/`transform_into` | the returned `std::expected` (`expected<void, error>` for the mdspan writers) |
| a precondition: a cursor stepped past its end, a raw read off the grid | the error handler |

`to<T>()` is a plain `T` where the conversion is total, else an `expected`.
A named math output over grids whose range the library does
not bound (`exp` of `|x|` past 4096, `pow` of a base grid reaching 0) needs
`clamp` or `wrap`.

A grid number's arithmetic is compile time only: an overflow or a zero
divisor there is a compile error. At run time an exact value is an inside,
whose grid bounds every result.

An inside never stores a `std::expected`: `T x = a / b;`, `x = a / b;`,
`x += a / b;` and the policy handles' `=` are deleted. Check the `expected`
and store `*e`, or call `T::try_make(e)`, which passes its error on. No path
throws `std::bad_expected_access`; only calling `.value()` on an `expected`
yourself does.

## The error handler

A precondition that no channel can take (a cursor stepped past its end, a
raw value read off the grid) goes
through one handler. By default it writes `ins: <message>` to
`stderr` and calls `std::abort()`; a freestanding build traps.
`set_error_handler` installs your own:

```cpp
[[noreturn]] void on_inside_error(ins::error code, const char* what) noexcept;

ins::set_error_handler(&on_inside_error);   // returns the previous handler
// get_error_handler() reads it; set_error_handler(nullptr) restores the default
```

The handler must not return: log, reset, `longjmp`, terminate, or throw an
exception of your own (the library is exception-neutral). `what` is a static
string, so no allocation happens.

In a constant expression a failed check is a compile error that names it: with
exceptions on, `uncaught exception of type 'ins::detail::inside_error';
'what()': '…'` (a C++26 constexpr exception, never thrown at run time);
without, a call to `constexpr_error` whose template argument is the text. There is one handler per program. It
is atomic, so one thread may install a handler while others raise, and it has
default visibility, so shared libraries built with `-fvisibility=hidden` see
the same one. [Freestanding](freestanding.md) shows it on bare metal.
