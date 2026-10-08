<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

# Development

## Configure and Build the Project Using CMake Presets

The simplest way of configuring and building the project is to use [CMake
Presets](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html). Appropriate
presets for major compilers have been included by default.  You can use `cmake
--list-presets=workflow` to see all available presets.

Here is an example of invoking the `gcc-debug` preset:

```shell
cmake --workflow --preset gcc-debug
```

Generally, there are two kinds of presets, `debug` and `release`.

The `debug` presets are designed to aid development, so they have debuginfo and sanitizers
enabled.

> [!NOTE]
>
> The sanitizers that are enabled vary from compiler to compiler.  See the toolchain files
> under ([`infra/cmake`](infra/cmake/)) to determine the exact configuration used for each
> preset.

The `release` presets are designed for production use, and
consequently have the highest optimization turned on (e.g. `O3`).

## Configure and Build Manually

If the presets are not suitable for your use case, a traditional CMake invocation will
provide more configurability.

To configure, build and test the project manually, you can run this set of commands. Note
that this requires GoogleTest to be installed.

```bash
cmake \
  -B build \
  -S . \
  -DCMAKE_CXX_STANDARD=23 \
  # Your extra arguments here.
cmake --build build
ctest --test-dir build
```

> [!IMPORTANT]
>
> Beman projects are [passive projects](
> https://github.com/bemanproject/beman/blob/main/docs/beman_standard.md#cmakepassive_projects),
> so you need to specify the C++ version via `CMAKE_CXX_STANDARD` when manually
> configuring the project.

## Dependency Management

### vcpkg

The best way to install the project's dependencies is to use the vcpkg workflow.

To do so, make sure vcpkg is installed and `VCPKG_ROOT` is defined in your environment,
then specify
`-DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"`. Vcpkg will handle
the project's dependencies, including GoogleTest.

Example commands:

```shell
cmake \
  -B build \
  -S . \
  -DCMAKE_CXX_STANDARD=23 \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build
ctest --test-dir build
```

The file `./vcpkg.json` configures the list of dependencies that will be configured by
vcpkg.

### FetchContent

Instead of installing the project's dependencies via a package manager, you can optionally
configure beman.inside to fetch them automatically via CMake FetchContent.

To do so, specify
`-DCMAKE_PROJECT_TOP_LEVEL_INCLUDES=./infra/cmake/use-fetch-content.cmake`. This will
bring in GoogleTest automatically along with any other dependency the project may require.

Example commands:

```shell
cmake \
  -B build \
  -S . \
  -DCMAKE_CXX_STANDARD=23 \
  -DCMAKE_PROJECT_TOP_LEVEL_INCLUDES=./infra/cmake/use-fetch-content.cmake
cmake --build build
ctest --test-dir build
```

The file `./lockfile.json` configures the list of dependencies and versions that will be
acquired by FetchContent.

## Project-specific configure arguments

Project-specific options are prefixed with `BEMAN_INSIDE`.
You can see the list of available options with:

```bash
cmake -LH -S . -B build | grep "BEMAN_INSIDE" -C 2
```

The library options are in the [README's table](README.md#build-environment).
The project's own: `BEMAN_INSIDE_BUILD_TESTS` and `BEMAN_INSIDE_BUILD_EXAMPLES`
(both `ON`), and `BEMAN_INSIDE_INSTALL_CONFIG_FILE_PACKAGE` (`ON`, needed for
`find_package(beman.inside)`). `BEMAN_INSIDE_BUILD_TOOLS` also adds the
`beman.inside.perf_report` and `beman.inside.accuracy_report` targets, which
regenerate `docs/performance.md` and `docs/accuracy.md`.

## Maintenance targets

- `beman.inside.amalgamate` regenerates the committed single header
  `single_include/beman/inside/inside.hpp`. The
  `beman.inside.tests.amalgamate_up_to_date` test fails when it is stale.
- The compile-fail suite (`tests/beman/inside/fail/`, ctest label
  `compile-fail`) checks that ill-formed uses keep their diagnostics; each case
  names the expected message on its first line (`// EXPECT: ...`).
- `tests/beman/inside/check_perf.py` runs the cachegrind instruction-count gate
  against `tests/beman/inside/perf_baseline.json` (needs valgrind and
  `BEMAN_INSIDE_BUILD_TOOLS=ON`).
