cmake_minimum_required (VERSION 3.24)

project(bound VERSION 1.0.0 LANGUAGES CXX)

# The library targets C++23 by default but is C++20-clean (GCC 12): under
# C++20 the std::format integration and the slim::optional move_only_function
# specialization are feature-gated off, and the error channel uses the
# slim::expected backport instead of <expected>. Configure with -DBOUND_CXX20=ON
# (or a GCC 12 toolchain) to build/test that path.
option(BOUND_CXX20 "Build against C++20 (GCC 12 compatibility) instead of the default C++23" OFF)
if(BOUND_CXX20)
    set(CMAKE_CXX_STANDARD 20)
else()
    set(CMAKE_CXX_STANDARD 23)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(bound_bound INTERFACE)
add_library(bound::bound ALIAS bound_bound)

# bnd::math engine select. Default: the reproducible double engine (own
# polynomials + std::fma + std::sqrt; requires an IEEE-754 FPU, no -ffast-math).
# -DBOUND_MATH_FIXED=ON selects the integer/CORDIC engine — FPU-free and
# unconditionally reproducible, for embedded / maximum-portability targets.
option(BOUND_MATH_FIXED "Use the integer/CORDIC math engine instead of the double engine" OFF)
if(BOUND_MATH_FIXED)
    target_compile_definitions(bound_bound INTERFACE BND_MATH_FIXED)
endif()

# -DBOUND_MATH_FLOAT=ON makes the UNQUALIFIED bnd::math::fn use the float
# (binary32) engine — for single-precision-only FPUs. All engines stay reachable
# by namespace (cordic::/dbl::/flt::) regardless; this only picks the default.
# Ignored under BOUND_MATH_FIXED (the integer engine wins / no FP).
option(BOUND_MATH_FLOAT "Default bnd::math to the float (binary32) engine instead of double" OFF)
if(BOUND_MATH_FLOAT)
    target_compile_definitions(bound_bound INTERFACE BND_MATH_FLOAT)
endif()

# By default an incompatible assignment/conversion (`b = x;`, `bound{x}`, a by-value
# bound argument) reports the *reason* at compile time via named static_asserts. That
# makes `bound` advertise itself is_constructible/convertible from incompatible types
# (the error surfaces on use, not on the trait). -DBOUND_STRICT_SFINAE=ON drops the
# diagnostic overloads so those traits stay honest — for code that embeds `bound` in
# variant/optional or otherwise probes convertibility. The free `bnd::why_assignable`
# probe still works in strict builds.
option(BOUND_STRICT_SFINAE "Drop assignment/conversion diagnostic hints to keep bound SFINAE-pure (honest is_constructible)" OFF)
if(BOUND_STRICT_SFINAE)
    target_compile_definitions(bound_bound INTERFACE BND_STRICT_SFINAE)
endif()

# Error reporting carries no strings: a checked violation funnels through the
# installed bnd::error_handler (default throws bnd::bound_error / traps under
# -fno-exceptions) with a static const-char* message — the core never drags
# <string>/<system_error> into a translation unit. General-purpose
# stringification (to_string / operator<< / std::formatter) lives in the opt-in
# "bound/io.hpp"; in the single header that block is wrapped in BND_NO_STRING so
# a freestanding consumer can drop it (and its heavy includes) wholesale.

target_include_directories(bound_bound
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_compile_options(bound_bound
    INTERFACE
        $<$<CXX_COMPILER_ID:GNU,Clang>:
            -Wall -Wextra
            -Wnon-virtual-dtor -Wold-style-cast
            -Woverloaded-virtual -Wnull-dereference
            # bound's default constructor is intentionally trivial (Raw left
            # uninitialised) so the type stays trivial for NTTP / memcpy use.
            -Wno-uninitialized
            -Wno-missing-field-initializers
            -Wformat=2
        >
        $<$<CXX_COMPILER_ID:MSVC>:
            /W4 /WX /permissive-
            /utf-8   # source is UTF-8 (e.g. ≤ in literals) — avoid C4566
            # Suppressed warnings are intentional patterns in this numeric template
            # code (GCC/Clang at -Wall -Wextra do not flag these):
            #   C4146 — unary minus on unsigned (deliberate modular negation on umax)
            #   C4127 — conditional expression is constant (runtime `if` on constexpr)
            #   C4459 — local declaration hides a global (benign local naming)
            #   C4702 — unreachable code (per-instantiation dead branches of if constexpr)
            #   C4100 — unreferenced formal parameter (unused policy/action args)
            #   C4244 — conversion, possible data loss (intentional numeric narrowing)
            #   C4305 — truncation in initialization (intentional numeric narrowing)
            /wd4146 /wd4127 /wd4459 /wd4702 /wd4100 /wd4244 /wd4305
        >
)


#---------------------------------------------------------------------------
# Coverage (opt-in; instruments the bound interface so test TUs are covered)
#---------------------------------------------------------------------------
option(BOUND_COVERAGE "Enable gcov instrumentation for tests" OFF)
if(BOUND_COVERAGE)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR "BOUND_COVERAGE requires GCC or Clang")
    endif()
    target_compile_options(bound_bound INTERFACE --coverage -O0 -g)
    target_link_options   (bound_bound INTERFACE --coverage)
