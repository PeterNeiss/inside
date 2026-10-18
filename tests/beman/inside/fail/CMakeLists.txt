# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#---------------------------------------------------------------------------
# Compile-fail diagnostics suite — each *.cpp here is deliberately ill-formed
# and must FAIL to compile with the message named on its first line
# (`// EXPECT: <substring>`; keep it free of regex metacharacters). Two
# optional lines follow in either order: `// REQUIRES: <tags>` (the tags are
# described in the loop below) and `// FLAGS: <options>`, extra compile
# options. The test invokes the build for the object target;
# PASS_REGULAR_EXPRESSION passes the test only when the build output contains
# the expected diagnostic — a TU that wrongly compiles produces no diagnostic
# and fails the test. RUN_SERIAL keeps concurrent `cmake --build` invocations
# out of each other's way.
# control_compiles.test.cpp is the harness control: valid code that must build.
#---------------------------------------------------------------------------
file(GLOB BEMAN_INSIDE_FAIL_SOURCES CONFIGURE_DEPENDS *.test.cpp)

# What the headers themselves detect (detail/config.hpp), probed with the
# flags the cases compile with: a deleted overload's reason (`= delete("…")`),
# a static_assert message with values (P2741), and big grids (C++26 static
# reflection), which compile the `64-bit-grids` cases.
include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_INCLUDES ${PROJECT_SOURCE_DIR}/include)
set(CMAKE_REQUIRED_FLAGS "")
if(MSVC)
    set(CMAKE_REQUIRED_FLAGS "/utf-8 /Zc:__cplusplus /Zc:preprocessor")
elseif(BEMAN_INSIDE_HAS_FREFLECTION)
    set(CMAKE_REQUIRED_FLAGS "-freflection")
endif()
foreach(feature IN ITEMS DELETE_REASON VALUED_DIAG BIG_GRIDS)
    if(feature STREQUAL "BIG_GRIDS")
        set(macro BEMAN_INSIDE_BIG_GRIDS)
    else()
        set(macro BEMAN_INSIDE_HAS_${feature})
    endif()
    check_cxx_source_compiles(
        "#include <beman/inside/detail/config.hpp>
#if !${macro}
#error ${macro} is 0
#endif
int main() {}
"
        BEMAN_INSIDE_FAIL_${feature}
    )
endforeach()
unset(CMAKE_REQUIRED_INCLUDES)
unset(CMAKE_REQUIRED_FLAGS)

foreach(src ${BEMAN_INSIDE_FAIL_SOURCES})
    get_filename_component(case_name ${src} NAME_WE)
    if(case_name STREQUAL "control_compiles")
        continue()
    endif()

    # Up to three header lines: `// EXPECT:` first, then `// REQUIRES:` and
    # `// FLAGS:` in either order. UTF-8: a message may hold "−".
    file(STRINGS ${src} head_lines LIMIT_COUNT 3 ENCODING UTF-8)
    list(GET head_lines 0 first_line)
    set(requires_line "")
    set(flags_line "")
    foreach(line IN LISTS head_lines)
        if(line MATCHES "^// REQUIRES: ")
            set(requires_line "${line}")
        elseif(line MATCHES "^// FLAGS: ")
            set(flags_line "${line}")
        endif()
    endforeach()
    # REQUIRES tags: `64-bit-grids` skips the case where big grids compile it; `gnu` / `msvc` build it only with GCC/Clang or
    # only with MSVC; `delete-reason`, `gnu-message` and `valued-diagnostics`
    # check the message only where the compiler prints it (a deleted
    # overload's reason; a constexpr_error's text, which MSVC leaves out; a
    # static_assert message with values, C++26) — elsewhere the case must
    # still be ill-formed.
    set(needs_message TRUE)
    if(requires_line)
        if(requires_line MATCHES "64-bit-grids" AND BEMAN_INSIDE_FAIL_BIG_GRIDS)
            continue()
        endif()
        if(requires_line MATCHES "REQUIRES: (.* )?msvc( |$)" AND NOT MSVC)
            continue()
        endif()
        if(requires_line MATCHES "REQUIRES: (.* )?gnu( |$)" AND MSVC)
            continue()
        endif()
        if(
            requires_line MATCHES "delete-reason"
            AND NOT BEMAN_INSIDE_FAIL_DELETE_REASON
        )
            set(needs_message FALSE)
        endif()
        if(requires_line MATCHES "gnu-message" AND MSVC)
            set(needs_message FALSE)
        endif()
        if(
            requires_line MATCHES "valued-diagnostics"
            AND NOT BEMAN_INSIDE_FAIL_VALUED_DIAG
        )
            set(needs_message FALSE)
        endif()
    endif()
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
    if(flags_line)
        string(REGEX REPLACE "^// FLAGS: *" "" extra_flags "${flags_line}")
        separate_arguments(extra_flags)
        target_compile_options(${fail_target} PRIVATE ${extra_flags})
    endif()

    add_test(
        NAME ${fail_target}
        COMMAND
            ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ${fail_target}
            --config $<CONFIG>
    )
    if(BEMAN_INSIDE_STRICT_SFINAE OR NOT needs_message)
        # Strict mode drops the diagnostic overloads that carry the named
        # messages (and an older compiler drops a deleted overload's reason);
        # the case must still be ill-formed, just without the hint.
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
    control_compiles.test.cpp
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
