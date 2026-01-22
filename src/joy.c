/*
 *  module  : joy.c
 *  version : 1.0
 *  date    : 01/20/26
 *
 *  Implementation of the public Joy library API.
 *  This file wraps the internal interpreter with a clean public interface.
 */
#include "globals.h"
#include "joy/joy.h"

/* Internal: JoyContext is the same as Env */
struct JoyContext {
    Env env;
    JoyIO* io;           /* custom I/O callbacks */
    JoyResult last_result;
    size_t gc_count;
};

/* Version string - uses macro from joy.h */
static const char* version_string = "Joy " JOY_VERSION_STRING;

/*
 * Result code to string mapping
 */
const char* joy_result_string(JoyResult result)
{
    switch (result) {
    case JOY_OK:                  return "OK";
    case JOY_ERROR_SYNTAX:        return "Syntax error";
    case JOY_ERROR_RUNTIME:       return "Runtime error";
    case JOY_ERROR_TYPE:          return "Type error";
    case JOY_ERROR_STACK_UNDERFLOW: return "Stack underflow";
    case JOY_ERROR_OUT_OF_MEMORY: return "Out of memory";
    case JOY_ERROR_IO:            return "I/O error";
    case JOY_ERROR_QUIT:          return "Quit requested";
    case JOY_ERROR_ABORT:         return "Abort requested";
    default:                      return "Unknown error";
    }
}

/*
 * Get the library version string.
 */
const char* joy_version(void)
{
    return version_string;
}

/* Track number of active contexts for proper cleanup */
static int active_context_count = 0;

/*
 * Create a new interpreter context.
 *
 * Note: Due to internal memory management using static variables,
 * only one context should be active at a time. Creating multiple
 * contexts is supported but they share memory state.
 */
JoyContext* joy_create(const JoyConfig* config)
{
    active_context_count++;

    JoyContext* ctx = calloc(1, sizeof(JoyContext));
    if (!ctx)
        return NULL;

    pEnv env = &ctx->env;

    /*
     * Initialize global GC once on first context creation.
     * This is needed even in NOBDW mode because kvec.h macros use
     * GC_malloc directly (not through env->gc_ctx).
     */
    static int gc_initialized = 0;
    if (!gc_initialized) {
        static char gc_root_anchor;
        bottom_of_stack = &gc_root_anchor;
        GC_INIT();
        gc_initialized = 1;
    }

#ifdef NOBDW
    /* Create per-context GC (Phase 3) */
    env->gc_ctx = gc_ctx_create();
    if (!env->gc_ctx) {
        free(ctx);
        return NULL;
    }
    /* Set stack bottom for per-context GC */
    env->gc_ctx->stack_bottom = bottom_of_stack;
#endif

    /* Initialize scanner state */
    env->scanner.ilevel = -1;

    /* Initialize clock */
    env->startclock = clock();

    /* Initialize vectors */
    vec_init(env->pathnames);
    vec_init(env->string);
    vec_init(env->pushback);
    vec_init(env->tokens);
    vec_init(env->symtab);

    /* Initialize symbol table */
    inisymboltable(env);

    /* Initialize memory */
#ifdef NOBDW
    inimem1(env, 0);
    inimem2(env);
#endif

    /* Apply configuration */
    if (config) {
        ctx->io = config->io;
        env->config.autoput = config->enable_autoput ? 1 : 0;
        env->config.autoput_set = 1;
        env->config.echoflag = config->enable_echo ? 1 : 0;
        env->config.tracegc = config->enable_gc_trace ? 1 : 0;

        /* Copy I/O callbacks to env for use by internal functions */
        if (config->io) {
            env->io.user_data = config->io->user_data;
            env->io.read_char = config->io->read_char;
            env->io.write_char = config->io->write_char;
            env->io.write_string = config->io->write_string;
            /* Cast needed: JoyResult is int-compatible but types differ */
            env->io.on_error = (void (*)(void*, int, const char*, const char*, int, int))
                               config->io->on_error;
        }
    } else {
        /* Defaults */
        env->config.autoput = INIAUTOPUT;
        env->config.echoflag = INIECHOFLAG;
        env->config.tracegc = INITRACEGC;
    }

    env->config.undeferror = INIUNDEFERROR;
    env->config.overwrite = INIWARNING;

    /* Disable output buffering */
    setbuf(stdout, 0);

    return ctx;
}

