/*
 *  module  : vector.c
 *  version : 1.2
 *  date    : 01/22/26
 *
 *  Vectorized operations on numeric lists and matrices.
 *  See doc/vector.md for design rationale.
 *
 *  Vectors (1D):
 *    Element-wise: vplus, vminus, vmul, vdiv
 *    Scalar: vscale
 *    Linear algebra: dot
 *    Reductions: vsum, vprod, vmin, vmax
 *    Creation: vzeros, vones, vrange
 *
 *  Matrices (2D - lists of lists):
 *    Element-wise: mplus, mminus, mmul, mdiv
 *    Scalar: mscale
 *    Linear algebra: mm (matmul), mv (matrix-vector), transpose
 *    Properties: det, inv, trace
 *    Creation: meye (identity matrix)
 *
 *  Optional BLAS support (-DJOY_BLAS=ON):
 *    Uses cblas_dgemm for matrix multiply, cblas_dgemv for matrix-vector.
 *    Enabled above BLAS_THRESHOLD to amortize list-to-array conversion cost.
 */
#include "globals.h"
#include <math.h>

#ifdef JOY_BLAS
#ifdef __APPLE__
#include <Accelerate/Accelerate.h>
#else
#include <cblas.h>
#endif

/*
 * Minimum matrix dimension to use BLAS. Below this threshold, the overhead
 * of converting linked lists to contiguous arrays exceeds BLAS benefits.
 * Determined empirically - BLAS wins for matrices roughly 32x32 and larger.
 */
#define BLAS_THRESHOLD 32
#endif /* JOY_BLAS */

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
 *
 * IMPORTANT: Calls ensure_capacity first to prevent GC from running
 * during the loop, which would invalidate local Index variables.
 */
