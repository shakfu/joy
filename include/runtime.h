/*
 *  module  : runtime.h
 *  version : 1.4
 *  date    : 01/22/26
 *
 *  Runtime validation macros for Joy builtins.
 *
 *  IMPORTANT: All macros require 'pEnv env' to be in scope.
 *
 *  These macros provide runtime parameter checking for builtins. When NCHECK
 *  is defined, all checks become no-ops for maximum performance (use only
 *  after thorough testing).
 *
 *  Macro Categories:
 *
 *  1. Stack Depth Checks:
 *     ONEPARAM, TWOPARAMS, THREEPARAMS, FOURPARAMS, FIVEPARAMS
 *     - Verify minimum number of items on stack
 *     - Call execerror() and return on failure
 *
 *  2. Type Checks (single parameter):
 *     ONEQUOTE, STRING, INTEGER, CHARACTER, FLOAT, LIST, USERDEF, ISFILE
 *     - Verify top of stack is expected type
 *
 *  3. Type Checks (multiple parameters):
 *     TWOQUOTES, THREEQUOTES, FOURQUOTES - Multiple quotations
 *     STRING2, INTEGER2, NUMERIC2, LIST2 - Second parameter type
 *     INTEGERS2, FLOAT2, SAME2TYPES - Both parameters
 *
 *  4. Value Checks:
 *     CHECKZERO, CHECKDIVISOR - Non-zero values
 *     CHECKEMPTYSET, CHECKEMPTYSTRING, CHECKEMPTYLIST - Non-empty
 *     POSITIVEINDEX - Non-negative integer
 *     CHECKSETMEMBER - Valid set element (0-63)
 *
 *  5. Float Conversion Helpers:
 *     FLOATABLE, FLOATABLE2, FLOATVAL, FLOATVAL2
 *     FLOAT_U, FLOAT_P, FLOAT_I - Apply float operations
 *
 *  Usage Pattern:
 *     void plus_(pEnv env) {
 *         TWOPARAMS("+");       // Need 2 params
 *         INTEGERS2("+");       // Both must be integers
 *         BINARY(INTEGER_NEWNODE,
 *                nodevalue(nextnode1(env->stck)).num +
 *                nodevalue(env->stck).num);
 *     }
 */

/*
 * Float conversion helpers - check if values can be treated as floats
 * and extract float values from INTEGER_ or FLOAT_ nodes.
 */
#define FLOATABLE                                                             \
    (nodetype(env->stck) == INTEGER_ || nodetype(env->stck) == FLOAT_)
#define FLOATABLE2                                                            \
    ((nodetype(env->stck) == FLOAT_                                           \
      && nodetype(nextnode1(env->stck)) == FLOAT_)                            \
     || (nodetype(env->stck) == FLOAT_                                        \
         && nodetype(nextnode1(env->stck)) == INTEGER_)                       \
     || (nodetype(env->stck) == INTEGER_                                      \
         && nodetype(nextnode1(env->stck)) == FLOAT_))
#define FLOATVAL                                                              \
    (nodetype(env->stck) == FLOAT_ ? nodevalue(env->stck).dbl                 \
                                   : (double)nodevalue(env->stck).num)
#define FLOATVAL2                                                             \
    (nodetype(nextnode1(env->stck)) == FLOAT_                                 \
         ? nodevalue(nextnode1(env->stck)).dbl                                \
         : (double)nodevalue(nextnode1(env->stck)).num)
#define FLOAT_U(OPER)                                                         \
    if (FLOATABLE) {                                                          \
        UNARY(FLOAT_NEWNODE, OPER(FLOATVAL));                                 \
        return;                                                               \
    }
#define FLOAT_P(OPER)                                                         \
    if (FLOATABLE2) {                                                         \
        BINARY(FLOAT_NEWNODE, OPER(FLOATVAL2, FLOATVAL));                     \
        return;                                                               \
    }
#define FLOAT_I(OPER)                                                         \
    if (FLOATABLE2) {                                                         \
        BINARY(FLOAT_NEWNODE, (FLOATVAL2)OPER(FLOATVAL));                     \
        return;                                                               \
    }

/*
 * Parameter validation macros (active when NCHECK is not defined)
 *
 * These macros check preconditions and call execerror() + return on failure.
 * The NAME parameter is used in error messages to identify the failing op.
 *
 * All macros have these properties:
 * @requires: env in scope
 * @param NAME: String name of operation (for error messages)
 * @modifies: Nothing on success; calls execerror() and returns on failure
 */
#ifndef NCHECK

