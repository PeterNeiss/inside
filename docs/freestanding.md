# Freestanding and bare metal

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

inside runs on freestanding and bare-metal targets with a small standard
library. The core has **no `<system_error>` dependency**, builds no
`std::string` on its own, and reports errors through one **replaceable
handler** instead of exceptions. The math engine needs no FPU. This page shows
how to build for such a target, and where the limits are. It is also the home
of `BEMAN_INSIDE_MATH_NO_FP`.

**In short:**

- compile with `-fno-exceptions`, and install a handler with
  `set_error_handler`;
- don't include `io.hpp`, `units.hpp`, `constants.hpp` or `random.hpp` (with
  the single header, define `BEMAN_INSIDE_NO_STRING`);
- the math is included: under `-ffreestanding` `BEMAN_INSIDE_MATH_NO_FP` turns
  on by itself, and `<cmath>` is never included.

## What compiles freestanding

Measured with GCC 15 and libstdc++,
`g++ -std=c++23 -ffreestanding -fno-exceptions -fsyntax-only`:

| Header | Freestanding |
|---|---|
| `inside.hpp` (the umbrella), `casts.hpp`, `arithmetic.hpp`, `range.hpp`, `cursor.hpp` | yes |
| `cmath.hpp` (the math engine), `formats.hpp`, `predicates.hpp`, `numeric_limits.hpp` | yes |
| `io.hpp` | no: `<string>`, `<ostream>`, `<format>` |
| `units.hpp`, `units_math.hpp`, `constants.hpp` | no: they include io.hpp |
| `random.hpp` | no: `<random>` |

No `<system_error>`, `<string>`, `<ostream>`, `<format>` or `<stdexcept>`
reaches the core in that configuration. Units need a hosted library today.
Under `BEMAN_INSIDE_MATH_NO_FP` on a *hosted* build (no FPU, but a full
library) units and their math work unchanged: neither header uses floating
point. The mathematical constants (`math::pi`, …) come with cmath.hpp and are
freestanding too.

## Two build conditions

### 1. Exceptions off: install a handler

Every checked failure (an out-of-range store, a division by zero, a rounding
mismatch, an overflow, a non-finite input) goes through
`beman::inside::detail::raise`, which calls the installed handler. The
**default** handler throws `beman::inside::inside_error`, which needs
`<stdexcept>`, a hosted header. With `-fno-exceptions` that include goes away
and the default handler **traps** instead.

For anything other than a trap, install your own handler:

```cpp
#include <beman/inside/inside.hpp>

// The handler must not return (raise() traps if it does): log, reset, longjmp.
[[noreturn]] void on_inside_error(beman::inside::errc code, const char* what) noexcept {
    board_log(static_cast<int>(code), what);   // `what` is a static message
    board_fault_reset();
    for (;;) {}
}

int main() {
    beman::inside::set_error_handler(&on_inside_error);   // returns the previous handler
    // get_error_handler() reads it; set_error_handler(nullptr) restores the default
}
```

`what` is a static, null-terminated string (`errc_message(code)` by default):
no allocation, no `<string>`.

### 2. Drop the text layer

`to_string`, `operator<<`, `operator>>` and the `std::formatter`
specializations live in `io.hpp`, the only header that pulls in `<string>`,
`<ostream>` and `<format>`. Don't include it. With the
[single header](single-header.md), define `BEMAN_INSIDE_NO_STRING` before the
include; it drops the text layer, and with it the units and the physical
constants:

```cpp
#define BEMAN_INSIDE_NO_STRING
#include <beman/inside/inside.hpp>   // single_include/beman/inside/inside.hpp
```

Report state through the handler and the error codes instead.

## Errors without exceptions

Besides the handler, errors come back as values, without `std::error_code`:

- construction through `try_make`, an `std::expected<inside, errc>`;
- stores and free functions through an `errc&` out-parameter;
- `clamp` and `wrap` policies, which never fail;
- `std::expected` results of division and math. Test `has_value()` and read
  `error()`; don't call `.value()`, which would throw.

[policies.md](policies.md#error-codes-instead-of-exceptions) shows the surface.

## Math without `<cmath>` (`BEMAN_INSIDE_MATH_NO_FP`)

The math functions (`math::sin`, `cos`, `exp`, `log`, `sqrt`, `pow`, `atan`, …
in `cmath.hpp`) work on freestanding targets. Under `BEMAN_INSIDE_MATH_NO_FP`
the engine's `double` and double-double tiers, and their `#include <cmath>`,
are compiled out **entirely**; the integer path computes every result. The
interface, the output grids, the types **and the values** are unchanged:
the floating-point tiers only ever return the integer path's point
([math.md](math.md#what-is-guaranteed)).

- It turns on by itself when `__STDC_HOSTED__ == 0` (`-ffreestanding`), or set
  the CMake option `BEMAN_INSIDE_MATH_NO_FP=ON`, or define the macro.
- It holds for the modular headers and for the single header. The CI smoke
  `single_header_nofp_smoke` compiles the single header with a *poison*
  `<cmath>` first on the include path, so any `<cmath>` that sneaks in breaks
  the build. That smoke defines `BEMAN_INSIDE_NO_STRING` too, because libc++'s
  `<ostream>` includes `<format>`, which includes `<cmath>`.
- The whole test suite also runs with `BEMAN_INSIDE_MATH_NO_FP=ON` on a hosted
  build, units included.
- Every math function is `constexpr`, so it works at compile time too.

```bash
g++ -std=c++23 -ffreestanding -I single_include my_app.cpp              # NO_FP on by itself
g++ -std=c++23 -DBEMAN_INSIDE_MATH_NO_FP -I single_include my_app.cpp   # or forced
```

## Limits

- **It depends on the toolchain.** GCC 15's libstdc++ makes `<ranges>`,
  `<algorithm>`, `<expected>`, `<memory>`, `<functional>`, `<tuple>`,
  `<numeric>`, `<string_view>` and more freestanding (C++26 P2407, P2738).
  On older libstdc++ or on libc++ some of them are still hosted and block
  `-ffreestanding`; the library does not work around that.
- **Compile versus link.** `-ffreestanding` here is a compile property. A real
  bare-metal link also needs `abort` or `__builtin_trap` for the failure path,
  libm's `sqrt` and `fma` unless `BEMAN_INSIDE_MATH_NO_FP` is on, and a start-up
  that does not assume a hosted environment.
- **Exceptions on, freestanding** does not work out of the box: the default
  handler needs `<stdexcept>`. Use `-fno-exceptions`, or replace the handler.
- **Units are hosted only**, for now: units.hpp includes io.hpp for its text
  output ([roadmap.md](roadmap.md)).

## A worked example

`tests/beman/inside/single_header_freestanding_smoke.test.cpp` is a complete
translation unit that sees **only** the single header, with the text layer
dropped and exceptions off. It installs a trapping handler and runs clamp,
wrap, the error codes and checked arithmetic:

```sh
ctest --test-dir <build-dir> -R single_header_freestanding_smoke
# built with -DBEMAN_INSIDE_NO_STRING -fno-exceptions, -I single_include only
```