static Index build_float_list(pEnv env, double* values, int len)
{
    Index head = 0;
    Index tail = 0;
    Index node;
    int i;

    /* Pre-allocate to avoid GC during loop (which would invalidate head/tail) */
    ensure_capacity(env, len);

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
 *
 * IMPORTANT: Calls ensure_capacity first to prevent GC from running
 * during the loop, which would invalidate local Index variables.
 */
static Index build_int_list(pEnv env, int64_t* values, int len)
{
    Index head = 0;
    Index tail = 0;
    Index node;
    int i;

    /* Pre-allocate to avoid GC during loop (which would invalidate head/tail) */
    ensure_capacity(env, len);

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

    #pragma omp simd
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

    #pragma omp simd
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

    #pragma omp simd
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

    #pragma omp simd
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

    #pragma omp simd
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

    #pragma omp simd reduction(+:result)
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

/* ========== Advanced vector operations ========== */

/**
Q0  OK  3530  vnorm  :  V  ->  N
N is the Euclidean norm (magnitude) of numeric list V.
*/
void vnorm_(pEnv env)
{
    Index list, node;
    int ok;
    double sum = 0.0;

    ONEPARAM("vnorm");
    LIST("vnorm");

    list = nodevalue(env->stck).lis;

    for (node = list; node; node = nextnode1(node)) {
        double val = get_numeric(env, node, &ok);
        if (!ok) {
            execerror(env, "numeric list", "vnorm");
            return;
        }
        sum += val * val;
    }

    UNARY(FLOAT_NEWNODE, sqrt(sum));
}

/**
Q0  OK  3540  vnormalize  :  V  ->  V2
V2 is the unit vector in the direction of numeric list V.
Returns zero vector if V has zero magnitude.
*/
void vnormalize_(pEnv env)
{
    Index list;
    int len, i;
    double *values, *result;
    double norm = 0.0;

    ONEPARAM("vnormalize");
    LIST("vnormalize");

    list = nodevalue(env->stck).lis;
    len = check_numeric_list(env, list, "vnormalize");
    if (len < 0) return;

    if (len == 0) {
        UNARY(LIST_NEWNODE, 0);
        return;
    }

    values = malloc(len * sizeof(double));
    result = malloc(len * sizeof(double));

    extract_values(env, list, values, len);

    /* Calculate norm */
    #pragma omp simd reduction(+:norm)
    for (i = 0; i < len; i++) {
        norm += values[i] * values[i];
    }
    norm = sqrt(norm);

    /* Normalize (handle zero vector) */
    if (norm > 1e-15) {
        #pragma omp simd
        for (i = 0; i < len; i++) {
            result[i] = values[i] / norm;
        }
    } else {
        #pragma omp simd
        for (i = 0; i < len; i++) {
            result[i] = 0.0;
        }
    }

    UNARY(LIST_NEWNODE, build_float_list(env, result, len));

    free(values);
    free(result);
}

/**
Q0  OK  3550  cross  :  V1 V2  ->  V3
V3 is the cross product of 3D vectors V1 and V2.
Both vectors must have exactly 3 elements.
*/
void cross_(pEnv env)
{
    Index list1, list2;
    int len1, len2;
    double a[3], b[3], result[3];

    TWOPARAMS("cross");
    LIST("cross");
    LIST2("cross");

    list2 = nodevalue(env->stck).lis;
    list1 = nodevalue(nextnode1(env->stck)).lis;

    len2 = check_numeric_list(env, list2, "cross");
    if (len2 < 0) return;
    len1 = check_numeric_list(env, list1, "cross");
    if (len1 < 0) return;

    if (len1 != 3 || len2 != 3) {
        execerror(env, "3-element vectors", "cross");
        return;
    }

    extract_values(env, list1, a, 3);
    extract_values(env, list2, b, 3);

    /* Cross product: a x b */
    result[0] = a[1] * b[2] - a[2] * b[1];
    result[1] = a[2] * b[0] - a[0] * b[2];
    result[2] = a[0] * b[1] - a[1] * b[0];

    BINARY(LIST_NEWNODE, build_float_list(env, result, 3));
}

/**
Q0  OK  3560  vmean  :  V  ->  N
N is the arithmetic mean of all elements in numeric list V.
*/
void vmean_(pEnv env)
{
    Index list, node;
    int ok, count = 0;
    double sum = 0.0;

    ONEPARAM("vmean");
    LIST("vmean");

    list = nodevalue(env->stck).lis;

    if (!list) {
        execerror(env, "non-empty list", "vmean");
        return;
    }

    for (node = list; node; node = nextnode1(node)) {
        double val = get_numeric(env, node, &ok);
        if (!ok) {
            execerror(env, "numeric list", "vmean");
            return;
        }
        sum += val;
        count++;
    }

    UNARY(FLOAT_NEWNODE, sum / count);
}

/**
Q0  OK  3570  vlinspace  :  A B N  ->  V
V is a list of N linearly spaced values from A to B inclusive.
*/
void vlinspace_(pEnv env)
{
    int64_t n;
    int i, ok;
    double a, b, step;
    double *result;

    THREEPARAMS("vlinspace");
    POSITIVEINDEX(env->stck, "vlinspace");

    n = nodevalue(env->stck).num;
    POP(env->stck);

    b = get_numeric(env, env->stck, &ok);
    if (!ok) {
        execerror(env, "numeric end value", "vlinspace");
        return;
    }
    POP(env->stck);

    a = get_numeric(env, env->stck, &ok);
    if (!ok) {
        execerror(env, "numeric start value", "vlinspace");
        return;
    }

    if (n == 0) {
        UNARY(LIST_NEWNODE, 0);
        return;
    }

    if (n == 1) {
        result = malloc(sizeof(double));
        result[0] = a;
        UNARY(LIST_NEWNODE, build_float_list(env, result, 1));
        free(result);
        return;
    }

    result = malloc(n * sizeof(double));
    step = (b - a) / (n - 1);

    for (i = 0; i < n; i++) {
        result[i] = a + i * step;
    }
    /* Ensure last element is exactly b (avoid floating point drift) */
    result[n - 1] = b;

    UNARY(LIST_NEWNODE, build_float_list(env, result, (int)n));

    free(result);
}

/* ========== Matrix helpers ========== */

/*
 * Helper: Check if a list is a valid matrix (list of numeric lists).
 * Returns rows in *rows, cols in *cols (0 for empty matrix).
 * Returns 0 on success, -1 on error.
 */
static int check_matrix(pEnv env, Index mat, int* rows, int* cols, char* name)
{
    Index row_node;
    int r = 0;
    int c = -1;  /* -1 means not yet determined */

    *rows = 0;
    *cols = 0;

    if (!mat) {
        return 0;  /* Empty matrix is valid */
    }

    for (row_node = mat; row_node; row_node = nextnode1(row_node)) {
        Index row;
        int row_len;

        if (nodetype(row_node) != LIST_) {
            execerror(env, "matrix (list of lists)", name);
            return -1;
        }

        row = nodevalue(row_node).lis;
        row_len = check_numeric_list(env, row, name);
        if (row_len < 0) return -1;

        if (c == -1) {
            c = row_len;
        } else if (row_len != c) {
            execerror(env, "matrix with uniform row lengths", name);
            return -1;
        }

        r++;
    }

    *rows = r;
    *cols = (c == -1) ? 0 : c;
    return 0;
}

/*
 * Helper: Extract matrix values into a 2D array (row-major).
 * Caller must allocate array of size rows*cols.
 */
static void extract_matrix(pEnv env, Index mat, double* values, int rows, int cols)
{
    Index row_node;
    int r = 0;

    for (row_node = mat; row_node && r < rows; row_node = nextnode1(row_node)) {
        Index row = nodevalue(row_node).lis;
        extract_values(env, row, values + r * cols, cols);
        r++;
    }
}

/*
 * Helper: Build a matrix (list of lists) from a 2D array (row-major).
 */
static Index build_matrix(pEnv env, double* values, int rows, int cols)
{
    Index head = 0;
    Index tail = 0;
    int r;

    for (r = 0; r < rows; r++) {
        Index row = build_float_list(env, values + r * cols, cols);
        Index row_node = LIST_NEWNODE(row, 0);

        if (!head) {
            head = row_node;
            tail = row_node;
        } else {
            nextnode1(tail) = row_node;
            tail = row_node;
        }
    }

    return head;
}

/* ========== Matrix element-wise operations ========== */

/**
Q0  OK  3600  m+\0mplus  :  M1 M2  ->  M3
Matrix M3 is the element-wise sum of matrices M1 and M2.
Matrices must have equal dimensions.
*/
void mplus_(pEnv env)
{
    Index mat1, mat2;
    int rows1, cols1, rows2, cols2, i, size;
    double *a, *b, *result;

    TWOPARAMS("m+");
    LIST("m+");
    LIST2("m+");

    mat2 = nodevalue(env->stck).lis;
    mat1 = nodevalue(nextnode1(env->stck)).lis;

    if (check_matrix(env, mat1, &rows1, &cols1, "m+") < 0) return;
    if (check_matrix(env, mat2, &rows2, &cols2, "m+") < 0) return;

    if (rows1 != rows2 || cols1 != cols2) {
        execerror(env, "matrices of equal dimensions", "m+");
        return;
    }

    if (rows1 == 0 || cols1 == 0) {
        BINARY(LIST_NEWNODE, 0);
        return;
    }

    size = rows1 * cols1;
    a = malloc(size * sizeof(double));
    b = malloc(size * sizeof(double));
    result = malloc(size * sizeof(double));

    extract_matrix(env, mat1, a, rows1, cols1);
    extract_matrix(env, mat2, b, rows2, cols2);

    #pragma omp simd
    for (i = 0; i < size; i++) {
        result[i] = a[i] + b[i];
    }

    BINARY(LIST_NEWNODE, build_matrix(env, result, rows1, cols1));

    free(a);
    free(b);
    free(result);
}

/**
Q0  OK  3610  m-\0mminus  :  M1 M2  ->  M3
Matrix M3 is the element-wise difference of matrices M1 and M2.
Matrices must have equal dimensions.
*/
void mminus_(pEnv env)
{
    Index mat1, mat2;
    int rows1, cols1, rows2, cols2, i, size;
    double *a, *b, *result;

    TWOPARAMS("m-");
    LIST("m-");
    LIST2("m-");

    mat2 = nodevalue(env->stck).lis;
    mat1 = nodevalue(nextnode1(env->stck)).lis;

    if (check_matrix(env, mat1, &rows1, &cols1, "m-") < 0) return;
    if (check_matrix(env, mat2, &rows2, &cols2, "m-") < 0) return;

    if (rows1 != rows2 || cols1 != cols2) {
        execerror(env, "matrices of equal dimensions", "m-");
        return;
    }

    if (rows1 == 0 || cols1 == 0) {
        BINARY(LIST_NEWNODE, 0);
        return;
    }

    size = rows1 * cols1;
    a = malloc(size * sizeof(double));
    b = malloc(size * sizeof(double));
    result = malloc(size * sizeof(double));

    extract_matrix(env, mat1, a, rows1, cols1);
    extract_matrix(env, mat2, b, rows2, cols2);

    #pragma omp simd
    for (i = 0; i < size; i++) {
        result[i] = a[i] - b[i];
    }

    BINARY(LIST_NEWNODE, build_matrix(env, result, rows1, cols1));

    free(a);
    free(b);
    free(result);
}

/**
Q0  OK  3620  m*\0mmul  :  M1 M2  ->  M3
Matrix M3 is the element-wise product of matrices M1 and M2.
Matrices must have equal dimensions.
*/
void mmul_(pEnv env)
{
    Index mat1, mat2;
    int rows1, cols1, rows2, cols2, i, size;
    double *a, *b, *result;

    TWOPARAMS("m*");
    LIST("m*");
    LIST2("m*");

    mat2 = nodevalue(env->stck).lis;
    mat1 = nodevalue(nextnode1(env->stck)).lis;

    if (check_matrix(env, mat1, &rows1, &cols1, "m*") < 0) return;
    if (check_matrix(env, mat2, &rows2, &cols2, "m*") < 0) return;

    if (rows1 != rows2 || cols1 != cols2) {
        execerror(env, "matrices of equal dimensions", "m*");
        return;
    }

    if (rows1 == 0 || cols1 == 0) {
        BINARY(LIST_NEWNODE, 0);
        return;
    }

    size = rows1 * cols1;
    a = malloc(size * sizeof(double));
    b = malloc(size * sizeof(double));
    result = malloc(size * sizeof(double));

    extract_matrix(env, mat1, a, rows1, cols1);
    extract_matrix(env, mat2, b, rows2, cols2);

    #pragma omp simd
    for (i = 0; i < size; i++) {
        result[i] = a[i] * b[i];
    }

    BINARY(LIST_NEWNODE, build_matrix(env, result, rows1, cols1));

    free(a);
    free(b);
    free(result);
}

/**
Q0  OK  3630  m/\0mdiv  :  M1 M2  ->  M3
Matrix M3 is the element-wise quotient of matrices M1 and M2.
Matrices must have equal dimensions. Division by zero yields infinity.
*/
void mdiv_(pEnv env)
{
    Index mat1, mat2;
    int rows1, cols1, rows2, cols2, i, size;
    double *a, *b, *result;

    TWOPARAMS("m/");
    LIST("m/");
    LIST2("m/");

    mat2 = nodevalue(env->stck).lis;
    mat1 = nodevalue(nextnode1(env->stck)).lis;

    if (check_matrix(env, mat1, &rows1, &cols1, "m/") < 0) return;
    if (check_matrix(env, mat2, &rows2, &cols2, "m/") < 0) return;

    if (rows1 != rows2 || cols1 != cols2) {
        execerror(env, "matrices of equal dimensions", "m/");
        return;
    }

    if (rows1 == 0 || cols1 == 0) {
        BINARY(LIST_NEWNODE, 0);
        return;
    }

    size = rows1 * cols1;
    a = malloc(size * sizeof(double));
    b = malloc(size * sizeof(double));
    result = malloc(size * sizeof(double));

    extract_matrix(env, mat1, a, rows1, cols1);
    extract_matrix(env, mat2, b, rows2, cols2);

    #pragma omp simd
    for (i = 0; i < size; i++) {
        result[i] = a[i] / b[i];
    }

    BINARY(LIST_NEWNODE, build_matrix(env, result, rows1, cols1));

    free(a);
    free(b);
    free(result);
}

/**
Q0  OK  3640  mscale  :  M S  ->  M2
Matrix M2 is matrix M scaled by scalar S.
*/
void mscale_(pEnv env)
{
    Index mat;
    int rows, cols, i, size, ok;
    double scalar;
    double *values, *result;

    TWOPARAMS("mscale");
    FLOAT("mscale");

    scalar = get_numeric(env, env->stck, &ok);
    if (!ok) {
        execerror(env, "numeric scalar", "mscale");
        return;
    }

    POP(env->stck);
    LIST("mscale");

    mat = nodevalue(env->stck).lis;
    if (check_matrix(env, mat, &rows, &cols, "mscale") < 0) return;

    if (rows == 0 || cols == 0) {
        UNARY(LIST_NEWNODE, 0);
        return;
    }

    size = rows * cols;
    values = malloc(size * sizeof(double));
    result = malloc(size * sizeof(double));

    extract_matrix(env, mat, values, rows, cols);

    #pragma omp simd
    for (i = 0; i < size; i++) {
        result[i] = values[i] * scalar;
    }

    UNARY(LIST_NEWNODE, build_matrix(env, result, rows, cols));

    free(values);
    free(result);
}

/* ========== Matrix linear algebra ========== */

/**
Q0  OK  3650  mm  :  M1 M2  ->  M3
Matrix M3 is the matrix product of M1 and M2.
M1 must have dimensions (m x n) and M2 must have dimensions (n x p).
Result M3 has dimensions (m x p).
*/
void mm_(pEnv env)
{
    Index mat1, mat2;
    int rows1, cols1, rows2, cols2, i, j, k;
    double *a, *b, *result;

    TWOPARAMS("mm");
    LIST("mm");
    LIST2("mm");

    mat2 = nodevalue(env->stck).lis;
    mat1 = nodevalue(nextnode1(env->stck)).lis;

    if (check_matrix(env, mat1, &rows1, &cols1, "mm") < 0) return;
    if (check_matrix(env, mat2, &rows2, &cols2, "mm") < 0) return;

    if (cols1 != rows2) {
        execerror(env, "compatible matrix dimensions for multiplication", "mm");
        return;
    }

    if (rows1 == 0 || cols2 == 0) {
        BINARY(LIST_NEWNODE, 0);
        return;
    }

    a = malloc(rows1 * cols1 * sizeof(double));
    b = malloc(rows2 * cols2 * sizeof(double));
    result = calloc(rows1 * cols2, sizeof(double));

    extract_matrix(env, mat1, a, rows1, cols1);
    extract_matrix(env, mat2, b, rows2, cols2);

#ifdef JOY_BLAS
    if (rows1 >= BLAS_THRESHOLD || cols1 >= BLAS_THRESHOLD || cols2 >= BLAS_THRESHOLD) {
        /* Use BLAS: C = 1.0 * A * B + 0.0 * C */
        cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
                    rows1, cols2, cols1,    /* m, n, k */
                    1.0, a, cols1,          /* alpha, A, lda */
                    b, cols2,               /* B, ldb */
                    0.0, result, cols2);    /* beta, C, ldc */
    } else
#endif
    {
        /* Fallback: manual matrix multiplication */
        for (i = 0; i < rows1; i++) {
            for (j = 0; j < cols2; j++) {
                double sum = 0.0;
                #pragma omp simd reduction(+:sum)
                for (k = 0; k < cols1; k++) {
                    sum += a[i * cols1 + k] * b[k * cols2 + j];
                }
                result[i * cols2 + j] = sum;
            }
        }
    }

    BINARY(LIST_NEWNODE, build_matrix(env, result, rows1, cols2));

    free(a);
    free(b);
    free(result);
}

/**
Q0  OK  3660  mv  :  M V  ->  V2
Vector V2 is the matrix-vector product of matrix M and vector V.
M must have dimensions (m x n) and V must have length n.
Result V2 has length m.
*/
void mv_(pEnv env)
{
    Index mat, vec;
    int rows, cols, vec_len, i, k;
    double *m, *v, *result;

    TWOPARAMS("mv");
    LIST("mv");
    LIST2("mv");

    vec = nodevalue(env->stck).lis;
    mat = nodevalue(nextnode1(env->stck)).lis;

    if (check_matrix(env, mat, &rows, &cols, "mv") < 0) return;
    vec_len = check_numeric_list(env, vec, "mv");
    if (vec_len < 0) return;

    if (cols != vec_len) {
        execerror(env, "matrix columns equal to vector length", "mv");
        return;
    }

    if (rows == 0) {
        BINARY(LIST_NEWNODE, 0);
        return;
    }

    m = malloc(rows * cols * sizeof(double));
    v = malloc(vec_len * sizeof(double));
    result = calloc(rows, sizeof(double));

    extract_matrix(env, mat, m, rows, cols);
    extract_values(env, vec, v, vec_len);

    /* Matrix-vector multiplication: y[i] = sum(M[i][k] * v[k]) */
    for (i = 0; i < rows; i++) {
        double sum = 0.0;
        #pragma omp simd reduction(+:sum)
        for (k = 0; k < cols; k++) {
            sum += m[i * cols + k] * v[k];
        }
        result[i] = sum;
    }

    BINARY(LIST_NEWNODE, build_float_list(env, result, rows));

    free(m);
    free(v);
    free(result);
}

/**
Q0  OK  3670  transpose  :  M  ->  M2
Matrix M2 is the transpose of matrix M.
If M has dimensions (m x n), M2 has dimensions (n x m).
*/
void transpose_(pEnv env)
{
    Index mat;
    int rows, cols, i, j;
    double *m, *result;

    ONEPARAM("transpose");
    LIST("transpose");

    mat = nodevalue(env->stck).lis;
    if (check_matrix(env, mat, &rows, &cols, "transpose") < 0) return;

    if (rows == 0 || cols == 0) {
        UNARY(LIST_NEWNODE, 0);
        return;
    }

    m = malloc(rows * cols * sizeof(double));
    result = malloc(cols * rows * sizeof(double));

    extract_matrix(env, mat, m, rows, cols);

    /* Transpose: result[j][i] = m[i][j] */
    for (i = 0; i < rows; i++) {
        for (j = 0; j < cols; j++) {
            result[j * rows + i] = m[i * cols + j];
        }
    }

    UNARY(LIST_NEWNODE, build_matrix(env, result, cols, rows));

    free(m);
    free(result);
}

/* ========== Matrix properties ========== */

/**
Q0  OK  3680  trace  :  M  ->  N
N is the trace (sum of diagonal elements) of square matrix M.
*/
void trace_(pEnv env)
{
    Index mat;
    int rows, cols, i;
    double *m;
    double result = 0.0;

    ONEPARAM("trace");
    LIST("trace");

    mat = nodevalue(env->stck).lis;
    if (check_matrix(env, mat, &rows, &cols, "trace") < 0) return;

    if (rows != cols) {
        execerror(env, "square matrix", "trace");
        return;
    }

    if (rows == 0) {
        UNARY(FLOAT_NEWNODE, 0.0);
        return;
    }

    m = malloc(rows * cols * sizeof(double));
    extract_matrix(env, mat, m, rows, cols);

    for (i = 0; i < rows; i++) {
        result += m[i * cols + i];
    }

    UNARY(FLOAT_NEWNODE, result);

    free(m);
}

/*
 * Helper: Compute determinant of a square matrix using LU decomposition.
 * Modifies the input matrix in place.
 * Returns the determinant.
 */
static double compute_determinant(double* m, int n)
{
    int i, j, k, max_row;
    double det = 1.0;
    double temp;

    for (i = 0; i < n; i++) {
        /* Find pivot */
        max_row = i;
        for (k = i + 1; k < n; k++) {
            if (fabs(m[k * n + i]) > fabs(m[max_row * n + i])) {
                max_row = k;
            }
        }

        /* Swap rows if needed */
        if (max_row != i) {
            for (j = 0; j < n; j++) {
                temp = m[i * n + j];
                m[i * n + j] = m[max_row * n + j];
                m[max_row * n + j] = temp;
            }
            det = -det;  /* Row swap changes sign */
        }

        /* Check for zero pivot */
        if (fabs(m[i * n + i]) < 1e-15) {
            return 0.0;  /* Singular matrix */
        }

        det *= m[i * n + i];

        /* Eliminate below */
        for (k = i + 1; k < n; k++) {
            double factor = m[k * n + i] / m[i * n + i];
            for (j = i; j < n; j++) {
                m[k * n + j] -= factor * m[i * n + j];
            }
        }
    }

    return det;
}

/**
Q0  OK  3690  det  :  M  ->  N
N is the determinant of square matrix M.
*/
void det_(pEnv env)
{
    Index mat;
    int rows, cols, size;
    double *m;
    double result;

    ONEPARAM("det");
    LIST("det");

    mat = nodevalue(env->stck).lis;
    if (check_matrix(env, mat, &rows, &cols, "det") < 0) return;

    if (rows != cols) {
        execerror(env, "square matrix", "det");
        return;
    }

    if (rows == 0) {
        UNARY(FLOAT_NEWNODE, 1.0);  /* det of empty matrix is 1 by convention */
        return;
    }

    size = rows * cols;
    m = malloc(size * sizeof(double));
    extract_matrix(env, mat, m, rows, cols);

    result = compute_determinant(m, rows);

    UNARY(FLOAT_NEWNODE, result);

    free(m);
}

/*
 * Helper: Compute matrix inverse using Gauss-Jordan elimination.
 * Returns 0 on success, -1 if matrix is singular.
 */
static int compute_inverse(double* m, double* inv, int n)
{
    int i, j, k, max_row;
    double temp, factor;

    /* Initialize inverse as identity */
    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            inv[i * n + j] = (i == j) ? 1.0 : 0.0;
        }
    }

    /* Forward elimination */
    for (i = 0; i < n; i++) {
        /* Find pivot */
        max_row = i;
        for (k = i + 1; k < n; k++) {
            if (fabs(m[k * n + i]) > fabs(m[max_row * n + i])) {
                max_row = k;
            }
        }

        /* Swap rows */
        if (max_row != i) {
            for (j = 0; j < n; j++) {
                temp = m[i * n + j];
                m[i * n + j] = m[max_row * n + j];
                m[max_row * n + j] = temp;

                temp = inv[i * n + j];
                inv[i * n + j] = inv[max_row * n + j];
                inv[max_row * n + j] = temp;
            }
        }

        /* Check for zero pivot */
        if (fabs(m[i * n + i]) < 1e-15) {
            return -1;  /* Singular matrix */
        }

        /* Scale pivot row */
        factor = m[i * n + i];
        for (j = 0; j < n; j++) {
            m[i * n + j] /= factor;
            inv[i * n + j] /= factor;
        }

        /* Eliminate column */
        for (k = 0; k < n; k++) {
            if (k != i) {
                factor = m[k * n + i];
                for (j = 0; j < n; j++) {
                    m[k * n + j] -= factor * m[i * n + j];
                    inv[k * n + j] -= factor * inv[i * n + j];
                }
            }
        }
    }

    return 0;
}

