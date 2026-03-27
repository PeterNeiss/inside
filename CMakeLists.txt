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

include(CheckCXXSourceCompiles)

set(CMAKE_REQUIRED_FLAGS "-std=c++23")
#set(CMAKE_REQUIRED_LIBRARIES stdc++exp)
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
        $<$<NOT:$<CONFIG:Release>>:stdc++_libbacktrace>
    )
endif()