/* Stack depth checks - verify minimum items on stack */
#define ONEPARAM(NAME)                                                        \
    if (!env->stck) {                                                         \
        execerror(env, "one parameter", NAME);                                \
        return;                                                               \
    }
#define TWOPARAMS(NAME)                                                       \
    if (!env->stck || !nextnode1(env->stck)) {                                \
        execerror(env, "two parameters", NAME);                               \
        return;                                                               \
    }
#define THREEPARAMS(NAME)                                                     \
    if (!env->stck || !nextnode1(env->stck) || !nextnode2(env->stck)) {       \
        execerror(env, "three parameters", NAME);                             \
        return;                                                               \
    }
#define FOURPARAMS(NAME)                                                      \
    if (!env->stck || !nextnode1(env->stck) || !nextnode2(env->stck)          \
        || !nextnode3(env->stck)) {                                           \
        execerror(env, "four parameters", NAME);                              \
        return;                                                               \
    }
#define FIVEPARAMS(NAME)                                                      \
    if (!env->stck || !nextnode1(env->stck) || !nextnode2(env->stck)          \
        || !nextnode3(env->stck) || !nextnode4(env->stck)) {                  \
        execerror(env, "five parameters", NAME);                              \
        return;                                                               \
    }

/* Type checks - verify stack items are expected types */
#define ONEQUOTE(NAME)                                                        \
    if (nodetype(env->stck) != LIST_) {                                       \
        execerror(env, "quotation as top parameter", NAME);                   \
        return;                                                               \
    }
#define TWOQUOTES(NAME)                                                       \
    ONEQUOTE(NAME);                                                           \
    if (nodetype(nextnode1(env->stck)) != LIST_) {                            \
        execerror(env, "quotation as second parameter", NAME);                \
        return;                                                               \
    }
#define THREEQUOTES(NAME)                                                     \
    TWOQUOTES(NAME);                                                          \
    if (nodetype(nextnode2(env->stck)) != LIST_) {                            \
        execerror(env, "quotation as third parameter", NAME);                 \
        return;                                                               \
    }
#define FOURQUOTES(NAME)                                                      \
    THREEQUOTES(NAME);                                                        \
    if (nodetype(nextnode3(env->stck)) != LIST_) {                            \
        execerror(env, "quotation as fourth parameter", NAME);                \
        return;                                                               \
    }
#define SAME2TYPES(NAME)                                                      \
    if (nodetype(env->stck) != nodetype(nextnode1(env->stck))) {              \
        execerror(env, "two parameters of the same type", NAME);              \
        return;                                                               \
    }
#define STRING(NAME)                                                          \
    if (nodetype(env->stck) != STRING_) {                                     \
        execerror(env, "string", NAME);                                       \
        return;                                                               \
    }
#define STRING2(NAME)                                                         \
    if (nodetype(nextnode1(env->stck)) != STRING_) {                          \
        execerror(env, "string as second parameter", NAME);                   \
        return;                                                               \
    }
#define INTEGER(NAME)                                                         \
    if (nodetype(env->stck) != INTEGER_) {                                    \
        execerror(env, "integer", NAME);                                      \
        return;                                                               \
    }
#define INTEGER2(NAME)                                                        \
    if (nodetype(nextnode1(env->stck)) != INTEGER_) {                         \
        execerror(env, "integer as second parameter", NAME);                  \
        return;                                                               \
    }
#define CHARACTER(NAME)                                                       \
    if (nodetype(env->stck) != CHAR_) {                                       \
        execerror(env, "character", NAME);                                    \
        return;                                                               \
    }
#define INTEGERS2(NAME)                                                       \
    if (nodetype(env->stck) != INTEGER_                                       \
        || nodetype(nextnode1(env->stck)) != INTEGER_) {                      \
        execerror(env, "two integers", NAME);                                 \
        return;                                                               \
    }
#define NUMERICTYPE(NAME)                                                     \
    if (nodetype(env->stck) != INTEGER_ && nodetype(env->stck) != CHAR_       \
        && nodetype(env->stck) != BOOLEAN_) {                                 \
        execerror(env, "numeric", NAME);                                      \
        return;                                                               \
    }
#define NUMERIC2(NAME)                                                        \
    if (nodetype(nextnode1(env->stck)) != INTEGER_                            \
        && nodetype(nextnode1(env->stck)) != CHAR_) {                         \
        execerror(env, "numeric second parameter", NAME);                     \
        return;                                                               \
    }
#define CHECKNUMERIC(NODE, NAME)                                              \
    if (nodetype(NODE) != INTEGER_) {                                         \
        execerror(env, "numeric list", NAME);                                 \
        return;                                                               \
    }
