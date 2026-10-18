# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# The modules beman.inside and beman.inside.units (experimental): wrappers
# that include the headers in their purview and export the public names.

if(CMAKE_VERSION VERSION_LESS 3.28)
    message(FATAL_ERROR "BEMAN_INSIDE_BUILD_MODULES needs CMake 3.28 or later")
endif()
if(NOT CMAKE_GENERATOR MATCHES "Ninja|Visual Studio")
    message(
        FATAL_ERROR
        "BEMAN_INSIDE_BUILD_MODULES needs a generator with C++ module support (Ninja or Visual Studio), not ${CMAKE_GENERATOR}"
    )
endif()

add_library(beman.inside.module STATIC)
add_library(beman::inside_module ALIAS beman.inside.module)
target_sources(
    beman.inside.module
    PUBLIC
        FILE_SET CXX_MODULES
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
            FILES beman.inside.cppm beman.inside.units.cppm
)
target_include_directories(
    beman.inside.module
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(beman.inside.module PUBLIC beman::inside)
target_compile_features(beman.inside.module PUBLIC cxx_std_23)

# The examples against the module: shim headers named like the library's
# (modules/shim), each only an import, come first on the include path, so an
# example compiles unchanged. A name the module fails to export fails its
# build; each example checks its own output when run.
if(BEMAN_INSIDE_BUILD_EXAMPLES)
    file(GLOB examples CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/examples/*.cpp)
    foreach(example_src ${examples})
        get_filename_component(example ${example_src} NAME_WE)
        set(target beman.inside.module_examples.${example})
        add_executable(${target} ${example_src})
        target_include_directories(
            ${target}
            BEFORE
            PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/shim
        )
        target_link_libraries(${target} PRIVATE beman.inside.module)
        add_test(NAME ${target} COMMAND ${target})
        set_tests_properties(${target} PROPERTIES LABELS "module")
    endforeach()
endif()
