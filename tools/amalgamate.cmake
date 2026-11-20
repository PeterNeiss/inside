# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
# Pure-CMake amalgamator: inlines the inside/ header tree into a single
# self-contained header. Run as a script:
#
#   cmake -D INSIDE_AMALGAMATE_INCLUDE_DIR=<repo>/include \
#         -D INSIDE_AMALGAMATE_OUTPUT=<repo>/single_include/inside/inside.hpp \
#         -D INSIDE_AMALGAMATE_VERSION=0.10.0 \
#         [-D INSIDE_AMALGAMATE_COMPARE=<committed header>] \
#         -P tools/amalgamate.cmake
#
# Algorithm (mirrors the C preprocessor closely enough to be correct here):
#   * Roots = all public headers (include/inside/*.hpp); detail/* is
#     pulled in transitively. inside.hpp is emitted first for readability.
#   * Each file is emitted at most once (a global EMITTED set mirrors the
#     original include guards' "include once").
#   * Internal #include <inside/..> are replaced *in place* by the
#     included file's body, preserving any surrounding #ifdef.
#   * The file's own include guard (#ifndef INSIDE_<x>_HPP / #define / trailing
#     #endif) and #pragma once are dropped; one outer guard wraps the result.
#   * Per-file Copyright / SPDX banner lines are dropped; one banner up top.
#   * #include <system> at conditional-depth 0 is hoisted+deduped to the top;
#     conditional ones (e.g. <format> under #ifdef __cpp_lib_format) stay in
#     place so their guard is preserved.
#
# Assumption (holds for inside's acyclic, non-branching include graph): no header
# is included from two mutually-exclusive #ifdef branches, so emit-once dedup and
# hoisting of a child's depth-0 system includes are always safe.
#---------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.24)

foreach(_req IN ITEMS INSIDE_AMALGAMATE_INCLUDE_DIR INSIDE_AMALGAMATE_OUTPUT)
    if(NOT DEFINED ${_req})
        message(FATAL_ERROR "amalgamate: ${_req} is not set")
    endif()
endforeach()
if(NOT DEFINED INSIDE_AMALGAMATE_VERSION)
    set(INSIDE_AMALGAMATE_VERSION "unknown")
endif()

get_filename_component(INC "${INSIDE_AMALGAMATE_INCLUDE_DIR}" ABSOLUTE)
if(NOT IS_DIRECTORY "${INC}")
    message(FATAL_ERROR "amalgamate: include dir not found: ${INC}")
endif()

string(ASCII 10 NL) # newline, for list-free line splitting

set(BODY_FILE "${INSIDE_AMALGAMATE_OUTPUT}.body.tmp")
get_filename_component(_out_dir "${INSIDE_AMALGAMATE_OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${_out_dir}")
file(WRITE "${BODY_FILE}" "")
set_property(GLOBAL PROPERTY AMALG_EMITTED "")
set_property(GLOBAL PROPERTY AMALG_SYS "")
# When TRUE, a header's depth-0 system includes are kept inline rather than
# hoisted to the top — used for inside/inside/io.hpp so its <string>/<ostream>/<format>
# stay inside the INSIDE_NO_STRING guard and vanish when the block is dropped.
set_property(GLOBAL PROPERTY AMALG_INLINE_SYS FALSE)