/**
Q0  OK  3700  inv  :  M  ->  M2
Matrix M2 is the inverse of square matrix M.
Error if M is singular (non-invertible).
*/
void inv_(pEnv env)
{
    Index mat;
    int rows, cols, size;
    double *m, *result;

    ONEPARAM("inv");
    LIST("inv");

    mat = nodevalue(env->stck).lis;
    if (check_matrix(env, mat, &rows, &cols, "inv") < 0) return;

    if (rows != cols) {
        execerror(env, "square matrix", "inv");
        return;
    }

    if (rows == 0) {
        UNARY(LIST_NEWNODE, 0);
        return;
    }

    size = rows * cols;
    m = malloc(size * sizeof(double));
    result = malloc(size * sizeof(double));

    extract_matrix(env, mat, m, rows, cols);

    if (compute_inverse(m, result, rows) < 0) {
        free(m);
        free(result);
        execerror(env, "non-singular matrix", "inv");
        return;
    }

    UNARY(LIST_NEWNODE, build_matrix(env, result, rows, cols));

    free(m);
    free(result);
}

/* ========== Matrix creation ========== */

/**
Q0  OK  3710  meye  :  N  ->  M
M is an N x N identity matrix.
*/
void meye_(pEnv env)
{
    int64_t n;
    int i, size;
    double *result;

    ONEPARAM("meye");
    POSITIVEINDEX(env->stck, "meye");

    n = nodevalue(env->stck).num;

    if (n == 0) {
        UNARY(LIST_NEWNODE, 0);
        return;
    }

    size = (int)(n * n);
    result = calloc(size, sizeof(double));

    for (i = 0; i < n; i++) {
        result[i * n + i] = 1.0;
    }

    UNARY(LIST_NEWNODE, build_matrix(env, result, (int)n, (int)n));

    free(result);
}

