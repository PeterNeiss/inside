# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

cmake_minimum_required(VERSION 3.30...4.4)

project(
    beman.inside
    DESCRIPTION
        "Numbers that cannot go out of range: the range and step size live in the type"
    LANGUAGES CXX
    VERSION 0.5.0
)

# The library requires C++26 with static reflection (GCC 16, -freflection):
# grid numbers of any size are interned with std::define_static_array.

# [CMAKE.SKIP_TESTS]
option(
    BEMAN_INSIDE_WARNINGS_AS_ERRORS
    "Make warnings errors in the tests and examples (-Werror)."
    OFF
)
option(
    BEMAN_INSIDE_BUILD_TESTS
    "Enable building tests and test infrastructure. Default: ${PROJECT_IS_TOP_LEVEL}. Values: { ON, OFF }."
    ${PROJECT_IS_TOP_LEVEL}
)

# [CMAKE.SKIP_EXAMPLES]
option(
    BEMAN_INSIDE_BUILD_EXAMPLES
    "Enable building examples. Default: ${PROJECT_IS_TOP_LEVEL}. Values: { ON, OFF }."
    ${PROJECT_IS_TOP_LEVEL}
)

# Benchmarks, the docs reports (performance.md, accuracy.md) and the
# cachegrind perf workload: slow to compile and validating nothing an end user
# compiles, so they are opt-in. (The fuzzer and the accuracy sweep always run
# as tests.)
option(
    BEMAN_INSIDE_BUILD_TOOLS
    "Enable building benchmarks, the docs reports and the perf workload. Default: OFF. Values: { ON, OFF }."
    OFF
)

# The certify test checks the pairs of docs/certified.md up to 2^20 inputs
# (seconds); BEMAN_INSIDE_CERTIFY_FULL=ON checks every pair up to 2^32 (the
# weekly CI run, minutes).
option(
    BEMAN_INSIDE_CERTIFY_FULL
    "Check every certified math pair, not only those up to 2^20 inputs. Default: OFF. Values: { ON, OFF }."
    OFF
)

# BEMAN_INSIDE_MATH_NO_FP=ON builds with no hardware floating point: the math
# engine's double and dd tiers compile out (its integer path computes every
# result; results do not change).
# Auto-enabled on freestanding targets.
option(
    BEMAN_INSIDE_MATH_NO_FP
    "Build without hardware floating point. Default: OFF. Values: { ON, OFF }."
    OFF
)

# By default an incompatible assignment/conversion (`b = x;`, `inside{x}`, a
# by-value inside argument) reports the *reason* at compile time via named
# static_asserts. That makes `inside` advertise itself is_constructible /
# convertible from incompatible types (the error surfaces on use, not on the
# trait). BEMAN_INSIDE_STRICT_SFINAE=ON drops the diagnostic overloads so those
# traits stay honest — for code that embeds `inside` in variant/optional or
# otherwise probes convertibility. The free `beman::inside::explain_assignable`
# probe still works in strict builds.
option(
    BEMAN_INSIDE_STRICT_SFINAE
    "Drop assignment/conversion diagnostic hints to keep inside SFINAE-pure (honest is_constructible). Default: OFF. Values: { ON, OFF }."
    OFF
)

# The double/float math engines evaluate their polynomials with std::fma, which
# is what makes them bit-reproducible across platforms. Baseline x86-64 has no
# FMA instruction, so without -mfma every std::fma is a software-emulated libm
# call (math::sin ~4x slower; the results are identical either way). ON adds
# -mfma to everything that links beman::inside on x86-64 GCC/Clang. The binary
# then needs AVX+FMA (-mfma implies AVX; Intel Haswell / AMD Piledriver, 2013+) and
# dies with SIGILL on older ones — turn this OFF to target those. AArch64 and
# other ISAs with FMA in their baseline need nothing.
option(
    BEMAN_INSIDE_FMA
    "Add -mfma on x86-64 GCC/Clang so std::fma is one instruction, not a libm call. Default: ON. Values: { ON, OFF }."
    ON
)

# for find of beman_install_library and configure_build_telemetry
include(infra/cmake/beman-install-library.cmake)
include(infra/cmake/BuildTelemetryConfig.cmake)

add_library(beman.inside INTERFACE)
add_library(beman::inside ALIAS beman.inside)

