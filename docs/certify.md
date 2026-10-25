# Certifying the math of your own grids

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

The math engine returns the correctly rounded point of the output grid
wherever it can decide it, and a deterministic guess where it cannot
([math.md](math.md#what-is-guaranteed)). No bound known today rules the guess
out for an arbitrary grid: this is the table-maker's dilemma. For a grid
your program actually uses, though, the question has a definite answer, and
a machine can find it by computing every input. That is what the certify
tool does.

## What a certificate says

For one function and one pair of grids (input → output), the tool computes
**every input** twice:

1. as any run-time call does, through the `double` and double-double tiers
   where they apply;
2. on the integer path alone (`math::driver.IntegerOnly`), which is what
   constant evaluation and a `BEMAN_INSIDE_MATH_NO_FP` build compute.

The pair is **certified** when the two agree on every input and the integer
path decided every result before its precision cap. Then, for every input of
that pair:

- the result is the **correctly rounded** point under the output's rounding
  mode, not a guess; and
- it is the **same in every build**: at compile time, at run time, with and
  without an FPU, at any optimisation level (under the build rules of
  [determinism.md](determinism.md)).

It also prints a **SHA-256 hash of all the results**. Two machines that print
the same hash for a pair computed the same bits for every input. Run the tool
on another compiler, another OS, an ARM board or a `BEMAN_INSIDE_MATH_NO_FP`
build, and compare. That is a determinism check you can run yourself, on the
grids you ship.

The library's own certified pairs, with their hashes, are in
[certified.md](certified.md). Every CI configuration (GCC, Clang, AppleClang
on arm64, MSVC, the FP-free build) recomputes them and must match.

## Certify your program's grids

**1. Build your program unoptimized**, with GCC or Clang: a Debug build, or
add `-O0` (or `-fkeep-inline-functions`). Each math call instantiates
`beman::inside::math::adaptive::<fn>_into<Out, In>`, and an unoptimized build
keeps that instantiation as a symbol whose demangled name spells both grids
exactly. That includes outputs deduced by `math::sin(x)`, aliases, and the
numbers inside quantities.

**2. Find the pairs.** `find_grids.py` scans a folder and its subfolders for
object files, libraries and executables, and reads their symbols with `nm`:

```sh
python3 tests/beman/inside/certify/find_grids.py path/to/build
```

```text
find_grids: 76 files scanned, 25 math pairs found
  acos                 8193 inputs  {[-1, 1], 1/4096} round_nearest -> {[0, 3217/1024], 1/4096} round_nearest
  atan2          1073807361 inputs  {[-1, 1], 1/16384} round_nearest x {[-1, 1], 1/16384} round_nearest -> ...
  ...
```

**3. See what certification costs.** `--run` writes a certify program for the
pairs (`--emit FILE` names it), compiles it optimized, and runs it.
Arguments after `--` go to that program; `--estimate` times a sample of each
pair and stops:

```sh
python3 tests/beman/inside/certify/find_grids.py path/to/build --run -- --estimate
```

```text
certify: 20 of 25 pairs, 2.2 G inputs, about 7 min 34 s
  acos             8193 inputs                ~<1 s  {[-1, 1], 1/4096} round_nearest -> ...
  atan2           1.1 G inputs         ~2 min 53 s  {[-1, 1], 1/16384} round_nearest × ...
  atan2         274.9 G inputs  skipped ~12 h 58 min  {[-1024, 1024], 1/256} round_nearest × ...
  sin           4.6e+18 inputs  skipped ~9.2e+04 years  {[0, 4611686018427387903], 1} checked -> ...
```

**4. Certify.** Drop `--estimate`. On a terminal, the tool shows its progress
and the time left. Each pair ends with a line, and the table follows on stdout:

```text
[3/20] sin  61%  14.2 M/s  ETA 3 s (all: 6 min 50 s)
[3/20] certified sin in 5 s, about 6 min 45 s left
```

`--markdown FILE` also writes the table as a page. Keep it next to your code:
`--check FILE` later fails if any hash changes, which makes a regression test
of every math result your program can produce.

The certify program's options:

| Option | Effect |
|---|---|
| `--estimate` | list the pairs and the estimated time, then stop |
| `--max-slots N` | skip pairs with more than N inputs (default 2³², about an hour; 4096 in a debug build) |
| `--max-seconds S` | skip pairs estimated to take longer than S seconds |
| `--pair TEXT` | only the pairs whose "function input output" contains TEXT |
| `--markdown FILE` | also write the table as a Markdown page |
| `--check FILE` | fail unless every hash equals the one in FILE's table |
| `--dump` | print every result record (what the hash covers) |
| `--progress`, `--no-progress` | progress on stderr; the default is "when stderr is a terminal" |

`find_grids.py` takes `--nm` (an `nm` of your toolchain, such as `llvm-nm`),
`--cxx` and `--cxxflags` for the compile, `-I` for the beman.inside include
directory (default: this repository's), `--std` and `--json FILE` (the pairs
as JSON).

## Reading the table

| function | input | output | inputs | guesses | fp ≠ integer | SHA-256 of the results | verdict |
|:--|:--|:--|--:|--:|--:|:--|:--|
| `sin` | `{[-4, 4], 1/262144} round_nearest` | `{[-64, 64], 1/16} round_floor, clamp` | 2097153 | 0 | 0 | `bd2965…` | certified |

- **inputs** is the number of grid points of the input (the product, for
  `atan2`, `hypot` and `pow`).
- **guesses** counts results the integer path left undecided at its cap. Any
  guess fails the pair; the guess itself is still deterministic, but it may
  be the wrong neighbour.
- **fp ≠ integer** counts inputs where the floating-point tiers returned
  something other than the integer path. That would break "the same in every
  build", so any count fails the pair too. No pair has had one yet.
- **SHA-256** is defined by the results alone. Each input, in grid order
  (for two inputs, the first argument's points outer), contributes 17 bytes:
  a kind byte (0 for a value ≥ 0, 1 for a negative value, 2 for an `errc`),
  then the reduced value's numerator magnitude and denominator, 8 bytes
  little-endian each (for an `errc`, its code and 0). `--dump` prints the
  records; `tests/beman/inside/certify/check_hash.py` recomputes the hash with
  Python's `hashlib`.

## Your own pair list

A certify program is a list of pairs handed to `certify::main`.
`find_grids.py --emit` writes one; you can also write or extend one by hand,
for example for a call that only runs at compile time (it leaves no symbol):

```cpp
#include "certify.hpp" // tests/beman/inside/certify; before any beman.inside header

using namespace beman::inside;
using angle = inside<{{-4, 4}, per<16384>}, round_nearest>;
using level = inside<{{-1, 1}, per<1024>}, round_floor>;

void pairs(certify::runner& r) {
    r.unary<level, angle>("sin", [](angle x) { return math::sin_into<level>(x); });
    r.binary<level, angle, angle>("atan2", [](angle y, angle x) { return math::atan2_into<level>(y, x); });
}
int main(int argc, char** argv) { return certify::main(argc, argv, pairs); }
```

Compile it optimized (`-O2 -DNDEBUG`) with `-I tests/beman/inside/certify` and
the beman.inside include directory. The repository's own list,
[`certify.test.cpp`](../tests/beman/inside/certify.test.cpp), is written the
same way.

## Limits

- **Size.** Exhaustion has to compute every input. A run computes a few
  million results a second per pair, so pairs up to about 2³² inputs (hours)
  are practical. Larger ones are skipped with their estimated time. A grid of
  2⁶⁰ points cannot be certified this way. For it, the guarantee is the one in
  [math.md](math.md#what-is-guaranteed): correctly rounded except within about
  2^−4W of a boundary, and the same in every build.
- **What the scanner cannot see.**
  - Calls inlined away by an optimized build, and calls evaluated only at
    compile time, leave no symbol. Build unoptimized, or add the pair by hand.
  - `units::sin_into` and `cos_into` of an angle in degrees or turns run the
    integer path alone and are reported, but not certified: the tool cannot
    rebuild their unit types.
  - Inputs on continuous grids have no finite set of inputs, and are
    reported as not certifiable. So is every pair of a C++26 build with big
    grids (`-freflection`), where every grid's fields are big numbers the
    scanner does not read. Scan a C++23 build of the same code, or one with
    `BEMAN_INSIDE_BIG_GRIDS=0`; the certify program itself runs under C++26.
- **MSVC.** Its symbols are not in the Itanium form the scanner reads. Scan a
  GCC or Clang build of the same code; the certify program itself builds with
  MSVC.
- **Inverse trigonometry into degrees.** `value_cast` of a radian result
  rounds a second time ([math.md](math.md#what-is-guaranteed)). A certified
  `asin` pair covers the radian result, not the conversion.
