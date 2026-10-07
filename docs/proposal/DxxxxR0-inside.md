# `inside`: numbers whose range and step size are part of the type

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

| | |
|---|---|
| Document | DxxxxR0 (draft, no number assigned) |
| Date | 2026-10-07 |
| Audience | SG6 (Numerics), LEWG Incubator |
| Reply-to | Peter Neiss (<https://github.com/PeterNeiss>) |
| Reference implementation | [beman.inside v0.1.0](https://github.com/PeterNeiss/inside/tree/v0.1.0) |

## 1. Abstract

We propose a class template `inside<grid, policy>` for numbers whose interval
and step size (*notch*, an exact rational) are compile-time properties of the
type. Arithmetic computes its result grid at compile time, so `+`, `-` and `*`
cannot overflow; division is exact unless a rounding is asked for by name, and
reports failure as a value only where failure is possible. What happens when a
value is stored into a narrower grid (check, clamp, wrap, round) is part of the
declaration. A companion facility returns transcendental functions correctly
rounded onto any output grid.

The library is implemented, tested on GCC, Clang and Apple Clang with
libstdc++ and libc++, and documented; a long-form motivation with runnable
exhibits is [*int considered harmful*](../int-considered-harmful.pdf) (the
*paper* below).

## 2. Motivation

The built-in integer types carry no range, no unit and no failure value. The
paper reproduces, with compiled programs, eleven consequences: signed overflow
that lets the optimiser delete range checks (and, in one exhibit, the whole of
`main`), `-INT_MIN` and `abs(INT_MIN)` negative, `SIGFPE` on `INT_MIN / -1`,
the signed/unsigned comparison that iterates over an empty vector, promotion
and silent narrowing (the Ariane 5 shape), truncating division, the midpoint
bug, and cents added to dollars. `double` replaces these with a different set:
inexact decimals, accumulating drift, non-associativity and `NaN`.

The existing defences each recover part of the missing information:
sanitizers find overflow at runtime in instrumented builds; `-fwrapv` makes the
wrong answer reliable; checked-integer libraries detect overflow but not units,
rounding or a range narrower than the underlying type. C++26 adds saturation
arithmetic ([P0543]), which chooses one edge policy for the full range of a
built-in type.

The information each of these lacks is the same: the range a value may
occupy, its scale, the rounding rule, and the edge behaviour. `inside` records
it in the type, where the compiler can act on it.

## 3. Design overview

```cpp
using namespace beman::inside;

using pct    = inside<{0, 100}>;                                  // integers 0..100
using money  = inside<{{0, 1'000'000}, per<100>}, round_nearest>; // cents
using angle  = inside<{0, 359}, wrap>;                            // modular
using sample = inside<{{-1, 1}, per<16384>}, round_nearest>;      // Q1.14

pct a = 42, b = 58;
auto s = a + b;           // inside<{0, 200}>: the result grid is computed
auto m = (a + b) / 2_ins; // exact, and a plain value: 2 is not zero
```

- **Grid.** An interval `[lower, upper]` and a notch; every value is
  `lower + k · notch`. Limits and notch are exact rationals (`per<100>` is
  1/100, `0.1_r` is one tenth); a floating-point literal is taken at its exact
  binary value, and one that is not a short binary fraction is rejected.
- **Storage** is chosen from the grid: the narrowest integer that numbers every
  point (so `inside<{1000, 1255}>` is one byte), an exact fraction for a
  continuous grid, or `double`/`float` on request for grids exact in them.
- **Arithmetic widens.** `a + b` over `[0,100]` is `[0,200]`; negation mirrors
  the interval, so there is no `-INT_MIN`. No operation produces a value
  outside its result type, so overflow is not checked: it is unreachable.
- **Division** is exact by default (`7 / 3` is `2 1/3`). Rounding is named per
  call (`div(a, b, rounded_floor)`) or in the type. The result is
  `std::expected<inside, errc>` when the divisor's grid holds zero (or the exact
  quotient could exceed its 64-bit rational), and a plain `inside` when the
  grids prove neither can happen.
- **Policies** say what happens when a value enters a narrower grid:
  checked (the default), `clamp`, `wrap`, rounding modes, error codes, or
  callbacks (`on_clamp`, `on_wrap`, `on_overflow`) that receive the overshoot —
  a clock's seconds field carries into minutes with `on_wrap`.
- **Bare scalars do not mix in**: `a + 1` is ill-formed with a diagnostic that
  names the fix (`1_ins`, `just<1>`), because `1` has no grid.
- **Math.** `math::sin_into<Out>(x)` and its siblings return the correctly
  rounded point of `Out`; the auto forms deduce `Out` from the input. Results
  are identical at compile time and run time, on every platform, with or
  without an FPU.

## 4. Design decisions

- **Checked by default.** Every policy range-checks a narrowing store unless it
  says `unsafe`. The checks that remain are the ones the grids cannot
  discharge; on a chain of operations the library emits fewer checks than a
  hand-written checked integer (paper §7.4: zero instead of three, 22% faster).
- **`std::expected` only at boundaries.** Fallible results appear only as
  return values of operations that can fail at runtime; a total operation
  returns a plain value. This keeps the error vocabulary honest: the type says
  whether failure is possible.
- **One condition, one error code.** `errc::overflow` (does not fit),
  `domain_error` (outside a function's domain), `division_by_zero`,
  `rounding_error`, `not_finite`, `invalid_format`. No `<system_error>`; the
  failure handler is replaceable, so the core runs freestanding without
  exceptions.
- **Exact rationals, not binary fractions,** for the notch, so cents and thirds
  are exact. Floating-point storage is opt-in and rejected at compile time on a
  grid it cannot represent exactly.
- **Grids past 64 bits.** With C++26 static reflection
  (`std::define_static_array`) grid numbers have no size limit and equal grids
  remain the same type. Under C++23 grids are 64-bit and larger ones are
  rejected at compile time.
- **Correct rounding over speed for the math facility.** A correctly rounded
  result is a function of the input and the output grid alone, which is what
  makes compile-time and run-time results agree. At coarse output grids this is
  as fast as `<cmath>`; at the full resolution of a `double` it costs 8–18×
  (paper §7.6).

## 5. Implementation experience

The reference implementation is header-only C++23 (GCC 14+, Clang 19+, Clang 18
with libc++), with a single-header amalgamation, a CMake package and a vcpkg
port, and follows the Beman Standard. Its test suite (577 tests, 585 with C++26
reflection) runs in CI on 53 compiler/library/sanitizer configurations,
including a check that the integer fast paths compile to call-free,
vectorising code. Integer-aligned arithmetic compiles to the same instructions
as `int` (paper §7.1); `inside<unsafe>` runs at native speed and
`inside<checked>` at 87% of the fastest hand-written range-enforcing code
(paper §7.3).

## 6. Prior art

- **bounded::integer** (David Stone): integer ranges in the type with widening
  arithmetic; `inside` generalises the idea to rational grids.
- **Boost.SafeNumerics** (Robert Ramey) and **SafeInt**: overflow detection for
  built-in integers.
- **CNL** (John McFarlane) and its proposals: fixed-point numbers [P0037],
  elastic integers [P0828], composition of arithmetic types [P0554].
- **Intel safe-arithmetic**: interval unions and compile-time proofs.
- Numerics work in progress [P1889]; saturation arithmetic [P0543].

A feature comparison is in the repository's `docs/resources.md`.

## 7. Open questions

1. **Scope.** Should the core type and the correctly rounded math facility be
   separate proposals?
2. **Naming.** `inside`, `grid`, `notch`/`per`, and the policy names are the
   implementation's; the committee may prefer others (`bounded`, `ranged`).
3. **Failure reporting.** `std::expected` at boundaries plus a replaceable
   handler, versus contracts (P2900) for the checked policy.
4. **Minimum standard and reflection.** Grids past 64 bits depend on static
   reflection; a standard version could require it from the start.
5. **Interaction with C++26 saturation arithmetic** and with `std::linalg`
   element types.

## 8. Wording

Not yet. A synopsis of the proposed header, for orientation:

```cpp
namespace std {
  struct interval;                               // [lower, upper], exact rationals
  struct grid;                                   // interval + notch
  template <grid G, policy_flag P = /*checked*/> struct inside;

  enum class errc_inside { domain_error = 1, division_by_zero, overflow,
                           rounding_error, not_finite, invalid_format };

  // arithmetic: + - * return a widened inside; / and % return inside or
  // expected<inside, errc_inside> as described in §3
  template <class L, class R, class Policy> constexpr auto div(L, R, Policy);
  template <class L, class R, class Policy> constexpr auto mod(L, R, Policy);

  // conversions and predicates
  template <class B, class N> constexpr B clamp_cast(N);
  template <class B, class N> constexpr B wrap_cast(N);
  template <class B, class N> constexpr bool conversion_overflows(N);

  namespace math {                               // correctly rounded onto Out
    template <class Out, class In> constexpr Out sin_into(In);
    // ... cos, tan, exp, exp2, log, log2, log10, sqrt, cbrt, pow, hypot,
    //     atan2, asin, acos, atan, sinh, cosh, tanh, asinh, acosh, atanh
  }
}
```

## 9. References

- [P0037] J. McFarlane, *Fixed-Point Real Numbers*. <https://wg21.link/p0037>
- [P0543] J. Maurer, *Saturation arithmetic*. <https://wg21.link/p0543>
- [P0554] J. McFarlane, *Composition of Arithmetic Types*. <https://wg21.link/p0554>
- [P0828] J. McFarlane, *Elastic Integers*. <https://wg21.link/p0828>
- [P1889] A. Zaitsev, A. Polukhin, *C++ Numerics Work In Progress*. <https://wg21.link/p1889>
- [P2900] J. Berne, T. Doumler, A. Krzemieński et al., *Contracts for C++*. <https://wg21.link/p2900>
- P. Neiss, [*int considered harmful*](../int-considered-harmful.pdf), 2026.
- A. Ziv, *Fast evaluation of elementary mathematical functions with correctly
  rounded last bit*, ACM TOMS 17(3), 1991.

[P0037]: https://wg21.link/p0037
[P0543]: https://wg21.link/p0543
[P0554]: https://wg21.link/p0554
[P0828]: https://wg21.link/p0828
[P1889]: https://wg21.link/p1889
