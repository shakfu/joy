/*
 *  module  : vector.c
 *  version : 1.0
 *  date    : 01/22/26
 *
 *  Vectorized operations on numeric lists.
 *  See doc/vector.md for design rationale.
 *
 *  Element-wise: vplus, vminus, vmul, vdiv
 *  Scalar: vscale
 *  Linear algebra: dot
 *  Reductions: vsum, vprod, vmin, vmax
 *  Creation: vzeros, vones, vrange
 */
#include "globals.h"
#include <math.h>

/*
 * Helper: Get numeric value from a node as double.
 * Returns 0 and sets *ok = 0 if node is not numeric.
 */
static double get_numeric(pEnv env, Index node, int* ok)
{
    (void)env;  /* May be needed for error reporting in future */
    if (nodetype(node) == INTEGER_) {
        *ok = 1;
        return (double)nodevalue(node).num;
    }
    if (nodetype(node) == FLOAT_) {
        *ok = 1;
        return nodevalue(node).dbl;
    }
    *ok = 0;
    return 0.0;
}

/*
 * Helper: Check if a list contains only numeric values.
 * Returns list length, or -1 if not a valid numeric list.
 */
static int check_numeric_list(pEnv env, Index list, char* name)
{
    int len = 0;
    int ok;
    Index node = list;

    while (node) {
        get_numeric(env, node, &ok);
        if (!ok) {
            execerror(env, "numeric list", name);
            return -1;
        }
        len++;
        node = nextnode1(node);
    }
    return len;
}

/*
 * Helper: Build a result list from an array of doubles.
 * Uses FLOAT_ nodes for all results to preserve precision.
 */
static Index build_float_list(pEnv env, double* values, int len)
{
    Index head = 0;
    Index tail = 0;
    Index node;
    int i;

    for (i = 0; i < len; i++) {
        node = FLOAT_NEWNODE(values[i], 0);
        if (!head) {
            head = node;
            tail = node;
        } else {
            nextnode1(tail) = node;
            tail = node;
        }
    }
    return head;
}

/*
 * Helper: Build a result list from an array of integers.
 */
static Index build_int_list(pEnv env, int64_t* values, int len)
{
    Index head = 0;
    Index tail = 0;
    Index node;
    int i;

    for (i = 0; i < len; i++) {
        node = INTEGER_NEWNODE(values[i], 0);
        if (!head) {
            head = node;
            tail = node;
        } else {
            nextnode1(tail) = node;
            tail = node;
        }
    }
    return head;
}

/*
 * Helper: Extract values from a numeric list into a double array.
 * Caller must ensure array is large enough.
 */
static void extract_values(pEnv env, Index list, double* values, int len)
{
    int i = 0;
    int ok;
    Index node = list;

    while (node && i < len) {
        values[i++] = get_numeric(env, node, &ok);
        node = nextnode1(node);
    }
}

/* ========== Element-wise operations ========== */

/**
Q0  OK  3400  v+\0vplus  :  V1 V2  ->  V3
Vector V3 is the element-wise sum of numeric lists V1 and V2.
Lists must have equal length.
*/
void vplus_(pEnv env)
{
    Index list1, list2;
    int len1, len2, i;
    double *a, *b, *result;

    TWOPARAMS("v+");
    LIST("v+");
    LIST2("v+");

    list2 = nodevalue(env->stck).lis;
    list1 = nodevalue(nextnode1(env->stck)).lis;

    len2 = check_numeric_list(env, list2, "v+");
    if (len2 < 0) return;
    len1 = check_numeric_list(env, list1, "v+");
    if (len1 < 0) return;

    if (len1 != len2) {
        execerror(env, "lists of equal length", "v+");
        return;
    }

    if (len1 == 0) {
        BINARY(LIST_NEWNODE, 0);
        return;
    }

    a = malloc(len1 * sizeof(double));
    b = malloc(len1 * sizeof(double));
    result = malloc(len1 * sizeof(double));

    extract_values(env, list1, a, len1);
    extract_values(env, list2, b, len1);

    for (i = 0; i < len1; i++) {
        result[i] = a[i] + b[i];
    }

    BINARY(LIST_NEWNODE, build_float_list(env, result, len1));

    free(a);
    free(b);
    free(result);
}

