#---------------------------------------------------------------------------
# Copyright (C) 2026 Peter Neiss
#---------------------------------------------------------------------------
# Pure-CMake amalgamator: inlines the bound/ + slim/ header tree into a single
# self-contained header. Run as a script:
#
#   cmake -D BOUND_AMALGAMATE_INCLUDE_DIR=<repo>/include \
#         -D BOUND_AMALGAMATE_OUTPUT=<repo>/single_include/bound/bound.hpp \
#         -D BOUND_AMALGAMATE_VERSION=1.0.0 \
#         [-D BOUND_AMALGAMATE_COMPARE=<committed header>] \
#         -P cmake/amalgamate.cmake
#
# Algorithm (mirrors the C preprocessor closely enough to be correct here):
#   * Roots = all public headers (include/bound/*.hpp); detail/* and slim/* are
#     pulled in transitively. bound.hpp is emitted first for readability.
#   * Each file is emitted at most once (a global EMITTED set mirrors the
#     original include guards' "include once").
#   * Internal #include "bound/.." / "slim/.." are replaced *in place* by the
#     included file's body, preserving any surrounding #ifdef.
#   * The file's own include guard (#ifndef BND<x>HPP / #define / trailing
#     #endif) and #pragma once are dropped; one outer guard wraps the result.
#   * Per-file Copyright / SPDX banner lines are dropped; one banner up top.
#   * #include <system> at conditional-depth 0 is hoisted+deduped to the top;
#     conditional ones (e.g. <format> under #ifdef __cpp_lib_format) stay in
#     place so their guard is preserved.
#
# Assumption (holds for bound's acyclic, non-branching include graph): no header
# is included from two mutually-exclusive #ifdef branches, so emit-once dedup and
# hoisting of a child's depth-0 system includes are always safe.
#---------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.24)

foreach(_req IN ITEMS BOUND_AMALGAMATE_INCLUDE_DIR BOUND_AMALGAMATE_OUTPUT)
  if(NOT DEFINED ${_req})
    message(FATAL_ERROR "amalgamate: ${_req} is not set")
  endif()
endforeach()
if(NOT DEFINED BOUND_AMALGAMATE_VERSION)
  set(BOUND_AMALGAMATE_VERSION "unknown")
endif()

get_filename_component(INC "${BOUND_AMALGAMATE_INCLUDE_DIR}" ABSOLUTE)
if(NOT IS_DIRECTORY "${INC}")
  message(FATAL_ERROR "amalgamate: include dir not found: ${INC}")
endif()

string(ASCII 10 NL)    # newline, for list-free line splitting

set(BODY_FILE "${BOUND_AMALGAMATE_OUTPUT}.body.tmp")
get_filename_component(_out_dir "${BOUND_AMALGAMATE_OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${_out_dir}")
file(WRITE "${BODY_FILE}" "")
set_property(GLOBAL PROPERTY AMALG_EMITTED "")
set_property(GLOBAL PROPERTY AMALG_SYS "")
# When TRUE, a header's depth-0 system includes are kept inline rather than
# hoisted to the top — used for bound/io.hpp so its <string>/<ostream>/<format>
# stay inside the BND_NO_STRING guard and vanish when the block is dropped.
set_property(GLOBAL PROPERTY AMALG_INLINE_SYS FALSE)