#ifdef JOY_NATIVE_TYPES
/* ========== Native Vector/Matrix Type Support ========== */

/**
Q0  OK  3720  vector?\0vector_p  :  X  ->  B
[NATIVE] B is true if X is a native vector, false otherwise.
*/
void vector_p_(pEnv env)
{
    ONEPARAM("vector?");
    UNARY(BOOLEAN_NEWNODE, nodetype(env->stck) == VECTOR_);
}

/**
Q0  OK  3730  matrix?\0matrix_p  :  X  ->  B
[NATIVE] B is true if X is a native matrix, false otherwise.
*/
void matrix_p_(pEnv env)
{
    ONEPARAM("matrix?");
    UNARY(BOOLEAN_NEWNODE, nodetype(env->stck) == MATRIX_);
}

/**
Q0  OK  3740  >vec\0tovec  :  L  ->  V
[NATIVE] Converts numeric list L to a native contiguous vector V.
*/
void tovec_(pEnv env)
{
    Index list;
    int len, i;
    VectorData* vec;
    int ok;
    Index node;

    ONEPARAM(">vec");

    /* If already a VECTOR_, just return it */
    if (nodetype(env->stck) == VECTOR_) {
        return;
    }

    LIST(">vec");
    list = nodevalue(env->stck).lis;
    len = check_numeric_list(env, list, ">vec");
    if (len < 0) return;

    /* Allocate VectorData with flexible array */
    vec = GC_CTX_MALLOC_ATOMIC(env, sizeof(VectorData) + len * sizeof(double));
    vec->len = len;

    /* Extract values */
    for (i = 0, node = list; node && i < len; node = nextnode1(node), i++) {
        vec->data[i] = get_numeric(env, node, &ok);
    }

    UNARY(VECTOR_NEWNODE, vec);
}

