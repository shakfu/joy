# Joy Plugin API Design

A clean C extension system for Joy that allows runtime loading of plugins without recompiling the interpreter.

## Core Principles

1. **Opaque handles** - plugins don't see internal structs directly
2. **Stable ABI** - versioned API that can evolve without breaking plugins
3. **Simple registration** - one function to register multiple builtins
4. **Dynamic loading** - dlopen/LoadLibrary at runtime

---

## Plugin API Header (`include/joy/plugin.h`)

```c
#ifndef JOY_PLUGIN_H
#define JOY_PLUGIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

/* API version for compatibility checking */
#define JOY_PLUGIN_API_VERSION 1

/* Opaque handle to Joy environment */
typedef struct JoyEnv JoyEnv;

/* Plugin function signature - same as internal builtins */
typedef void (*JoyPluginFn)(JoyEnv* env);

/* Node types (stable subset) */
typedef enum {
    JOY_TYPE_INTEGER = 1,
    JOY_TYPE_FLOAT   = 2,
    JOY_TYPE_STRING  = 4,
    JOY_TYPE_LIST    = 8,
    JOY_TYPE_BOOL    = 16,
    JOY_TYPE_DICT    = 32
} JoyType;

/* Result codes */
typedef enum {
    JOY_OK = 0,
    JOY_ERROR = -1,
    JOY_ERROR_TYPE = -2,
    JOY_ERROR_UNDERFLOW = -3
} JoyResult;

/*============================================================
 * Stack Operations
 *============================================================*/

/* Query stack */
int         joy_stack_depth(JoyEnv* env);
JoyType     joy_peek_type(JoyEnv* env, int depth);  /* 0 = top */

/* Pop values (returns JOY_ERROR_UNDERFLOW if empty) */
JoyResult   joy_pop_int(JoyEnv* env, int64_t* out);
JoyResult   joy_pop_float(JoyEnv* env, double* out);
JoyResult   joy_pop_bool(JoyEnv* env, int* out);
JoyResult   joy_pop_string(JoyEnv* env, const char** out, size_t* len);

/* Push values */
void        joy_push_int(JoyEnv* env, int64_t val);
void        joy_push_float(JoyEnv* env, double val);
void        joy_push_bool(JoyEnv* env, int val);
void        joy_push_string(JoyEnv* env, const char* str, size_t len);

/* List operations */
JoyResult   joy_pop_list_begin(JoyEnv* env);        /* start iterating list */
int         joy_list_next(JoyEnv* env);             /* advance, returns 0 at end */
void        joy_list_end(JoyEnv* env);              /* finish iteration */

void        joy_push_list_begin(JoyEnv* env);       /* start building list */
void        joy_push_list_item(JoyEnv* env);        /* add top of stack to list */
void        joy_push_list_end(JoyEnv* env);         /* finish and push list */

/*============================================================
 * Error Handling
 *============================================================*/

void        joy_error(JoyEnv* env, const char* msg, const char* name);
void        joy_type_error(JoyEnv* env, const char* expected, const char* name);

/*============================================================
 * Plugin Registration
 *============================================================*/

/* Builtin descriptor */
typedef struct {
    const char*   name;      /* Joy name (e.g., "my-func") */
    JoyPluginFn   func;      /* C function pointer */
    const char*   stack_effect;  /* e.g., "A B -> C" for documentation */
    const char*   doc;       /* description */
} JoyBuiltinDesc;

/* Plugin descriptor - returned by joy_plugin_init */
typedef struct {
    int           api_version;    /* must be JOY_PLUGIN_API_VERSION */
    const char*   name;           /* plugin name */
    const char*   version;        /* plugin version string */
    const JoyBuiltinDesc* builtins;  /* null-terminated array */
} JoyPluginDesc;

/* Plugin entry point - every plugin must export this */
typedef const JoyPluginDesc* (*JoyPluginInitFn)(void);

#define JOY_PLUGIN_ENTRY joy_plugin_init

#ifdef __cplusplus
}
#endif

#endif /* JOY_PLUGIN_H */
```

---

## Example Plugin (`plugins/example_math.c`)

