# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#---------------------------------------------------------------------------
# Compile-fail diagnostics suite — each *.cpp here is deliberately ill-formed
# and must FAIL to compile with the message named on its first line
# (`// EXPECT: <substring>`; keep it free of regex metacharacters). The test
# invokes the build for the object target; PASS_REGULAR_EXPRESSION passes the
# test only when the build output contains the expected diagnostic — a TU that
# wrongly compiles produces no diagnostic and fails the test. RUN_SERIAL keeps
# concurrent `cmake --build` invocations out of each other's way.
# control_compiles.cpp is the harness control: valid code that must build.
#---------------------------------------------------------------------------
file(GLOB BEMAN_INSIDE_FAIL_SOURCES CONFIGURE_DEPENDS *.cpp)

foreach(src ${BEMAN_INSIDE_FAIL_SOURCES})
    get_filename_component(case_name ${src} NAME_WE)
    if(case_name STREQUAL "control_compiles")
        continue()
    endif()

    file(STRINGS ${src} first_line LIMIT_COUNT 1)
    string(REGEX REPLACE "^// EXPECT: *" "" expected_message "${first_line}")
    if(NOT first_line MATCHES "^// EXPECT: ")
        message(
            FATAL_ERROR
            "compile-fail case ${src} lacks a '// EXPECT: <message>' first line"
        )
    endif()

    set(fail_target beman.inside.tests.fail.${case_name})
    add_library(${fail_target} OBJECT EXCLUDE_FROM_ALL ${src})
    target_link_libraries(${fail_target} PRIVATE beman::inside)

    add_test(
        NAME ${fail_target}
        COMMAND
            ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target
            ${fail_target} --config $<CONFIG>
    )
    if(BEMAN_INSIDE_STRICT_SFINAE)
        # Strict mode drops the diagnostic overloads that carry the named
        # messages; the case must still be ill-formed, just without the hint.
        set_tests_properties(
            ${fail_target}
            PROPERTIES WILL_FAIL TRUE RUN_SERIAL TRUE LABELS "compile-fail"
        )
    else()
        set_tests_properties(
            ${fail_target}
            PROPERTIES
                PASS_REGULAR_EXPRESSION "${expected_message}"
                RUN_SERIAL TRUE
                LABELS "compile-fail"
        )
    endif()
endforeach()

add_library(
    beman.inside.tests.fail.control_compiles
    OBJECT
    EXCLUDE_FROM_ALL
    control_compiles.cpp
)
target_link_libraries(
    beman.inside.tests.fail.control_compiles
    PRIVATE beman::inside
)
add_test(
    NAME beman.inside.tests.fail.control_compiles
    COMMAND
        ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target
        beman.inside.tests.fail.control_compiles --config $<CONFIG>
)
set_tests_properties(
    beman.inside.tests.fail.control_compiles
    PROPERTIES RUN_SERIAL TRUE LABELS "compile-fail"
)
