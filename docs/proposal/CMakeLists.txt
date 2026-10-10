# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# The proposal's listings are regions of these programs: each is built and run
# as a ctest, and listings_match checks that inside.md quotes them verbatim.
file(GLOB PROPOSAL_EXAMPLES CONFIGURE_DEPENDS examples/*.cpp)

foreach(example_src ${PROPOSAL_EXAMPLES})
    get_filename_component(example ${example_src} NAME_WE)
    set(example_target beman.inside.proposal.${example})
    add_executable(${example_target} ${example_src})
    target_link_libraries(
        ${example_target}
        PRIVATE beman::inside beman.inside.warnings
    )
    add_test(NAME ${example_target} COMMAND ${example_target})
    set_tests_properties(${example_target} PROPERTIES LABELS "proposal")
endforeach()

find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
    add_test(
        NAME beman.inside.proposal.listings_match
        COMMAND
            Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/listings.py check
    )
    set_tests_properties(
        beman.inside.proposal.listings_match
        PROPERTIES LABELS "proposal"
    )
endif()
