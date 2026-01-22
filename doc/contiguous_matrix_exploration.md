# Exploration: Native Contiguous Matrix Type

**Status:** Exploratory design document (not scheduled for implementation)

## Problem Statement

Joy's current matrix representation (list of lists) creates a fundamental mismatch
with high-performance linear algebra libraries like BLAS:

| Aspect | Joy Lists | BLAS Requirement |
|--------|-----------|------------------|
| Storage | Linked list nodes scattered in heap | Contiguous row-major or column-major array |
| Access | O(n) to reach element n | O(1) random access |
| Conversion | O(n*m) to convert n x m matrix | Zero - native format |

### Measured Impact

Benchmarks show that for matrix-vector multiply (`mv`), list-to-array conversion
consumes ~50% of total execution time, completely negating BLAS benefits:

```
mv 500x500 without BLAS: 65ms
mv 500x500 with BLAS:    65ms  (no improvement)
```

Matrix multiply (`mm`) still benefits because it's O(n^3) while conversion is O(n^2):

```
mm 200x200 without BLAS: 40ms
mm 200x200 with BLAS:    5ms   (8x speedup)
```

## Design Options

### Option A: New Primitive Type `MATRIX_`

Add a new node type for contiguous matrices alongside existing types (INTEGER_,
FLOAT_, LIST_, STRING_, etc.).

**Node structure:**
```c
typedef struct {
    int rows;
    int cols;
    double* data;  /* Contiguous row-major array */
} Matrix;

/* In Types union: */
typedef union {
    /* ... existing types ... */
    Matrix* mat;  /* New matrix type */
} Types;
```

**Syntax options:**
```joy
(* Option A1: Prefix literal syntax - RECOMMENDED *)
m[[1 2 3] [4 5 6]]          (* 2x3 matrix - lowercase, consistent with Joy *)
M[[1 2 3] [4 5 6]]          (* 2x3 matrix - uppercase, more distinct *)

(* Option A2: Special character prefix *)
#[[1 2 3] [4 5 6]]          (* 2x3 matrix - requires new token *)

(* Option A3: Constructor function *)
[[1 2 3] [4 5 6]] matrix    (* Convert list-of-lists to matrix *)

(* Option A4: Creation functions only *)
3 4 mzeros                   (* 3x4 zero matrix *)
2 3 1.0 mfill               (* 2x3 matrix filled with 1.0 *)
```

**Recommended: `m[[...]]` syntax**

The `m[` prefix is clean, intuitive, and easy to parse:
- Scanner recognizes `m[` as MATRIX_LITERAL token start
- Rows are standard Joy lists inside
- Consistent with Joy's lowercase conventions
- Clear visual distinction from regular lists

Example usage:
```joy
m[[1 2] [3 4]] m[[5 6] [7 8]] mm .   (* Matrix multiply *)
m[[1 2 3] [4 5 6]] [1 2 3] mv .      (* Matrix-vector, vector stays as list *)
m[[1 2] [3 4]] transpose .            (* Returns matrix *)
m[[1 2] [3 4]] det .                  (* Returns scalar *)
```

**Vector literal syntax: `v[...]`**

For consistency with `m[[...]]`, vectors should also have a contiguous literal:
```joy
v[1 2 3 4 5]                (* Contiguous vector *)
```

This enables full BLAS performance across all operations:
```joy
v[1 2 3] v[4 5 6] dot .              (* cblas_ddot, no conversion *)
m[[1 2] [3 4]] v[5 6] mv .           (* cblas_dgemv, both contiguous *)
v[1 2 3 4 5] vnorm .                 (* cblas_dnrm2 directly *)
v[1 2 3] 2.0 vscale .                (* cblas_dscal directly *)
```

**Interoperability with lists:**
```joy
v[1 2 3] >list .                     (* -> [1.0 2.0 3.0] *)
[1 2 3] >vec .                       (* -> v[1 2 3] *)
m[[1 2] [3 4]] >list .               (* -> [[1.0 2.0] [3.0 4.0]] *)
[[1 2] [3 4]] >mat .                 (* -> m[[1 2] [3 4]] *)
```

