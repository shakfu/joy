#   module  : CMakeLists.txt
#   version : 2.1
#   date    : 01/20/26
#
set(JOY_EXECUTABLE $<TARGET_FILE:joy>)
set(CRASH_TEST_RUNNER "${CMAKE_SOURCE_DIR}/tools/run_crash_test.sh")
set(TEST4_SOURCE_DIR "${CMAKE_SOURCE_DIR}/tests/test4")
set(TEST4_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}")

# Macros for output generation (existing functionality).
# These scripts raise errors on purpose, so the exit status is ignored.
set(JOY_OUTPUT_RUNNER "${CMAKE_SOURCE_DIR}/tools/run_joy_output.cmake")

macro(joy_output src args)
add_custom_target(${src}.out ALL
          WORKING_DIRECTORY ${TEST4_SOURCE_DIR}
          DEPENDS joy ${TEST4_SOURCE_DIR}/${src}.joy
          COMMAND ${CMAKE_COMMAND} -DJOY=${JOY_EXECUTABLE}
                  -DOUT=${TEST4_OUTPUT_DIR}/${src}.out
                  "-DARGS=${args}" ${ARGN} -P ${JOY_OUTPUT_RUNNER}
          VERBATIM)
endmacro(joy_output)

macro(exe9 src)
joy_output(${src} "${TEST4_SOURCE_DIR}/${src}.joy")
endmacro(exe9)

macro(exe8 src)
joy_output(${src} "-x|${TEST4_SOURCE_DIR}/${src}.joy")
endmacro(exe8)

macro(exe7 src)
joy_output(${src} "-t|${TEST4_SOURCE_DIR}/${src}.joy")
endmacro(exe7)

macro(exe6 src)
joy_output(${src} "-kh|${TEST4_SOURCE_DIR}/${src}.joy")
endmacro(exe6)

macro(exe5 src)
joy_output(${src} "-y|${TEST4_SOURCE_DIR}/${src}.joy")
endmacro(exe5)

macro(exe4 src)
joy_output(${src} "-adegiluvw|${TEST4_SOURCE_DIR}/${src}.joy")
endmacro(exe4)

macro(exe3 src)
joy_output(${src} "dummy|${TEST4_SOURCE_DIR}/${src}.joy")
endmacro(exe3)

macro(exe2 src)
joy_output(${src} "" -DIN=${TEST4_SOURCE_DIR}/${src}.joy)
endmacro(exe2)

macro(exe1 src)
joy_output(${src} "-s|${TEST4_SOURCE_DIR}/${src}.joy")
endmacro(exe1)

macro(exe0 src)
joy_output(${src} "-p|${TEST4_SOURCE_DIR}/${src}.joy")
endmacro(exe0)

# Macros for CTest crash tests (verify no segfaults)
macro(joy_crash_test src)
add_test(NAME test4_${src}
         COMMAND ${CRASH_TEST_RUNNER} ${JOY_EXECUTABLE} ${TEST4_SOURCE_DIR}/${src}.joy
         WORKING_DIRECTORY ${TEST4_SOURCE_DIR})
endmacro(joy_crash_test)

macro(joy_crash_test_flags src)
add_test(NAME test4_${src}
         COMMAND ${CRASH_TEST_RUNNER} ${JOY_EXECUTABLE} ${ARGN} ${TEST4_SOURCE_DIR}/${src}.joy
         WORKING_DIRECTORY ${TEST4_SOURCE_DIR})
endmacro(joy_crash_test_flags)

# Generate output files (existing functionality)
exe9(factor)
exe8(gcno)
exe7(interp)
exe6(main1)
exe5(main2)
exe4(main3)
exe3(main4)
exe2(main5)
exe1(main6)
exe9(module1)
exe9(module2)
exe0(scan1)
exe9(scan2)
exe9(symbol)
exe9(forget)
exe9(utils1)
exe9(utils2)
exe9(errors)

# Compiled C tests for library API
add_executable(test_api api.c)
target_link_libraries(test_api PRIVATE joycore_static)
target_include_directories(test_api PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}
    ${JOY_GENERATED_DIR})
add_dependencies(test_api joy_generated)
add_test(NAME test4_api COMMAND test_api)