#define FLOAT(NAME)                                                           \
    if (!FLOATABLE) {                                                         \
        execerror(env, "float or integer", NAME);                             \
        return;                                                               \
    }
#define FLOAT2(NAME)                                                          \
    if (!(FLOATABLE2                                                          \
          || (nodetype(env->stck) == INTEGER_                                 \
              && nodetype(nextnode1(env->stck)) == INTEGER_))) {              \
        execerror(env, "two floats or integers", NAME);                       \
        return;                                                               \
    }
#define ISFILE(NAME)                                                          \
    if (nodetype(env->stck) != FILE_ || !nodevalue(env->stck).fil) {          \
        execerror(env, "file", NAME);                                         \
        return;                                                               \
    }
#define CHECKZERO(NAME)                                                       \
    if (nodevalue(env->stck).num == 0) {                                      \
        execerror(env, "non-zero operand", NAME);                             \
        return;                                                               \
    }
#define CHECKDIVISOR(NAME)                                                    \
    if ((nodetype(env->stck) == FLOAT_ && nodevalue(env->stck).dbl == 0.0)    \
        || (nodetype(env->stck) == INTEGER_                                   \
            && nodevalue(env->stck).num == 0)) {                              \
        execerror(env, "non-zero divisor", NAME);                             \
        return;                                                               \
    }
#define LIST(NAME)                                                            \
    if (nodetype(env->stck) != LIST_) {                                       \
        execerror(env, "list", NAME);                                         \
        return;                                                               \
    }
#define LIST2(NAME)                                                           \
    if (nodetype(nextnode1(env->stck)) != LIST_) {                            \
        execerror(env, "list as second parameter", NAME);                     \
        return;                                                               \
    }
#define USERDEF(NAME)                                                         \
    if (nodetype(env->stck) != USR_) {                                        \
        execerror(env, "user defined symbol", NAME);                          \
        return;                                                               \
    }
#define USERDEF2(NODE, NAME)                                                  \
    if (nodetype(NODE) != USR_) {                                             \
        execerror(env, "user defined symbol", NAME);                          \
        return;                                                               \
    }
#define CHECKLIST(OPR, NAME)                                                  \
    if (OPR != LIST_) {                                                       \
        execerror(env, "internal list", NAME);                                \
        return;                                                               \
    }
#define CHECKSETMEMBER(NODE, NAME)                                            \
    if ((nodetype(NODE) != INTEGER_ && nodetype(NODE) != CHAR_)               \
        || nodevalue(NODE).num < 0 || nodevalue(NODE).num >= SETSIZE) {       \
        execerror(env, "small numeric", NAME);                                \
        return;                                                               \
    }
#define CHECKCHARACTER(NODE, NAME)                                            \
    if (nodetype(NODE) != CHAR_) {                                            \
        execerror(env, "character", NAME);                                    \
        return;                                                               \
    }
#define CHECKEMPTYSET(SET, NAME)                                              \
    if (SET == 0) {                                                           \
        execerror(env, "non-empty set", NAME);                                \
        return;                                                               \
    }
#define CHECKEMPTYSTRING(STRING, NAME)                                        \
    if (*STRING == '\0') {                                                    \
        execerror(env, "non-empty string", NAME);                             \
        return;                                                               \
    }
#define CHECKEMPTYLIST(LIST, NAME)                                            \
    if (!LIST) {                                                              \
        execerror(env, "non-empty list", NAME);                               \
        return;                                                               \
    }
#define CHECKSTACK(NAME)                                                      \
    if (!env->stck) {                                                         \
        execerror(env, "non-empty stack", NAME);                              \
        return;                                                               \
    }
#define CHECKVALUE(NAME)                                                      \
    if (!env->stck) {                                                         \
        execerror(env, "value to push", NAME);                                \
        return;                                                               \
    }
#define CHECKNAME(STRING, NAME)                                               \
    if (!STRING || *STRING) {                                                 \
        execerror(env, "valid name", NAME);                                   \
        return;                                                               \
    }
#define CHECKFORMAT(SPEC, NAME)                                               \
    if (!strchr("dioxX", SPEC)) {                                             \
        execerror(env, "one of: d i o x X", NAME);                            \
        return;                                                               \
    }
#define CHECKFORMATF(SPEC, NAME)                                              \
    if (!strchr("eEfgG", SPEC)) {                                             \
        execerror(env, "one of: e E f g G", NAME);                            \
        return;                                                               \
    }
#define POSITIVEINDEX(INDEX, NAME)                                            \
    if ((nodetype(INDEX) != INTEGER_ && nodetype(INDEX) != BOOLEAN_)          \
        || nodevalue(INDEX).num < 0) {                                        \
        execerror(env, "non-negative integer", NAME);                         \
        return;                                                               \
    }

