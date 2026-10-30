# Run joy and write its stdout to OUT, ignoring the exit status.
# For output targets whose scripts raise runtime errors on purpose.
# Usage: cmake -DJOY=<exe> -DOUT=<file> [-DARGS=a|b|c] [-DIN=<stdin file>] -P run_joy_output.cmake
string(REPLACE "|" ";" args "${ARGS}")
if(IN)
  execute_process(COMMAND ${JOY} ${args} INPUT_FILE ${IN} OUTPUT_FILE ${OUT})
else()
  execute_process(COMMAND ${JOY} ${args} OUTPUT_FILE ${OUT})
endif()