/**
Q0  OK  3750  >mat\0tomat  :  M  ->  MATRIX
[NATIVE] Converts matrix M (list of numeric lists) to a native contiguous matrix.
*/
void tomat_(pEnv env)
{
    Index mat;
    int rows, cols, r, c;
    MatrixData* result;
    Index row_node, cell_node;
    int ok;

    ONEPARAM(">mat");

    /* If already a MATRIX_, just return it */
    if (nodetype(env->stck) == MATRIX_) {
        return;
    }

    LIST(">mat");
    mat = nodevalue(env->stck).lis;

    if (check_matrix(env, mat, &rows, &cols, ">mat") < 0) return;

    /* Allocate MatrixData with flexible array */
    result = GC_CTX_MALLOC_ATOMIC(env, sizeof(MatrixData) + rows * cols * sizeof(double));
    result->rows = rows;
    result->cols = cols;

    /* Extract values row by row */
    r = 0;
    for (row_node = mat; row_node && r < rows; row_node = nextnode1(row_node), r++) {
        Index row = nodevalue(row_node).lis;
        c = 0;
        for (cell_node = row; cell_node && c < cols; cell_node = nextnode1(cell_node), c++) {
            result->data[r * cols + c] = get_numeric(env, cell_node, &ok);
        }
    }

    UNARY(MATRIX_NEWNODE, result);
}

