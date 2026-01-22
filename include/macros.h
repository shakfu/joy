/*
 *  module  : macros.h
 *  version : 1.3
 *  date    : 01/22/26
 *
 *  Stack manipulation and node creation macros for Joy builtins.
 *
 *  IMPORTANT: All macros in this file require 'pEnv env' to be in scope.
 *  They access env->stck, env->bucket, env->memory (NOBDW), and call
 *  newnode()/newnode2() which also require env.
 *
 *  Macro Categories:
 *  - POP: Advance a list pointer to next node
 *  - *_NEWNODE: Create typed nodes (INTEGER_NEWNODE, LIST_NEWNODE, etc.)
 *  - NULLARY/UNARY/BINARY: Push results consuming 0/1/2 stack items
 *  - G* variants: Generic versions using newnode2 for copying
 *  - GETSTRING: Access string data (differs between NOBDW and BDW)
 */

/*
 * POP(X) - Advance list pointer X to the next node
 *
 * @requires: env in scope (for nextnode1 macro)
 * @param X: An Index (list pointer) to advance
 * @modifies: X
 *
 * Example:
 *   Index list = env->stck;
 *   POP(list);  // list now points to second element
 */
#define POP(X) X = nextnode1(X)

/*
 * TYPE_NEWNODE(u, r) - Create a new node of TYPE with value u, next pointer r
 *
 * @requires: env in scope
 * @param u: The value to store (type depends on node type)
 * @param r: The next pointer (Index) for the new node
 * @returns: Index to the newly created node
 * @modifies: env->bucket (temporary storage for value)
 *
 * These macros use env->bucket as a temporary to pass the typed value
 * to newnode(). The bucket is a Types union, so assignment sets the
 * appropriate union member.
 *
 * Example:
 *   Index node = INTEGER_NEWNODE(42, 0);  // Create node with value 42, no next
 *   Index list = LIST_NEWNODE(inner, outer);  // Create list node
 */
#define USR_NEWNODE(u, r)                                                     \
    (env->bucket.ent = u, newnode(env, USR_, env->bucket, r))
#define ANON_FUNCT_NEWNODE(u, r)                                              \
    (env->bucket.proc = u, newnode(env, ANON_FUNCT_, env->bucket, r))
#define BOOLEAN_NEWNODE(u, r)                                                 \
    (env->bucket.num = u, newnode(env, BOOLEAN_, env->bucket, r))
#define CHAR_NEWNODE(u, r)                                                    \
    (env->bucket.num = u, newnode(env, CHAR_, env->bucket, r))
#define INTEGER_NEWNODE(u, r)                                                 \
    (env->bucket.num = u, newnode(env, INTEGER_, env->bucket, r))
#define SET_NEWNODE(u, r)                                                     \
    (env->bucket.set = u, newnode(env, SET_, env->bucket, r))
#define STRING_NEWNODE(u, r)                                                  \
    (env->bucket.str = u, newnode(env, STRING_, env->bucket, r))
#define LIST_NEWNODE(u, r)                                                    \
    (env->bucket.lis = u, newnode(env, LIST_, env->bucket, r))
#define FLOAT_NEWNODE(u, r)                                                   \
    (env->bucket.dbl = u, newnode(env, FLOAT_, env->bucket, r))
#define FILE_NEWNODE(u, r)                                                    \
    (env->bucket.fil = u, newnode(env, FILE_, env->bucket, r))
#define BIGNUM_NEWNODE(u, r)                                                  \
    (env->bucket.str = u, newnode(env, BIGNUM_, env->bucket, r))

/*
 * NULLARY/UNARY/BINARY - Push result onto stack, consuming 0/1/2 items
 *
 * These are the primary macros for implementing Joy primitives. They
 * create a new node with the computed VALUE and link it into the stack,
 * removing the consumed operands.
 *
 * @requires: env in scope
 * @param CONSTRUCTOR: A *_NEWNODE macro (e.g., INTEGER_NEWNODE)
 * @param VALUE: The value to store in the new node
 * @modifies: env->stck
 *
 * Stack effects:
 *   NULLARY: ( -- x )      Push without consuming (e.g., 'true', 'clock')
 *   UNARY:   ( a -- x )    Replace top with result (e.g., 'not', 'abs')
 *   BINARY:  ( a b -- x )  Replace top two with result (e.g., '+', 'and')
 *
 * Example:
 *   // Implement 'succ' (increment top of stack)
 *   UNARY(INTEGER_NEWNODE, nodevalue(env->stck).num + 1);
 *
 *   // Implement '+' (add top two integers)
 *   BINARY(INTEGER_NEWNODE,
 *          nodevalue(nextnode1(env->stck)).num + nodevalue(env->stck).num);
 */
#define NULLARY(CONSTRUCTOR, VALUE) env->stck = CONSTRUCTOR(VALUE, env->stck)
#define UNARY(CONSTRUCTOR, VALUE)                                             \
    env->stck = CONSTRUCTOR(VALUE, nextnode1(env->stck))
#define BINARY(CONSTRUCTOR, VALUE)                                            \
    env->stck = CONSTRUCTOR(VALUE, nextnode2(env->stck))

/*
 * GNULLARY/GUNARY/GBINARY/GTERNARY - Generic versions using newnode2
 *
 * These copy an existing node onto the stack rather than creating a
 * new node with a computed value. Used when the result is an existing
 * node (e.g., 'first' returns the first element of a list).
 *
 * @requires: env in scope
 * @param NODE: An existing Index to copy onto the stack
 * @modifies: env->stck
 *
 * Example:
 *   // Implement 'first' - push first element of list
 *   GUNARY(nodevalue(env->stck).lis);
 */
#define GNULLARY(NODE) env->stck = newnode2(env, NODE, env->stck)
#define GUNARY(NODE) env->stck = newnode2(env, NODE, nextnode1(env->stck))
#define GBINARY(NODE) env->stck = newnode2(env, NODE, nextnode2(env->stck))
#define GTERNARY(NODE) env->stck = newnode2(env, NODE, nextnode3(env->stck))

/*
 * GETSTRING(NODE) - Get pointer to string data from a STRING_ node
 *
 * @requires: env in scope (for nodevalue macro)
 * @param NODE: An Index pointing to a STRING_ node
 * @returns: char* to the string data
 *
 * Note: String storage differs between memory models:
 * - NOBDW: Strings stored inline in consecutive nodes, access via address
 * - BDW: Strings stored in GC-managed memory, access via .str pointer
 *
 * Example:
 *   char* s = GETSTRING(env->stck);
 *   printf("%s\n", s);
 */
#ifdef NOBDW
#define GETSTRING(NODE) (char*)&nodevalue(NODE)
#else
#define GETSTRING(NODE) nodevalue(NODE).str
#endif
