# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

find_package(GTest REQUIRED)
include(GoogleTest)

#---------------------------------------------------------------------------
# Unit tests: one executable per <name>.test.cpp
#---------------------------------------------------------------------------
file(GLOB BEMAN_INSIDE_TEST_SOURCES CONFIGURE_DEPENDS *.test.cpp)
# The tools and smoke builds below are *.test.cpp too (Beman file.test_names),
# but not GTest executables.
list(
    FILTER BEMAN_INSIDE_TEST_SOURCES
    EXCLUDE
    REGEX "/(accuracy|bench|fuzz|perf_codegen|perf_workload|single_header(_[a-z]+)?_smoke)\\.test\\.cpp$"
)
foreach(test_src ${BEMAN_INSIDE_TEST_SOURCES})
    get_filename_component(test_name ${test_src} NAME)
    string(REGEX REPLACE "\\.test\\.cpp$" "" test_name "${test_name}")
    set(test_target beman.inside.tests.${test_name})
    add_executable(${test_target})
    target_sources(${test_target} PRIVATE ${test_src})
    target_link_libraries(
        ${test_target}
        PRIVATE beman::inside beman.inside.warnings GTest::gtest_main
    )
    # DISCOVERY_MODE PRE_TEST: enumerate tests at `ctest` time, not at build
    # time, so cross builds (where the host can't run the target binary)
    # discover through the configured emulator.
    gtest_discover_tests(
        ${test_target}
        DISCOVERY_MODE PRE_TEST
        DISCOVERY_TIMEOUT 60
    )
endforeach()

# Compile-fail diagnostics suite — regression-tests that ill-formed uses stay
# ill-formed and keep their intended messages (see fail/CMakeLists.txt).
add_subdirectory(fail)

#---------------------------------------------------------------------------
# Single header
#---------------------------------------------------------------------------
set(single_include_dir "${PROJECT_SOURCE_DIR}/single_include")

# Drift guard: regenerate into the build tree and byte-compare against the
# committed header. Fails if include/ changed without regenerating. Pure host
# CMake, so it is safe under cross-compilation (no target binary is run).
add_test(
    NAME beman.inside.tests.amalgamate_up_to_date
    COMMAND
        ${CMAKE_COMMAND} -D
        BEMAN_INSIDE_AMALGAMATE_INCLUDE_DIR=${PROJECT_SOURCE_DIR}/include -D
        BEMAN_INSIDE_AMALGAMATE_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/_amalgamate_check.hpp
        -D BEMAN_INSIDE_AMALGAMATE_VERSION=${PROJECT_VERSION} -D
        BEMAN_INSIDE_AMALGAMATE_COMPARE=${BEMAN_INSIDE_SINGLE_HEADER} -P
        ${PROJECT_SOURCE_DIR}/cmake/amalgamate.cmake
)
set_tests_properties(
    beman.inside.tests.amalgamate_up_to_date
    PROPERTIES LABELS "tooling"
)

# Self-containment smoke: a TU that sees ONLY single_include/ proves the single
# header stands alone.
add_executable(beman.inside.tests.single_header_smoke single_header_smoke.test.cpp)
target_include_directories(
    beman.inside.tests.single_header_smoke
    PRIVATE "${single_include_dir}"
)
target_compile_features(
    beman.inside.tests.single_header_smoke
    PRIVATE cxx_std_23
)
target_link_libraries(
    beman.inside.tests.single_header_smoke
    PRIVATE beman.inside.warnings
)

# Freestanding-leaning smoke: the single header with the io/string block dropped
# (BEMAN_INSIDE_NO_STRING) and exceptions off, installing a custom
# beman::inside::error_handler in place of the default throw. Proves the core
# compiles with neither <string> nor the C++ exception ABI on its own surface.
add_executable(
    beman.inside.tests.single_header_freestanding_smoke
    single_header_freestanding_smoke.test.cpp
)
target_include_directories(
    beman.inside.tests.single_header_freestanding_smoke
    PRIVATE "${single_include_dir}"
)
target_compile_features(
    beman.inside.tests.single_header_freestanding_smoke
    PRIVATE cxx_std_23
)
target_compile_definitions(
    beman.inside.tests.single_header_freestanding_smoke
    PRIVATE BEMAN_INSIDE_NO_STRING
)
target_compile_options(
    beman.inside.tests.single_header_freestanding_smoke
    PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-fno-exceptions>
)
target_link_libraries(
    beman.inside.tests.single_header_freestanding_smoke
    PRIVATE beman.inside.warnings
)

foreach(
    smoke
    beman.inside.tests.single_header_smoke
    beman.inside.tests.single_header_freestanding_smoke
)
    if(BEMAN_INSIDE_MATH_NO_FP)
        target_compile_definitions(${smoke} PRIVATE BEMAN_INSIDE_MATH_NO_FP)
    endif()
    add_test(NAME ${smoke} COMMAND ${smoke})