```c
#include <joy/plugin.h>
#include <math.h>

/* Hypotenuse: A B -> sqrt(A^2 + B^2) */
static void hypot_(JoyEnv* env)
{
    double a, b;
    if (joy_pop_float(env, &b) != JOY_OK) {
        joy_type_error(env, "float", "hypot");
        return;
    }
    if (joy_pop_float(env, &a) != JOY_OK) {
        joy_type_error(env, "float", "hypot");
        return;
    }
    joy_push_float(env, hypot(a, b));
}

/* Clamp: X MIN MAX -> clamped value */
static void clamp_(JoyEnv* env)
{
    double x, min, max;
    if (joy_pop_float(env, &max) != JOY_OK ||
        joy_pop_float(env, &min) != JOY_OK ||
        joy_pop_float(env, &x) != JOY_OK) {
        joy_type_error(env, "three floats", "clamp");
        return;
    }
    if (x < min) x = min;
    if (x > max) x = max;
    joy_push_float(env, x);
}

/* Lerp: A B T -> A + (B-A)*T */
static void lerp_(JoyEnv* env)
{
    double a, b, t;
    if (joy_pop_float(env, &t) != JOY_OK ||
        joy_pop_float(env, &b) != JOY_OK ||
        joy_pop_float(env, &a) != JOY_OK) {
        joy_type_error(env, "three floats", "lerp");
        return;
    }
    joy_push_float(env, a + (b - a) * t);
}

/* Plugin builtins table */
static const JoyBuiltinDesc builtins[] = {
    { "hypot", hypot_, "A B -> C", "Euclidean distance sqrt(A^2 + B^2)" },
    { "clamp", clamp_, "X MIN MAX -> Y", "Clamp X to range [MIN, MAX]" },
    { "lerp",  lerp_,  "A B T -> C", "Linear interpolation A + (B-A)*T" },
    { NULL, NULL, NULL, NULL }  /* terminator */
};

/* Plugin descriptor */
static const JoyPluginDesc plugin_desc = {
    .api_version = JOY_PLUGIN_API_VERSION,
    .name = "example_math",
    .version = "1.0.0",
    .builtins = builtins
};

/* Entry point */
const JoyPluginDesc* JOY_PLUGIN_ENTRY(void)
{
    return &plugin_desc;
}
```

---

## Plugin Loader (`src/plugin.c`)

```c
#include "globals.h"
#include <joy/plugin.h>

#ifdef _WIN32
  #include <windows.h>
  #define DLOPEN(path)    LoadLibraryA(path)
  #define DLSYM(h, name)  (void*)GetProcAddress(h, name)
  #define DLCLOSE(h)      FreeLibrary(h)
  #define DLERROR()       "LoadLibrary failed"
#else
  #include <dlfcn.h>
  #define DLOPEN(path)    dlopen(path, RTLD_NOW | RTLD_LOCAL)
  #define DLSYM(h, name)  dlsym(h, name)
  #define DLCLOSE(h)      dlclose(h)
  #define DLERROR()       dlerror()
#endif

/* Register a single builtin from plugin descriptor */
static int register_plugin_builtin(pEnv env, const JoyBuiltinDesc* desc)
{
    Entry ent;
    memset(&ent, 0, sizeof(ent));

    ent.name = GC_strdup(desc->name);
    ent.u.proc = (proc_t)desc->func;
    ent.is_user = 0;
    ent.flags = OK;
    ent.qcode = Q0;

    int index = vec_size(env->symtab);
    addsymbol(env, ent, index);
    vec_push(env->symtab, ent);

    return 0;
}

/* Load plugin from shared library */
int joy_load_plugin(pEnv env, const char* path)
{
    void* handle = DLOPEN(path);
    if (!handle) {
        fprintf(stderr, "plugin: cannot load '%s': %s\n", path, DLERROR());
        return -1;
    }

    JoyPluginInitFn init = (JoyPluginInitFn)DLSYM(handle, "joy_plugin_init");
    if (!init) {
        fprintf(stderr, "plugin: '%s' missing joy_plugin_init\n", path);
        DLCLOSE(handle);
        return -1;
    }

    const JoyPluginDesc* desc = init();
    if (!desc) {
        fprintf(stderr, "plugin: '%s' init returned NULL\n", path);
        DLCLOSE(handle);
        return -1;
    }

    if (desc->api_version != JOY_PLUGIN_API_VERSION) {
        fprintf(stderr, "plugin: '%s' API version mismatch (got %d, want %d)\n",
                path, desc->api_version, JOY_PLUGIN_API_VERSION);
        DLCLOSE(handle);
        return -1;
    }

    /* Register all builtins */
    for (const JoyBuiltinDesc* b = desc->builtins; b->name; b++) {
        register_plugin_builtin(env, b);
    }

    printf("plugin: loaded '%s' v%s\n", desc->name, desc->version);
    return 0;
}
```

---

## Plugin API Implementation (`src/plugin_api.c`)

