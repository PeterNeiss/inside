# The single header

The whole library is also available as one self-contained header at
[`single_include/beman/inside/inside.hpp`](../single_include/beman/inside/inside.hpp). It inlines
the entire `inside/` tree, so it needs only the C++ standard library —
there are no `inside/...` sub-includes left to resolve. Drop the one
file into a project, put `single_include/` on the include path, and use it exactly
as the full tree:

```cpp
#include <beman/inside/inside.hpp>   // single_include/ on the include path — nothing else needed
```

It is behaviourally identical to the multi-header form; `BEMAN_INSIDE_MATH_NO_FP`
and `BEMAN_INSIDE_MATH_TABLE_SLOTS` apply the same way, as ordinary compiler flags. The string/printing layer
is wrapped in a guard — define `BEMAN_INSIDE_NO_STRING` to drop it and its
`<string>`/`<ostream>`/`<format>` includes wholesale (see
[freestanding.md](freestanding.md)).

## On Compiler Explorer

Where there is no include tree to set up, the single header is the easy way in:

- paste it into a second source pane named `beman/inside/inside.hpp` and `#include` it; or
- pull it in with a single raw-URL include (Compiler Explorer resolves URL
  includes only for single-header libraries — which is exactly what this is):

  ```cpp
  #include <https://raw.githubusercontent.com/NiceAndPeter/inside/main/single_include/beman/inside/inside.hpp>
  ```

## Regenerating it

The committed single header is **generated, not hand-edited** — it is produced
from `include/` by a pure-CMake amalgamator (`cmake/amalgamate.cmake` — no Python
or other tooling). After editing any header under `include/`, regenerate and
commit it:

```bash
cmake --build build --target beman.inside.amalgamate  # rewrites single_include/beman/inside/inside.hpp
ctest --test-dir build -L tooling                  # amalgamate_up_to_date: fails if it drifted
ctest --test-dir build -R single_header_smoke        # compiles a TU seeing ONLY single_include/
```

`ctest` runs `amalgamate_up_to_date` as part of the normal suite, so a stale
single header fails the build until it is regenerated. CI additionally builds
`single_header_freestanding_smoke` (exceptions off, `BEMAN_INSIDE_NO_STRING`) and
`single_header_nofp_smoke` (a poison `<cmath>` shim proving `BEMAN_INSIDE_MATH_NO_FP`
pulls no floating-point header) against it.
