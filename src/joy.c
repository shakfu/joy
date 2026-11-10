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
    int eval_depth;      /* nested eval calls, from I/O callbacks */
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

/*
 * Create a new interpreter context.
 *
 * Contexts have separate node heaps but share the conservative GC heap,
 * which holds vectors, strings and payloads. Each context's Env is a root.
 */
JoyContext* joy_create(const JoyConfig* config)
{
    JoyContext* ctx = calloc(1, sizeof(JoyContext));
    if (!ctx)
        return NULL;

    pEnv env = &ctx->env;

    static int gc_initialized = 0;
    if (!gc_initialized) {
        GC_INIT();
        gc_initialized = 1;
    }

#ifdef NOBDW
    /* Roots the Env before the vectors below are allocated in the GC heap */
    inimem1(env, 0);
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

#ifdef NOBDW
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

    /* Free hash tables */
    if (ctx->env.hash)
        kh_destroy(Symtab, ctx->env.hash);
    if (ctx->env.prim)
        kh_destroy(Funtab, ctx->env.prim);

    dict_destroy_all(&ctx->env);

#ifdef NOBDW
    /* Free copying GC memory for this context */
    if (ctx->env.memory) {
        free(ctx->env.memory);
        ctx->env.memory = NULL;
    }
    /* Its vectors and strings become garbage in the shared GC heap */
    gc_ctx_remove_root_fn(gc_get_default_context(), mark_env_roots, &ctx->env);
#endif

    free(ctx);
}

/*
 * Run repl on the input of one eval: what it pushed back, or the file it
 * reads or includes. The scanner raises ABORT_EOI where that input ends, so
 * the eval reads neither stdin nor the unread input of an outer eval (a
 * nested eval comes from an I/O callback). Unread input is discarded.
 */
static JoyResult run_input(JoyContext* ctx, int mode, int pushed_from,
                           const char* include_path)
{
    pEnv env = &ctx->env;
    EnvScanner* scan = &env->scanner;
    unsigned char outer_mode = scan->endmode;
    Operator outer_sym = scan->sym;
    int outer_level = scan->endlevel;
    int outer_push = scan->endpush;
    jmp_buf outer_jmp;
    JoyResult result;
    int err;

    memcpy(outer_jmp, env->error_jmp, sizeof(jmp_buf));
    scan->endmode = mode;
    scan->endlevel = scan->ilevel;
    scan->endpush = pushed_from;
    scan->sym = '.'; /* input that ends after a '.' ends cleanly */
    err = setjmp(env->error_jmp);
    if (!err) {
        if (include_path && include(env, (char*)include_path))
            err = -1;
        else
            repl(env); /* returns only from a bytecode run */
    }
    switch (err) {
    case 0:
        result = JOY_OK;
        break;
    case -1:
        result = JOY_ERROR_IO;
        break;
    case ABORT_EOI:
        result = JOY_OK;
        if (scan->sym != '.') {
            joy_report_error(env, JOY_ERROR_SYNTAX, "syntax error: input ends before '.'\n");
            result = JOY_ERROR_SYNTAX;
        }
        break;
    case ABORT_QUIT:
        result = JOY_ERROR_QUIT;
        break;
    default:
        result = JOY_ERROR_ABORT;
        break;
    }
    while (scan->ilevel > scan->endlevel) {
        fclose(scan->srcfile);
        scan->srcfile = scan->infile[--scan->ilevel].fp;
        scan->linenum = scan->infile[scan->ilevel].line;
        scan->srcfilename = scan->infile[scan->ilevel].name;
    }
    if (vec_size(env->pushback) > scan->endpush)
        vec_setsize(env->pushback, scan->endpush);
    /* The stack is unspecified after an error; the dumps must be empty. A
     * nested eval leaves them: the outer statement is still running. */
    if (result != JOY_OK && ctx->eval_depth == 1)
        env->conts = env->dump = env->dump1 = env->dump2 = env->dump3 =
            env->dump4 = env->dump5 = 0;
    scan->endmode = outer_mode;
    scan->sym = outer_sym;
    scan->endlevel = outer_level;
    scan->endpush = outer_push;
    memcpy(env->error_jmp, outer_jmp, sizeof(jmp_buf));
    ctx->last_result = result;
    return result;
}

