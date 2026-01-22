# Vector and Matrix Operations Implementation

This document describes the design and implementation of vectorized and matrix operators in Joy.

## Design Philosophy

The vector and matrix operations follow **Approach A** from [doc/vector.md](vector.md): explicit operators with `v`-prefixes for vectors and `m`-prefixes for matrices. This approach was chosen because:

1. **Explicit is better than implicit** - No magic behavior changes for existing operators
2. **Backward compatible** - Existing code continues to work unchanged
3. **Clear semantics** - Users know exactly when vectorization is happening
4. **Simple implementation** - No type inference or automatic broadcasting

## Implementation Overview

All operations are implemented in `src/builtin/vector.c`. The implementation uses simple loops, with proper error handling for type mismatches and dimension errors.

### Architecture

```
src/builtin/vector.c
    |
    +-- Vector helpers (internal)
    |   +-- get_numeric()         - Extract numeric value from node
    |   +-- check_numeric_list()  - Validate list contains only numbers
    |   +-- extract_values()      - Copy list elements to C array
    |   +-- build_float_list()    - Build result list from doubles
    |   +-- build_int_list()      - Build result list from integers
    |
    +-- Vector element-wise
    |   +-- vplus_(), vminus_(), vmul_(), vdiv_()
    |
    +-- Vector scalar/linear algebra
    |   +-- vscale_(), dot_(), cross_()
    |
    +-- Vector reductions
    |   +-- vsum_(), vprod_(), vmin_(), vmax_(), vmean_()
    |
    +-- Vector norms
    |   +-- vnorm_(), vnormalize_()
    |
    +-- Vector creation
    |   +-- vzeros_(), vones_(), vrange_(), vlinspace_()
    |
    +-- Matrix helpers (internal)
    |   +-- check_matrix()        - Validate list of lists structure
    |   +-- extract_matrix()      - Copy matrix to row-major C array
    |   +-- build_matrix()        - Build list of lists from array
    |
    +-- Matrix element-wise
    |   +-- mplus_(), mminus_(), mmul_(), mdiv_()
    |
    +-- Matrix scalar
    |   +-- mscale_()
    |
    +-- Matrix linear algebra
    |   +-- mm_() (matmul), mv_() (matrix-vector), transpose_()
    |
    +-- Matrix properties
    |   +-- trace_(), det_(), inv_()
    |   +-- compute_determinant() - LU decomposition helper
    |   +-- compute_inverse()     - Gauss-Jordan elimination helper
    |
    +-- Matrix creation
        +-- meye_() (identity matrix)
```

## Helper Functions

### `get_numeric()`

Extracts a numeric value from a Joy node as a `double`:

```c
static double get_numeric(pEnv env, Index node, int* ok)
{
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
```

This function handles both `INTEGER_` and `FLOAT_` node types, returning the value as a double for uniform processing.

### `check_numeric_list()`

Validates that a list contains only numeric values and returns its length:

```c
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
```

Returns `-1` on error (with error message set), or the list length on success.

### `build_float_list()` / `build_int_list()`

Build Joy lists from C arrays. Results are always `FLOAT_` nodes for arithmetic operations (to preserve precision) and `INTEGER_` nodes for creation operations like `vzeros`.

```c
static Index build_float_list(pEnv env, double* values, int len)
{
    Index head = 0;
    Index tail = 0;
    Index node;

    for (int i = 0; i < len; i++) {
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
```

## Operation Categories

### Element-wise Operations (`v+`, `v-`, `v*`, `v/`)

Pattern for binary element-wise operations:

1. Validate two parameters exist (`TWOPARAMS`)
2. Validate both are lists (`LIST`, `LIST2`)
3. Check both lists contain only numeric values
4. Check lists have equal length
5. Extract values to C arrays
6. Perform element-wise operation
7. Build result list
8. Clean up temporary arrays

Example implementation (simplified):

