# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
# The library is C++26 with static reflection only: fail when a header (or a
# test or example) tests for an older language mode, another compiler, or the
# 64-bit grid fallback again — e.g. after a merge from a branch that keeps them.
#
#   cmake -D BEMAN_INSIDE_ROOT=<source dir> -P no_compat_code.cmake
set(pattern
    "BEMAN_INSIDE_BIG_GRIDS|BEMAN_INSIDE_DELETE|BEMAN_INSIDE_WARNINGS_|BEMAN_INSIDE_MSVC|BEMAN_INSIDE_HAS_(STD_FORMAT|VALUED_DIAG|DELETE_REASON|OVERFLOW_BUILTINS|UMUL128)|_MSC_VER|__cplusplus|grid_gcd_fits|grid_sum_fits|grid_product_fits"
)
file(
    GLOB_RECURSE sources
    "${BEMAN_INSIDE_ROOT}/include/*.hpp"
    "${BEMAN_INSIDE_ROOT}/tests/*.cpp"
    "${BEMAN_INSIDE_ROOT}/tests/*.hpp"
    "${BEMAN_INSIDE_ROOT}/examples/*.cpp"
)
set(found "")
foreach(source IN LISTS sources)
    file(STRINGS "${source}" hits REGEX "${pattern}")
    if(hits)
        file(RELATIVE_PATH name "${BEMAN_INSIDE_ROOT}" "${source}")
        list(JOIN hits "\n    " text)
        string(APPEND found "\n  ${name}:\n    ${text}")
    endif()
endforeach()
if(found)
    message(FATAL_ERROR "pre-C++26 compatibility code:${found}")
endif()
