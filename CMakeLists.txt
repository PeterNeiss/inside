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

enable_testing()

include(FetchContent)

#---------------------------------------------------------------------------
# Catch2 (test framework)
#---------------------------------------------------------------------------
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.5.4
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

#---------------------------------------------------------------------------
# ctrack (benchmarking)
#---------------------------------------------------------------------------
set(DISABLE_EXAMPLES ON  CACHE BOOL "" FORCE)
set(BUILD_TESTS      OFF CACHE BOOL "" FORCE)   # ctrack's timing-based tests are flaky
FetchContent_Declare(
  ctrack
  GIT_REPOSITORY https://github.com/Compaile/ctrack.git
  GIT_TAG        v1.1.0
)
FetchContent_MakeAvailable(ctrack)

add_executable(bench tests/bench.cpp)
target_link_libraries(bench PUBLIC bound::bound)
get_target_property(CTRACK_INCLUDE_DIRS ctrack INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(bench SYSTEM PUBLIC ${CTRACK_INCLUDE_DIRS})

add_executable(bound_fuzz tests/fuzz.cpp)
target_link_libraries(bound_fuzz PUBLIC bound::bound)

#---------------------------------------------------------------------------
# MinGW link workaround
#---------------------------------------------------------------------------
# GCC 13.x for x86_64-w64-mingw32 ships a libstdc++.a whose tinfo.o carries an
# out-of-line `std::type_info::operator==`, while user/Catch2 objects emit the
# inline form from <typeinfo>. Static-linking them trips a spurious
# "multiple definition of std::type_info::operator==". Tolerate it on MinGW
# (the two definitions are identical); harmless elsewhere, so it stays scoped.
if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    foreach(tgt bound_tests bench bound_fuzz)
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
if(BOUND_MATH_FIXED)
    target_compile_definitions(single_header_freestanding_smoke PRIVATE BND_MATH_FIXED)
endif()
if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(single_header_freestanding_smoke PRIVATE -Wl,--allow-multiple-definition)
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
