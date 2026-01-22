/*
 *  module  : joy/joy.h
 *  version : 1.0
 *  date    : 01/22/26
 *
 *  Public API for the Joy programming language interpreter.
 *  This header provides the stable interface for embedding Joy.
 */
#ifndef JOY_JOY_H
#define JOY_JOY_H

#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Version information
 */
#define JOY_VERSION_MAJOR 1
#define JOY_VERSION_MINOR 0
#define JOY_VERSION_PATCH 0
#define JOY_VERSION_STRING "1.0.0"

/*
 * Result codes returned by Joy API functions.
 */
typedef enum JoyResult {
    JOY_OK = 0,                    /* Success */
    JOY_ERROR_SYNTAX,              /* Syntax/parse error */
    JOY_ERROR_RUNTIME,             /* General runtime error */
    JOY_ERROR_TYPE,                /* Type mismatch error */
    JOY_ERROR_STACK_UNDERFLOW,     /* Stack underflow error */
    JOY_ERROR_OUT_OF_MEMORY,       /* Memory allocation failed */
    JOY_ERROR_IO,                  /* I/O error */
    JOY_ERROR_QUIT,                /* Quit was requested */
    JOY_ERROR_ABORT                /* Abort was requested */
} JoyResult;

/*
 * Opaque interpreter context handle.
 */
typedef struct JoyContext JoyContext;

/*
 * Custom I/O callbacks for redirecting interpreter input/output.
 * All callbacks are optional (may be NULL).
 */
typedef struct JoyIO {
    void* user_data;                                      /* User context passed to callbacks */
    int (*read_char)(void* user_data);                    /* Read a character (return -1 on EOF) */
    void (*write_char)(void* user_data, int ch);          /* Write a character */
    void (*write_string)(void* user_data, const char* s); /* Write a string */
    void (*on_error)(void* user_data, JoyResult result,   /* Error callback */
                     const char* message, const char* file,
                     int line, int column);
} JoyIO;

/*
 * Configuration for creating a new interpreter context.
 * All fields are optional and have sensible defaults when set to 0/NULL.
 */
typedef struct JoyConfig {
    size_t initial_memory_size;    /* Initial memory pool size (0 = default) */
    size_t max_memory_size;        /* Maximum memory limit (0 = unlimited) */
    int enable_gc_trace;           /* Enable GC tracing output */
    int enable_autoput;            /* Auto-print stack after each line */
    int enable_echo;               /* Echo input lines */
    JoyIO* io;                     /* Custom I/O callbacks (NULL = stdio) */
} JoyConfig;

/*
 * Convert a result code to a human-readable string.
 */
const char* joy_result_string(JoyResult result);

/*
 * Get the library version string (e.g., "Joy 1.0.0").
 */
const char* joy_version(void);

/*
 * Create a new interpreter context.
 *
 * @param config Configuration options (NULL for defaults)
 * @return New context or NULL on failure
 */
JoyContext* joy_create(const JoyConfig* config);

/*
 * Destroy an interpreter context and free resources.
 *
 * @param ctx Context to destroy (may be NULL)
 */
void joy_destroy(JoyContext* ctx);

/*
 * Evaluate a string of Joy code.
 *
 * @param ctx  Interpreter context
 * @param source  Joy source code (must end with '.' terminator)
 * @return Result code
 */
JoyResult joy_eval_string(JoyContext* ctx, const char* source);

/*
 * Evaluate Joy code from a file.
 *
 * @param ctx  Interpreter context
 * @param fp   File pointer to read from
 * @param filename  Filename for error messages (may be NULL)
 * @return Result code
 */
JoyResult joy_eval_file(JoyContext* ctx, FILE* fp, const char* filename);

/*
 * Load the Joy standard library.
 *
 * @param ctx  Interpreter context
 * @param lib_path  Path to library file (NULL = "usrlib.joy")
 * @return Result code
 */
JoyResult joy_load_stdlib(JoyContext* ctx, const char* lib_path);

/*
 * Get the current stack depth.
 */
size_t joy_stack_depth(JoyContext* ctx);

/*
 * Check if the stack is empty.
 */
int joy_stack_empty(JoyContext* ctx);

/*
 * Clear the stack.
 */
void joy_stack_clear(JoyContext* ctx);

/*
 * Get the last error message.
 */
const char* joy_error_message(JoyContext* ctx);

/*
 * Get the line number of the last error.
 */
int joy_error_line(JoyContext* ctx);

/*
 * Get the column number of the last error.
 */
int joy_error_column(JoyContext* ctx);

/*
 * Set autoput mode (auto-print stack after input).
 */
void joy_set_autoput(JoyContext* ctx, int enabled);

/*
 * Get autoput mode.
 */
int joy_get_autoput(JoyContext* ctx);

/*
 * Set echo mode (echo input lines).
 */
void joy_set_echo(JoyContext* ctx, int enabled);

/*
 * Get echo mode.
 */
int joy_get_echo(JoyContext* ctx);

/*
 * Get number of nodes currently in use.
 */
size_t joy_memory_used(JoyContext* ctx);

/*
 * Get maximum memory size available.
 */
size_t joy_memory_max(JoyContext* ctx);

/*
 * Get number of garbage collections performed.
 */
size_t joy_gc_count(JoyContext* ctx);

#ifdef __cplusplus
}
#endif

#endif /* JOY_JOY_H */
