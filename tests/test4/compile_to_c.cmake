# Compile a Joy program to C, build and run it, and require the same output
# as the interpreter.
# Usage: cmake -DJOY=<exe> -DCC=<compiler> -DLIB=<libjoycore_static>
#              -DINC=<dir;dir;...> -DWORK=<dir> -P compile_to_c.cmake

# Covers nested quotations, '=' (eql_), a hyphenated builtin (regex_match_),
# a user definition, inlined builtins, chars, floats, integer wraparound,
# div (two results) and folded integer comparison above 2^53.
set(defs "DEFINE sq == dup *.")
set(term [=[3 3 = put 4 sq put [1 [2 [3]] 'a] put "abc" "b" regex-match put
[[1 2] [3]] [size] map put 1 2 swap - put 'A put 0.1 put [1 2] [3 4] = put
[[1 [2]] [3]] [first] map put 7 2 div put put maxint 1 + put maxint succ put
maxint 1 + -1 / put maxint 1 + neg put 9007199254740993 9007199254740992 = put]=])

file(MAKE_DIRECTORY ${WORK})
file(WRITE ${WORK}/interp.joy "${defs}\n${term}.\n")
file(WRITE ${WORK}/compile.joy "${defs}\n[${term}] \"${WORK}/prog.c\" compile-to-file.\n")

execute_process(COMMAND ${JOY} ${WORK}/interp.joy
                OUTPUT_VARIABLE expected RESULT_VARIABLE rv)
if(NOT rv EQUAL 0)
  message(FATAL_ERROR "interpreter failed: ${rv}")
endif()

execute_process(COMMAND ${JOY} ${WORK}/compile.joy RESULT_VARIABLE rv)
if(NOT rv EQUAL 0)
  message(FATAL_ERROR "compile-to-file failed: ${rv}")
endif()

set(incs)
foreach(dir ${INC})
  list(APPEND incs -I${dir})
endforeach()
execute_process(COMMAND ${CC} -DNOBDW ${incs} ${WORK}/prog.c ${LIB} -lm -o ${WORK}/prog
                RESULT_VARIABLE rv ERROR_VARIABLE err)
if(NOT rv EQUAL 0)
  message(FATAL_ERROR "C compile failed:\n${err}")
endif()

execute_process(COMMAND ${WORK}/prog OUTPUT_VARIABLE actual RESULT_VARIABLE rv)
if(NOT rv EQUAL 0)
  message(FATAL_ERROR "compiled program failed: ${rv}")
endif()
if(NOT actual STREQUAL expected)
  message(FATAL_ERROR "output differs\nexpected: ${expected}\nactual:   ${actual}")
endif()
