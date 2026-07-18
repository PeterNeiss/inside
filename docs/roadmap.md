# Roadmap — features gated on future C++ standards

A few capabilities are deliberately *not* implemented because the language doesn't yet
allow them (or allows them only on toolchains that haven't shipped). This page records
what they are, what standard facility unblocks each, and the current state of any
scaffolding already in the tree. Nothing here is a promise of a date — it's a map of
which doors the standard still has to open.

## Adoptable with C++26 (turn on when toolchains ship)

### Constexpr double math engine
The default (double) `bnd::math` engine is runtime-only today; the integer/CORDIC
engine (`-DBND_MATH_FIXED`) is already `constexpr`. Constexpr `<cmath>`
([P1383], feature macro `__cpp_lib_constexpr_cmath`) makes the double engine
`constexpr` too.

The **gate is already planted**: `BND_MATH_FN` (`bound/cmath.hpp`) and `BND_DBL_FN`
(`bound/cmath_double.hpp`) expand to `constexpr` exactly when a toolchain defines the
macro, and to nothing otherwise — so the upgrade is automatic, no source change needed.

> Caveat: the engine uses `std::fma`, `std::sqrt`, and `std::nearbyint`. GCC already
> constant-folds `fma`/`sqrt` but not `nearbyint`; MSVC folds none. So activation is
> per-function and per-toolchain, not guaranteed by the standard version alone. A
> softfloat emulation that would enable it *today* was considered and rejected as not
> worth the weight — the library waits for the standard instead.

### "Every rational" grids — unbounded `bigratio` NTTP via static promotion
A `bound`'s grid is a non-type template parameter built from `rational { umax Numerator;
imax Denominator; }` — two 64-bit integers. That fixed width is the ceiling on how fine
or how large a grid can be: a combined denominator past `imax::max()` falls back to exact
storage or errors.

C++26 reflection plus `std::define_static_array` ([P3491]) **promotes** a
constexpr-computed limb array to *static* storage and yields a pointer that is a legal
constant-expression result. A structural type holding such pointers + lengths can then be
an NTTP with per-value length and no fixed capacity in the type — effectively *any*
rational as a grid parameter.

> Contingent on two things before it can be relied on: a toolchain shipping the facility
> for this project, and confirming the **NTTP pointer-interning guarantee** (two grids of
> equal value must intern to the *same* pointer, or they'd become distinct template
> arguments and break by-value grid identity). Feasibility-analysed only; not started.

A legal-**today** partial step exists independently: fixed-capacity inline limb arrays
(e.g. 128/256-bit `rational`) raise the practical ceiling without any language change —
tracked separately from this standards-gated path.

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

- **Freestanding `<cmath>` removal** — the core is already freestanding; only the math
  engine pulls `<cmath>`. Gating the double-engine include under `BND_MATH_FIXED` and
  swapping its three `std::` calls for `__builtin_*` would remove the header on GCC/Clang
  with no language dependency. See [freestanding.md](freestanding.md).
- **`dyn_bound`** (runtime-valued bounds) — evaluated and **declined** on design grounds
  (no space/time-efficient implementation), not deferred.
- **Type-level interval unions** — Intel's [safe-arithmetic](resources.md) models
  *disjoint* interval unions in the type (`ival<-1000,-1> || ival<1,1000>`), so e.g. a
  zero-excluding divisor makes division provably total at compile time. `bound`'s
  `grid::operator/` already computes the two zero-free halves internally
  (`grid.hpp`) — the missing piece is expressing the union in the *type* (a
  `bound` over a set of grids) rather than collapsing to the hull. Large surface
  (every operator/predicate would need a union story), so this stays a design
  sketch until a concrete use case demands it.
- **128-bit rounded store** — the cold assignment path forms `(rhs − Lower)/Notch` as
  an exact 64-bit rational before rounding; a full-mantissa `double`-derived source
  (denominator 2^52+) on a grid with large `|Lower|` can need 65+ bits *before* the
  round even though the rounded slot index is tiny. Mixed-sign offsets are rescued in
  128-bit and the rest report `errc::overflow` today; computing the *rounded* index
  directly in 128-bit (as `to_fixed` already does in `cmath.hpp`) would make those
  stores exact instead of an error.
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