#---------------------------------------------------------------------------
# amalg_process(<rel>) — inline the header at <INC>/<rel> (e.g. "inside/inside/detail/core.hpp")
# into BODY_FILE, recursing into its internal includes.
#---------------------------------------------------------------------------
function(amalg_process rel)
    get_filename_component(abs "${INC}/${rel}" ABSOLUTE)
    if(NOT EXISTS "${abs}")
        message(
            FATAL_ERROR
            "amalgamate: missing header ${abs} (referenced as ${rel})"
        )
    endif()

    get_property(emitted GLOBAL PROPERTY AMALG_EMITTED)
    if("${abs}" IN_LIST emitted)
        return()
    endif()
    list(APPEND emitted "${abs}")
    set_property(GLOBAL PROPERTY AMALG_EMITTED "${emitted}")

    # Read the file. Iterate line-by-line via FIND/SUBSTRING rather than splitting
    # into a CMake list: list semantics mangle lines containing '[' or a trailing
    # backslash (line continuation), and drop blank lines.
    file(READ "${abs}" content)
    string(REGEX REPLACE "\r" "" content "${content}")
    if(NOT content MATCHES "${NL}$")
        string(APPEND content "${NL}") # ensure a clean final line
    endif()

    set(buf "${NL}")
    string(
        APPEND buf
        "// ======================================================================${NL}"
    )
    string(APPEND buf "//  ${rel}${NL}")
    string(
        APPEND buf
        "// ======================================================================${NL}"
    )

    set(depth 0)
    set(guard_macro "")
    set(expect_define FALSE)

    while(NOT content STREQUAL "")
        string(FIND "${content}" "${NL}" _pos)
        if(_pos EQUAL -1)
            set(line "${content}")
            set(content "")
        else()
            string(SUBSTRING "${content}" 0 "${_pos}" line)
            math(EXPR _rest "${_pos}+1")
            string(SUBSTRING "${content}" "${_rest}" -1 content)
        endif()

        # --- file include guard: drop #ifndef INSIDE_<x>_HPP / #define <same>
        if(
            guard_macro STREQUAL ""
            AND line
                MATCHES
                "^[ \t]*#[ \t]*ifndef[ \t]+(INSIDE_[A-Za-z0-9_]*_HPP)[ \t]*$"
        )
            set(guard_macro "${CMAKE_MATCH_1}")
            set(expect_define TRUE)
            continue()
        endif()
        if(expect_define)
            set(expect_define FALSE)
            if(line MATCHES "^[ \t]*#[ \t]*define[ \t]+${guard_macro}[ \t]*$")
                continue()
            endif()
        endif()
        if(line MATCHES "^[ \t]*#[ \t]*pragma[ \t]+once")
            continue()
        endif()

        # --- per-file copyright / license banner ---------------------------------
        if(
            line MATCHES "^//[ \t]*Copyright"
            OR line MATCHES "^//[ \t]*SPDX-License-Identifier"
        )
            continue()
        endif()

        # --- internal include: inline in place -----------------------------------
        # Flush what we have, then recurse so the child's body lands exactly where
        # the directive was (preserving any surrounding #ifdef). The directive line
        # itself is dropped.
        if(line MATCHES "^[ \t]*#[ \t]*include[ \t]*[<\"]inside/([^\">]+)[>\"]")
            file(APPEND "${BODY_FILE}" "${buf}")
            set(buf "")
            amalg_process("inside/${CMAKE_MATCH_1}")
            continue()
        endif()

        # --- conditional nesting depth (ignoring the file's own guard) -----------
        if(line MATCHES "^[ \t]*#[ \t]*(if|ifdef|ifndef)([ \t]|$)")
            math(EXPR depth "${depth}+1")
        elseif(line MATCHES "^[ \t]*#[ \t]*endif")
            if(depth EQUAL 0)
                # the outermost #endif is the file's include-guard close -> drop it
                continue()
            endif()
            math(EXPR depth "${depth}-1")
        endif()

        # --- system include: hoist when unconditional (unless inline-sys) ---------
        if(line MATCHES "^[ \t]*#[ \t]*include[ \t]*<([^>]+)>")
            if(depth EQUAL 0)
                get_property(inline_sys GLOBAL PROPERTY AMALG_INLINE_SYS)
                if(inline_sys)
                    string(APPEND buf "#include <${CMAKE_MATCH_1}>${NL}")
                else()
                    get_property(sys GLOBAL PROPERTY AMALG_SYS)
                    list(APPEND sys "#include <${CMAKE_MATCH_1}>")
                    set_property(GLOBAL PROPERTY AMALG_SYS "${sys}")
                endif()
                continue()
            endif()
        endif()

        string(APPEND buf "${line}${NL}")
    endwhile()

    file(APPEND "${BODY_FILE}" "${buf}")
endfunction()

#---------------------------------------------------------------------------
# Roots: every public header - the umbrellas (inside.hpp, math.hpp,
# units.hpp) and the public headers of their folders, not detail/ - with
# inside.hpp first.
#---------------------------------------------------------------------------
file(GLOB root_abs "${INC}/inside/*.hpp" "${INC}/inside/*/*.hpp")
list(SORT root_abs)
set(roots "")
foreach(r IN LISTS root_abs)
    file(RELATIVE_PATH rel "${INC}" "${r}")
    list(APPEND roots "${rel}")
endforeach()
list(REMOVE_ITEM roots "inside/inside.hpp")
list(INSERT roots 0 "inside/inside.hpp")

