# Freestanding & bare-metal

`bound` is designed to run on freestanding / bare-metal targets with a minimal
standard-library surface. The core carries **no `<system_error>` dependency**, never
builds an `std::string` on its own surface, and replaces exceptions with a single
**replaceable error handler**. This page explains how to build for such a target and
where the current limits are.

> **TL;DR** — Compile with `-fno-exceptions`, don't include `bound/io.hpp` (or define
> `BND_NO_STRING` for the single header), and install a `bnd::set_error_handler`. The
> core arithmetic library then needs no hosted-only header. Transcendental math is
> included: under `BND_MATH_NO_FP` (auto-enabled by `-ffreestanding`) the `<cmath>`
> dependency is compiled out entirely — see
> [Math without `<cmath>`](#math-without-cmath-bnd_math_no_fp).

## What you get

The core umbrella — `bound/bound.hpp` plus the free-function layers
`bound/casts.hpp`, `bound/arithmetic.hpp`, `bound/range.hpp` — compiles under
`-ffreestanding` (verified on GCC 15 / libstdc++) once exceptions are off:

```sh
g++ -std=c++23 -ffreestanding -fno-exceptions -I include -fsyntax-only my_tu.cpp
```

No `<system_error>`, no `<string>`/`<ostream>`/`<format>`, no `<stdexcept>` reach the
core in that configuration.

## The two build conditions

### 1. Exceptions off → install a handler

Every checked failure (out-of-range assignment, division by zero, rounding mismatch,
overflow, non-finite input) funnels through `bnd::detail::raise`, which calls the
installed handler. The **default** handler throws `bnd::bound_error` (carrying the
`bnd::errc`) — which needs `<stdexcept>`, a hosted header. Compiling with
`-fno-exceptions` removes that include and makes the default handler **trap** instead.

For anything other than "trap on error", install your own handler:

```cpp
#include "bound/bound.hpp"

// Contract: the handler MUST NOT return (raise() traps if it does). Redirect a
// failure to a log/reset/longjmp — a real target would not spin forever.
[[noreturn]] void on_bound_error(bnd::errc code, const char* what) noexcept
{
    board_log(static_cast<int>(code), what);   // `what` = static message text
    board_fault_reset();
    for (;;) {}
}

int main()
{
    bnd::set_error_handler(&on_bound_error);    // returns the previous handler
    // ... bnd::get_error_handler() reads the current one;
    //     bnd::set_error_handler(nullptr) restores the default.
}
```

`what` is a static, null-terminated `const char*` (`bnd::errc_message(code)` by
default) — no allocation, no `<string>`. See
[Replacing the throw handler](policies.md#replacing-the-throw-handler-freestanding--bare-metal)
in the policies guide.

### 2. Drop the string/printing layer

All stringification — `to_string`, `operator<<`, and the `std::formatter`
specializations — lives in the opt-in header `bound/io.hpp`, the only place that pulls
`<string>`, `<ostream>`, and `<format>`. **Simply don't include `bound/io.hpp`** and
the core never sees those headers.

For the [single-header amalgamation](single-header.md), that block is wrapped in
a guard — define `BND_NO_STRING` to drop it (and its heavy includes) wholesale:

```cpp
#define BND_NO_STRING
#include "bound/bound.hpp"   // single_include/bound/bound.hpp
```

You give up `bnd::to_string` / `operator<<` / `std::format`; report state through the
error handler and the error-code channel instead.

## Error reporting without exceptions

Besides the handler, errors can be captured in a `bnd::errc` out-parameter — the
throw-free reporting channel, with **no `std::error_code` / `<system_error>`**:

```cpp
bnd::errc ec{};                    // value-init: errc{} == 0 means "no error"

bnd::bound<{0, 100}> x(150, ec);   // construction
y.policy(ec) = 200;                // per-operation
auto s = add(y, y, ec);            // free arithmetic

if (ec != bnd::errc{})             // first error is sticky
    handle(ec);                    // bnd::errc_message(ec) -> const char*
```

See [Error code mode](policies.md#error-code-mode) for the full surface. `clamp` /
`wrap` / `sentinel` policies and `slim::optional` / `slim::expected` results are all
non-throwing and work unchanged on freestanding.

## Math without `<cmath>` (`BND_MATH_NO_FP`)

The transcendental math API (`bnd::math::sin/cos/exp/log/sqrt/pow/atan/…` in
**`bound/cmath.hpp`**) works on freestanding targets too. Define **`BND_MATH_NO_FP`**
and the floating-point engines — including their `#include <cmath>` — are compiled out
**entirely**, leaving the always-present integer/CORDIC engine to serve the full
`bnd::math` API. The public surface, output grids, and types are unchanged; only the
compute backend differs.

- **Auto-enabled** when `__STDC_HOSTED__ == 0` (i.e. `-ffreestanding`), and **implied
  by `BND_MATH_FIXED`** — selecting the integer engine is itself an FP-free build.
- Holds for the modular headers **and** the amalgamated
  [single header](single-header.md). A CI smoke (`single_header_nofp_smoke`) compiles
  the single header with a *poison* `<cmath>` shim first on the include path, so the
  build fails if any `<cmath>` sneaks in.
- All transcendentals are `constexpr` under `BND_MATH_NO_FP`, so they evaluate at
  compile time as well as runtime.

See [Compiling without floating point](math.md#compiling-without-floating-point-bnd_math_no_fp)
in the math guide for the full story and the engine trade-offs.

## Older toolchains: C++20 / GCC 12 mode

The library also builds against **C++20 on GCC 12**: configure with
`-DBOUND_CXX20=ON`. In that mode the error channel uses the bundled
`slim::expected` backport instead of `<expected>`, and the `std::format`
integration is feature-gated off (`to_string()` / `operator<<` remain available)
— everything else is identical.

```bash
cmake -B build20 -DBOUND_CXX20=ON -DCMAKE_CXX_COMPILER=g++-12
cmake --build build20
```

## Limitations & caveats

- **Toolchain-dependent.** The "core is freestanding" result was verified on GCC 15 /
  libstdc++, which makes `<ranges>`, `<algorithm>`, `<optional>`, `<memory>`,
  `<functional>`, `<tuple>`, `<numeric>`, `<string_view>`, … freestanding (C++26
  P2407/P2738). On older libstdc++ or libc++ several of these are still hosted and
  would also block `-ffreestanding`; the library does not work around that.
- **Compile-time vs. link-time.** `-ffreestanding` here is a *compile* property. A real
  bare-metal **link** additionally needs: libm symbols that the FP path may call
  (`sqrt`, `fma` — not needed under `BND_MATH_NO_FP`); `abort` / `__builtin_trap` for
  the no-exceptions failure path; and a
  startup/runtime that does not assume a hosted environment.
- **Exceptions on + freestanding don't mix** out of the box — the default throwing
  handler needs `<stdexcept>`. Use `-fno-exceptions`, or replace the handler and avoid
  the throwing default.
- `slim::optional`'s heavyweight standard-type specializations (`<chrono>`, `<thread>`,
  `<any>`, `<coroutine>`, `<span>`, `<stop_token>`, …) are already compiled out via
  `SLIM_OPTIONAL_LEAN_AND_MEAN`, which the library defines for its own use.

## Worked example

`tests/single_header_freestanding_smoke.cpp` is a complete TU that sees **only** the
amalgamated single header with the string block dropped and exceptions off. It installs
a trapping handler and exercises clamp/wrap, the error-code channel, and checked
arithmetic:

```sh
cmake --build <build-dir> --target single_header_freestanding_smoke
# compiled with: -DBND_NO_STRING -fno-exceptions, -I single_include only
```