```c
void vplus_(pEnv env)
{
    TWOPARAMS("v+");
    LIST("v+");
    LIST2("v+");

    Index list2 = nodevalue(env->stck).lis;
    Index list1 = nodevalue(nextnode1(env->stck)).lis;

    int len2 = check_numeric_list(env, list2, "v+");
    int len1 = check_numeric_list(env, list1, "v+");
    if (len1 < 0 || len2 < 0) return;

    if (len1 != len2) {
        execerror(env, "lists of equal length", "v+");
        return;
    }

    double *a = malloc(len1 * sizeof(double));
    double *b = malloc(len1 * sizeof(double));
    double *result = malloc(len1 * sizeof(double));

    extract_values(env, list1, a, len1);
    extract_values(env, list2, b, len1);

    for (int i = 0; i < len1; i++) {
        result[i] = a[i] + b[i];
    }

    BINARY(LIST_NEWNODE, build_float_list(env, result, len1));

    free(a); free(b); free(result);
}
```

### Scalar Operations (`vscale`)

Pattern:
1. Validate list and scalar parameters
2. Extract scalar value
3. Extract list values
4. Multiply each element by scalar
5. Build result list

### Reductions (`vsum`, `vprod`, `vmin`, `vmax`)

Reductions iterate through the list once without copying to arrays:

```c
void vsum_(pEnv env)
{
    ONEPARAM("vsum");
    LIST("vsum");

    Index list = nodevalue(env->stck).lis;
    double result = 0.0;

    for (Index node = list; node; node = nextnode1(node)) {
        int ok;
        double val = get_numeric(env, node, &ok);
        if (!ok) {
            execerror(env, "numeric list", "vsum");
            return;
        }
        result += val;
    }

    UNARY(FLOAT_NEWNODE, result);
}
```

### Creation Operations (`vzeros`, `vones`, `vrange`)

Creation operations build lists of integers:

```c
void vzeros_(pEnv env)
{
    ONEPARAM("vzeros");
    POSITIVEINDEX(env->stck, "vzeros");

    int64_t n = nodevalue(env->stck).num;
    int64_t* values = malloc(n * sizeof(int64_t));

    for (int64_t i = 0; i < n; i++) {
        values[i] = 0;
    }

    UNARY(LIST_NEWNODE, build_int_list(env, values, (int)n));
    free(values);
}
```

## Error Handling

All operations provide clear error messages:

| Error Condition | Message |
|-----------------|---------|
| Non-numeric element in list | `"numeric list needed for <op>"` |
| Lists of different lengths | `"lists of equal length needed for <op>"` |
| Empty list for min/max | `"non-empty list needed for <op>"` |
| Non-integer for creation | `"non-negative integer needed for <op>"` |
| Invalid matrix structure | `"matrix (list of lists) needed for <op>"` |
| Non-uniform row lengths | `"matrix with uniform row lengths needed for <op>"` |
| Incompatible dimensions | `"compatible matrix dimensions needed for <op>"` |
| Non-square matrix | `"square matrix needed for <op>"` |
| Singular matrix | `"non-singular matrix needed for <op>"` |

Example error outputs:

```
run time error: lists of equal length needed for v+
run time error: numeric list needed for v*
run time error: non-empty list needed for vmin
run time error: square matrix needed for det
run time error: compatible matrix dimensions for multiplication needed for mm
```

## Type Handling

### Input Types

Operations accept both `INTEGER_` and `FLOAT_` node types in lists. Mixed lists are supported:

```joy
[1 2.0 3] [4.0 5 6.0] v+.  (* -> [5.0 7.0 9.0] *)
```

### Output Types

- **Element-wise and scalar operations**: Always produce `FLOAT_` results to preserve precision
- **Reductions**: Always produce `FLOAT_` results
- **Creation operations**: Produce `INTEGER_` results

## Performance Considerations

### Current Implementation

The current implementation prioritizes correctness and simplicity:

- Uses `malloc`/`free` for temporary arrays
- Simple sequential loops
- No SIMD vectorization

### Future Optimizations

Potential optimizations documented in [doc/vector.md](vector.md):

1. **Contiguous storage** - Detect homogeneous numeric lists and store as contiguous arrays
2. **SIMD vectorization** - Use `#pragma omp simd` or intrinsics for loops
3. **In-place operations** - Avoid allocation for some operations
4. **BLAS integration** - Link to optimized BLAS libraries for matrix operations

