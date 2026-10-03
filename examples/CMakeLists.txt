# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# Each example is also registered as a ctest test under the "example" label, so
# `ctest -L example` runs them all (through CMAKE_CROSSCOMPILING_EMULATOR on
# cross builds).
file(GLOB ALL_EXAMPLES CONFIGURE_DEPENDS *.cpp)

foreach(example_src ${ALL_EXAMPLES})
    get_filename_component(example ${example_src} NAME_WE)
    set(example_target beman.inside.examples.${example})
    add_executable(${example_target})
    target_sources(${example_target} PRIVATE ${example_src})
    target_link_libraries(
        ${example_target}
        PRIVATE beman::inside beman.inside.warnings
    )
    add_test(NAME ${example_target} COMMAND ${example_target})
    set_tests_properties(${example_target} PROPERTIES LABELS "example")
endforeach()
