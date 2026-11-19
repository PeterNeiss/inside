# Contributing

How to build and test the library, what to run before a pull request, and
how the generated files are made. The library needs **GCC 16** in C++26 mode
with static reflection (`-freflection`); no other compiler has it yet.

## Build and test

The CMake workflow presets configure, build and test in one step:

```bash
cmake --workflow --preset gcc-debug      # Debug with sanitizers
cmake --workflow --preset gcc-release    # RelWithDebInfo
```

The `gcc-*` presets use the `g++` on the path, which must be GCC 16; the
`gcc16-*` presets name `g++-16` (their build directory is then
`build/gcc16-debug`, …). `cmake --list-presets=workflow` lists them.
GoogleTest is the installed one if CMake finds it, else a pinned commit
fetched with FetchContent (`tests/CMakeLists.txt`).

Configured by hand:

```bash
cmake -B build -S . -DCMAKE_CXX_COMPILER=g++-16 -DCMAKE_CXX_STANDARD=26
cmake --build build
ctest --test-dir build
```

### Options

| Option | Default | Effect |
|---|---|---|
| `INSIDE_BUILD_TESTS` | `ON` at top level | the tests |
| `INSIDE_BUILD_EXAMPLES` | `ON` at top level | the examples, each also a test |
| `INSIDE_BUILD_TOOLS` | `OFF` | the benchmarks, the perf workload and the targets that regenerate `docs/performance.md` and `docs/accuracy.md` |
| `INSIDE_CERTIFY_FULL` | `OFF` | the certify test checks every pair of `docs/certified.md` (up to 2³² inputs, minutes), not only those up to 2²⁰ |
| `INSIDE_SLOW_TESTS` | `OFF` | the slow tests that need a Debug build (`find_grids`, which reads the examples' math instantiations) |
| `INSIDE_WARNINGS_AS_ERRORS` | `OFF` | `-Werror` for the tests and examples |
| `INSIDE_MATH_NO_FP` | `OFF` | build without hardware floating point: the math engine's integer path computes every result |
| `INSIDE_FMA` | `ON` | add `-mfma` on x86-64 to everything that links `inside::inside` |
| `INSIDE_INSTALL` | `ON` at top level | install the headers and the files `find_package(inside)` needs |

`INSIDE_FMA` needs a Haswell/Piledriver (2013) or later CPU; turn it
off for older ones. The results do not depend on it, only the speed of the
math functions.

### Test labels

`ctest -L <label>` runs one group:

| Label | Runs |
|---|---|
| `example` | the 44 programs in `examples/` |
| `compile-fail` | `tests/fail/`: ill-formed uses that must keep their diagnostics; each case names the expected message on its first line (`// EXPECT: …`) |
| `fuzz` | the property fuzzer and the differential oracle (`tests/differential`), which recomputes arithmetic, stores and conversions exactly in Python |
| `accuracy` | the math accuracy sweep |
| `certify` | the certified math pairs of `docs/certified.md` |
| `tooling` | the single header is up to date, and no pre-C++26 compatibility code or old name came back |
| `perf` | the perf workload (with `INSIDE_BUILD_TOOLS=ON`) |

## Before a pull request

1. **Format** with the pinned tools: `pre-commit run --all-files`. It runs
   clang-format 22, gersemi for CMake and codespell. Use it rather than a
   system clang-format, whose version formats differently.
2. **Regenerate the single header** if a header under `include/` changed
   ([below](#the-single-header)), after formatting, since it copies the text.
3. **Run the configurations CI runs** (`.github/workflows/ci.yml`): Debug
   with sanitizers (`gcc-debug`), Release, `-DINSIDE_MATH_NO_FP=ON`, and
   Debug with `-DINSIDE_WARNINGS_AS_ERRORS=ON`. A weekly CI run adds the
   slow tests (`-DINSIDE_SLOW_TESTS=ON`), `-DINSIDE_CERTIFY_FULL=ON` and
   TSan.
4. **Update the docs and the changelog.** A user-visible change gets an entry
   under *Unreleased* in [CHANGELOG.md](CHANGELOG.md), and the pages that
   describe it change with it ([below](#documentation)).

## The single header

`single_include/inside/inside.hpp` is generated from `include/` by a
pure-CMake amalgamator (`tools/amalgamate.cmake`) and committed; it is never
edited by hand.

```bash
pre-commit run --all-files                       # clang-format first: the header copies its text
cmake --build build --target inside.amalgamate   # rewrites single_include/inside/inside.hpp
ctest --test-dir build -L tooling                # amalgamate_up_to_date fails if it drifted
ctest --test-dir build -R single_header          # translation units that see only single_include/
```

CI also builds `single_header_freestanding_smoke` (exceptions off,
`INSIDE_NO_STRING`) and `single_header_nofp_smoke`, which puts a poisoned
`<cmath>` first on the include path to prove that `INSIDE_MATH_NO_FP`
pulls in no floating-point header.

## Developer tools

`tools/` holds what no test runs: the amalgamator, the minimax generator, the
perf comparison scripts below, `symbol_lengths.sh` (how long the names of
`inside` and quantity types are, mangled and printed) and
`units_compile_time.sh`.

## Generated files

| File | Made by | Regenerate |
|---|---|---|
| `docs/performance.md` | `inside.bench` | `cmake --build <dir> --target inside.perf_report` (`INSIDE_BUILD_TOOLS=ON`, a Release build) |
| `docs/accuracy.md` | `inside.accuracy` | `cmake --build <dir> --target inside.accuracy_report` (`INSIDE_BUILD_TOOLS=ON`) |
| `docs/certified.md` | `inside.certify` | `cmake --build <dir> --target inside.certify_report` (an optimized build) |
| `include/inside/math/detail/minimax.hpp` | Sollya 8.0 | `python3 tools/minimax/gen.py` (about a minute) |

The certify test compares every result hash with `docs/certified.md`, so a
change to a math result shows up there;
[advanced/certify.md](docs/advanced/certify.md) explains the tool.

### The instruction-count gate

`tests/check_perf.py` runs the perf workload under cachegrind
and compares its instruction count with `tests/perf_baseline.json`
(needs valgrind and `INSIDE_BUILD_TOOLS=ON`). Instruction counts do not
depend on the machine's load; compare before and after on the same build, or
refresh the baseline with `--update`.

### Comparing two versions of the headers

The scripts in `tools/perf_gate/` compare a change with a
baseline, given as two include dirs (a baseline:
`git archive HEAD include | tar -x -C <dir>`). They need g++-16, and write to
`build/perf-gate/`. Each script's header says how to call it.

| Script | Measures |
|---|---|
| `irgate.sh <a> <b>` | the cachegrind instruction count of the perf workload's 13 keys, before and after (needs valgrind) |
| `gate.sh <label> <inc>`, `insdiff.py <a.md> <b.md>` | the benchmark table for one include dir; the rows whose ins/op moved |
| `kern.sh <inc> <kernels.cpp>` | per `k_*` function: hot-path instructions, divisions, stack canary, calls |
| `dyn.sh <inc> <driver.cpp>` | instructions per iteration of a loop, with `perf stat` |
| `matrix.sh <commit>` | a commit's Debug and `INSIDE_MATH_NO_FP` builds and tests, in a separate worktree |

## Documentation

The docs are in three levels, mapped in [docs/README.md](docs/README.md):
[Getting started](docs/getting-started.md), the everyday guides in
`docs/guide/`, and the details in `docs/advanced/`. A guide stays short and
links into the advanced pages for the rest.

- Every C++ block compiles against the current headers, and a value in a
  comment is the value the code produces.
- Links are relative, so they work on GitHub and in a checkout.
- The three draft papers are in `docs/proposal`: `make -C docs/proposal`
  builds them, and `make -C docs/proposal check` checks that their listings
  equal `docs/proposal/examples/*.cpp`.