/**
Q0  OK  3410  v-\0vminus  :  V1 V2  ->  V3
Vector V3 is the element-wise difference of numeric lists V1 and V2.
Lists must have equal length.
*/
void vminus_(pEnv env)
{
    Index list1, list2;
    int len1, len2, i;
    double *a, *b, *result;

    TWOPARAMS("v-");
    LIST("v-");
    LIST2("v-");

    list2 = nodevalue(env->stck).lis;
    list1 = nodevalue(nextnode1(env->stck)).lis;

    len2 = check_numeric_list(env, list2, "v-");
    if (len2 < 0) return;
    len1 = check_numeric_list(env, list1, "v-");
    if (len1 < 0) return;

    if (len1 != len2) {
        execerror(env, "lists of equal length", "v-");
        return;
    }

    if (len1 == 0) {
        BINARY(LIST_NEWNODE, 0);
        return;
    }

    a = malloc(len1 * sizeof(double));
    b = malloc(len1 * sizeof(double));
    result = malloc(len1 * sizeof(double));

    extract_values(env, list1, a, len1);
    extract_values(env, list2, b, len1);

    for (i = 0; i < len1; i++) {
        result[i] = a[i] - b[i];
    }

    BINARY(LIST_NEWNODE, build_float_list(env, result, len1));

    free(a);
    free(b);
    free(result);
}

/**
Q0  OK  3420  v*\0vmul  :  V1 V2  ->  V3
Vector V3 is the element-wise product of numeric lists V1 and V2.
Lists must have equal length.
*/
void vmul_(pEnv env)
{
    Index list1, list2;
    int len1, len2, i;
    double *a, *b, *result;

    TWOPARAMS("v*");
    LIST("v*");
    LIST2("v*");

    list2 = nodevalue(env->stck).lis;
    list1 = nodevalue(nextnode1(env->stck)).lis;

    len2 = check_numeric_list(env, list2, "v*");
    if (len2 < 0) return;
    len1 = check_numeric_list(env, list1, "v*");
    if (len1 < 0) return;

    if (len1 != len2) {
        execerror(env, "lists of equal length", "v*");
        return;
    }

    if (len1 == 0) {
        BINARY(LIST_NEWNODE, 0);
        return;
    }

    a = malloc(len1 * sizeof(double));
    b = malloc(len1 * sizeof(double));
    result = malloc(len1 * sizeof(double));

    extract_values(env, list1, a, len1);
    extract_values(env, list2, b, len1);

    for (i = 0; i < len1; i++) {
        result[i] = a[i] * b[i];
    }

    BINARY(LIST_NEWNODE, build_float_list(env, result, len1));

    free(a);
    free(b);
    free(result);
}

/**
Q0  OK  3430  v/\0vdiv  :  V1 V2  ->  V3
Vector V3 is the element-wise quotient of numeric lists V1 and V2.
Lists must have equal length. Division by zero yields infinity.
*/
void vdiv_(pEnv env)
{
    Index list1, list2;
    int len1, len2, i;
    double *a, *b, *result;

    TWOPARAMS("v/");
    LIST("v/");
    LIST2("v/");

    list2 = nodevalue(env->stck).lis;
    list1 = nodevalue(nextnode1(env->stck)).lis;

    len2 = check_numeric_list(env, list2, "v/");
    if (len2 < 0) return;
    len1 = check_numeric_list(env, list1, "v/");
    if (len1 < 0) return;

    if (len1 != len2) {
        execerror(env, "lists of equal length", "v/");
        return;
    }

    if (len1 == 0) {
        BINARY(LIST_NEWNODE, 0);
        return;
    }

    a = malloc(len1 * sizeof(double));
    b = malloc(len1 * sizeof(double));
    result = malloc(len1 * sizeof(double));

    extract_values(env, list1, a, len1);
    extract_values(env, list2, b, len1);

    for (i = 0; i < len1; i++) {
        result[i] = a[i] / b[i];
    }

    BINARY(LIST_NEWNODE, build_float_list(env, result, len1));

    free(a);
    free(b);
    free(result);
}