/**
Q0  OK  3760  >list\0tolist  :  X  ->  L
[NATIVE] Converts native vector or matrix X back to a linked list.
*/
void tolist_(pEnv env)
{
    ONEPARAM(">list");

    if (nodetype(env->stck) == VECTOR_) {
        VectorData* vec = nodevalue(env->stck).vec;
        if (!vec || vec->len == 0) {
            UNARY(LIST_NEWNODE, 0);
            return;
        }
        UNARY(LIST_NEWNODE, build_float_list(env, vec->data, vec->len));
        return;
    }

    if (nodetype(env->stck) == MATRIX_) {
        MatrixData* mat = nodevalue(env->stck).mat;
        if (!mat || mat->rows == 0 || mat->cols == 0) {
            UNARY(LIST_NEWNODE, 0);
            return;
        }
        UNARY(LIST_NEWNODE, build_matrix(env, mat->data, mat->rows, mat->cols));
        return;
    }

    /* Already a list, return as-is */
    if (nodetype(env->stck) == LIST_) {
        return;
    }

    execerror(env, "vector, matrix, or list", ">list");
}

/* ========== Native BLAS operations ========== */

/**
Q0  OK  3770  ndot  :  V1 V2  ->  N
[NATIVE] Dot product of two native vectors. Uses BLAS cblas_ddot when available.
*/
void ndot_(pEnv env)
{
    VectorData *v1, *v2;
    double result = 0.0;
    int i;

    TWOPARAMS("ndot");

    if (nodetype(env->stck) != VECTOR_ || nodetype(nextnode1(env->stck)) != VECTOR_) {
        execerror(env, "two native vectors", "ndot");
        return;
    }

    v2 = nodevalue(env->stck).vec;
    v1 = nodevalue(nextnode1(env->stck)).vec;

    if (!v1 || !v2 || v1->len != v2->len) {
        execerror(env, "vectors of equal length", "ndot");
        return;
    }

    if (v1->len == 0) {
        BINARY(FLOAT_NEWNODE, 0.0);
        return;
    }

#ifdef JOY_BLAS
    result = cblas_ddot(v1->len, v1->data, 1, v2->data, 1);
#else
    #pragma omp simd reduction(+:result)
    for (i = 0; i < v1->len; i++) {
        result += v1->data[i] * v2->data[i];
    }
#endif

    BINARY(FLOAT_NEWNODE, result);
}