/* Error macros - report specific error conditions */
#define INDEXTOOLARGE(NAME)                                                   \
    {                                                                         \
        execerror(env, "smaller index", NAME);                                \
        return;                                                               \
    }
#define BADAGGREGATE(NAME)                                                    \
    {                                                                         \
        execerror(env, "aggregate parameter", NAME);                          \
        return;                                                               \
    }
#define BADDATA(NAME)                                                         \
    {                                                                         \
        execerror(env, "different type", NAME);                               \
        return;                                                               \
    }
#else
/*
 * NCHECK mode: All validation macros become no-ops.
 * Use only after thorough testing for maximum performance.
 */
#define ONEPARAM(NAME)
#define TWOPARAMS(NAME)
#define THREEPARAMS(NAME)
#define FOURPARAMS(NAME)
#define FIVEPARAMS(NAME)
#define ONEQUOTE(NAME)
#define TWOQUOTES(NAME)
#define THREEQUOTES(NAME)
#define FOURQUOTES(NAME)
#define SAME2TYPES(NAME)
#define STRING(NAME)
#define STRING2(NAME)
#define INTEGER(NAME)
#define INTEGER2(NAME)
#define CHARACTER(NAME)
#define INTEGERS2(NAME)
#define NUMERICTYPE(NAME)
#define NUMERIC2(NAME)
#define CHECKNUMERIC(NODE, NAME)
#define FLOAT(NAME)
#define FLOAT2(NAME)
#define ISFILE(NAME)
#define CHECKZERO(NAME)
#define CHECKDIVISOR(NAME)
#define LIST(NAME)
#define LIST2(NAME)
#define USERDEF(NAME)
#define USERDEF2(NODE, NAME)
#define CHECKLIST(OPR, NAME)
#define CHECKSETMEMBER(NODE, NAME)
#define CHECKCHARACTER(NODE, NAME)
#define CHECKEMPTYSET(SET, NAME)
#define CHECKEMPTYSTRING(STRING, NAME)
#define CHECKEMPTYLIST(LIST, NAME)
#define CHECKSTACK(NAME)
#define CHECKVALUE(NAME)
#define CHECKNAME(STRING, NAME)
#define CHECKFORMAT(SPEC, NAME)
#define CHECKFORMATF(SPEC, NAME)
#define POSITIVEINDEX(INDEX, NAME)
#define INDEXTOOLARGE(NAME)
#define BADAGGREGATE(NAME)
#define BADDATA(NAME)
#endif

/*
 * Dump stack macros - for saving/restoring state during combinator execution
 *
 * The dump is a stack of saved states used by combinators like 'dip', 'i',
 * and recursive combinators. DMP/DMP1-5 access the list inside dump nodes.
 * SAVESTACK pushes current stack onto dump. SAVED1-6 access saved values.
 *
 * @requires: env in scope
 */
#define DMP nodevalue(env->dump).lis
#define DMP1 nodevalue(env->dump1).lis
#define DMP2 nodevalue(env->dump2).lis
#define DMP3 nodevalue(env->dump3).lis
#define DMP4 nodevalue(env->dump4).lis
#define DMP5 nodevalue(env->dump5).lis
#define SAVESTACK env->dump = LIST_NEWNODE(env->stck, env->dump)
#define SAVED1 DMP
#define SAVED2 nextnode1(DMP)
#define SAVED3 nextnode2(DMP)
#define SAVED4 nextnode3(DMP)
#define SAVED5 nextnode4(DMP)
#define SAVED6 nextnode5(DMP)

#include "src/builtin/andorxor.h"
#include "src/builtin/bfloat.h"
#include "src/builtin/compare.h"
#include "src/builtin/comprel.h"
#include "src/builtin/comprel2.h"
#include "src/builtin/cons_swons.h"
#include "src/builtin/dipped.h"
#include "src/builtin/fileget.h"
#include "src/builtin/help.h"
#include "src/builtin/if_type.h"
#include "src/builtin/inhas.h"
#include "src/builtin/maxmin.h"
#include "src/builtin/n_ary.h"
#include "src/builtin/of_at.h"
#include "src/builtin/ordchr.h"
#include "src/builtin/plusminus.h"
#include "src/builtin/predsucc.h"
#include "src/builtin/push.h"
#include "src/builtin/someall.h"
#include "src/builtin/type.h"
#include "src/builtin/ufloat.h"
#include "src/builtin/unmktime.h"
#include "src/builtin/usetop.h"