/* ========== Scalar operations ========== */

/**
Q0  OK  3440  vscale  :  V S  ->  V2
Vector V2 is numeric list V scaled by scalar S.
*/
void vscale_(pEnv env)
{
    Index list;
    int len, i, ok;
    double scalar;
    double *values, *result;

    TWOPARAMS("vscale");
    FLOAT("vscale");

    scalar = get_numeric(env, env->stck, &ok);
    if (!ok) {
        execerror(env, "numeric scalar", "vscale");
        return;
    }

    POP(env->stck);
    LIST("vscale");

    list = nodevalue(env->stck).lis;
    len = check_numeric_list(env, list, "vscale");
    if (len < 0) return;

    if (len == 0) {
        UNARY(LIST_NEWNODE, 0);
        return;
    }

    values = malloc(len * sizeof(double));
    result = malloc(len * sizeof(double));

    extract_values(env, list, values, len);

    for (i = 0; i < len; i++) {
        result[i] = values[i] * scalar;
    }

    UNARY(LIST_NEWNODE, build_float_list(env, result, len));

    free(values);
    free(result);
}

/* ========== Linear algebra ========== */

/**
Q0  OK  3450  dot  :  V1 V2  ->  N
N is the dot product of numeric lists V1 and V2.
Lists must have equal length.
*/
void dot_(pEnv env)
{
    Index list1, list2;
    int len1, len2, i;
    double *a, *b;
    double result = 0.0;

    TWOPARAMS("dot");
    LIST("dot");
    LIST2("dot");

    list2 = nodevalue(env->stck).lis;
    list1 = nodevalue(nextnode1(env->stck)).lis;

    len2 = check_numeric_list(env, list2, "dot");
    if (len2 < 0) return;
    len1 = check_numeric_list(env, list1, "dot");
    if (len1 < 0) return;

    if (len1 != len2) {
        execerror(env, "lists of equal length", "dot");
        return;
    }

    if (len1 == 0) {
        BINARY(FLOAT_NEWNODE, 0.0);
        return;
    }

    a = malloc(len1 * sizeof(double));
    b = malloc(len1 * sizeof(double));

    extract_values(env, list1, a, len1);
    extract_values(env, list2, b, len1);

    for (i = 0; i < len1; i++) {
        result += a[i] * b[i];
    }

    BINARY(FLOAT_NEWNODE, result);

    free(a);
    free(b);
}

/* ========== Reductions ========== */

/**
Q0  OK  3460  vsum  :  V  ->  N
N is the sum of all elements in numeric list V.
*/
void vsum_(pEnv env)
{
    Index list, node;
    int ok;
    double result = 0.0;

    ONEPARAM("vsum");
    LIST("vsum");

    list = nodevalue(env->stck).lis;

    for (node = list; node; node = nextnode1(node)) {
        double val = get_numeric(env, node, &ok);
        if (!ok) {
            execerror(env, "numeric list", "vsum");
            return;
        }
        result += val;
    }

    UNARY(FLOAT_NEWNODE, result);
}

/**
Q0  OK  3470  vprod  :  V  ->  N
N is the product of all elements in numeric list V.
*/
void vprod_(pEnv env)
{
    Index list, node;
    int ok;
    double result = 1.0;

    ONEPARAM("vprod");
    LIST("vprod");

    list = nodevalue(env->stck).lis;

    for (node = list; node; node = nextnode1(node)) {
        double val = get_numeric(env, node, &ok);
        if (!ok) {
            execerror(env, "numeric list", "vprod");
            return;
        }
        result *= val;
    }

    UNARY(FLOAT_NEWNODE, result);
}