# inside/inside/io.hpp gathers all <string>/<ostream>/<format> support; wrap its inlined
# body in INSIDE_NO_STRING so a freestanding single-header user can drop it (and its
# heavy system includes) by defining the macro. Processed like any root, but its
# depth-0 system includes are kept inline (see AMALG_INLINE_SYS) so they sit
# inside the guard.
# inside/inside/random.hpp needs <random> — hosted-only, and it pulls <cmath> — so
# it sits inside a guard the same way: a freestanding or FP-free
# (INSIDE_MATH_NO_FP, resolved earlier in the header) build drops it.
# inside/units.hpp (units/core.hpp and units/systems.hpp), units/math.hpp
# and units/constants.hpp sit inside a
# INSIDE_NO_UNITS guard, which drops them (and their compile time);
# units/io.hpp prints through inside/io.hpp, so it needs both guards, and
# units/random.hpp samples through inside/random.hpp, so it needs its guard
# and INSIDE_NO_UNITS. The math
# constants come in earlier, through math.hpp and math/constants.hpp.
# inside/inside/mdspan.hpp (<mdspan>, <ranges>: hosted only) sits inside a
# INSIDE_NO_MDSPAN guard, and units/mdspan.hpp also inside INSIDE_NO_UNITS.
foreach(r IN LISTS roots)
    set(_guard_open "")
    set(_guard_close "")
    if(r STREQUAL "inside/inside/io.hpp")
        set(_guard_open "#ifndef INSIDE_NO_STRING")
        set(_guard_close "#endif // INSIDE_NO_STRING")
    elseif(
        r STREQUAL "inside/units.hpp"
        OR r STREQUAL "inside/units/core.hpp"
        OR r STREQUAL "inside/units/systems.hpp"
        OR r STREQUAL "inside/units/math.hpp"
        OR r STREQUAL "inside/units/constants.hpp"
        OR r STREQUAL "inside/units/geoalg.hpp"
    )
        set(_guard_open "#ifndef INSIDE_NO_UNITS")
        set(_guard_close "#endif // INSIDE_NO_UNITS")
    elseif(r STREQUAL "inside/units/io.hpp")
        set(_guard_open
            "#if !defined(INSIDE_NO_STRING) && !defined(INSIDE_NO_UNITS)"
        )
        set(_guard_close "#endif // !INSIDE_NO_STRING && !INSIDE_NO_UNITS")
    elseif(r STREQUAL "inside/units/random.hpp")
        set(_guard_open
            "#if __STDC_HOSTED__ && !defined(INSIDE_MATH_NO_FP) && !defined(INSIDE_NO_UNITS)"
        )
        set(_guard_close
            "#endif // __STDC_HOSTED__ && !INSIDE_MATH_NO_FP && !INSIDE_NO_UNITS"
        )
    elseif(r STREQUAL "inside/inside/mdspan.hpp")
        set(_guard_open "#if __STDC_HOSTED__ && !defined(INSIDE_NO_MDSPAN)")
        set(_guard_close "#endif // __STDC_HOSTED__ && !INSIDE_NO_MDSPAN")
    elseif(r STREQUAL "inside/units/mdspan.hpp")
        set(_guard_open
            "#if __STDC_HOSTED__ && !defined(INSIDE_NO_MDSPAN) && !defined(INSIDE_NO_UNITS)"
        )
        set(_guard_close
            "#endif // __STDC_HOSTED__ && !INSIDE_NO_MDSPAN && !INSIDE_NO_UNITS"
        )
    elseif(r STREQUAL "inside/inside/random.hpp")
        set(_guard_open "#if __STDC_HOSTED__ && !defined(INSIDE_MATH_NO_FP)")
        set(_guard_close "#endif // __STDC_HOSTED__ && !INSIDE_MATH_NO_FP")
    endif()
    if(_guard_open)
        file(APPEND "${BODY_FILE}" "${NL}${_guard_open}${NL}")
        set_property(GLOBAL PROPERTY AMALG_INLINE_SYS TRUE)
        amalg_process("${r}")
        set_property(GLOBAL PROPERTY AMALG_INLINE_SYS FALSE)
        file(APPEND "${BODY_FILE}" "${NL}${_guard_close}${NL}")
    else()
        amalg_process("${r}")
    endif()
endforeach()

#---------------------------------------------------------------------------
# Assemble: banner + outer guard + hoisted system includes + body.
#---------------------------------------------------------------------------
get_property(sys GLOBAL PROPERTY AMALG_SYS)
list(REMOVE_DUPLICATES sys)
list(SORT sys)
string(REPLACE ";" "\n" sys_block "${sys}")

file(READ "${BODY_FILE}" body)
file(REMOVE "${BODY_FILE}")

set(banner
    "// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//---------------------------------------------------------------------------
// inside ${INSIDE_AMALGAMATE_VERSION} — single-header amalgamation
//
//   *** GENERATED FILE — DO NOT EDIT BY HAND ***
//
// Regenerate with:  cmake --build <build-dir> --target inside.amalgamate
// Source of truth:  include/inside/ (inside.hpp, math.hpp, units.hpp and their folders)
//---------------------------------------------------------------------------
#ifndef INSIDE_SINGLE_HEADER_HPP
#define INSIDE_SINGLE_HEADER_HPP

${sys_block}
"
)

file(
    WRITE "${INSIDE_AMALGAMATE_OUTPUT}"
    "${banner}${body}\n#endif // INSIDE_SINGLE_HEADER_HPP\n"
)

#---------------------------------------------------------------------------
# Drift check mode.
#---------------------------------------------------------------------------
if(DEFINED INSIDE_AMALGAMATE_COMPARE)
    execute_process(
        COMMAND
            "${CMAKE_COMMAND}" -E compare_files "${INSIDE_AMALGAMATE_OUTPUT}"
            "${INSIDE_AMALGAMATE_COMPARE}"
        RESULT_VARIABLE _diff
    )
    if(_diff)
        message(
            FATAL_ERROR
            "Single header is out of date:\n"
            "  ${INSIDE_AMALGAMATE_COMPARE}\n"
            "differs from a freshly generated amalgamation. Run the 'inside.amalgamate' "
            "target and commit the result."
        )
    endif()
    message(STATUS "amalgamate: single header is up to date.")
else()
    message(STATUS "amalgamate: wrote ${INSIDE_AMALGAMATE_OUTPUT}")
endif()
