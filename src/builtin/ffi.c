/*
 *  module  : ffi.c
 *  version : 1.0
 *  date    : 11/01/26
 *
 *  Calls to C functions in shared libraries, through libffi. See doc/ffi.md.
 *  The builtins always exist; without JOY_FFI they raise an error.
 */
#include "globals.h"

#ifdef JOY_FFI
#include <dlfcn.h>
#include <ffi.h>

enum { FFI_MAX_ARGS = 16 };

typedef enum { K_INT, K_CHAR, K_BOOL, K_FLOAT, K_DOUBLE, K_STRING, K_POINTER } FfiKind;

typedef struct {
    const char* name;
    FfiKind kind;
    int is_signed;
    ffi_type* type;
} FfiType;

/* ffi_type objects are not constants on all platforms: filled in at first use */
static FfiType ffi_types[13];

static void init_ffi_types(void)
{
    const FfiType table[] = {
        { "int", K_INT, 1, &ffi_type_sint },
        { "uint", K_INT, 0, &ffi_type_uint },
        { "long", K_INT, 1, &ffi_type_slong },
        { "ulong", K_INT, 0, &ffi_type_ulong },
        { "int64", K_INT, 1, &ffi_type_sint64 },
        { "uint64", K_INT, 0, &ffi_type_uint64 },
        { "size_t", K_INT, 0, sizeof(size_t) == 8 ? &ffi_type_uint64 : &ffi_type_uint32 },
        { "char", K_CHAR, 1, &ffi_type_sint8 },
        { "bool", K_BOOL, 0, &ffi_type_uint8 },
        { "float", K_FLOAT, 1, &ffi_type_float },
        { "double", K_DOUBLE, 1, &ffi_type_double },
        { "string", K_STRING, 0, &ffi_type_pointer },
        { "pointer", K_POINTER, 0, &ffi_type_pointer },
    };

    memcpy(ffi_types, table, sizeof(table));
}

/* A signature's arg_types must stay valid: the cif refers to them */
typedef struct {
    ffi_cif cif;
    void (*fn)(void);
    const FfiType* args[FFI_MAX_ARGS];
    ffi_type* arg_types[FFI_MAX_ARGS];
    const FfiType* ret; /* NULL: void */
    int nargs;
} FfiFunction;

KHASH_MAP_INIT_STR(FfiCache, FfiFunction*)

/* Never freed or closed: returned pointers may refer into the libraries */
static khash_t(FfiCache)* ffi_cache;

/* Type words are symbols, or builtins for char, float and string. */
static const FfiType* parse_type(pEnv env, Index node)
{
    const char* name;

    if (nodetype(node) == USR_)
        name = vec_at(env->symtab, nodevalue(node).ent).name;
    else if (nodetype(node) == ANON_FUNCT_)
        name = opername(operindex(env, nodevalue(node).proc));
    else
        return NULL;
    for (size_t i = 0; i < sizeof(ffi_types) / sizeof(ffi_types[0]); i++)
        if (!strcmp(ffi_types[i].name, name))
            return &ffi_types[i];
    return NULL;
}

static void ffi_fail(pEnv env, const char* what, const char* detail, char* op)
{
    char msg[512];

    snprintf(msg, sizeof(msg), "%s (%s)", what, detail ? detail : "unknown");
    execerror(env, msg, op);
}

static FfiFunction* build_function(pEnv env, const char* lib, const char* sym,
                                   Index arg_list, Index ret_list, char* op)
{
    FfiFunction* f = calloc(1, sizeof(FfiFunction));
    void* handle;
    void* address;

    if (!f)
        execerror(env, "memory", op);
    for (Index n = arg_list; n; n = nextnode1(n)) {
        if (f->nargs == FFI_MAX_ARGS) {
            free(f);
            execerror(env, "at most 16 argument types", op);
        }
        f->args[f->nargs] = parse_type(env, n);
        if (!f->args[f->nargs]) {
            free(f);
            execerror(env, "known argument type", op);
        }
        f->arg_types[f->nargs] = f->args[f->nargs]->type;
        f->nargs++;
    }
    if (ret_list) {
        f->ret = parse_type(env, ret_list);
        if (!f->ret || nextnode1(ret_list)) {
            free(f);
            execerror(env, "[] or one known return type", op);
        }
    }
    if (ffi_prep_cif(&f->cif, FFI_DEFAULT_ABI, f->nargs,
                     f->ret ? f->ret->type : &ffi_type_void, f->arg_types)
        != FFI_OK) {
        free(f);
        execerror(env, "valid C signature", op);
    }
    /* Empty name: the program and the libraries already loaded */
    handle = dlopen(*lib ? lib : NULL, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        free(f);
        ffi_fail(env, "loadable library", dlerror(), op);
    }
    dlerror();
    address = dlsym(handle, sym);
    if (!address) {
        free(f);
        ffi_fail(env, "symbol in library", dlerror(), op);
    }
    /* POSIX requires this conversion to work */
    memcpy(&f->fn, &address, sizeof(f->fn));
    return f;
}