/**
Q0  OK  3480  vmin  :  V  ->  N
N is the minimum element in numeric list V.
*/
void vmin_(pEnv env)
{
    Index list, node;
    int ok, first = 1;
    double result = 0.0;

    ONEPARAM("vmin");
    LIST("vmin");

    list = nodevalue(env->stck).lis;

    if (!list) {
        execerror(env, "non-empty list", "vmin");
        return;
    }

    for (node = list; node; node = nextnode1(node)) {
        double val = get_numeric(env, node, &ok);
        if (!ok) {
            execerror(env, "numeric list", "vmin");
            return;
        }
        if (first || val < result) {
            result = val;
            first = 0;
        }
    }

    UNARY(FLOAT_NEWNODE, result);
}

/**
Q0  OK  3490  vmax  :  V  ->  N
N is the maximum element in numeric list V.
*/
void vmax_(pEnv env)
{
    Index list, node;
    int ok, first = 1;
    double result = 0.0;

    ONEPARAM("vmax");
    LIST("vmax");

    list = nodevalue(env->stck).lis;

    if (!list) {
        execerror(env, "non-empty list", "vmax");
        return;
    }

    for (node = list; node; node = nextnode1(node)) {
        double val = get_numeric(env, node, &ok);
        if (!ok) {
            execerror(env, "numeric list", "vmax");
            return;
        }
        if (first || val > result) {
            result = val;
            first = 0;
        }
    }

    UNARY(FLOAT_NEWNODE, result);
}

/* ========== Creation ========== */

/**
Q0  OK  3500  vzeros  :  N  ->  V
V is a list of N zeros.
*/
void vzeros_(pEnv env)
{
    int64_t n, i;
    int64_t* values;

    ONEPARAM("vzeros");
    POSITIVEINDEX(env->stck, "vzeros");

    n = nodevalue(env->stck).num;

    if (n == 0) {
        UNARY(LIST_NEWNODE, 0);
        return;
    }

    values = malloc(n * sizeof(int64_t));
    for (i = 0; i < n; i++) {
        values[i] = 0;
    }

    UNARY(LIST_NEWNODE, build_int_list(env, values, (int)n));

    free(values);
}

/**
Q0  OK  3510  vones  :  N  ->  V
V is a list of N ones.
*/
void vones_(pEnv env)
{
    int64_t n, i;
    int64_t* values;

    ONEPARAM("vones");
    POSITIVEINDEX(env->stck, "vones");

    n = nodevalue(env->stck).num;

    if (n == 0) {
        UNARY(LIST_NEWNODE, 0);
        return;
    }

    values = malloc(n * sizeof(int64_t));
    for (i = 0; i < n; i++) {
        values[i] = 1;
    }

    UNARY(LIST_NEWNODE, build_int_list(env, values, (int)n));

    free(values);
}

/**
Q0  OK  3520  vrange  :  A B  ->  V
V is a list of integers from A to B inclusive.
*/
void vrange_(pEnv env)
{
    int64_t a, b, i, len;
    int64_t* values;

    TWOPARAMS("vrange");
    INTEGER("vrange");
    INTEGER2("vrange");

    b = nodevalue(env->stck).num;
    a = nodevalue(nextnode1(env->stck)).num;

    if (b < a) {
        BINARY(LIST_NEWNODE, 0);
        return;
    }

    len = b - a + 1;
    values = malloc(len * sizeof(int64_t));
    for (i = 0; i < len; i++) {
        values[i] = a + i;
    }

    BINARY(LIST_NEWNODE, build_int_list(env, values, (int)len));

    free(values);
}
