# CMakeLists.txt for Joy embedding examples
#
# These examples demonstrate how to embed the Joy interpreter
# into C programs.
#
# Build from the main project:
#   cmake -B build -DBUILD_EXAMPLES=ON
#   cmake --build build
#
# Or build standalone (after installing libjoy):
#   cmake -B build
#   cmake --build build

cmake_minimum_required(VERSION 3.10)

# When building standalone
if(NOT TARGET joycore_static)
    project(joy_examples C)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GC REQUIRED bdw-gc)
    find_library(JOY_LIBRARY NAMES joycore_static joycore)
    find_path(JOY_INCLUDE_DIR joy/joy.h)
    if(NOT JOY_LIBRARY OR NOT JOY_INCLUDE_DIR)
        message(FATAL_ERROR "Joy library not found. Build and install the main project first.")
    endif()
endif()

# Simple embedding example
add_executable(embed_simple embed_simple.c)
if(TARGET joycore_static)
    target_link_libraries(embed_simple PRIVATE joycore_static)
    target_include_directories(embed_simple PRIVATE ${CMAKE_SOURCE_DIR}/include)
else()
    target_link_libraries(embed_simple PRIVATE ${JOY_LIBRARY} ${GC_LIBRARIES})
    target_include_directories(embed_simple PRIVATE ${JOY_INCLUDE_DIR} ${GC_INCLUDE_DIRS})
endif()

# Custom I/O example
add_executable(embed_custom_io embed_custom_io.c)
if(TARGET joycore_static)
    target_link_libraries(embed_custom_io PRIVATE joycore_static)
    target_include_directories(embed_custom_io PRIVATE ${CMAKE_SOURCE_DIR}/include)
else()
    target_link_libraries(embed_custom_io PRIVATE ${JOY_LIBRARY} ${GC_LIBRARIES})
    target_include_directories(embed_custom_io PRIVATE ${JOY_INCLUDE_DIR} ${GC_INCLUDE_DIRS})
endif()

# Set output directory
set_target_properties(embed_simple embed_custom_io
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples)