/* Note: memoryindex and mem_low are now in env struct (env->memoryindex, env->mem_low) */

/*
 * Destroy an interpreter context.
 */
void joy_destroy(JoyContext* ctx)
{
    if (!ctx)
        return;

    active_context_count--;

    /* Free hash tables */
    if (ctx->env.hash)
        kh_destroy(Symtab, ctx->env.hash);
    if (ctx->env.prim)
        kh_destroy(Funtab, ctx->env.prim);

#ifdef NOBDW
    /* Free copying GC memory for this context */
    if (ctx->env.memory) {
        free(ctx->env.memory);
        ctx->env.memory = NULL;
    }
    /* Destroy per-context conservative GC (Phase 3) */
    if (ctx->env.gc_ctx) {
        gc_ctx_destroy(ctx->env.gc_ctx);
        ctx->env.gc_ctx = NULL;
    }
#endif

    /*
     * Note: Symbol table strings and vectors use GC-managed memory,
     * so they don't need explicit freeing. The GC handles cleanup.
     */

    free(ctx);
}

/*
 * Evaluate a string of Joy code.
 */
JoyResult joy_eval_string(JoyContext* ctx, const char* source)
{
    if (!ctx || !source)
        return JOY_ERROR_RUNTIME;

    pEnv env = &ctx->env;
    int err;

    /* Set up error recovery */
    err = setjmp(env->error_jmp);
    if (err == ABORT_QUIT) {
        ctx->last_result = JOY_ERROR_QUIT;
        return JOY_ERROR_QUIT;
    }
    if (err == ABORT_RETRY) {
        ctx->last_result = JOY_ERROR_ABORT;
        return JOY_ERROR_ABORT;
    }

    /* Set up source as if from stdin by using pushback.
     * Add trailing spaces so getsym doesn't read past end into srcfile. */
    vec_push(env->pushback, ' ');
    vec_push(env->pushback, ' ');
    size_t len = strlen(source);
    for (size_t i = len; i > 0; i--) {
        vec_push(env->pushback, source[i - 1]);
    }

    /* Initialize line buffer if not already done */
    if (!env->scanner.srcfile) {
        inilinebuffer(env);
    }

    /* Parse and evaluate */
    int ch = getch(env);
    ch = getsym(env, ch);

    while (env->scanner.sym != '.') {
        if (env->scanner.sym == LIBRA || env->scanner.sym == HIDE || env->scanner.sym == MODULE_ ||
            env->scanner.sym == PRIVATE || env->scanner.sym == PUBLIC || env->scanner.sym == CONST_) {
            ch = compound_def(env, ch);
            /* compound_def may leave sym as '.', check before continuing */
            if (env->scanner.sym == '.')
                break;
            ch = getsym(env, ch);
        } else {
            ch = readterm(env, ch);
            if (env->stck) {
                exec_term(env, nodevalue(env->stck).lis);
                env->stck = nextnode1(env->stck);
            }
            /* readterm reads until terminator, check if it's '.' */
            if (env->scanner.sym == '.')
                break;
            ch = getsym(env, ch);
        }
    }

    ctx->last_result = JOY_OK;
    return JOY_OK;
}

/*
 * Evaluate Joy code from a file.
 */
