# Roadmap — features gated on future C++ standards

A few capabilities are deliberately *not* implemented because the language doesn't yet
allow them (or allows them only on toolchains that haven't shipped). This page records
what they are, what standard facility unblocks each, and the current state of any
scaffolding already in the tree. Nothing here is a promise of a date — it's a map of
which doors the standard still has to open.

## Adoptable with C++26 (turn on when toolchains ship)

### "Every rational" grids — unbounded grid numbers via static promotion — **done**
Under C++23 an `inside`'s grid is built from `rational { umax Numerator; imax
Denominator; }` — two 64-bit integers, which cap how fine or how large a grid can be.

C++26 reflection plus `std::define_static_array` ([P3491]) **promotes** a
constexpr-computed limb array to static storage and yields a pointer that is a legal
constant-expression result. `detail::big_rational` (`detail/big_rational.hpp`) holds
such limbs, so under C++26 a grid's limits and notch have no size limit (see
[storage.md](storage.md#grids-past-64-bits-c26) and [internals.md](internals.md) §2a).

The two contingencies are settled on GCC 16 (`-freflection`): the facility ships, and
equal arrays intern to the same object — so with one canonical form per value
(reduced, no leading zero limbs, inline when it fits one limb) equal grids are the
same template argument. The headers detect reflection through
`__cpp_impl_reflection` and `<meta>` (that GCC snapshot does not define
`__cpp_lib_define_static_array`).

Big grid numbers are spelled with the `_g` literal (`1e-30_g`, `1267650600228229401496703205376_g`),
exact double limits (`0x1p100`) or grid arithmetic. The math functions take and return
such grids, correctly rounded ([math.md](math.md)). `per<D>`, `frac<N, D>` and the `_r` /
`_ins` literals stay 64-bit.

### Rich, formatted `static_assert` messages
Compile-time diagnostics currently use static text. Embedding the offending value /
interval / notch in a `static_assert` message needs C++26's constexpr formatting and
user-generated `static_assert` messages ([P2741]-family) — there is no portable mechanism
before then.

## Needs a language change *beyond* C++26

### A genuinely heap-backed, unbounded `bigratio` NTTP
The static-promotion path above sidesteps the limit by putting limbs in static storage.
A *truly* dynamic, heap-backed bignum as an NTTP remains impossible, blocked by three
independent language rules:

1. **Structural-type rule** ([temp.param]/7) — an NTTP class must expose every member
   publicly and structurally; `std::vector`/`std::string` (private pointers) can't qualify.
2. **Pointer NTTP values must designate static storage** ([temp.arg.nontype]) — the
   address of memory allocated during constant evaluation is not a permitted template
   argument.
3. **Non-transient `constexpr` allocation is unstandardized** — C++20 ([P0784]) allows
   *transient* allocation (freed before evaluation ends); promoting it to survive as a
   static object awaits [P1974], which has stalled.

You can *compute* with bignums at compile time today; you cannot *store* an unbounded one
as a template argument until P1974-style standardization lands. This is the one item on
this page that C++26 does **not** unblock.

## Not gated on the standard

For completeness — these came up alongside the above but are *not* blocked by the language:

- **Freestanding `<cmath>` removal** — **done.** `BEMAN_INSIDE_MATH_NO_FP` compiles the
  math engine's double tier — and its `#include <cmath>` — out wholesale, leaving the
  integer path to compute every result (results do not change). Auto-enabled under
  `-ffreestanding`; a CI smoke compiles the single header against a poison `<cmath>`
  shim to keep it that way. See [freestanding.md](freestanding.md#math-without-cmath-beman_inside_math_no_fp).
- **`dyn_inside`** (runtime-valued bounds) — evaluated and **declined** on design grounds
  (no space/time-efficient implementation), not deferred.
- **Type-level interval unions** — Intel's [safe-arithmetic](resources.md) models
  *disjoint* interval unions in the type (`ival<-1000,-1> || ival<1,1000>`), so e.g. a
  zero-excluding divisor makes division provably total at compile time. `inside`'s
  `grid::operator/` already computes the two zero-free halves internally
  (`grid.hpp`) — the missing piece is expressing the union in the *type* (a
  `inside` over a set of grids) rather than collapsing to the hull. Large surface
  (every operator/predicate would need a union story), so this stays a design
  sketch until a concrete use case demands it.
- **Wide rounded store** — **done.** The cold assignment path forms
  `(rhs − Lower)/Notch` as an exact 64-bit rational before rounding; a full-mantissa
  `double`-derived source on a grid with large `|Lower|` can need more than 64 bits
  *before* the round even though the rounded slot index is tiny. When that exact
  formation overflows, the store takes the slot from the exact wide index
  (`exact_index`, `detail/wide_value.hpp`), sized from the grid, with the same
  value-space rounding as every other store. (This replaced a 128-bit envelope that
  reported `errc::overflow` past it.)
- **Modules / compile-time-footprint work** — parked for later; modules is C++20, not a
  blocker.

<sub>Proposal references: [P1383] constexpr `<cmath>`; [P3491] `std::define_static_array`;
[P2741] user-generated `static_assert` messages; [P0784] (transient) constexpr allocation,
C++20; [P1974] non-transient constexpr allocation (stalled). Compiled from the project's
design notes — corrections welcome.</sub>

[P1383]: https://wg21.link/p1383
[P3491]: https://wg21.link/p3491
[P2741]: https://wg21.link/p2741
[P0784]: https://wg21.link/p0784
[P1974]: https://wg21.link/p1974