target_sources(
    beman.inside
    PUBLIC FILE_SET HEADERS BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
set_target_properties(
    beman.inside
    PROPERTIES VERIFY_INTERFACE_HEADER_SETS ${PROJECT_IS_TOP_LEVEL}
)
target_compile_features(beman.inside INTERFACE cxx_std_26)

if(BEMAN_INSIDE_MATH_NO_FP)
    target_compile_definitions(beman.inside INTERFACE BEMAN_INSIDE_MATH_NO_FP)
endif()
if(BEMAN_INSIDE_STRICT_SFINAE)
    target_compile_definitions(
        beman.inside
        INTERFACE BEMAN_INSIDE_STRICT_SFINAE
    )
endif()
# Static reflection (std::define_static_array) interns grid numbers of any
# size. GCC 16 gates it behind -freflection.
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-freflection BEMAN_INSIDE_HAS_FREFLECTION)
if(NOT BEMAN_INSIDE_HAS_FREFLECTION)
    message(
        FATAL_ERROR
        "beman.inside needs C++26 static reflection: build with g++-16 (-freflection)"
    )
endif()
target_compile_options(beman.inside INTERFACE -freflection)
if(BEMAN_INSIDE_FMA AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
    target_compile_options(
        beman.inside
        INTERFACE -mfma
    )
endif()

# Error reporting carries no strings: a checked violation funnels through the
# installed beman::inside::error_handler (default throws beman::inside::inside_error
# / traps under -fno-exceptions) with a static const-char* message — the core
# never drags <string>/<system_error> into a translation unit. General-purpose
# stringification (to_string / operator<< / std::formatter) lives in the opt-in
# <beman/inside/io.hpp>; in the single header that block is wrapped in
# BEMAN_INSIDE_NO_STRING so a freestanding consumer can drop it wholesale.

add_subdirectory(include/beman/inside)

beman_install_library(beman.inside TARGETS beman.inside)
configure_build_telemetry()

# The amalgamated single header shares the path beman/inside/inside.hpp with the
# umbrella header, so it cannot live in the same include root. Ship it under a
# separate directory; consumers who want the single-header form add
# <prefix>/${CMAKE_INSTALL_DATADIR}/beman.inside/single_include to their include path.
set(BEMAN_INSIDE_SINGLE_HEADER
    "${CMAKE_CURRENT_SOURCE_DIR}/single_include/beman/inside/inside.hpp"
    CACHE FILEPATH
    "Path of the committed amalgamated single header"
)
if(EXISTS "${BEMAN_INSIDE_SINGLE_HEADER}")
    install(
        DIRECTORY single_include
        DESTINATION ${CMAKE_INSTALL_DATADIR}/beman.inside
        FILES_MATCHING
        PATTERN "*.hpp"
    )
endif()

# Single-header amalgamation. The committed single header is generated by
# inlining the whole include/ tree (pure CMake, see cmake/amalgamate.cmake — no
# Python or other tooling). Regenerate after touching any include/ header with:
#
#     cmake --build <build-dir> --target beman.inside.amalgamate
#
# It is the drop-in form of the library and the artifact behind the Compiler
# Explorer raw-URL include:
#   #include <https://raw.githubusercontent.com/PeterNeiss/inside/main/single_include/beman/inside/inside.hpp>
add_custom_target(
    beman.inside.amalgamate
    COMMAND
        ${CMAKE_COMMAND} -D
        BEMAN_INSIDE_AMALGAMATE_INCLUDE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/include
        -D BEMAN_INSIDE_AMALGAMATE_OUTPUT=${BEMAN_INSIDE_SINGLE_HEADER} -D
        BEMAN_INSIDE_AMALGAMATE_VERSION=${PROJECT_VERSION} -P
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/amalgamate.cmake
    COMMENT "Amalgamating beman.inside into ${BEMAN_INSIDE_SINGLE_HEADER}"
    VERBATIM
)

if(BEMAN_INSIDE_BUILD_TESTS OR BEMAN_INSIDE_BUILD_EXAMPLES)
    # Warning flags for this project's own tests and examples. Kept off the
    # public beman.inside interface so consumers don't inherit them.
    add_library(beman.inside.warnings INTERFACE)
    target_compile_options(
        beman.inside.warnings
        INTERFACE
            -Wall
            -Wextra
            -Wnon-virtual-dtor
            -Wold-style-cast
            -Woverloaded-virtual
            -Wnull-dereference
            # inside's default constructor is intentionally trivial (Raw left
            # uninitialised) so the type stays trivial for NTTP / memcpy use.
            -Wno-uninitialized
            -Wno-missing-field-initializers
            -Wformat=2
            $<$<BOOL:${BEMAN_INSIDE_WARNINGS_AS_ERRORS}>:-Werror>
    )
endif()

if(BEMAN_INSIDE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/beman/inside)
    add_subdirectory(docs/proposal)
endif()

if(BEMAN_INSIDE_BUILD_EXAMPLES)
    if(NOT BEMAN_INSIDE_BUILD_TESTS)
        enable_testing()
    endif()
    add_subdirectory(examples)
endif()