/*
 * Evaluate a string of Joy code.
 */
static JoyResult eval_string(JoyContext* ctx, const char* source)
{
    pEnv env = &ctx->env;
    size_t i;
    int base;

    if (!env->scanner.srcfile)
        inilinebuffer(env);
    base = vec_size(env->pushback);
    vec_push(env->pushback, '\n'); /* ends the last token before the input */
    for (i = strlen(source); i > 0; i--)
        vec_push(env->pushback, source[i - 1]);
    return run_input(ctx, EVAL_STRING, base, NULL);
}

/*
 * Evaluate Joy code from a file. The file stands in for the current input
 * level and stays open: it belongs to the caller.
 */
static JoyResult eval_file(JoyContext* ctx, FILE* fp, const char* filename)
{
    pEnv env = &ctx->env;
    EnvScanner* scan = &env->scanner;
    FILE* outer_fp;
    char* outer_name;
    int outer_line, outer_pos;
    JoyResult result;

    if (!scan->srcfile)
        inilinebuffer(env);
    outer_fp = scan->srcfile;
    outer_name = scan->srcfilename;
    outer_line = scan->linenum;
    outer_pos = scan->linepos;
    scan->srcfile = scan->infile[scan->ilevel].fp = fp;
    scan->srcfilename = (char*)(filename ? filename : "file");
    scan->linenum = scan->linepos = 0;
    result = run_input(ctx, EVAL_FILE, vec_size(env->pushback), NULL);
    scan->srcfile = scan->infile[scan->ilevel].fp = outer_fp;
    scan->srcfilename = outer_name;
    scan->linenum = outer_line;
    scan->linepos = outer_pos;
    return result;
}

/*
 * Load the standard library: read it to the end, like an included file.
 */
static JoyResult load_stdlib(JoyContext* ctx, const char* lib_path)
{
    pEnv env = &ctx->env;

    if (!env->scanner.srcfile)
        inilinebuffer(env);
    return run_input(ctx, EVAL_STRING, vec_size(env->pushback),
                     lib_path ? lib_path : "usrlib.joy");
}

/*
 * The embedder's stack is unknown at joy_create, and calls may come from any
 * frame. So each outermost call scans the stack from its own frame: GC-owned
 * pointers exist in C locals only during a call. A nested call must keep the
 * outer bound, which covers it.
 */
static void enter_eval(JoyContext* ctx, char* frame)
{
    if (ctx->eval_depth++ == 0) {
        bottom_of_stack = frame; /* read by GC_gcollect */
        gc_ctx_set_stack_bottom(gc_get_default_context(), frame);
    }
}

static void leave_eval(JoyContext* ctx)
{
    if (--ctx->eval_depth == 0) {
        bottom_of_stack = NULL;
        gc_ctx_set_stack_bottom(gc_get_default_context(), NULL);
    }
}

JoyResult joy_eval_string(JoyContext* ctx, const char* source)
{
    char frame;
    JoyResult result;

    if (!ctx || !source)
        return JOY_ERROR_RUNTIME;
    enter_eval(ctx, &frame);
    result = eval_string(ctx, source);
    leave_eval(ctx);
    return result;
}

JoyResult joy_eval_file(JoyContext* ctx, FILE* fp, const char* filename)
{
    char frame;
    JoyResult result;

    if (!ctx || !fp)
        return JOY_ERROR_IO;
    enter_eval(ctx, &frame);
    result = eval_file(ctx, fp, filename);
    leave_eval(ctx);
    return result;
}

JoyResult joy_load_stdlib(JoyContext* ctx, const char* lib_path)
{
    char frame;
    JoyResult result;

    if (!ctx)
        return JOY_ERROR_RUNTIME;
    enter_eval(ctx, &frame);
    result = load_stdlib(ctx, lib_path);
    leave_eval(ctx);
    return result;
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
 * Get number of nodes currently in use, as __memoryindex reports.
 */
size_t joy_memory_used(JoyContext* ctx)
{
    if (!ctx)
        return 0;
    return (size_t)ctx->env.memoryindex;
}

/*
 * Get the node capacity of the heap, as __memorymax reports.
 */
size_t joy_memory_max(JoyContext* ctx)
{
    if (!ctx)
        return 0;
    return ctx->env.memorymax;
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
