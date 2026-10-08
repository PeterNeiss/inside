# Roadmap — features gated on future C++ standards

Capabilities deliberately *not* implemented because the language doesn't yet
allow them, with the standard facility that unblocks each. Nothing here is a
promise of a date.

## Adoptable with C++26

### Rich, formatted `static_assert` messages
Compile-time diagnostics use static text. Embedding the offending value /
interval / notch needs C++26's constexpr formatting and user-generated
`static_assert` messages ([P2741]-family).

## Needs a language change beyond C++26

### A heap-backed, unbounded `bigratio` NTTP
Under C++26, grids past 64 bits already work by promoting limbs to static
storage ([storage.md](storage.md#grids-past-64-bits-c26)). A *heap-backed*
bignum as an NTTP stays impossible: an NTTP class must be structural, with
public members only ([temp.param]/7), so `std::vector` cannot qualify; a
pointer NTTP must designate static storage ([temp.arg.nontype]); and
non-transient `constexpr` allocation ([P1974], after C++20's transient
[P0784]) has stalled.

## Not gated on the standard

- **`dyn_inside`** (runtime-valued bounds) — declined on design grounds (no
  space/time-efficient implementation), not deferred.
- **Type-level interval unions** — Intel's [safe-arithmetic](resources.md)
  models disjoint unions in the type (`ival<-1000,-1> || ival<1,1000>`), so a
  zero-excluding divisor makes division total at compile time. `grid::operator/`
  already computes the two zero-free halves internally; expressing the union in
  the type would touch every operator and predicate, so it stays a sketch until a
  use case demands it.
- **Modules / compile-time footprint** — parked; modules are C++20, not a blocker.

[P2741]: https://wg21.link/p2741
[P0784]: https://wg21.link/p0784
[P1974]: https://wg21.link/p1974
