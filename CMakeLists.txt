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
            -Wall -Wextra -Wpedantic -Werror
            -Wshadow -Wconversion -Wsign-conversion
            -Wnon-virtual-dtor -Wold-style-cast
            -Woverloaded-virtual -Wnull-dereference
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

add_executable(test tests/test.cpp)
target_link_libraries(test PUBLIC bound::bound)

add_executable(algo tests/algo.cpp)
target_link_libraries(algo PUBLIC bound::bound)

include(FetchContent)
set(DISABLE_EXAMPLES ON CACHE BOOL "" FORCE)
FetchContent_Declare(
  ctrack
  GIT_REPOSITORY https://github.com/Compaile/ctrack.git
  GIT_TAG        main
)
FetchContent_MakeAvailable(ctrack)

add_executable(bench tests/bench.cpp)
target_link_libraries(bench PUBLIC bound::bound)
get_target_property(CTRACK_INCLUDE_DIRS ctrack INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(bench SYSTEM PUBLIC ${CTRACK_INCLUDE_DIRS})

file(GLOB EXAMPLE_SOURCES examples/*.cpp)
list(FILTER EXAMPLE_SOURCES EXCLUDE REGEX "division\\.cpp$")
foreach(example_src ${EXAMPLE_SOURCES})
  get_filename_component(example_name ${example_src} NAME_WE)
  add_executable(example_${example_name} ${example_src})
  target_link_libraries(example_${example_name} PUBLIC bound::bound)
endforeach()

