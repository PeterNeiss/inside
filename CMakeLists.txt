cmake_minimum_required (VERSION 3.24)

project(bound LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(bound_bound INTERFACE)
add_library(bound::bound ALIAS bound_bound)

target_include_directories(bound_bound
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_compile_options(bound_bound
    INTERFACE
        $<$<CXX_COMPILER_ID:GNU,Clang>:
            -Wall -Wextra -Wpedantic
            -Wconversion -Wsign-conversion
            -Wnon-virtual-dtor -Wold-style-cast
            -Woverloaded-virtual -Wnull-dereference
            # bound's default constructor is intentionally trivial (Raw left
            # uninitialised) so the type stays trivial for NTTP / memcpy use.
            -Wno-uninitialized
            -Wno-missing-field-initializers
            -Wformat=2
        >
        $<$<CXX_COMPILER_ID:MSVC>:
            /W4 /WX /permissive-
        >
)

include(CheckCXXSourceCompiles)

set(CMAKE_REQUIRED_FLAGS "-std=c++23")
set(CMAKE_REQUIRED_LIBRARIES stdc++exp)
check_cxx_source_compiles("
    #include <stacktrace>
    int main() { return std::stacktrace::current().size(); }
" BOUND_HAS_STACKTRACE)
unset(CMAKE_REQUIRED_FLAGS)
unset(CMAKE_REQUIRED_LIBRARIES)

if(BOUND_HAS_STACKTRACE)
    target_compile_definitions(bound_bound INTERFACE
        $<$<NOT:$<CONFIG:Release>>:BOUND_HAS_STACKTRACE>
    )
    target_link_libraries(bound_bound INTERFACE
        $<$<NOT:$<CONFIG:Release>>:stdc++exp>
    )
endif()

#---------------------------------------------------------------------------
# Coverage (opt-in; instruments the bound interface so test TUs are covered)
#---------------------------------------------------------------------------
option(BOUND_COVERAGE "Enable gcov instrumentation for tests" OFF)
if(BOUND_COVERAGE)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR "BOUND_COVERAGE requires GCC or Clang")
    endif()
    target_compile_options(bound_bound INTERFACE --coverage -O0 -g)
    target_link_options   (bound_bound INTERFACE --coverage)
endif()

enable_testing()

include(FetchContent)

#---------------------------------------------------------------------------
# Catch2 (test framework)
#---------------------------------------------------------------------------
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.5.4
)
FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)

file(GLOB BOUND_TEST_SOURCES CONFIGURE_DEPENDS tests/test_*.cpp)
add_executable(bound_tests ${BOUND_TEST_SOURCES})
target_link_libraries(bound_tests PRIVATE bound::bound Catch2::Catch2WithMain)
catch_discover_tests(bound_tests)

#---------------------------------------------------------------------------
# ctrack (benchmarking)
#---------------------------------------------------------------------------
set(DISABLE_EXAMPLES ON CACHE BOOL "" FORCE)
FetchContent_Declare(
  ctrack
  GIT_REPOSITORY https://github.com/Compaile/ctrack.git
  GIT_TAG        v1.1.0
)
FetchContent_MakeAvailable(ctrack)

add_executable(bench tests/bench.cpp)
target_link_libraries(bench PUBLIC bound::bound)
get_target_property(CTRACK_INCLUDE_DIRS ctrack INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(bench SYSTEM PUBLIC ${CTRACK_INCLUDE_DIRS})

#---------------------------------------------------------------------------
# Coverage target — runs ctest under --coverage and produces an HTML report.
# Only useful when configured with -DBOUND_COVERAGE=ON.
#---------------------------------------------------------------------------
if(BOUND_COVERAGE)
    find_program(LCOV_EXE     lcov    REQUIRED)
    find_program(GENHTML_EXE  genhtml REQUIRED)

    # Match gcov to the compiler so .gcda format versions agree (system /usr/bin/gcov
    # may belong to a different GCC than the one selected via CMAKE_CXX_COMPILER).
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        string(REGEX MATCH "^[0-9]+" GCC_MAJOR "${CMAKE_CXX_COMPILER_VERSION}")
        find_program(GCOV_EXE
            NAMES gcov-${CMAKE_CXX_COMPILER_VERSION} gcov-${GCC_MAJOR} gcov
            REQUIRED)
    else()
        find_program(GCOV_EXE NAMES llvm-cov gcov REQUIRED)
    endif()

    add_custom_target(coverage
        COMMAND ${LCOV_EXE} --gcov-tool ${GCOV_EXE} --directory . --zerocounters
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        COMMAND ${LCOV_EXE} --gcov-tool ${GCOV_EXE} --capture --directory .
                            --base-directory ${CMAKE_SOURCE_DIR}
                            --no-external
                            --output-file coverage.info
        COMMAND ${LCOV_EXE} --remove coverage.info '*/tests/*'
                            --ignore-errors unused
                            --output-file coverage.info
        COMMAND ${GENHTML_EXE} coverage.info --output-directory coverage
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Coverage: running ctest, generating HTML at build/coverage/index.html"
        VERBATIM)
endif()

#---------------------------------------------------------------------------
# Examples
#---------------------------------------------------------------------------
file(GLOB EXAMPLE_SOURCES examples/*.cpp)
foreach(example_src ${EXAMPLE_SOURCES})
  get_filename_component(example_name ${example_src} NAME_WE)
  add_executable(example_${example_name} ${example_src})
  target_link_libraries(example_${example_name} PUBLIC bound::bound)
endforeach()