/* Find or build the function for L S A R. No Joy allocation: nothing moves. */
static FfiFunction* resolve_function(pEnv env, Index lib, Index sym, Index args,
                                     Index ret, char* op)
{
    const char* lib_name = GETSTRING(lib);
    const char* sym_name = GETSTRING(sym);
    size_t size = strlen(lib_name) + strlen(sym_name) + 3;
    FfiFunction* f;
    char* key;
    khint_t k;
    int absent;

    if (!ffi_cache) {
        ffi_cache = kh_init(FfiCache);
        init_ffi_types();
    }
    for (Index n = args; n; n = nextnode1(n)) {
        const FfiType* type = parse_type(env, n);

        if (!type)
            execerror(env, "known argument type", op);
        size += strlen(type->name) + 1;
    }
    for (Index n = ret; n; n = nextnode1(n)) {
        const FfiType* type = parse_type(env, n);

        if (!type)
            execerror(env, "[] or one known return type", op);
        size += strlen(type->name) + 1;
    }
    size += 1;
    if (!(key = malloc(size)))
        execerror(env, "memory", op);
    {
        size_t used = snprintf(key, size, "%s\n%s\n", lib_name, sym_name);

        for (Index n = args; n; n = nextnode1(n))
            used += snprintf(key + used, size - used, "%s,", parse_type(env, n)->name);
        used += snprintf(key + used, size - used, ">");
        for (Index n = ret; n; n = nextnode1(n))
            used += snprintf(key + used, size - used, "%s,", parse_type(env, n)->name);
    }
    k = kh_get(FfiCache, ffi_cache, key);
    if (k != kh_end(ffi_cache)) {
        free(key);
        return kh_value(ffi_cache, k);
    }
    /* An error here loses key: build_function does not return to free it */
    f = build_function(env, lib_name, sym_name, args, ret, op);
    k = kh_put(FfiCache, ffi_cache, key, &absent);
    kh_value(ffi_cache, k) = f;
    return f;
}

static int int_in_range(int64_t value, const FfiType* type)
{
    int bits = (int)type->type->size * 8;

    if (bits == 64)
        return type->is_signed || value >= 0;
    if (type->is_signed)
        return value >= -((int64_t)1 << (bits - 1)) && value < ((int64_t)1 << (bits - 1));
    return value >= 0 && value < ((int64_t)1 << bits);
}

/* Check one argument without side effects. */
static void check_argument(pEnv env, Index node, const FfiType* type, char* op)
{
    int op_type = nodetype(node);

    switch (type->kind) {
    case K_INT:
        if (op_type != INTEGER_)
            execerror(env, "integer argument", op);
        if (!int_in_range(nodevalue(node).num, type))
            execerror(env, "integer argument in range of its C type", op);
        break;
    case K_CHAR:
        if (op_type != CHAR_)
            execerror(env, "character argument", op);
        break;
    case K_BOOL:
        if (op_type != BOOLEAN_)
            execerror(env, "logical argument", op);
        break;
    case K_FLOAT:
    case K_DOUBLE:
        if (op_type != FLOAT_ && op_type != INTEGER_)
            execerror(env, "numeric argument", op);
        break;
    case K_STRING:
        if (op_type != STRING_)
            execerror(env, "string argument", op);
        break;
    case K_POINTER:
        if (op_type != INTEGER_)
            execerror(env, "integer argument for pointer", op);
        break;
    }
}

typedef union {
    int8_t i8;
    int16_t i16;
    int32_t i32;
    int64_t i64;
    uint8_t u8;
    float f;
    double d;
    void* p;
    ffi_arg ret; /* libffi widens small integer returns to this */
    ffi_sarg sret;
} FfiValue;

static void store_argument(pEnv env, Index node, const FfiType* type, FfiValue* value)
{
    int is_float = nodetype(node) == FLOAT_;

    switch (type->kind) {
    case K_INT:
        switch (type->type->size) {
        case 1: value->i8 = (int8_t)nodevalue(node).num; break;
        case 2: value->i16 = (int16_t)nodevalue(node).num; break;
        case 4: value->i32 = (int32_t)nodevalue(node).num; break;
        default: value->i64 = nodevalue(node).num; break;
        }
        break;
    case K_CHAR:
        value->i8 = (int8_t)nodevalue(node).num;
        break;
    case K_BOOL:
        value->u8 = nodevalue(node).num != 0;
        break;
    case K_FLOAT:
        value->f = is_float ? (float)nodevalue(node).dbl : (float)nodevalue(node).num;
        break;
    case K_DOUBLE:
        value->d = is_float ? nodevalue(node).dbl : (double)nodevalue(node).num;
        break;
    case K_STRING:
        /* A copy: foreign code must not be able to write into a node */
        value->p = strdup(GETSTRING(node));
        if (!value->p)
            execerror(env, "memory", "ffi-call");
        break;
    case K_POINTER:
        value->p = (void*)(intptr_t)nodevalue(node).num;
        break;
    }
}