#---------------------------------------------------------------------------
# amalg_process(<rel>) — inline the header at <INC>/<rel> (e.g. "bound/core.hpp")
# into BODY_FILE, recursing into its internal includes.
#---------------------------------------------------------------------------
function(amalg_process rel)
  get_filename_component(abs "${INC}/${rel}" ABSOLUTE)
  if(NOT EXISTS "${abs}")
    message(FATAL_ERROR "amalgamate: missing header ${abs} (referenced as ${rel})")
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
    string(APPEND content "${NL}")          # ensure a clean final line
  endif()

  set(buf "${NL}")
  string(APPEND buf "// ======================================================================${NL}")
  string(APPEND buf "//  ${rel}${NL}")
  string(APPEND buf "// ======================================================================${NL}")

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

    # --- file include guard: drop #ifndef BND<x>HPP / #define <same> ---------
    if(guard_macro STREQUAL "" AND
       line MATCHES "^[ \t]*#[ \t]*ifndef[ \t]+(BND[A-Za-z0-9_]*HPP)[ \t]*$")
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
    if(line MATCHES "^//[ \t]*Copyright" OR line MATCHES "^//[ \t]*SPDX-License-Identifier")
      continue()
    endif()

    # --- internal include: inline in place -----------------------------------
    # Flush what we have, then recurse so the child's body lands exactly where
    # the directive was (preserving any surrounding #ifdef). The directive line
    # itself is dropped.
    if(line MATCHES "^[ \t]*#[ \t]*include[ \t]*[<\"](bound|slim)/([^\">]+)[>\"]")
      file(APPEND "${BODY_FILE}" "${buf}")
      set(buf "")
      amalg_process("${CMAKE_MATCH_1}/${CMAKE_MATCH_2}")
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
# Roots: every public header, bound.hpp first.
#---------------------------------------------------------------------------
file(GLOB root_abs "${INC}/bound/*.hpp")
list(SORT root_abs)
set(roots "")
foreach(r IN LISTS root_abs)
  get_filename_component(name "${r}" NAME)
  list(APPEND roots "bound/${name}")
endforeach()
list(REMOVE_ITEM roots "bound/bound.hpp")
list(INSERT roots 0 "bound/bound.hpp")

# bound/io.hpp gathers all <string>/<ostream>/<format> support; wrap its inlined
# body in BND_NO_STRING so a freestanding single-header user can drop it (and its
# heavy system includes) by defining the macro. Processed like any root, but its
# depth-0 system includes are kept inline (see AMALG_INLINE_SYS) so they sit
# inside the guard.
foreach(r IN LISTS roots)
  if(r STREQUAL "bound/io.hpp")
    file(APPEND "${BODY_FILE}" "${NL}#ifndef BND_NO_STRING${NL}")
    set_property(GLOBAL PROPERTY AMALG_INLINE_SYS TRUE)
    amalg_process("${r}")
    set_property(GLOBAL PROPERTY AMALG_INLINE_SYS FALSE)
    file(APPEND "${BODY_FILE}" "${NL}#endif // BND_NO_STRING${NL}")
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
"//---------------------------------------------------------------------------
// bound ${BOUND_AMALGAMATE_VERSION} — single-header amalgamation
//
//   *** GENERATED FILE — DO NOT EDIT BY HAND ***
//
// Regenerate with:  cmake --build <build-dir> --target amalgamate
// Source of truth:  include/bound/*.hpp, include/slim/*.hpp
//
// Copyright (C) 2026 Peter Neiss
// slim/* components are MIT-licensed (SPDX-License-Identifier: MIT).
//---------------------------------------------------------------------------
#ifndef BND_SINGLE_HEADER_HPP
#define BND_SINGLE_HEADER_HPP

${sys_block}
")

file(WRITE "${BOUND_AMALGAMATE_OUTPUT}" "${banner}${body}\n#endif // BND_SINGLE_HEADER_HPP\n")

#---------------------------------------------------------------------------
# Drift check mode.
#---------------------------------------------------------------------------
if(DEFINED BOUND_AMALGAMATE_COMPARE)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files
            "${BOUND_AMALGAMATE_OUTPUT}" "${BOUND_AMALGAMATE_COMPARE}"
    RESULT_VARIABLE _diff)
  if(_diff)
    message(FATAL_ERROR
      "Single header is out of date:\n"
      "  ${BOUND_AMALGAMATE_COMPARE}\n"
      "differs from a freshly generated amalgamation. Run the 'amalgamate' "
      "target and commit the result.")
  endif()
  message(STATUS "amalgamate: single header is up to date.")
else()
  message(STATUS "amalgamate: wrote ${BOUND_AMALGAMATE_OUTPUT}")
endif()