endforeach()

# FP-free smoke: compiles the single header with BEMAN_INSIDE_MATH_NO_FP and a
# poison <cmath> shim placed FIRST on the include path, so the build hard-errors
# if any <cmath> is pulled in. Proves the single header honors the FP-free guard
# (the math engine runs on its integer path). GNU/Clang only — the proof relies on
# -I ordering for angle-bracket includes; bare-metal toolchains are GCC/Clang.
# Compile-only (an object library), as before.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_library(
        beman.inside.tests.single_header_nofp_smoke
        OBJECT
        single_header_nofp_smoke.test.cpp
    )
    # The poison dir must precede single_include AND the system dirs. BEFORE
    # keeps it ahead of single_include; angle-bracket search already places -I
    # dirs ahead of the system headers, so <cmath> resolves to the poison shim.
    target_include_directories(
        beman.inside.tests.single_header_nofp_smoke
        BEFORE
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/nofp_poison"
            "${single_include_dir}"
    )
    target_compile_features(
        beman.inside.tests.single_header_nofp_smoke
        PRIVATE cxx_std_23
    )
    target_compile_definitions(
        beman.inside.tests.single_header_nofp_smoke
        PRIVATE BEMAN_INSIDE_MATH_NO_FP BEMAN_INSIDE_NO_STRING
    )
endif()

#---------------------------------------------------------------------------
# Codegen guard (label "perf") — compiles the arithmetic fast paths at -O2 and
# asserts via objdump that the scalar path stays call-free and the loop
# vectorizes. Pure disassembly inspection (x86-64 mnemonics), so it is x86-64
# GNU/Clang only; it compiles independently of the build type, so coverage /
# sanitizer flags don't perturb it.
#---------------------------------------------------------------------------
if(
    UNIX
    AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang"
    AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64"
)
    if(CMAKE_CXX_STANDARD)
        set(codegen_std ${CMAKE_CXX_STANDARD})
    else()
        set(codegen_std 23)
    endif()
    add_test(
        NAME beman.inside.tests.codegen_guard
        COMMAND
            ${CMAKE_CURRENT_SOURCE_DIR}/check_codegen.sh ${CMAKE_CXX_COMPILER}
            ${CMAKE_CURRENT_SOURCE_DIR}/perf_codegen.test.cpp
            ${PROJECT_SOURCE_DIR}/include ${codegen_std} "${CMAKE_CXX_FLAGS}"
    )
    set_tests_properties(
        beman.inside.tests.codegen_guard
        PROPERTIES LABELS "perf"
    )
endif()

#---------------------------------------------------------------------------
# Tooling (opt-in): benchmarks, property fuzzer, accuracy sweep, perf workload
#---------------------------------------------------------------------------
if(BEMAN_INSIDE_BUILD_TOOLS)
    include(FetchContent)
    FetchContent_Declare(
        nanobench
        GIT_REPOSITORY https://github.com/martinus/nanobench.git
        GIT_TAG v4.3.11
        SYSTEM
    )
    FetchContent_MakeAvailable(nanobench)

    add_executable(beman.inside.bench bench.test.cpp)
    target_link_libraries(
        beman.inside.bench
        PRIVATE beman::inside beman.inside.warnings nanobench
    )

    # Regenerates the committed performance tables: every nanobench group
    # prints native first, so the relative column is the inside-vs-native ratio.
    add_custom_target(
        beman.inside.perf_report
        COMMAND beman.inside.bench ${PROJECT_SOURCE_DIR}/docs/performance.md
        COMMENT "Regenerating docs/performance.md"
    )

    add_executable(beman.inside.fuzz fuzz.test.cpp)
    target_link_libraries(
        beman.inside.fuzz
        PRIVATE beman::inside beman.inside.warnings
    )

    # Accuracy characterization: sweeps the math engine against a long-double
    # <cmath> reference and emits a markdown error table.
    add_executable(beman.inside.accuracy accuracy.test.cpp)
    target_link_libraries(
        beman.inside.accuracy
        PRIVATE beman::inside beman.inside.warnings
    )
    add_custom_target(
        beman.inside.accuracy_report
        COMMAND beman.inside.accuracy ${PROJECT_SOURCE_DIR}/docs/accuracy.md
        COMMENT "Regenerating docs/accuracy.md"
    )

    # Cachegrind instruction-count workload, driven by check_perf.py.
    add_executable(beman.inside.perf_workload perf_workload.test.cpp)
    target_link_libraries(
        beman.inside.perf_workload
        PRIVATE beman::inside beman.inside.warnings
    )
endif()