# CTest crash tests - verify interpreter handles errors without crashing
joy_crash_test(factor)
joy_crash_test_flags(gcno -x)
joy_crash_test_flags(interp -t)
joy_crash_test_flags(main2 -y)
joy_crash_test_flags(main3 -adegiluvw)
joy_crash_test_flags(main6 -s)
joy_crash_test(module1)
joy_crash_test(module2)
joy_crash_test_flags(scan1 -p)
joy_crash_test(symbol)
joy_crash_test(forget)
joy_crash_test(utils1)
joy_crash_test(utils2)
joy_crash_test(errors)

# Parallel combinator tests. Without JOY_PARALLEL they check the sequential
# fallback; run_test.sh makes a false assertion fail the test.
foreach(t parallel_stress parallel_test)
  add_test(NAME ${t}
           COMMAND ${CMAKE_SOURCE_DIR}/tools/run_test.sh ${JOY_EXECUTABLE}
                   ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/tests/${t}.joy)
endforeach()

# Session persistence scripts (need a JOY_SESSION build)
if(JOY_SESSION AND SQLite3_FOUND)
  foreach(t persist snapshot errors)
    add_test(NAME session_${t}
             COMMAND ${CMAKE_SOURCE_DIR}/tests/test_session_${t}.sh)
    set_tests_properties(session_${t} PROPERTIES ENVIRONMENT "JOY=${JOY_EXECUTABLE}")
  endforeach()
endif()

# CLI exit status: nonzero after a runtime error
add_test(NAME cli_exit_on_error
         COMMAND ${CMAKE_COMMAND} -DJOY=${JOY_EXECUTABLE}
                 -DSRC=${TEST4_SOURCE_DIR}/exitcode.joy -DEXPECT=1
                 -P ${TEST4_SOURCE_DIR}/expect_exit.cmake)

# compile-to-c: generated C must build and match the interpreter's output.
# Builds with extra link dependencies (sessions, BLAS, OpenMP) are skipped.
if(NOT MSVC AND NOT JOY_SESSION AND NOT JOY_BLAS AND NOT JOY_PARALLEL)
  add_test(NAME compile_to_c
           COMMAND ${CMAKE_COMMAND} -DJOY=${JOY_EXECUTABLE}
                   -DCC=${CMAKE_C_COMPILER}
                   -DLIB=$<TARGET_FILE:joycore_static>
                   "-DINC=${CMAKE_SOURCE_DIR}/include;${JOY_GENERATED_DIR};${CMAKE_SOURCE_DIR}"
                   -DWORK=${CMAKE_CURRENT_BINARY_DIR}/compile_to_c
                   -P ${TEST4_SOURCE_DIR}/compile_to_c.cmake)
endif()

# Bindings restored when an error unwinds through let/match/cases
add_test(NAME error_restore
         COMMAND ${CMAKE_SOURCE_DIR}/tools/run_test.sh ${JOY_EXECUTABLE}
                 ${CMAKE_SOURCE_DIR} ${TEST4_SOURCE_DIR}/error_restore.joy)
set_tests_properties(error_restore PROPERTIES ENVIRONMENT "ALLOW_ERRORS=1")

# Oversized vector/matrix dimensions raise errors (native types only)
if(JOY_NATIVE_TYPES)
  add_test(NAME size_errors
           COMMAND ${CMAKE_SOURCE_DIR}/tools/run_test.sh ${JOY_EXECUTABLE}
                   ${CMAKE_SOURCE_DIR} ${TEST4_SOURCE_DIR}/size_errors.joy)
  set_tests_properties(size_errors PROPERTIES ENVIRONMENT "ALLOW_ERRORS=1")
endif()

# LSP protocol tests (tests/lsp/test_lsp.py)
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND AND NOT WIN32)
  # Interactive REPL through a pseudo-terminal (POSIX only)
  add_test(NAME repl
           COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tests/repl/test_repl.py
                   ${JOY_EXECUTABLE})
endif()
if(Python3_Interpreter_FOUND)
  add_test(NAME lsp
           COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tests/lsp/test_lsp.py
                   $<TARGET_FILE:joy-lsp>)
endif()
