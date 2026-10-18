# Roadmap — features gated on future C++ standards

Capabilities deliberately *not* implemented because the language doesn't yet
allow them, with the standard facility that unblocks each. Nothing here is a
promise of a date.

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
- **Modules** — an experimental spike since 2026-10-18 ([modules.md](modules.md)):
  `import beman.inside;` and `import beman.inside.units;` behind
  `BEMAN_INSIDE_BUILD_MODULES` (off). Clang 20 builds all 38 examples against
  them; GCC 16 all but one, with limits in how it merges standard headers. An
  import saves the header parse (about 0.4 s per file), not the instantiation
  cost that dominates heavy files. Next step when GCC's header merging works,
  or when users ask: install the modules (the Beman infra supports it) and add
  a Clang CI cell. Until then they stay opt-in and uninstalled.

[P0784]: https://wg21.link/p0784
[P1974]: https://wg21.link/p1974
