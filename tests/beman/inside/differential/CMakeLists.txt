# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# The differential oracle: beman.inside.differential prints random cases of
# arithmetic, %/divmod, rounding stores, bounded continuous grids, float
# conversions and π conversions; check.py recomputes each one exactly. A
# fixed seed here; a scheduled CI run takes a random seed and more cases.
add_executable(beman.inside.differential)
target_sources(
    beman.inside.differential
    PRIVATE
        dump.cpp
        dump_arith.cpp
        dump_round.cpp
        dump_bounded.cpp
        dump_float.cpp
        dump_pi.cpp
)
target_link_libraries(
    beman.inside.differential
    PRIVATE beman::inside beman.inside.warnings
)

find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_test(
        NAME beman.inside.tests.differential
        COMMAND
            ${Python3_EXECUTABLE} -I ${CMAKE_CURRENT_SOURCE_DIR}/check.py --seed
            1 --n 200 -- ${CMAKE_CROSSCOMPILING_EMULATOR}
            $<TARGET_FILE:beman.inside.differential>
    )
    set_tests_properties(
        beman.inside.tests.differential
        PROPERTIES LABELS "differential"
    )
else()
    message(
        STATUS
        "beman.inside: no Python 3, the differential test is skipped"
    )
endif()
