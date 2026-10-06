# Freestanding & bare-metal

`inside` is designed to run on freestanding / bare-metal targets with a minimal
standard-library surface. The core carries **no `<system_error>` dependency**, never
builds an `std::string` on its own surface, and replaces exceptions with a single
**replaceable error handler**. This page explains how to build for such a target and
where the current limits are.

> **TL;DR** — Compile with `-fno-exceptions`, don't include `beman/inside/io.hpp` (or define
> `BEMAN_INSIDE_NO_STRING` for the single header), and install a `beman::inside::set_error_handler`. The
> core arithmetic library then needs no hosted-only header. Transcendental math is
> included: under `BEMAN_INSIDE_MATH_NO_FP` (auto-enabled by `-ffreestanding`) the `<cmath>`
> dependency is compiled out entirely — see
> [Math without `<cmath>`](#math-without-cmath-beman_inside_math_no_fp).

## What you get

The core umbrella — `beman/inside/inside.hpp` plus the free-function layers
`beman/inside/casts.hpp`, `beman/inside/arithmetic.hpp`, `beman/inside/range.hpp` — compiles under
`-ffreestanding` (verified on GCC 15 / libstdc++) once exceptions are off:

```sh
g++ -std=c++23 -ffreestanding -fno-exceptions -I include -fsyntax-only my_tu.cpp
```

No `<system_error>`, no `<string>`/`<ostream>`/`<format>`, no `<stdexcept>` reach the
core in that configuration.

## The two build conditions

### 1. Exceptions off → install a handler

Every checked failure (out-of-range assignment, division by zero, rounding mismatch,
overflow, non-finite input) funnels through `beman::inside::detail::raise`, which calls the
installed handler. The **default** handler throws `beman::inside::inside_error` (carrying the
`beman::inside::errc`) — which needs `<stdexcept>`, a hosted header. Compiling with
`-fno-exceptions` removes that include and makes the default handler **trap** instead.

For anything other than "trap on error", install your own handler:

```cpp
#include <beman/inside/inside.hpp>

// Contract: the handler MUST NOT return (raise() traps if it does). Redirect a
// failure to a log/reset/longjmp — a real target would not spin forever.
[[noreturn]] void on_inside_error(beman::inside::errc code, const char* what) noexcept
{
    board_log(static_cast<int>(code), what);   // `what` = static message text
    board_fault_reset();
    for (;;) {}
}

int main()
{
    beman::inside::set_error_handler(&on_inside_error);    // returns the previous handler
    // ... beman::inside::get_error_handler() reads the current one;
    //     beman::inside::set_error_handler(nullptr) restores the default.
}
```

`what` is a static, null-terminated `const char*` (`beman::inside::errc_message(code)` by
default) — no allocation, no `<string>`. See
[Replacing the throw handler](policies.md#replacing-the-throw-handler-freestanding--bare-metal)
in the policies guide.

### 2. Drop the string/printing layer

All stringification — `to_string`, `operator<<`, and the `std::formatter`
specializations — lives in the opt-in header `beman/inside/io.hpp`, the only place that pulls
`<string>`, `<ostream>`, and `<format>`. **Simply don't include `beman/inside/io.hpp`** and
the core never sees those headers.

For the [single-header amalgamation](single-header.md), that block is wrapped in
a guard — define `BEMAN_INSIDE_NO_STRING` to drop it (and its heavy includes) wholesale:

```cpp
#define BEMAN_INSIDE_NO_STRING
#include <beman/inside/inside.hpp>   // single_include/beman/inside/inside.hpp
```

You give up `beman::inside::to_string` / `operator<<` / `std::format`; report state through the
error handler and the error-code channel instead.

## Error reporting without exceptions

Besides the handler, errors come back as values — the throw-free reporting
channel, with **no `std::error_code` / `<system_error>`**: construction through
`try_make` (an `expected<inside, errc>`), assignment and free arithmetic through a
`beman::inside::errc` out-parameter:

```cpp
auto x = beman::inside::inside<{0, 100}>::try_make(150);   // construction: !x, x.error()

beman::inside::errc ec{};                    // value-init: errc{} == 0 means "no error"
y.policy(ec) = 200;                // per-operation
auto s = add(y, y, ec);            // free arithmetic

if (ec != beman::inside::errc{})             // first error is sticky
    handle(ec);                    // beman::inside::errc_message(ec) -> const char*
```

See [Error code mode](policies.md#error-code-mode) for the full surface. `clamp` /
`wrap` policies and `std::expected` results are all non-throwing and work unchanged
on freestanding (test `has_value()` / `error()` rather than calling `.value()`,
which would need to throw).

## Math without `<cmath>` (`BEMAN_INSIDE_MATH_NO_FP`)

The transcendental math API (`beman::inside::math::sin/cos/exp/log/sqrt/pow/atan/…` in
**`beman/inside/cmath.hpp`**) works on freestanding targets too. Define **`BEMAN_INSIDE_MATH_NO_FP`**
and the math engine's double tier — including its `#include <cmath>` — is compiled out
**entirely**; the integer path computes every result. The public surface, output grids,
types **and values** are unchanged: results are correctly rounded either way.

- **Auto-enabled** when `__STDC_HOSTED__ == 0` (i.e. `-ffreestanding`).
- Holds for the modular headers **and** the amalgamated
  [single header](single-header.md). A CI smoke (`single_header_nofp_smoke`) compiles
  the single header with a *poison* `<cmath>` shim first on the include path, so the
  build fails if any `<cmath>` sneaks in. It defines `BEMAN_INSIDE_NO_STRING` too:
  `io.hpp`'s streams bring `<cmath>` along on libc++, whose `<ostream>` includes
  `<format>`.
- All transcendentals are `constexpr` in every build, so they evaluate at compile time
  as well as runtime.

See [Compiling without floating point](math.md#compiling-without-floating-point-beman_inside_math_no_fp)
in the math guide for the full story.

## Limitations & caveats

- **Toolchain-dependent.** The "core is freestanding" result was verified on GCC 15 /
  libstdc++, which makes `<ranges>`, `<algorithm>`, `<expected>`, `<memory>`,
  `<functional>`, `<tuple>`, `<numeric>`, `<string_view>`, … freestanding (C++26
  P2407/P2738). On older libstdc++ or libc++ several of these are still hosted and
  would also block `-ffreestanding`; the library does not work around that.
- **Compile-time vs. link-time.** `-ffreestanding` here is a *compile* property. A real
  bare-metal **link** additionally needs: libm symbols that the FP path may call
  (`sqrt`, `fma` — not needed under `BEMAN_INSIDE_MATH_NO_FP`); `abort` / `__builtin_trap` for
  the no-exceptions failure path; and a
  startup/runtime that does not assume a hosted environment.
- **Exceptions on + freestanding don't mix** out of the box — the default throwing
  handler needs `<stdexcept>`. Use `-fno-exceptions`, or replace the handler and avoid
  the throwing default.

## Worked example

`tests/beman/inside/single_header_freestanding_smoke.cpp` is a complete TU that sees **only** the
amalgamated single header with the string block dropped and exceptions off. It installs
a trapping handler and exercises clamp/wrap, the error-code channel, and checked
arithmetic:

```sh
ctest --test-dir <build-dir> -R single_header_freestanding_smoke
# compiled with: -DBEMAN_INSIDE_NO_STRING -fno-exceptions, -I single_include only
```