```c
#include "globals.h"
#include <joy/plugin.h>

/* Cast opaque handle to internal Env */
#define ENV(e) ((pEnv)(e))

int joy_stack_depth(JoyEnv* env)
{
    int depth = 0;
    for (Index p = ENV(env)->stck; p; p = nextnode1(p))
        depth++;
    return depth;
}

JoyType joy_peek_type(JoyEnv* env, int depth)
{
    Index p = ENV(env)->stck;
    while (depth-- > 0 && p)
        p = nextnode1(p);
    if (!p) return 0;

    switch (nodetype(p)) {
        case INTEGER_: return JOY_TYPE_INTEGER;
        case FLOAT_:   return JOY_TYPE_FLOAT;
        case STRING_:  return JOY_TYPE_STRING;
        case LIST_:    return JOY_TYPE_LIST;
        case BOOLEAN_: return JOY_TYPE_BOOL;
        case DICT_:    return JOY_TYPE_DICT;
        default:       return 0;
    }
}

JoyResult joy_pop_int(JoyEnv* env, int64_t* out)
{
    pEnv e = ENV(env);
    if (!e->stck) return JOY_ERROR_UNDERFLOW;
    if (nodetype(e->stck) != INTEGER_) return JOY_ERROR_TYPE;
    *out = nodevalue(e->stck).num;
    e->stck = nextnode1(e->stck);
    return JOY_OK;
}

JoyResult joy_pop_float(JoyEnv* env, double* out)
{
    pEnv e = ENV(env);
    if (!e->stck) return JOY_ERROR_UNDERFLOW;
    if (nodetype(e->stck) == FLOAT_) {
        *out = nodevalue(e->stck).dbl;
    } else if (nodetype(e->stck) == INTEGER_) {
        *out = (double)nodevalue(e->stck).num;  /* auto-convert */
    } else {
        return JOY_ERROR_TYPE;
    }
    e->stck = nextnode1(e->stck);
    return JOY_OK;
}

JoyResult joy_pop_string(JoyEnv* env, const char** out, size_t* len)
{
    pEnv e = ENV(env);
    if (!e->stck) return JOY_ERROR_UNDERFLOW;
    if (nodetype(e->stck) != STRING_) return JOY_ERROR_TYPE;
    *out = nodevalue(e->stck).str;
    if (len) *len = strlen(*out);
    e->stck = nextnode1(e->stck);
    return JOY_OK;
}

JoyResult joy_pop_bool(JoyEnv* env, int* out)
{
    pEnv e = ENV(env);
    if (!e->stck) return JOY_ERROR_UNDERFLOW;
    if (nodetype(e->stck) != BOOLEAN_) return JOY_ERROR_TYPE;
    *out = nodevalue(e->stck).num != 0;
    e->stck = nextnode1(e->stck);
    return JOY_OK;
}

void joy_push_int(JoyEnv* env, int64_t val)
{
    pEnv e = ENV(env);
    e->bucket.num = val;
    e->stck = newnode(e, INTEGER_, e->bucket, e->stck);
}

void joy_push_float(JoyEnv* env, double val)
{
    pEnv e = ENV(env);
    e->bucket.dbl = val;
    e->stck = newnode(e, FLOAT_, e->bucket, e->stck);
}

void joy_push_bool(JoyEnv* env, int val)
{
    pEnv e = ENV(env);
    e->bucket.num = val ? 1 : 0;
    e->stck = newnode(e, BOOLEAN_, e->bucket, e->stck);
}

void joy_push_string(JoyEnv* env, const char* str, size_t len)
{
    pEnv e = ENV(env);
    char* copy = GC_alloc(len + 1);
    memcpy(copy, str, len);
    copy[len] = '\0';
    e->bucket.str = copy;
    e->stck = newnode(e, STRING_, e->bucket, e->stck);
}

void joy_error(JoyEnv* env, const char* msg, const char* name)
{
    execerror(ENV(env), msg, name);
}

void joy_type_error(JoyEnv* env, const char* expected, const char* name)
{
    execerror(ENV(env), expected, name);
}
```

---

## Joy-side Loading

```joy
(* Load a plugin at runtime *)
"./plugins/libexample_math.so" plugin-load.

(* Now use the new builtins *)
3.0 4.0 hypot.          (* -> 5.0 *)
15.0 0.0 10.0 clamp.    (* -> 10.0 *)
0.0 100.0 0.5 lerp.     (* -> 50.0 *)
```

---

## Build Integration (`CMakeLists.txt` additions)

```cmake
# Plugin API as separate target
add_library(joy_plugin_api SHARED
    src/plugin.c
    src/plugin_api.c
)
target_link_libraries(joy_plugin_api PRIVATE joycore_shared)

# Example plugin
add_library(example_math MODULE plugins/example_math.c)
target_include_directories(example_math PRIVATE ${CMAKE_SOURCE_DIR}/include)
set_target_properties(example_math PROPERTIES PREFIX "lib")
```

---

## Summary

| Component | Purpose |
|-----------|---------|
| `joy/plugin.h` | Stable C API for plugins (opaque handles) |
| `plugin.c` | dlopen/LoadLibrary loader |
| `plugin_api.c` | Bridge from opaque API to internal Env |
| Plugin `.so/.dll` | Exports `joy_plugin_init()` returning descriptor |

## Key Design Choices

- **Opaque `JoyEnv*`** - plugins can't accidentally corrupt internals
- **Type-safe pop/push** - explicit functions per type, auto int->float conversion
- **Versioned API** - `JOY_PLUGIN_API_VERSION` for compatibility
- **Null-terminated arrays** - simple descriptor format, no size tracking
- **Cross-platform** - dlopen on Unix, LoadLibrary on Windows

## Future Extensions

Potential additions to the plugin API:

- **Quotation execution** - `joy_call_quotation()` for callbacks
- **Dictionary access** - `joy_dict_get()`, `joy_dict_put()`
- **Memory allocation** - `joy_alloc()` for GC-managed memory
- **Symbol lookup** - `joy_lookup_symbol()` to call other Joy functions
- **Plugin unloading** - `joy_unload_plugin()` with cleanup callbacks