endif()

#---------------------------------------------------------------------------
# Sanitizers (opt-in; AddressSanitizer + UndefinedBehaviorSanitizer).
# The library leans on hand-rolled 128-bit intermediates, signed-overflow
# detection, and bit-shifts in the fast paths — exactly the UB UBSan catches.
# -fno-sanitize-recover=all makes any finding a hard failure (non-zero exit)
# so CI fails instead of merely logging. Applied to the bound INTERFACE so it
# propagates to every test/example/fuzz TU. GCC or Clang only.
#---------------------------------------------------------------------------
option(BOUND_SANITIZE "Build tests with AddressSanitizer + UBSan" OFF)
if(BOUND_SANITIZE)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR "BOUND_SANITIZE requires GCC or Clang")
    endif()
    if(BOUND_COVERAGE)
        message(FATAL_ERROR "BOUND_SANITIZE and BOUND_COVERAGE are mutually exclusive")
    endif()
    target_compile_options(bound_bound INTERFACE
        -fsanitize=address,undefined -fno-sanitize-recover=all
        -fno-omit-frame-pointer -g)
    target_link_options(bound_bound INTERFACE -fsanitize=address,undefined)
endif()

enable_testing()

include(FetchContent)

#---------------------------------------------------------------------------
# Catch2 (test framework)
#---------------------------------------------------------------------------
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  # v3.8.1: v3.5.4 used the x64-only `_umul128` intrinsic unconditionally in its
  # RNG helper, breaking the Windows ARM64 build (C3861). Newer Catch2 guards it.
  GIT_TAG        v3.8.1
)
FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)

# Treat Catch2 headers as system headers so its known -Wnon-virtual-dtor /
# -Weffc++ noise stays out of the bound build log. Same pattern as ctrack.
get_target_property(CATCH2_INCLUDE_DIRS Catch2 INTERFACE_INCLUDE_DIRECTORIES)

file(GLOB BOUND_TEST_SOURCES CONFIGURE_DEPENDS tests/test_*.cpp)
add_executable(bound_tests ${BOUND_TEST_SOURCES})
target_link_libraries(bound_tests PRIVATE bound::bound Catch2::Catch2WithMain)
target_include_directories(bound_tests SYSTEM PRIVATE ${CATCH2_INCLUDE_DIRS})
# DISCOVERY_MODE PRE_TEST: enumerate tests at `ctest` time, not at build time.
# The default POST_BUILD mode runs the freshly-built executable on the build
# host to list tests — impossible for the MinGW/aarch64 cross builds, where the
# host can't execute the target binary (it gets parsed as a shell script).
# PRE_TEST defers discovery to test invocation, under the configured emulator.
catch_discover_tests(bound_tests DISCOVERY_MODE PRE_TEST)

# Compile-fail diagnostics suite — regression-tests that ill-formed uses stay
# ill-formed and keep their intended messages (see tests/fail/CMakeLists.txt).
add_subdirectory(tests/fail)

#---------------------------------------------------------------------------
# nanobench (benchmarking)
#---------------------------------------------------------------------------
FetchContent_Declare(
  nanobench
  GIT_REPOSITORY https://github.com/martinus/nanobench.git
  GIT_TAG        v4.3.11
)
FetchContent_MakeAvailable(nanobench)

