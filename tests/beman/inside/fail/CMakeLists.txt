# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#---------------------------------------------------------------------------
# Compile-fail diagnostics suite — each *.cpp here is deliberately ill-formed
# and must FAIL to compile with the message named on its first line
# (`// EXPECT: <substring>`; keep it free of regex metacharacters). An
# optional second line `// FLAGS: <options>` adds compile options. The test
# runs the compiler on the case directly (syntax only, with the library's
# interface and the build's flags), not a build of the tree, so the cases run
# in parallel. PASS_REGULAR_EXPRESSION passes the test only when the output
# contains the expected diagnostic — a TU that wrongly compiles produces no
# diagnostic and fails the test.
# control_compiles.test.cpp is the harness control: valid code that must build.
#---------------------------------------------------------------------------
file(GLOB BEMAN_INSIDE_FAIL_SOURCES CONFIGURE_DEPENDS *.test.cpp)

# The compile line every case shares: C++26, the library's include directory,
# compile options and definitions (-freflection, BEMAN_INSIDE_MATH_NO_FP, …),
# and the build's own flags.
string(TOUPPER "${CMAKE_BUILD_TYPE}" fail_build_type)
separate_arguments(
    fail_build_flags
    UNIX_COMMAND
    "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${fail_build_type}}"
)
if(CMAKE_CXX_EXTENSIONS)
    set(fail_std ${CMAKE_CXX26_EXTENSION_COMPILE_OPTION})
else()
    set(fail_std ${CMAKE_CXX26_STANDARD_COMPILE_OPTION})
endif()
get_target_property(fail_options beman.inside INTERFACE_COMPILE_OPTIONS)
get_target_property(fail_definitions beman.inside INTERFACE_COMPILE_DEFINITIONS)
if(NOT fail_options)
    set(fail_options "")
endif()
if(fail_definitions)
    list(TRANSFORM fail_definitions PREPEND -D)
else()
    set(fail_definitions "")
endif()
set(fail_compile
    ${CMAKE_CXX_COMPILER}
    ${fail_std}
    -I${PROJECT_SOURCE_DIR}/include
    ${fail_options}
    ${fail_definitions}
    ${fail_build_flags}
    -fsyntax-only
)

foreach(src ${BEMAN_INSIDE_FAIL_SOURCES})
    get_filename_component(case_name ${src} NAME_WE)
    if(case_name STREQUAL "control_compiles")
        continue()
    endif()

    # `// EXPECT:` first, then an optional `// FLAGS:`. UTF-8: a message may
    # hold "−".
    file(STRINGS ${src} head_lines LIMIT_COUNT 2 ENCODING UTF-8)
    list(GET head_lines 0 first_line)
    set(flags_line "")
    foreach(line IN LISTS head_lines)
        if(line MATCHES "^// FLAGS: ")
            set(flags_line "${line}")
        endif()
    endforeach()
    string(REGEX REPLACE "^// EXPECT: *" "" expected_message "${first_line}")
    if(NOT first_line MATCHES "^// EXPECT: ")
        message(
            FATAL_ERROR
            "compile-fail case ${src} lacks a '// EXPECT: <message>' first line"
        )
    endif()

    set(extra_flags "")
    if(flags_line)
        string(REGEX REPLACE "^// FLAGS: *" "" extra_flags "${flags_line}")
        separate_arguments(extra_flags)
    endif()

    set(fail_target beman.inside.tests.fail.${case_name})
    add_test(NAME ${fail_target} COMMAND ${fail_compile} ${extra_flags} ${src})
    set_tests_properties(
        ${fail_target}
        PROPERTIES
            PASS_REGULAR_EXPRESSION "${expected_message}"
            LABELS "compile-fail"
    )
endforeach()

add_test(
    NAME beman.inside.tests.fail.control_compiles
    COMMAND
        ${fail_compile} ${CMAKE_CURRENT_SOURCE_DIR}/control_compiles.test.cpp
)
set_tests_properties(
    beman.inside.tests.fail.control_compiles
    PROPERTIES LABELS "compile-fail"
)