JoyResult joy_eval_file(JoyContext* ctx, FILE* fp, const char* filename)
{
    if (!ctx || !fp)
        return JOY_ERROR_IO;

    pEnv env = &ctx->env;
    int err;

    /* Set up error recovery */
    err = setjmp(env->error_jmp);
    if (err == ABORT_QUIT) {
        ctx->last_result = JOY_ERROR_QUIT;
        return JOY_ERROR_QUIT;
    }
    if (err == ABORT_RETRY) {
        ctx->last_result = JOY_ERROR_ABORT;
        return JOY_ERROR_ABORT;
    }

    /* Set up source file */
    env->scanner.srcfile = fp;
    env->scanner.srcfilename = (char*)filename;
    env->scanner.linenum = 0;
    env->scanner.linepos = 0;
    env->scanner.linebuf[0] = 0;

    /* Use repl to process the file */
    repl(env);

    ctx->last_result = JOY_OK;
    return JOY_OK;
}

/*
 * Load the standard library.
 */
JoyResult joy_load_stdlib(JoyContext* ctx, const char* lib_path)
{
    if (!ctx)
        return JOY_ERROR_RUNTIME;

    pEnv env = &ctx->env;
    int err;

    /* Set up error recovery */
    err = setjmp(env->error_jmp);
    if (err != 0) {
        ctx->last_result = JOY_ERROR_IO;
        return JOY_ERROR_IO;
    }

    const char* path = lib_path ? lib_path : "usrlib.joy";
    if (include(env, (char*)path) != 0) {
        ctx->last_result = JOY_ERROR_IO;
        return JOY_ERROR_IO;
    }

    ctx->last_result = JOY_OK;
    return JOY_OK;
}

/*
 * Get the current stack depth.
 */
size_t joy_stack_depth(JoyContext* ctx)
{
    if (!ctx)
        return 0;

    pEnv env = &ctx->env;  /* Required for nextnode1 macro */
    size_t depth = 0;
    Index n = env->stck;
    while (n) {
        depth++;
        n = nextnode1(n);
    }
    return depth;
}

/*
 * Check if the stack is empty.
 */
int joy_stack_empty(JoyContext* ctx)
{
    if (!ctx)
        return 1;
    return ctx->env.stck == 0;
}

/*
 * Clear the stack.
 */
void joy_stack_clear(JoyContext* ctx)
{
    if (ctx)
        ctx->env.stck = 0;
}

/*
 * Get the last error message.
 */
const char* joy_error_message(JoyContext* ctx)
{
    if (!ctx)
        return "";
    return ctx->env.error.message;
}

/*
 * Get the line number where the last error occurred.
 */
int joy_error_line(JoyContext* ctx)
{
    if (!ctx)
        return 0;
    return ctx->env.error.line;
}

/*
 * Get the column where the last error occurred.
 */
int joy_error_column(JoyContext* ctx)
{
    if (!ctx)
        return 0;
    return ctx->env.error.column;
}

/*
 * Set autoput mode.
 */
void joy_set_autoput(JoyContext* ctx, int enabled)
{
    if (ctx) {
        ctx->env.config.autoput = enabled ? 1 : 0;
        ctx->env.config.autoput_set = 1;
    }
}

/*
 * Get autoput mode.
 */
int joy_get_autoput(JoyContext* ctx)
{
    if (!ctx)
        return 0;
    return ctx->env.config.autoput;
}

/*
 * Set echo mode.
 */
void joy_set_echo(JoyContext* ctx, int enabled)
{
    if (ctx)
        ctx->env.config.echoflag = enabled ? 1 : 0;
}

/*
 * Get echo mode.
 */
int joy_get_echo(JoyContext* ctx)
{
    if (!ctx)
        return 0;
    return ctx->env.config.echoflag;
}

/*
 * Get number of nodes currently allocated.
 */
size_t joy_memory_used(JoyContext* ctx)
{
    if (!ctx)
        return 0;
    return (size_t)ctx->env.stats.nodes;
}

/*
 * Get maximum memory size.
 */
size_t joy_memory_max(JoyContext* ctx)
{
    if (!ctx)
        return 0;
    return (size_t)ctx->env.stats.avail;
}

/*
 * Get number of GC collections performed.
 */
size_t joy_gc_count(JoyContext* ctx)
{
    if (!ctx)
        return 0;
    return (size_t)ctx->env.stats.collect;
}
