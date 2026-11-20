# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# GoogleTest: an installed one if find_package finds it, else this pinned commit.
include(FetchContent)
set(INSTALL_GTEST OFF)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG 52eb8108c5bdec04579160ae17225d66034bd723
    FIND_PACKAGE_ARGS NAMES GTest
)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)

#---------------------------------------------------------------------------
# Unit tests: one executable per <name>.test.cpp
#---------------------------------------------------------------------------
file(GLOB INSIDE_TEST_SOURCES CONFIGURE_DEPENDS *.test.cpp)
# The tools and smoke builds below are *.test.cpp too,
# but not GTest executables.
list(
    FILTER INSIDE_TEST_SOURCES
    EXCLUDE
    REGEX
        "/(accuracy|bench|certify|fuzz|perf_codegen|perf_workload|single_header(_[a-z]+)?_smoke)\\.test\\.cpp$"
)
foreach(test_src ${INSIDE_TEST_SOURCES})
    get_filename_component(test_name ${test_src} NAME)
    string(REGEX REPLACE "\\.test\\.cpp$" "" test_name "${test_name}")
    set(test_target inside.tests.${test_name})
    add_executable(${test_target})
    target_sources(${test_target} PRIVATE ${test_src})
    target_link_libraries(
        ${test_target}
        PRIVATE inside::inside inside.warnings GTest::gtest_main
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

# units_text has a second translation unit that sees fewer units (each call site's set is its own).
target_sources(inside.tests.units_text PRIVATE units_text_other.cpp)

# Compile-fail diagnostics suite — regression-tests that ill-formed uses stay
# ill-formed and keep their intended messages (see fail/CMakeLists.txt).
add_subdirectory(fail)

# The differential oracle against Python Fraction (differential/check.py).
add_subdirectory(differential)

# The property fuzzer and the math accuracy sweep, as tests: a fixed seed
# and few iterations per PR (a scheduled CI run takes a random seed and
# more, see fuzz.test.cpp), and every math result within half a notch.
add_executable(inside.fuzz fuzz.test.cpp)
target_link_libraries(
    inside.fuzz
    PRIVATE inside::inside inside.warnings GTest::gtest # (support.hpp's checks)
)
add_test(NAME inside.tests.fuzz COMMAND inside.fuzz 1 300)
set_tests_properties(inside.tests.fuzz PROPERTIES LABELS "fuzz")

add_executable(inside.accuracy accuracy.test.cpp)
target_link_libraries(
    inside.accuracy
    PRIVATE inside::inside inside.warnings GTest::gtest # (support.hpp's checks)
)
add_test(NAME inside.tests.accuracy COMMAND inside.accuracy --check)
set_tests_properties(inside.tests.accuracy PROPERTIES LABELS "accuracy")

# The math certification by exhaustion (certify/): every input of each pair,
# a run-time call against the integer path, no guess allowed, and every
# result hash equal to the committed docs/certified.md. An optimized build
# runs the pairs up to 2^20 inputs (seconds, under sanitizers too), with
# INSIDE_CERTIFY_FULL every pair up to 2^32 (minutes); a debug build
# the small ones.
add_executable(inside.certify certify.test.cpp)
target_link_libraries(inside.certify PRIVATE inside::inside inside.warnings)
# The cap is passed in every build; an unoptimized one lowers it to 4096.
if(INSIDE_CERTIFY_FULL)
    set(certify_cap --max-slots 4294967296)
else()
    set(certify_cap --max-slots 1048576)
endif()
add_test(
    NAME inside.tests.certify
    COMMAND
        inside.certify --no-progress ${certify_cap} --check
        ${PROJECT_SOURCE_DIR}/docs/certified.md --require-all
)
set_tests_properties(
    inside.tests.certify
    PROPERTIES LABELS "certify" TIMEOUT 3600
)
add_custom_target(
    inside.certify_report
    COMMAND
        inside.certify --no-progress --markdown
        ${PROJECT_SOURCE_DIR}/docs/certified.md
    COMMENT "Regenerating docs/certified.md"
)
add_subdirectory(certify)

#---------------------------------------------------------------------------
# Single header
#---------------------------------------------------------------------------
set(single_include_dir "${PROJECT_SOURCE_DIR}/single_include")

# Drift guard: regenerate into the build tree and byte-compare against the
# committed header. Fails if include/ changed without regenerating. Pure host
# CMake, so it is safe under cross-compilation (no target binary is run).
add_test(
    NAME inside.tests.amalgamate_up_to_date
    COMMAND
        ${CMAKE_COMMAND} -D
        INSIDE_AMALGAMATE_INCLUDE_DIR=${PROJECT_SOURCE_DIR}/include -D
        INSIDE_AMALGAMATE_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/_amalgamate_check.hpp
        -D INSIDE_AMALGAMATE_VERSION=${PROJECT_VERSION} -D
        INSIDE_AMALGAMATE_COMPARE=${INSIDE_SINGLE_HEADER} -P
        ${PROJECT_SOURCE_DIR}/tools/amalgamate.cmake
)
set_tests_properties(
    inside.tests.amalgamate_up_to_date
    PROPERTIES LABELS "tooling"
)

# The library is C++26 with reflection only: no pre-C++26 or other-compiler
# code comes back (a merge from main brings it).
add_test(
    NAME inside.tests.no_compat_code
    COMMAND
        ${CMAKE_COMMAND} -D INSIDE_ROOT=${PROJECT_SOURCE_DIR} -P
        ${CMAKE_CURRENT_SOURCE_DIR}/no_compat_code.cmake
)
set_tests_properties(inside.tests.no_compat_code PROPERTIES LABELS "tooling")

# Self-containment smoke: a TU that sees ONLY single_include/ proves the single
# header stands alone.
add_executable(inside.tests.single_header_smoke single_header_smoke.test.cpp)
target_include_directories(
    inside.tests.single_header_smoke
    PRIVATE "${single_include_dir}"
)
target_compile_features(inside.tests.single_header_smoke PRIVATE cxx_std_26)
target_link_libraries(inside.tests.single_header_smoke PRIVATE inside.warnings)

# Freestanding-leaning smoke: the single header with the io/string block dropped
# (INSIDE_NO_STRING) and exceptions off, installing a custom
# ins::error_handler in place of the default throw. Proves the core
# compiles with neither <string> nor the C++ exception ABI on its own surface.
add_executable(
    inside.tests.single_header_freestanding_smoke
    single_header_freestanding_smoke.test.cpp
)
target_include_directories(
    inside.tests.single_header_freestanding_smoke
    PRIVATE "${single_include_dir}"
)
target_compile_features(
    inside.tests.single_header_freestanding_smoke
    PRIVATE cxx_std_26
)
target_compile_definitions(
    inside.tests.single_header_freestanding_smoke
    PRIVATE INSIDE_NO_STRING
)
target_compile_options(
    inside.tests.single_header_freestanding_smoke
    PRIVATE -fno-exceptions
)
target_link_libraries(
    inside.tests.single_header_freestanding_smoke
    PRIVATE inside.warnings
)

foreach(
    smoke
    inside.tests.single_header_smoke
    inside.tests.single_header_freestanding_smoke
)
    if(INSIDE_MATH_NO_FP)
        target_compile_definitions(${smoke} PRIVATE INSIDE_MATH_NO_FP)
    endif()
    # The one flag a single-header user needs: static reflection.
    target_compile_options(${smoke} PRIVATE -freflection)
    add_test(NAME ${smoke} COMMAND ${smoke})
endforeach()

# FP-free smoke: compiles the single header with INSIDE_MATH_NO_FP and a
# poison <cmath> shim placed FIRST on the include path, so the build hard-errors
# if any <cmath> is pulled in. Proves the single header honors the FP-free guard
# (the math engine runs on its integer path). The proof relies on -I ordering
# for angle-bracket includes. Compile-only (an object library).
add_library(
    inside.tests.single_header_nofp_smoke
    OBJECT
    single_header_nofp_smoke.test.cpp
)
# The poison dir must precede single_include AND the system dirs. BEFORE
# keeps it ahead of single_include; angle-bracket search already places -I
# dirs ahead of the system headers, so <cmath> resolves to the poison shim.
target_include_directories(
    inside.tests.single_header_nofp_smoke
    BEFORE
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/nofp_poison" "${single_include_dir}"
)
target_compile_features(
    inside.tests.single_header_nofp_smoke
    PRIVATE cxx_std_26
)
target_compile_definitions(
    inside.tests.single_header_nofp_smoke
    PRIVATE INSIDE_MATH_NO_FP INSIDE_NO_STRING
)
target_compile_options(
    inside.tests.single_header_nofp_smoke
    PRIVATE -freflection
)

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
    add_test(
        NAME inside.tests.codegen_guard
        COMMAND
            ${CMAKE_CURRENT_SOURCE_DIR}/check_codegen.sh ${CMAKE_CXX_COMPILER}
            ${CMAKE_CURRENT_SOURCE_DIR}/perf_codegen.test.cpp
            ${PROJECT_SOURCE_DIR}/include 26 "${CMAKE_CXX_FLAGS}"
    )
    set_tests_properties(inside.tests.codegen_guard PROPERTIES LABELS "perf")
endif()

#---------------------------------------------------------------------------
# Tooling (opt-in): benchmarks, the docs reports, the perf workload
#---------------------------------------------------------------------------
if(INSIDE_BUILD_TOOLS)
    include(FetchContent)
    FetchContent_Declare(
        nanobench
        GIT_REPOSITORY https://github.com/martinus/nanobench.git
        GIT_TAG v4.3.11
        SYSTEM
    )
    FetchContent_MakeAvailable(nanobench)

    add_executable(inside.bench bench.test.cpp)
    target_link_libraries(
        inside.bench
        PRIVATE inside::inside inside.warnings nanobench
    )

    # Regenerates the committed performance tables: every nanobench group
    # prints native first, so the relative column is the inside-vs-native ratio.
    add_custom_target(
        inside.perf_report
        COMMAND
            ${CMAKE_COMMAND} -E env NANOBENCH_SUPPRESS_WARNINGS=1
            $<TARGET_FILE:inside.bench>
            ${PROJECT_SOURCE_DIR}/docs/performance.md
        COMMENT "Regenerating docs/performance.md"
    )

    # Accuracy characterization: the sweep (built above) as a markdown table.
    add_custom_target(
        inside.accuracy_report
        COMMAND inside.accuracy ${PROJECT_SOURCE_DIR}/docs/accuracy.md
        COMMENT "Regenerating docs/accuracy.md"
    )

    # Cachegrind instruction-count workload, driven by check_perf.py.
    add_executable(inside.perf_workload perf_workload.test.cpp)
    target_link_libraries(
        inside.perf_workload
        PRIVATE inside::inside inside.warnings
    )
endif()