/**
Q0  OK  3780  nmv  :  M V  ->  V2
[NATIVE] Native matrix-vector multiply. Uses BLAS cblas_dgemv when available.
M must be a native matrix, V must be a native vector.
*/
void nmv_(pEnv env)
{
    MatrixData* mat;
    VectorData* vec;
    VectorData* result;
    int i, k;

    TWOPARAMS("nmv");

    if (nodetype(env->stck) != VECTOR_) {
        execerror(env, "native vector as first parameter", "nmv");
        return;
    }
    if (nodetype(nextnode1(env->stck)) != MATRIX_) {
        execerror(env, "native matrix as second parameter", "nmv");
        return;
    }

    vec = nodevalue(env->stck).vec;
    mat = nodevalue(nextnode1(env->stck)).mat;

    if (!mat || !vec || mat->cols != vec->len) {
        execerror(env, "matrix columns equal to vector length", "nmv");
        return;
    }

    if (mat->rows == 0) {
        result = GC_CTX_MALLOC_ATOMIC(env, sizeof(VectorData));
        result->len = 0;
        BINARY(VECTOR_NEWNODE, result);
        return;
    }

    result = GC_CTX_MALLOC_ATOMIC(env, sizeof(VectorData) + mat->rows * sizeof(double));
    result->len = mat->rows;

#ifdef JOY_BLAS
    cblas_dgemv(CblasRowMajor, CblasNoTrans,
                mat->rows, mat->cols,
                1.0, mat->data, mat->cols,
                vec->data, 1,
                0.0, result->data, 1);
#else
    for (i = 0; i < mat->rows; i++) {
        double sum = 0.0;
        #pragma omp simd reduction(+:sum)
        for (k = 0; k < mat->cols; k++) {
            sum += mat->data[i * mat->cols + k] * vec->data[k];
        }
        result->data[i] = sum;
    }
#endif

    BINARY(VECTOR_NEWNODE, result);
}

/**
Q0  OK  3790  nmm  :  M1 M2  ->  M3
[NATIVE] Native matrix-matrix multiply. Uses BLAS cblas_dgemm when available.
Both M1 and M2 must be native matrices.
*/
void nmm_(pEnv env)
{
    MatrixData *m1, *m2, *result;
    int i, j, k;

    TWOPARAMS("nmm");

    if (nodetype(env->stck) != MATRIX_ || nodetype(nextnode1(env->stck)) != MATRIX_) {
        execerror(env, "two native matrices", "nmm");
        return;
    }

    m2 = nodevalue(env->stck).mat;
    m1 = nodevalue(nextnode1(env->stck)).mat;

    if (!m1 || !m2 || m1->cols != m2->rows) {
        execerror(env, "compatible matrix dimensions", "nmm");
        return;
    }

    if (m1->rows == 0 || m2->cols == 0) {
        result = GC_CTX_MALLOC_ATOMIC(env, sizeof(MatrixData));
        result->rows = m1->rows;
        result->cols = m2->cols;
        BINARY(MATRIX_NEWNODE, result);
        return;
    }

    result = GC_CTX_MALLOC_ATOMIC(env, sizeof(MatrixData) + m1->rows * m2->cols * sizeof(double));
    result->rows = m1->rows;
    result->cols = m2->cols;

#ifdef JOY_BLAS
    cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
                m1->rows, m2->cols, m1->cols,
                1.0, m1->data, m1->cols,
                m2->data, m2->cols,
                0.0, result->data, m2->cols);