# bench (nanobench) and bound_fuzz are EXCLUDE_FROM_ALL: both are large,
# slow-to-compile test harnesses (the all-in-one fuzz TU instantiates every
# property across the whole grid catalogue) that validate nothing an end user
# compiles, so the default `cmake --build` stays lean. Build on demand with
# `--target bench` / `--target bound_fuzz`.
add_executable(bench EXCLUDE_FROM_ALL tests/bench.cpp)
target_link_libraries(bench PUBLIC bound::bound)
get_target_property(NANOBENCH_INCLUDE_DIRS nanobench INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(bench SYSTEM PUBLIC ${NANOBENCH_INCLUDE_DIRS})

# Regenerates the committed performance tables (same pattern as
# accuracy_report): every nanobench group prints native first, so the
# relative column is the bound-vs-native ratio.
add_custom_target(perf_report
  COMMAND bench ${CMAKE_CURRENT_SOURCE_DIR}/docs/performance.md
  COMMENT "Regenerating docs/performance.md")
add_dependencies(perf_report bench)

add_executable(bound_fuzz EXCLUDE_FROM_ALL tests/fuzz.cpp)
target_link_libraries(bound_fuzz PUBLIC bound::bound)

# Accuracy characterization (fpm-style): sweeps every math engine against a
# long-double <cmath> reference and emits a markdown error table. On demand:
# `--target bound_accuracy` to build, `--target accuracy_report` to regenerate
# the committed docs/accuracy.md.
add_executable(bound_accuracy EXCLUDE_FROM_ALL tests/accuracy.cpp)
target_link_libraries(bound_accuracy PUBLIC bound::bound)

add_custom_target(accuracy_report
  COMMAND bound_accuracy ${CMAKE_CURRENT_SOURCE_DIR}/docs/accuracy.md
  COMMENT "Regenerating docs/accuracy.md")
add_dependencies(accuracy_report bound_accuracy)

#---------------------------------------------------------------------------
# MinGW link workaround
#---------------------------------------------------------------------------
# GCC 13.x for x86_64-w64-mingw32 ships a libstdc++.a whose tinfo.o carries an
# out-of-line `std::type_info::operator==`, while user/Catch2 objects emit the
# inline form from <typeinfo>. Static-linking them trips a spurious
# "multiple definition of std::type_info::operator==". Tolerate it on MinGW
# (the two definitions are identical); harmless elsewhere, so it stays scoped.
if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    foreach(tgt bound_tests bench bound_fuzz bound_accuracy)
        target_link_options(${tgt} PRIVATE -Wl,--allow-multiple-definition)
    endforeach()
endif()

#---------------------------------------------------------------------------
# Coverage target — runs ctest under --coverage and produces an HTML report.
# Only useful when configured with -DBOUND_COVERAGE=ON.
#---------------------------------------------------------------------------
if(BOUND_COVERAGE)
    find_program(LCOV_EXE     lcov    REQUIRED)
    find_program(GENHTML_EXE  genhtml REQUIRED)

    # Match gcov to the compiler so .gcda format versions agree (system /usr/bin/gcov
    # may belong to a different GCC than the one selected via CMAKE_CXX_COMPILER).
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        string(REGEX MATCH "^[0-9]+" GCC_MAJOR "${CMAKE_CXX_COMPILER_VERSION}")
        find_program(GCOV_EXE
            NAMES gcov-${CMAKE_CXX_COMPILER_VERSION} gcov-${GCC_MAJOR} gcov
            REQUIRED)
    else()
        find_program(GCOV_EXE NAMES llvm-cov gcov REQUIRED)
    endif()

    add_custom_target(coverage
        COMMAND ${LCOV_EXE} --gcov-tool ${GCOV_EXE} --directory . --zerocounters
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        COMMAND ${LCOV_EXE} --gcov-tool ${GCOV_EXE} --capture --directory .
                            --base-directory ${CMAKE_SOURCE_DIR}
                            --no-external
                            --output-file coverage.info
        # Positive filter: keep only the library headers. NOTE: no shell quotes
        # around the globs — under VERBATIM, CMake passes arguments to lcov
        # exactly as written, so quotes would be taken literally and match
        # nothing (the same trap that made the old --remove '*/tests/*' a no-op,
        # leaving test + Catch2 code in the headline number).
        COMMAND ${LCOV_EXE} --extract coverage.info
                            */include/bound/* */include/slim/*
                            --ignore-errors unused
                            --output-file coverage.info
        COMMAND ${GENHTML_EXE} coverage.info --output-directory coverage
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Coverage: running ctest, generating HTML at build/coverage/index.html"
        VERBATIM)
endif()

#---------------------------------------------------------------------------
# Examples
#---------------------------------------------------------------------------
# Each example is also registered as a ctest test under the "example" label, so
# `ctest -L example` runs them all (through CMAKE_CROSSCOMPILING_EMULATOR on the
# cross builds — qemu for aarch64, wine for MinGW). The unit-test runs use
# `ctest -LE example` to skip them.
file(GLOB EXAMPLE_SOURCES examples/*.cpp)
foreach(example_src ${EXAMPLE_SOURCES})
  get_filename_component(example_name ${example_src} NAME_WE)
  add_executable(example_${example_name} ${example_src})
  target_link_libraries(example_${example_name} PUBLIC bound::bound)
  if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(example_${example_name} PRIVATE -Wl,--allow-multiple-definition)
  endif()
  add_test(NAME example_${example_name} COMMAND example_${example_name})
  set_tests_properties(example_${example_name} PROPERTIES LABELS "example")
endforeach()

#---------------------------------------------------------------------------
# Single-header amalgamation
#
# The committed single header at single_include/bound/bound.hpp is generated by
# inlining the whole include/ tree (pure CMake, see cmake/amalgamate.cmake — no
# Python or other tooling). Regenerate after touching any include/ header with:
#
#     cmake --build <build-dir> --target amalgamate
#
# It is the drop-in form of the library and the artifact behind the Compiler
# Explorer raw-URL include:
#   #include <https://raw.githubusercontent.com/<user>/bound/<branch>/single_include/bound/bound.hpp>
#---------------------------------------------------------------------------
set(BOUND_SINGLE_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/single_include/bound/bound.hpp"
    CACHE FILEPATH "Path of the committed amalgamated single header")

add_custom_target(amalgamate
    COMMAND ${CMAKE_COMMAND}
            -D BOUND_AMALGAMATE_INCLUDE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/include
            -D BOUND_AMALGAMATE_OUTPUT=${BOUND_SINGLE_HEADER}
            -D BOUND_AMALGAMATE_VERSION=${PROJECT_VERSION}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/amalgamate.cmake
    COMMENT "Amalgamating bound into ${BOUND_SINGLE_HEADER}"
    VERBATIM)

# Drift guard: regenerate into the build tree and byte-compare against the
# committed header. Fails if include/ changed without regenerating. Pure host
# CMake, so it is safe under cross-compilation (no target binary is run).
add_test(NAME amalgamate_up_to_date
    COMMAND ${CMAKE_COMMAND}
            -D BOUND_AMALGAMATE_INCLUDE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/include
            -D BOUND_AMALGAMATE_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/_amalgamate_check.hpp
            -D BOUND_AMALGAMATE_VERSION=${PROJECT_VERSION}
            -D BOUND_AMALGAMATE_COMPARE=${BOUND_SINGLE_HEADER}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/amalgamate.cmake)
set_tests_properties(amalgamate_up_to_date PROPERTIES LABELS "tooling")

# On-demand self-containment check: compiles a TU that sees ONLY single_include/
# (no include/bound, no include/slim), proving the single header stands alone.
# EXCLUDE_FROM_ALL — build explicitly with `--target single_header_smoke`.
add_executable(single_header_smoke EXCLUDE_FROM_ALL tests/single_header_smoke.cpp)
target_include_directories(single_header_smoke PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/single_include")
if(BOUND_MATH_FIXED)
    target_compile_definitions(single_header_smoke PRIVATE BND_MATH_FIXED)
endif()
if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(single_header_smoke PRIVATE -Wl,--allow-multiple-definition)
endif()

# Freestanding-leaning smoke: the single header with the io/string block dropped
# (BND_NO_STRING) and exceptions off, installing a custom bnd::error_handler in
# place of the default throw. Proves the core compiles with neither <string> nor
# the C++ exception ABI on its own surface. EXCLUDE_FROM_ALL — build with
# `--target single_header_freestanding_smoke`.
add_executable(single_header_freestanding_smoke EXCLUDE_FROM_ALL
    tests/single_header_freestanding_smoke.cpp)
target_include_directories(single_header_freestanding_smoke PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/single_include")
target_compile_definitions(single_header_freestanding_smoke PRIVATE BND_NO_STRING)
target_compile_options(single_header_freestanding_smoke PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-fno-exceptions>)
# This target doesn't link bound::bound (it sees only single_include/), so the
# BOUND_SANITIZE interface flags don't reach it — apply them here so the
# -fno-exceptions error path gets exercised under ASan+UBSan too.
if(BOUND_SANITIZE)
    target_compile_options(single_header_freestanding_smoke PRIVATE
        -fsanitize=address,undefined -fno-sanitize-recover=all
        -fno-omit-frame-pointer -g)
    target_link_options(single_header_freestanding_smoke PRIVATE
        -fsanitize=address,undefined)
endif()
if(BOUND_MATH_FIXED)
    target_compile_definitions(single_header_freestanding_smoke PRIVATE BND_MATH_FIXED)
endif()
if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(single_header_freestanding_smoke PRIVATE -Wl,--allow-multiple-definition)
endif()

# FP-free smoke: compiles the single header with BND_MATH_NO_FP and a poison
# <cmath> shim placed FIRST on the include path, so the build hard-errors if any
# <cmath> is pulled in. Proves the single header honors the FP-free guard (only
# the integer/CORDIC engine remains). GNU/Clang only — the proof relies on -I
# ordering for angle-bracket includes; bare-metal toolchains are GCC/Clang.
# EXCLUDE_FROM_ALL — build with `--target single_header_nofp_smoke`.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_executable(single_header_nofp_smoke EXCLUDE_FROM_ALL
        tests/single_header_nofp_smoke.cpp)
    # The poison dir must precede single_include AND the system dirs. BEFORE keeps
    # it ahead of single_include; angle-bracket search already places -I dirs ahead
    # of the system headers, so <cmath> resolves to the poison shim.
    target_include_directories(single_header_nofp_smoke BEFORE PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/nofp_poison"
        "${CMAKE_CURRENT_SOURCE_DIR}/single_include")
    target_compile_definitions(single_header_nofp_smoke PRIVATE BND_MATH_NO_FP)
endif()

#---------------------------------------------------------------------------
# Performance regression gates (label "perf"; excluded from the normal unit run)
#
#  * codegen guard — compiles the arithmetic fast paths at -O2 and asserts via
#    objdump that the scalar path stays call-free and the loop vectorizes. Pure
#    disassembly inspection (x86-64 mnemonics), so it is x86-64 GNU/Clang only;
#    it compiles independently of the build type, so coverage/sanitizer flags on
#    the bound interface don't perturb it.
#  * cachegrind instruction-count gate — see tests/check_perf.py; opt-in via the
#    perf CI cell, which has valgrind.
#---------------------------------------------------------------------------
if(UNIX AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang"
        AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
    add_test(NAME codegen_guard
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_codegen.sh
                ${CMAKE_CXX_COMPILER}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/perf_codegen.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/include
                ${CMAKE_CXX_STANDARD})
    set_tests_properties(codegen_guard PROPERTIES LABELS "perf")
endif()

# Dedicated, deterministic perf workload for the cachegrind gate. Small fixed
# work scaled by BND_PERF_SCALE (compile-time), kept lean so cachegrind (~20-50x)
# stays inside the CI budget. EXCLUDE_FROM_ALL — built only by the perf cell.
add_executable(perf_workload EXCLUDE_FROM_ALL tests/perf_workload.cpp)
target_link_libraries(perf_workload PRIVATE bound::bound)
if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(perf_workload PRIVATE -Wl,--allow-multiple-definition)
endif()

#---------------------------------------------------------------------------
# Install / package config
#
# Headers go to <prefix>/include/bound/ and <prefix>/include/slim/.
# The exported target `bound::bound` is reachable via `find_package(bound)`.
# Layout under <prefix>/lib/cmake/bound/:
#   boundConfig.cmake          — entry point for find_package
#   boundConfigVersion.cmake   — version compatibility
#   boundTargets.cmake         — exported INTERFACE target
#---------------------------------------------------------------------------
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS bound_bound
        EXPORT  boundTargets)

install(DIRECTORY include/bound include/slim
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.hpp")

# The amalgamated single header shares the path bound/bound.hpp with the umbrella
# header, so it cannot live in the same include root. Ship it under a separate
# directory; consumers who want the single-header form add this to their include
# path: <prefix>/${CMAKE_INSTALL_DATADIR}/bound/single_include
if(EXISTS "${BOUND_SINGLE_HEADER}")
    install(DIRECTORY single_include
            DESTINATION ${CMAKE_INSTALL_DATADIR}/bound
            FILES_MATCHING PATTERN "*.hpp")
endif()

install(EXPORT boundTargets
        NAMESPACE   bound::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/bound)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/boundConfigVersion.cmake"
  VERSION       ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
  ARCH_INDEPENDENT)

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/boundConfig.cmake"
"include(\"\${CMAKE_CURRENT_LIST_DIR}/boundTargets.cmake\")\n"
"if(NOT TARGET bound::bound)\n"
"  add_library(bound::bound ALIAS bound::bound_bound)\n"
"endif()\n")

install(FILES
          "${CMAKE_CURRENT_BINARY_DIR}/boundConfig.cmake"
          "${CMAKE_CURRENT_BINARY_DIR}/boundConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/bound)