/* Replace the stack top args with the result. Returns 0 if out of range. */
static int push_result(pEnv env, const FfiType* type, FfiValue* result, Index rest)
{
    env->stck = rest;
    if (!type)
        return 1;
    switch (type->kind) {
    case K_INT:
        if (!type->is_signed && type->type->size == 8 && (int64_t)result->i64 < 0)
            return 0;
        if (type->type->size == 8)
            NULLARY(INTEGER_NEWNODE, result->i64);
        else if (type->is_signed)
            NULLARY(INTEGER_NEWNODE, (int64_t)result->sret);
        else
            NULLARY(INTEGER_NEWNODE, (int64_t)result->ret);
        break;
    case K_CHAR:
        NULLARY(CHAR_NEWNODE, (int64_t)(int8_t)result->sret);
        break;
    case K_BOOL:
        NULLARY(BOOLEAN_NEWNODE, (result->ret & 0xff) != 0);
        break;
    case K_FLOAT:
        NULLARY(FLOAT_NEWNODE, (double)result->f);
        break;
    case K_DOUBLE:
        NULLARY(FLOAT_NEWNODE, result->d);
        break;
    case K_STRING:
        if (!result->p)
            return 0;
        NULLARY(STRING_NEWNODE, (char*)result->p);
        break;
    case K_POINTER:
        NULLARY(INTEGER_NEWNODE, (int64_t)(intptr_t)result->p);
        break;
    }
    return 1;
}
#endif /* JOY_FFI */

/* L S A R on top: each checked, and A and R lists. */
#define FFI_PARAMS(NAME)                                                      \
    FOURPARAMS(NAME);                                                         \
    if (nodetype(env->stck) != LIST_                                          \
        || nodetype(nextnode1(env->stck)) != LIST_)                           \
        execerror(env, "two type lists", NAME);                               \
    if (nodetype(nextnode2(env->stck)) != STRING_                             \
        || nodetype(nextnode3(env->stck)) != STRING_)                         \
        execerror(env, "library and symbol strings", NAME)

/**
Q0  IGNORE_OK  4060  ffi-call\0ffi_call  :  A1..An L S [T1..Tn] [R]  ->  V
[IMPURE] Calls C function S in shared library L, with n arguments of types T1..Tn.
An is on top. [R] is [] for void or [T] to push result V. L "" searches loaded code.
Types: int uint long ulong int64 uint64 size_t char bool float double string pointer.
*/
void ffi_call_(pEnv env)
{
#ifdef JOY_FFI
    FfiFunction* f;
    FfiValue values[FFI_MAX_ARGS];
    void* pointers[FFI_MAX_ARGS];
    FfiValue result;
    Index node;
    int ok;

    PARALLEL_UNSAFE(env);
    FFI_PARAMS("ffi-call");
    f = resolve_function(env, nextnode3(env->stck), nextnode2(env->stck),
                         nodevalue(nextnode1(env->stck)).lis,
                         nodevalue(env->stck).lis, "ffi-call");
    node = nextnode1(nextnode3(env->stck));
    for (int i = f->nargs - 1; i >= 0; i--, node = nextnode1(node)) {
        if (!node)
            execerror(env, "more arguments", "ffi-call");
        check_argument(env, node, f->args[i], "ffi-call");
    }
    /* All checked: from here to the call nothing can fail but memory */
    node = nextnode1(nextnode3(env->stck));
    for (int i = f->nargs - 1; i >= 0; i--, node = nextnode1(node)) {
        store_argument(env, node, f->args[i], &values[i]);
        pointers[i] = &values[i];
    }
    memset(&result, 0, sizeof(result));
    ffi_call(&f->cif, f->fn, &result, f->nargs ? pointers : NULL);
    /* Before the copies go: the function may return a pointer into one */
    ok = push_result(env, f->ret, &result, node);
    for (int i = 0; i < f->nargs; i++)
        if (f->args[i]->kind == K_STRING)
            free(values[i].p);
    if (!ok)
        execerror(env, f->ret->kind == K_STRING
                  ? "non-NULL string result; use pointer"
                  : "result in range of integer", "ffi-call");
#else
    execerror(env, "a build with -DJOY_FFI=ON", "ffi-call");
#endif
}

/**
Q0  IGNORE_OK  4061  ffi  :  L S [T1..Tn] [R]  ->  [Q]
[IMPURE] Resolves C function S now, so that a bad name fails here. Q is the
quotation [L S [T1..Tn] [R] ffi-call].
*/
void ffi_(pEnv env)
{
#ifdef JOY_FFI
    Index quotation;

    PARALLEL_UNSAFE(env);
    FFI_PARAMS("ffi");
    resolve_function(env, nextnode3(env->stck), nextnode2(env->stck),
                     nodevalue(nextnode1(env->stck)).lis,
                     nodevalue(env->stck).lis, "ffi");
    quotation = ANON_FUNCT_NEWNODE(ffi_call_, 0);
    quotation = newnode2(env, env->stck, quotation);
    quotation = newnode2(env, nextnode1(env->stck), quotation);
    quotation = newnode2(env, nextnode2(env->stck), quotation);
    quotation = newnode2(env, nextnode3(env->stck), quotation);
    env->stck = LIST_NEWNODE(quotation, nextnode1(nextnode3(env->stck)));
#else
    execerror(env, "a build with -DJOY_FFI=ON", "ffi");
#endif
}