#else
    /* Initialize result to zero */
    for (i = 0; i < m1->rows * m2->cols; i++) {
        result->data[i] = 0.0;
    }
    /* Manual matrix multiplication */
    for (i = 0; i < m1->rows; i++) {
        for (j = 0; j < m2->cols; j++) {
            double sum = 0.0;
            #pragma omp simd reduction(+:sum)
            for (k = 0; k < m1->cols; k++) {
                sum += m1->data[i * m1->cols + k] * m2->data[k * m2->cols + j];
            }
            result->data[i * m2->cols + j] = sum;
        }
    }
#endif

    BINARY(MATRIX_NEWNODE, result);
}

/* ========== Native creation functions ========== */

/**
Q0  OK  3800  nvzeros  :  N  ->  V
[NATIVE] V is a native vector of N zeros.
*/
void nvzeros_(pEnv env)
{
    int64_t n;
    VectorData* vec;
    int i;

    ONEPARAM("nvzeros");
    POSITIVEINDEX(env->stck, "nvzeros");

    n = nodevalue(env->stck).num;

    vec = GC_CTX_MALLOC_ATOMIC(env, sizeof(VectorData) + n * sizeof(double));
    vec->len = (int)n;

    for (i = 0; i < n; i++) {
        vec->data[i] = 0.0;
    }

    UNARY(VECTOR_NEWNODE, vec);
}

/**
Q0  OK  3810  nvones  :  N  ->  V
[NATIVE] V is a native vector of N ones.
*/
void nvones_(pEnv env)
{
    int64_t n;
    VectorData* vec;
    int i;

    ONEPARAM("nvones");
    POSITIVEINDEX(env->stck, "nvones");

    n = nodevalue(env->stck).num;

    vec = GC_CTX_MALLOC_ATOMIC(env, sizeof(VectorData) + n * sizeof(double));
    vec->len = (int)n;

    for (i = 0; i < n; i++) {
        vec->data[i] = 1.0;
    }

    UNARY(VECTOR_NEWNODE, vec);
}

/**
Q0  OK  3820  nmzeros  :  R C  ->  M
[NATIVE] M is a native R x C matrix of zeros.
*/
void nmzeros_(pEnv env)
{
    int64_t r, c;
    int size, i;
    MatrixData* mat;

    TWOPARAMS("nmzeros");
    POSITIVEINDEX(env->stck, "nmzeros");
    POSITIVEINDEX(nextnode1(env->stck), "nmzeros");

    c = nodevalue(env->stck).num;
    r = nodevalue(nextnode1(env->stck)).num;

    size = (int)(r * c);
    mat = GC_CTX_MALLOC_ATOMIC(env, sizeof(MatrixData) + size * sizeof(double));
    mat->rows = (int)r;
    mat->cols = (int)c;

    for (i = 0; i < size; i++) {
        mat->data[i] = 0.0;
    }

    BINARY(MATRIX_NEWNODE, mat);
}

/**
Q0  OK  3830  nmones  :  R C  ->  M
[NATIVE] M is a native R x C matrix of ones.
*/
void nmones_(pEnv env)
{
    int64_t r, c;
    int size, i;
    MatrixData* mat;

    TWOPARAMS("nmones");
    POSITIVEINDEX(env->stck, "nmones");
    POSITIVEINDEX(nextnode1(env->stck), "nmones");

    c = nodevalue(env->stck).num;
    r = nodevalue(nextnode1(env->stck)).num;

    size = (int)(r * c);
    mat = GC_CTX_MALLOC_ATOMIC(env, sizeof(MatrixData) + size * sizeof(double));
    mat->rows = (int)r;
    mat->cols = (int)c;

    for (i = 0; i < size; i++) {
        mat->data[i] = 1.0;
    }

    BINARY(MATRIX_NEWNODE, mat);
}

/**
Q0  OK  3840  nmeye  :  N  ->  M
[NATIVE] M is a native N x N identity matrix.
*/
void nmeye_(pEnv env)
{
    int64_t n;
    int size, i;
    MatrixData* mat;

    ONEPARAM("nmeye");
    POSITIVEINDEX(env->stck, "nmeye");

    n = nodevalue(env->stck).num;

    size = (int)(n * n);
    mat = GC_CTX_MALLOC_ATOMIC(env, sizeof(MatrixData) + size * sizeof(double));
    mat->rows = (int)n;
    mat->cols = (int)n;

    /* Initialize to zeros then set diagonal */
    for (i = 0; i < size; i++) {
        mat->data[i] = 0.0;
    }
    for (i = 0; i < n; i++) {
        mat->data[i * n + i] = 1.0;
    }

    UNARY(MATRIX_NEWNODE, mat);
}

#endif /* JOY_NATIVE_TYPES */
