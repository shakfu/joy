#
#   module  : CMakeLists.txt
#   version : 1.37
#   date    : 02/18/25
#
cmake_minimum_required(VERSION 3.29)
project(Joy VERSION 1.0 LANGUAGES C)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(FATAL_ERROR "In-source builds are not supported. Use cmake -S . -B build")
endif()

if("${CMAKE_BUILD_TYPE}" STREQUAL "")
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
endif()

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
  option(RUN_TESTS "Run standard tests" ON)
else()
  option(RUN_TESTS "Run standard tests" OFF)
endif()

set(JOY_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${JOY_GENERATED_DIR}")

file(GLOB JOY_PRIMITIVE_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/builtin/*.c" "${CMAKE_SOURCE_DIR}/src/builtin/*.h")

add_custom_command(
  OUTPUT "${JOY_GENERATED_DIR}/builtin.c" "${JOY_GENERATED_DIR}/builtin.h"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${JOY_GENERATED_DIR}"
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tools/gen_builtins.py"
          "${CMAKE_SOURCE_DIR}" "${JOY_GENERATED_DIR}"
  DEPENDS ${JOY_PRIMITIVE_SOURCES} "${CMAKE_SOURCE_DIR}/tools/gen_builtins.py"
  VERBATIM
)

add_custom_command(
  OUTPUT "${JOY_GENERATED_DIR}/table.c"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${JOY_GENERATED_DIR}"
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tools/gen_table.py"
          "${CMAKE_SOURCE_DIR}" "${JOY_GENERATED_DIR}/table.c"
  DEPENDS ${JOY_PRIMITIVE_SOURCES} "${CMAKE_SOURCE_DIR}/tools/gen_table.py"
  VERBATIM
)

add_custom_target(joy_generated
  DEPENDS
    "${JOY_GENERATED_DIR}/builtin.c"
    "${JOY_GENERATED_DIR}/builtin.h"
    "${JOY_GENERATED_DIR}/table.c")

set(JOY_CORE_SOURCES
  src/error.c
  src/factor.c
  src/gc.c
  src/interp.c
  src/iolib.c
  src/joy.c
  src/module.c
  src/optable.c
  src/print.c
  src/repl.c
  src/scan.c
  src/setraw.c
  src/symbol.c
  src/undefs.c
  src/utils.c
  src/write.c
)

add_library(joycore_static STATIC ${JOY_CORE_SOURCES})
add_library(joycore_shared SHARED ${JOY_CORE_SOURCES})

add_dependencies(joycore_static joy_generated)
add_dependencies(joycore_shared joy_generated)

set(JOY_PUBLIC_INCLUDES "${CMAKE_SOURCE_DIR}/include")

foreach(target joycore_static joycore_shared)
  target_include_directories(${target}
    PUBLIC "${JOY_PUBLIC_INCLUDES}"
    PRIVATE "${JOY_GENERATED_DIR}" "${CMAKE_SOURCE_DIR}")
  target_compile_features(${target} PUBLIC c_std_11)
endforeach()

if(MSVC)
  set(JOY_RELEASE_COMPILER_FLAGS "/DNOBDW /W4 /WX")
  set(JOY_DEBUG_COMPILER_FLAGS "${JOY_RELEASE_COMPILER_FLAGS}")
  foreach(target joycore_static joycore_shared)
    target_compile_definitions(${target}
      PUBLIC NOBDW _CRT_SECURE_NO_DEPRECATE)
    target_compile_definitions(${target}
      PUBLIC
        VERS="NOBDW $<CONFIG> ${PROJECT_VERSION}"
        LINK="${CMAKE_EXE_LINKER_FLAGS}"
        COMP="$<IF:$<CONFIG:Debug>,${JOY_DEBUG_COMPILER_FLAGS},${JOY_RELEASE_COMPILER_FLAGS}>")
  endforeach()
else()
  set(JOY_RELEASE_COMPILER_FLAGS "-DNOBDW -O3 -Wall -Wextra -Wpedantic -Werror -Wno-unused-parameter -Wno-unused-result")
  set(JOY_DEBUG_COMPILER_FLAGS "${JOY_RELEASE_COMPILER_FLAGS} --coverage -fprofile-arcs -ftest-coverage")
  foreach(target joycore_static joycore_shared)
    target_compile_options(${target}
      PRIVATE -Wall -Wextra -Wpedantic -Werror -Wno-unused-parameter -Wno-unused-result)
    target_compile_definitions(${target}
      PUBLIC NOBDW
      PUBLIC
        VERS="NOBDW $<CONFIG> ${PROJECT_VERSION}"
        LINK="${CMAKE_EXE_LINKER_FLAGS}"
        COMP="$<IF:$<CONFIG:Debug>,${JOY_DEBUG_COMPILER_FLAGS},${JOY_RELEASE_COMPILER_FLAGS}>")
  endforeach()
  foreach(target joycore_static joycore_shared)
    target_link_libraries(${target} PUBLIC m)
  endforeach()
endif()

# Linenoise library for REPL line editing
add_library(linenoise STATIC ext/linenoise.c)
target_include_directories(linenoise PUBLIC ext)
if(NOT MSVC)
  target_compile_options(linenoise PRIVATE -Wno-unused-result)
endif()

add_executable(joy src/cli/main.c)
target_link_libraries(joy PRIVATE joycore_static linenoise)
target_include_directories(joy PRIVATE "${JOY_PUBLIC_INCLUDES}" "${JOY_GENERATED_DIR}" "${CMAKE_SOURCE_DIR}" ext)
target_compile_definitions(joy PRIVATE USE_LINENOISE)
add_dependencies(joy joy_generated)

if(RUN_TESTS)
  enable_testing()
  add_subdirectory(lib)
  add_subdirectory(tests/test2)
  add_subdirectory(tests/test4)
endif()

# Optional: Build embedding examples
option(BUILD_EXAMPLES "Build embedding examples" OFF)
if(BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()
