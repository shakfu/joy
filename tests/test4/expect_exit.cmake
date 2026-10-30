# Run joy on SRC and require exit status EXPECT.
# Usage: cmake -DJOY=<exe> -DSRC=<file> -DEXPECT=<n> -P expect_exit.cmake
execute_process(COMMAND ${JOY} ${SRC} RESULT_VARIABLE rv OUTPUT_QUIET ERROR_QUIET)
if(NOT rv STREQUAL EXPECT)
  message(FATAL_ERROR "joy ${SRC}: exit status ${rv}, expected ${EXPECT}")
endif()
