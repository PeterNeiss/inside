#---------------------------------------------------------------------------
# Compile-fail diagnostics suite — each *.cpp here is deliberately ill-formed
# and must FAIL to compile with the message named on its first line
# (`// EXPECT: <substring>`; keep it free of regex metacharacters). The test
# invokes the build for the object target; PASS_REGULAR_EXPRESSION passes the
# test only when the build output contains the expected diagnostic — a TU that
# wrongly compiles produces no diagnostic and fails the test. RUN_SERIAL keeps
# concurrent `cmake --build` invocations out of each other's way.
# control_compiles.cpp is the harness control: valid code that must build.
#---------------------------------------------------------------------------
file(GLOB BOUND_FAIL_SOURCES CONFIGURE_DEPENDS *.cpp)

foreach(src ${BOUND_FAIL_SOURCES})
  get_filename_component(case_name ${src} NAME_WE)
  if(case_name STREQUAL "control_compiles")
    continue()
  endif()

  file(STRINGS ${src} first_line LIMIT_COUNT 1)
  string(REGEX REPLACE "^// EXPECT: *" "" expected_message "${first_line}")
  if(NOT first_line MATCHES "^// EXPECT: ")
    message(FATAL_ERROR "compile-fail case ${src} lacks a '// EXPECT: <message>' first line")
  endif()

  add_library(fail_${case_name} OBJECT EXCLUDE_FROM_ALL ${src})
  target_link_libraries(fail_${case_name} PRIVATE bound::bound)

  add_test(NAME fail_${case_name}
           COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
                   --target fail_${case_name} --config $<CONFIG>)
  set_tests_properties(fail_${case_name} PROPERTIES
    PASS_REGULAR_EXPRESSION "${expected_message}"
    RUN_SERIAL TRUE
    LABELS "compile-fail")
endforeach()

add_library(fail_control OBJECT EXCLUDE_FROM_ALL control_compiles.cpp)
target_link_libraries(fail_control PRIVATE bound::bound)
add_test(NAME fail_control_compiles
         COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
                 --target fail_control --config $<CONFIG>)
set_tests_properties(fail_control_compiles PROPERTIES
  RUN_SERIAL TRUE
  LABELS "compile-fail")
