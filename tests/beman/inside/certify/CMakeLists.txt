# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# The certify tool's own checks: the result hash recomputed with Python's
# hashlib, and find_grids.py run over the examples (whose unoptimized objects
# keep every math instantiation as a symbol).
find_package(Python3 COMPONENTS Interpreter)
if(NOT Python3_Interpreter_FOUND OR CMAKE_CROSSCOMPILING)
    return()
endif()

add_test(
    NAME beman.inside.tests.certify_hash
    COMMAND
        ${Python3_EXECUTABLE} -I ${CMAKE_CURRENT_SOURCE_DIR}/check_hash.py --
        $<TARGET_FILE:beman.inside.certify> --no-progress --pair atanh --dump
)
set_tests_properties(
    beman.inside.tests.certify_hash
    PROPERTIES LABELS "certify"
)

# Only where the examples are built unoptimized, with an Itanium nm, and
# without big grids (their symbols have no 64-bit spelling).
string(TOUPPER "${CMAKE_BUILD_TYPE}" build_type)
if(
    BEMAN_INSIDE_BUILD_EXAMPLES
    AND CMAKE_NM
    AND NOT MSVC
    AND NOT BEMAN_INSIDE_HAS_FREFLECTION
    AND (build_type STREQUAL "" OR build_type STREQUAL "DEBUG")
)
    add_test(
        NAME beman.inside.tests.find_grids
        COMMAND
            ${Python3_EXECUTABLE} -I ${CMAKE_CURRENT_SOURCE_DIR}/find_grids.py
            ${PROJECT_BINARY_DIR}/examples --nm ${CMAKE_NM} --min-pairs 10
            --emit ${CMAKE_CURRENT_BINARY_DIR}/example_pairs.cpp --run --cxx
            ${CMAKE_CXX_COMPILER} --cxxflags "${CMAKE_CXX_FLAGS}" -I
            ${PROJECT_SOURCE_DIR}/include -- --estimate --max-slots 0
    )
    set_tests_properties(
        beman.inside.tests.find_grids
        PROPERTIES LABELS "certify" TIMEOUT 600
    )
endif()
