#   module  : CMakeLists.txt
#   version : 2.1
#   date    : 01/20/26
#
set(JOY_EXECUTABLE $<TARGET_FILE:joy>)
set(CRASH_TEST_RUNNER "${CMAKE_SOURCE_DIR}/tools/run_crash_test.sh")
set(TEST4_SOURCE_DIR "${CMAKE_SOURCE_DIR}/tests/test4")
set(TEST4_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}")

# Macros for output generation (existing functionality)
macro(exe9 src)
add_custom_target(${src}.out ALL
          WORKING_DIRECTORY ${TEST4_SOURCE_DIR}
          DEPENDS joy ${TEST4_SOURCE_DIR}/${src}.joy
          COMMAND ${JOY_EXECUTABLE} ${TEST4_SOURCE_DIR}/${src}.joy >${TEST4_OUTPUT_DIR}/${src}.out)
endmacro(exe9)

macro(exe8 src)
add_custom_target(${src}.out ALL
          WORKING_DIRECTORY ${TEST4_SOURCE_DIR}
          DEPENDS joy ${TEST4_SOURCE_DIR}/${src}.joy
          COMMAND ${JOY_EXECUTABLE} -x ${TEST4_SOURCE_DIR}/${src}.joy
                  >${TEST4_OUTPUT_DIR}/${src}.out)
endmacro(exe8)

macro(exe7 src)
add_custom_target(${src}.out ALL
          WORKING_DIRECTORY ${TEST4_SOURCE_DIR}
          DEPENDS joy ${TEST4_SOURCE_DIR}/${src}.joy
          COMMAND ${JOY_EXECUTABLE} -t ${TEST4_SOURCE_DIR}/${src}.joy
                  >${TEST4_OUTPUT_DIR}/${src}.out)
endmacro(exe7)

macro(exe6 src)
add_custom_target(${src}.out ALL
          WORKING_DIRECTORY ${TEST4_SOURCE_DIR}
          DEPENDS joy ${TEST4_SOURCE_DIR}/${src}.joy
          COMMAND ${JOY_EXECUTABLE} -kh ${TEST4_SOURCE_DIR}/${src}.joy)
endmacro(exe6)

macro(exe5 src)
add_custom_target(${src}.out ALL
          WORKING_DIRECTORY ${TEST4_SOURCE_DIR}
          DEPENDS joy ${TEST4_SOURCE_DIR}/${src}.joy
          COMMAND ${JOY_EXECUTABLE} -y ${TEST4_SOURCE_DIR}/${src}.joy
                  >${TEST4_OUTPUT_DIR}/${src}.out)
endmacro(exe5)

macro(exe4 src)
add_custom_target(${src}.out ALL
          WORKING_DIRECTORY ${TEST4_SOURCE_DIR}
          DEPENDS joy ${TEST4_SOURCE_DIR}/${src}.joy
          COMMAND ${JOY_EXECUTABLE} -adegiluvw ${TEST4_SOURCE_DIR}/${src}.joy
                  >${TEST4_OUTPUT_DIR}/${src}.out)
endmacro(exe4)

macro(exe3 src)
add_custom_target(${src}.out ALL
          WORKING_DIRECTORY ${TEST4_SOURCE_DIR}
          DEPENDS joy ${TEST4_SOURCE_DIR}/${src}.joy
          COMMAND ${JOY_EXECUTABLE} dummy ${TEST4_SOURCE_DIR}/${src}.joy
                  >${TEST4_OUTPUT_DIR}/${src}.out)
endmacro(exe3)

macro(exe2 src)
add_custom_target(${src}.out ALL
          WORKING_DIRECTORY ${TEST4_SOURCE_DIR}
          DEPENDS joy ${TEST4_SOURCE_DIR}/${src}.joy
          COMMAND ${JOY_EXECUTABLE} <${TEST4_SOURCE_DIR}/${src}.joy >${TEST4_OUTPUT_DIR}/${src}.out)
endmacro(exe2)

macro(exe1 src)
add_custom_target(${src}.out ALL
          WORKING_DIRECTORY ${TEST4_SOURCE_DIR}
          DEPENDS joy ${TEST4_SOURCE_DIR}/${src}.joy
          COMMAND ${JOY_EXECUTABLE} -s ${TEST4_SOURCE_DIR}/${src}.joy
                  >${TEST4_OUTPUT_DIR}/${src}.out)
endmacro(exe1)

macro(exe0 src)
add_custom_target(${src}.out ALL
          WORKING_DIRECTORY ${TEST4_SOURCE_DIR}
          DEPENDS joy ${TEST4_SOURCE_DIR}/${src}.joy
          COMMAND ${JOY_EXECUTABLE} -p ${TEST4_SOURCE_DIR}/${src}.joy
                  >${TEST4_OUTPUT_DIR}/${src}.out)
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