## Integration with Parallel Execution

Vector operations work seamlessly with `pmap`:

```joy
(* Process multiple vectors in parallel *)
[[1 2 3] [4 5 6] [7 8 9]] [[10 10 10] v+] pmap.
(* -> [[11.0 12.0 13.0] [14.0 15.0 16.0] [17.0 18.0 19.0]] *)

(* Parallel dot products *)
[[[1 2 3] [1 0 0]] [[1 2 3] [0 1 0]] [[1 2 3] [0 0 1]]] [i dot] pmap.
(* -> [1.0 2.0 3.0] *)
```

## Testing

### Vector Tests

Tests are in `tests/test2/vector.joy` with 28 assertions covering:

- Element-wise operations with various inputs
- Scalar operations with integer and float scalars
- Dot product including empty vectors
- All reduction operations including negative numbers
- Creation operations including edge cases (0-length)
- Mixed integer/float input handling

### Matrix Tests

Tests are in `tests/test2/matrix.joy` with 26 assertions covering:

- Element-wise matrix operations (m+, m-, m*, m/)
- Scalar operations (mscale)
- Matrix multiplication (mm) with various dimensions
- Matrix-vector product (mv)
- Transpose including non-square matrices
- Trace and determinant
- Matrix inverse with verification (M * inv(M) = I)
- Identity matrix creation (meye)

Run tests:

```bash
make test  # Runs all 182 tests including vector/matrix tests
./build/joy tests/test2/vector.joy  # Run vector tests directly
./build/joy tests/test2/matrix.joy  # Run matrix tests directly
```

## Operator Reference

### Vector Operators

| Operator | Stack Effect | Description |
|----------|--------------|-------------|
| `v+` | `V1 V2 -> V3` | Element-wise addition |
| `v-` | `V1 V2 -> V3` | Element-wise subtraction |
| `v*` | `V1 V2 -> V3` | Element-wise multiplication |
| `v/` | `V1 V2 -> V3` | Element-wise division |
| `vscale` | `V S -> V2` | Scalar multiplication |
| `dot` | `V1 V2 -> N` | Dot product |
| `vsum` | `V -> N` | Sum of elements |
| `vprod` | `V -> N` | Product of elements |
| `vmin` | `V -> N` | Minimum element |
| `vmax` | `V -> N` | Maximum element |
| `vnorm` | `V -> N` | Euclidean norm (magnitude) |
| `vnormalize` | `V -> V2` | Unit vector in direction of V |
| `vmean` | `V -> N` | Arithmetic mean of elements |
| `cross` | `V1 V2 -> V3` | Cross product (3D vectors only) |
| `vzeros` | `N -> V` | List of N zeros |
| `vones` | `N -> V` | List of N ones |
| `vrange` | `A B -> V` | List [A, A+1, ..., B] |
| `vlinspace` | `A B N -> V` | N linearly spaced values from A to B |

### Matrix Operators

| Operator | Stack Effect | Description |
|----------|--------------|-------------|
| `m+` | `M1 M2 -> M3` | Element-wise addition |
| `m-` | `M1 M2 -> M3` | Element-wise subtraction |
| `m*` | `M1 M2 -> M3` | Element-wise multiplication |
| `m/` | `M1 M2 -> M3` | Element-wise division |
| `mscale` | `M S -> M2` | Scalar multiplication |
| `mm` | `M1 M2 -> M3` | Matrix multiplication (m x n) * (n x p) -> (m x p) |
| `mv` | `M V -> V2` | Matrix-vector product (m x n) * (n) -> (m) |
| `transpose` | `M -> M2` | Transpose matrix (m x n) -> (n x m) |
| `trace` | `M -> N` | Sum of diagonal elements (square matrix) |
| `det` | `M -> N` | Determinant (square matrix) |
| `inv` | `M -> M2` | Matrix inverse (square, non-singular) |
| `meye` | `N -> M` | N x N identity matrix |

## Related Documentation

- [doc/vector.md](vector.md) - Design rationale and future directions
- [README.md](../README.md) - Quick reference and examples
- [CHANGELOG.md](../CHANGELOG.md) - Version history