**Type summary:**
| Literal | Type | Storage | Use case |
|---------|------|---------|----------|
| `[1 2 3]` | LIST_ | Linked nodes | General data, small vectors |
| `v[1 2 3]` | VECTOR_ | Contiguous double[] | Numeric vectors, BLAS Level 1 |
| `m[[1 2] [3 4]]` | MATRIX_ | Contiguous row-major double[] | Matrices, BLAS Level 2/3 |

**Pros:**
- Zero conversion overhead for BLAS operations
- O(1) element access
- Memory-efficient (no node overhead per element)
- Cache-friendly for numerical operations

**Cons:**
- New type breaks Joy's simplicity (currently only ~6 types)
- Requires new operators or overloading existing ones
- Garbage collection complexity (must free data array)
- Interop with existing list operations unclear

### Option B: Tagged Homogeneous Lists

Keep the LIST_ type but add a flag indicating the list is a contiguous numeric array
internally.

**Node structure:**
```c
typedef struct {
    Operator op;       /* LIST_ */
    unsigned char flags;  /* New: CONTIGUOUS_NUMERIC flag */
    union {
        Index lis;     /* Traditional linked list */
        struct {       /* When CONTIGUOUS_NUMERIC set */
            int len;
            double* data;
        } vec;
    } u;
} Node;
```

**Behavior:**
- Lists of numbers automatically stored contiguously
- Accessing elements converts to Index-based traversal API
- BLAS operations detect flag and use data directly
- Modification (cons, append) may trigger copy-out to linked form

**Pros:**
- Backward compatible (existing code works)
- Automatic optimization (no syntax changes)
- Single type system

**Cons:**
- Complex implementation (dual storage modes)
- Copy-on-modify overhead
- Only works for 1D vectors, not 2D matrices directly

### Option C: Opaque Numeric Array Type (NOT RECOMMENDED)

Add a completely opaque `ARRAY_` type that's only accessible through specific
operators, not general list operations.

```joy
(* Creation *)
[1 2 3 4 5 6] 2 3 reshape    (* Create 2x3 array from flat list *)
2 3 zeros                     (* 2x3 zero array *)
3 eye                         (* 3x3 identity *)
```

**Critical flaw:** The `reshape` approach creates a list first, then converts to
array - paying the exact conversion overhead we're trying to avoid. This defeats
the purpose of having a contiguous type.

Compare to literal syntax (Option A):
```joy
m[[1 2 3] [4 5 6]]           (* Parser creates contiguous array DIRECTLY *)
```

With literals, the scanner/parser allocates contiguous storage immediately from
the tokens - no intermediate list, no conversion overhead.

```joy
(* Continued Option C syntax...

(* Access - returns Joy values *)
arr 0 1 @                     (* Get element at row 0, col 1 *)
arr 0 row@                    (* Get row 0 as Joy list *)

(* Operations - stay in array domain *)
arr1 arr2 @+                  (* Element-wise add, returns array *)
arr1 arr2 @mm                 (* Matrix multiply, returns array *)

(* Conversion *)
arr >list                     (* Convert to list-of-lists *)
[[1 2] [3 4]] >array          (* Convert from list-of-lists *)
```

**Pros:**
- Clean separation (arrays for numerics, lists for general data)
- Full BLAS performance
- No backward compatibility issues

**Cons:**
- Parallel type system (lists vs arrays)
- Users must explicitly choose representation
- Many new operators needed

### Option D: External Library Integration

Keep Joy's type system unchanged. Provide FFI to external matrix libraries
(NumPy-style) where matrices live outside Joy's heap.

```joy
(* Hypothetical numpy-style integration *)
"numpy" foreign-library
[[1 2] [3 4]] np-array       (* Create numpy array *)
arr1 arr2 np-dot             (* Call numpy's dot product *)
arr np-to-list               (* Extract back to Joy *)
```

