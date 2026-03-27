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
            -Wformat=2
        >
        $<$<CXX_COMPILER_ID:MSVC>:
            /W4 /WX /permissive-
        >
)

add_executable(test tests/test.cpp)
target_link_libraries(test PUBLIC bound::bound)

try_compile(
    BOUND_HAS_STACKTRACE
    SOURCE_FROM_CONTENT stacktrace_check.cpp
    "#include <stacktrace>\nint main() { return std::stacktrace::current().size(); }\n"
    CMAKE_FLAGS "-DCMAKE_CXX_STANDARD=23"
    LINK_LIBRARIES stdc++_libbacktrace
)

if(BOUND_HAS_STACKTRACE)
    target_compile_definitions(bound_bound INTERFACE 
        $<$<NOT:$<CONFIG:Release>>:BOUND_HAS_STACKTRACE>
    )
    target_link_libraries(bound_bound INTERFACE 
        $<$<NOT:$<CONFIG:Release>>:stdc++_libbacktrace>
    )
endif()