**Pros:**
- No changes to Joy core
- Leverage existing optimized libraries
- Matrices can be arbitrarily large (not limited by Joy heap)

**Cons:**
- FFI complexity
- External dependency
- Data lives in two worlds

## Recommended Approach

**Option A with literal syntax (`v[...]` and `m[[...]]`)** is the clear winner:

1. **Zero conversion overhead** - Parser creates contiguous storage directly
2. **Clean syntax** - `m[[1 2] [3 4]]` is intuitive and Joy-like (lowercase)
3. **Full BLAS performance** - No intermediate representations
4. **Explicit types** - User knows exactly what they're getting
5. **Backward compatible** - Lists unchanged, new syntax is additive

Option C's `reshape` approach fails because it creates a list first, then converts - paying the exact overhead we're trying to eliminate.

### Minimal Viable Implementation

A minimal implementation would add:

**Types:**
- `VECTOR_` node type with `{int len; double* data}`
- `MATRIX_` node type with `{int rows; int cols; double* data}`

**Literal syntax:**
- `v[1 2 3]` - contiguous vector
- `m[[1 2] [3 4]]` - contiguous matrix (row-major)

**Conversion operators:**
- `>vec` - convert list to vector
- `>mat` - convert list-of-lists to matrix
- `>list` - convert vector or matrix to list(s)

**Vector operators (BLAS Level 1):**
- `dot` - dot product (cblas_ddot)
- `vnorm` - Euclidean norm (cblas_dnrm2)
- `vscale` - scale vector (cblas_dscal)
- `v+`, `v-`, `v*`, `v/` - element-wise (cblas_daxpy or loop)

**Matrix operators (BLAS Level 2/3):**
- `mm` - matrix multiply (cblas_dgemm)
- `mv` - matrix-vector multiply (cblas_dgemv)
- `transpose` - transpose matrix
- `det`, `inv`, `trace` - matrix properties

**Creation:**
- `vzeros`, `vones` - vector constructors
- `mzeros`, `mones`, `meye` - matrix constructors

### Memory Management Considerations

Arrays would need special GC handling:

```c
/* In gc copy function */
if (op == ARRAY_) {
    /* Deep copy the data array */
    Array* old_arr = env->old_memory[n].u.arr;
    Array* new_arr = malloc(sizeof(Array));
    new_arr->ndim = old_arr->ndim;
    new_arr->shape = malloc(old_arr->ndim * sizeof(int));
    memcpy(new_arr->shape, old_arr->shape, old_arr->ndim * sizeof(int));

    int size = 1;
    for (int i = 0; i < old_arr->ndim; i++) size *= old_arr->shape[i];

    new_arr->data = malloc(size * sizeof(double));
    memcpy(new_arr->data, old_arr->data, size * sizeof(double));

    env->memory[temp].u.arr = new_arr;
}
```

## Performance Projections

With native contiguous arrays, expected performance:

| Operation | Current (lists) | With Arrays | Speedup |
|-----------|-----------------|-------------|---------|
| mm 100x100 | 21ms | ~2ms | ~10x |
| mm 500x500 | ~500ms | ~10ms | ~50x |
| mv 100x100 | 12ms | ~1ms | ~12x |
| mv 500x500 | 65ms | ~3ms | ~20x |
| dot 10000 | <1ms | <0.1ms | ~10x |

These projections assume BLAS overhead dominates, which is true for larger sizes.

## Conclusion

A native contiguous array type would unlock significant performance gains for numerical computing in Joy. However, it adds complexity to Joy's elegantly simple type system.

**Recommendation:** Consider implementing if Joy is to be used for serious numerical work. For general-purpose use, the current BLAS integration for `mm` (8x speedup) may be sufficient.

## References

- [BLAS Quick Reference](https://www.netlib.org/blas/blasqr.pdf)
- [Apple Accelerate Framework](https://developer.apple.com/documentation/accelerate)
- [OpenBLAS](https://www.openblas.net/)
- [NumPy Array Interface](https://numpy.org/doc/stable/reference/arrays.interface.html)
