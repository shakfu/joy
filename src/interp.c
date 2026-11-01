/* FILE: interp.c */
/*
 *  module  : interp.c
 *  version : 1.87
 *  date    : 11/15/24
 */

/*
07-May-03 condnestrec
17-Mar-03 modules
04-Dec-02 argc, argv
04-Nov-02 undefs
03-Apr-02 # comments
01-Feb-02 case

30-Oct-01 Fixed Bugs in file interp.c :

   1. division (/) by zero when one or both operands are floats
   2. fremove and frename gave wrong truth value
      (now success  => true)
   3. nullary, unary, binary, ternary had two bugs:
      a. coredump when there was nothing to push
     e.g.   11 22 [pop pop] nullary
      b. produced circular ("infinite") list when no
     new node had been created
     e.g.   11 22 [pop] nullary
   4. app4 combinator was wrong.
      Also renamed:  app2 -> unary2,  app3 -> unary3, app4 -> unary4
      the old names can still be used, but are declared obsolete.
   5. Small additions to (raw) Joy:
      a)  putchars - previously defined in library file inilib.joy
      b) fputchars (analogous, for specified file)
     fputstring (== fputchars for Heiko Kuhrt's program)
*/
#include "builtin.h"
#include "globals.h"
#include "parallel.h"

/*
 * Profiler timing helper - returns current time in nanoseconds.
 * Uses platform-specific high-resolution clocks.
 */
static inline int64_t profiler_time_ns(void)
{
#if defined(__APPLE__)
    return (int64_t)clock_gettime_nsec_np(CLOCK_MONOTONIC);
#elif defined(_POSIX_TIMERS) && _POSIX_TIMERS > 0
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
#else
    return (int64_t)clock() * (1000000000LL / CLOCKS_PER_SEC);
#endif
}

/*
 * profiler_enter - Called when entering a user-defined function.
 * Records entry time and pushes a frame onto the call stack.
 */
static void profiler_enter(pEnv env, int sym_idx)
{
    EnvProfiler* prof = &env->profiler;

    /* Ensure entries vector is large enough */
    size_t needed = (size_t)sym_idx + 1;
    if (!prof->entries) {
        vec_init(prof->entries);
    }
    while ((size_t)vec_size(prof->entries) < needed) {
        ProfileEntry zero = {0, 0, 0};
        vec_push(prof->entries, zero);
    }

    /* Increment call count */
    vec_at(prof->entries, sym_idx).call_count++;

    /* Push call frame if there's room */
    if (prof->call_depth < DISPLAYMAX) {
        prof->call_stack[prof->call_depth].sym_idx = sym_idx;
        prof->call_stack[prof->call_depth].entry_time_ns = profiler_time_ns();
        prof->call_stack[prof->call_depth].child_time_ns = 0;
        prof->call_depth++;
    }
}

/*
 * profiler_exit - Called when exiting a user-defined function.
 * Calculates elapsed time and updates total/self time stats.
 */
static void profiler_exit(pEnv env, int sym_idx)
{
    EnvProfiler* prof = &env->profiler;
    int64_t now = profiler_time_ns();

    if (prof->call_depth <= 0)
        return;

    prof->call_depth--;
    int64_t entry_time = prof->call_stack[prof->call_depth].entry_time_ns;
    int64_t child_time = prof->call_stack[prof->call_depth].child_time_ns;
    int64_t elapsed = now - entry_time;
    int64_t self_time = elapsed - child_time;

    /* Update this symbol's timing */
    vec_at(prof->entries, sym_idx).total_time_ns += elapsed;
    vec_at(prof->entries, sym_idx).self_time_ns += self_time;

    /* Propagate elapsed time to parent as child_time */
    if (prof->call_depth > 0) {
        prof->call_stack[prof->call_depth - 1].child_time_ns += elapsed;
    }
}

#if defined(JOY_PARALLEL) && defined(NOBDW)
/*
 * Copy one node between the memories of a parent and a task. Lazy sequences
 * and dictionaries hold indices into their owner's memory, and files are
 * shared state: a task that meets one stops, so that the parent replays it.
 */
Index parallel_copy_value(pEnv dst, Node* src, Index node)
{
    Types u = src[node].u;
    Operator op = src[node].op;

    switch (op) {
    case FILE_:
    case LAZY_:
    case DICT_:
        abortexecution_(dst, ABORT_RETRY);
        break;
    case STRING_:
    case BIGNUM_:
        u.str = (char*)&src[node].u; /* newnode copies it */
        break;
    case LIST_:
        /* Recursion is bounded by the nesting depth */
        u.lis = parallel_copy_chain(dst, src, src[node].u.lis);
        break;
#ifdef JOY_NATIVE_TYPES
    case VECTOR_:
        if (u.vec) {
            size_t size = sizeof(VectorData) + u.vec->len * sizeof(double);
            u.vec = memcpy(GC_CTX_MALLOC_ATOMIC(dst, size), u.vec, size);
        }
        break;
    case MATRIX_:
        if (u.mat) {
            size_t size = sizeof(MatrixData)
                + u.mat->rows * u.mat->cols * sizeof(double);
            u.mat = memcpy(GC_CTX_MALLOC_ATOMIC(dst, size), u.mat, size);
        }
        break;
#endif
    default:
        break;
    }
    return newnode(dst, op, u, 0);
}

/* Iterative along next, for long lists. dump4 and dump5 root head and tail. */
Index parallel_copy_chain(pEnv env, Node* src, Index node)
{
    Index head, copied;

    if (!node)
        return 0;
    env->dump4 = LIST_NEWNODE(0, env->dump4);
    env->dump5 = LIST_NEWNODE(0, env->dump5);
    for (; node; node = src[node].next) {
        copied = parallel_copy_value(env, src, node);
        if (env->memory[env->dump4].u.lis)
            env->memory[env->memory[env->dump5].u.lis].next = copied;
        else
            env->memory[env->dump4].u.lis = copied;
        env->memory[env->dump5].u.lis = copied;
    }
    head = env->memory[env->dump4].u.lis;
    POP(env->dump5);
    POP(env->dump4);
    return head;
}
#endif /* JOY_PARALLEL && NOBDW */

static void writestack(pEnv env, Index n)
{
    if (n) {
        writestack(env, nextnode1(n));
        if (nextnode1(n))
            putchar(' ');
        writefactor(env, n, stdout);
    }
}

/*
 * debugger_prompt - prompt user for debugger command in step mode.
 * Commands: s=step, c=continue to next breakpoint, q=quit
 */
static void debugger_prompt(pEnv env)
{
    int ch;
    printf("[s]tep [c]ontinue [q]uit> ");
    fflush(stdout);

    SetRaw();
    ch = getchar();
    /* SetRaw pushes screen dimensions; discard them */
    POP(env->stck);
    POP(env->stck);
    SetNormal();
    putchar('\n');

    switch (ch) {
    case 's': case 'S': case '\n': case '\r':
        env->config.stepping = 1;
        break;
    case 'c': case 'C':
        env->config.stepping = 3;  /* continue to next breakpoint */
        break;
    case 'q': case 'Q':
        env->config.stepping = 0;
        env->config.debugging = 0;
        execerror(env, "quit", "debugger");
        break;
    }
}

#ifdef COMPILER
int count_quot(pEnv env)
{
    Node* node;
    int count = 0;

    for (node = env->stck; node; node = node->next)
        if (node->op == LIST_)
            count++;
        else
            break;
    return count;
}

int is_valid_C_identifier(char* str)
{
    if (*str != '_' && !isalpha((int)*str))
        return 0;
    for (str++; *str; str++)
        if (*str != '_' && !isalnum((int)*str))
            return 0;
    return 1;
}
#endif

/*
 * exec_term, but if an error unwinds through n, call cleanup(env, arg) before
 * passing the error on to the enclosing handler.
 */
void exec_term_protected(pEnv env, Index n, void (*cleanup)(pEnv, void*),
                         void* arg)
{
    jmp_buf outer;
    int code;

    memcpy(outer, env->error_jmp, sizeof(jmp_buf));
    if ((code = setjmp(env->error_jmp)) != 0) {
        memcpy(env->error_jmp, outer, sizeof(jmp_buf));
        cleanup(env, arg);
        longjmp(env->error_jmp, code);
    }
    exec_term(env, n);
    memcpy(env->error_jmp, outer, sizeof(jmp_buf));
}

/*
 * exec_term evaluates a sequence of factors. There is no protection against
 * recursion without end condition: it will overflow the call stack.
 */
void exec_term(pEnv env, Index n)
{
    Index p;
    int index;
    Entry ent;
#ifdef COMPILER
    const char* ptr;
    int leng, nofun;
#endif

start:
    env->stats.calls++;
    if (!n)
        return; /* skip empty program */
#ifdef NOBDW
    env->conts = LIST_NEWNODE(n, env->conts); /* root for garbage collector */
    while (nodevalue(env->conts).lis) {
#else
    while (n) {
#endif
#ifdef TRACEGC
        if (env->config.tracegc > 5) {
            printf("exec_term1: ");
            printnode(env, n);
        }
#endif
#ifdef NOBDW
        p = nodevalue(env->conts).lis;
        POP(nodevalue(env->conts).lis);
#else
        p = n;
#endif
        env->stats.opers++;

        /* Check for breakpoint hit on user-defined symbols */
        {
            int hit_breakpoint = 0;
            if (nodetype(p) == USR_ && env->config.breakpoints) {
                int sym_idx = nodevalue(p).ent;
                size_t bp_count = vec_size(env->config.breakpoints);
                for (size_t i = 0; i < bp_count; i++) {
                    if (vec_at(env->config.breakpoints, i) == sym_idx) {
                        hit_breakpoint = 1;
                        break;
                    }
                }
            }

            /* Display state if debugging, stepping, or hit breakpoint */
            if (env->config.debugging || env->config.stepping || hit_breakpoint) {
                writestack(env, env->stck);
                printf(" : ");
                writeterm(env, p, stdout);
                putchar('\n');
                fflush(stdout);

                if (hit_breakpoint) {
                    Entry ent = vec_at(env->symtab, nodevalue(p).ent);
                    printf("[break: %s]\n", ent.name);
                    env->config.stepping = 1;
                }

                if (env->config.stepping == 1) {
                    debugger_prompt(env);
                }
            }
        }

        switch (nodetype(p)) {
        case ILLEGAL_:
        case COPIED_:
            /* A stale index after GC: count it, so the exit status shows it */
            env->error.count++;
            fflush(stdout);
            fputs("exec_term: attempting to execute bad node\n", stderr);
#ifdef TRACEGC
            printnode(env, p);
#endif
            return;
        case USR_:
            index = nodevalue(p).ent;
            ent = vec_at(env->symtab, index);
            if (!ent.u.body) {
                if (env->config.undeferror)
                    execerror(env, "definition", ent.name);
#ifdef NOBDW
                continue; /* skip empty body */
#else
                break;
#endif
            }
            /* Profiler entry hook */
            if (env->profiler.enabled) {
                profiler_enter(env, index);
            }
#ifdef COMPILER
            if (env->compiling > 0) {
                /*
                 * Functions are inlined unless they are called recursively.
                 */
                if ((ent.cflags & IS_ACTIVE) == 0) {
                    /*
                     * The ACTIVE flag prevents endless recursion.
                     */
                    ent.cflags |= IS_ACTIVE;
                    vec_at(env->symtab, index) = ent;
                    exec_term(env, ent.u.body);
                    /*
                     * Update symbol table, but first reread the entry.
                     */
                    ent = vec_at(env->symtab, index);
                    ent.cflags &= ~IS_ACTIVE;
                    vec_at(env->symtab, index) = ent;
                    if (env->profiler.enabled) {
                        profiler_exit(env, index);
                    }
                    break;
                }
                /*
                 * A recursive function can be printed as is, if the name is
                 * a valid C identifier and otherwise it is made into one.
                 */
                if (!is_valid_C_identifier(ent.name)) {
                    ptr = identifier(ent.name);
                    leng = strlen(ptr) + 4;
                    ent.name = GC_CTX_MALLOC_ATOMIC(env, leng);
                    snprintf(ent.name, leng, "do_%s", ptr);
                }
                /*
                 * The USED flag causes the function to be printed.
                 */
                ent.cflags |= IS_USED;
                vec_at(env->symtab, index) = ent;
                printstack(env);
                fprintf(env->outfp, "%s(env);\n", ent.name);
                if (env->profiler.enabled) {
                    profiler_exit(env, index);
                }
                break;
            }
#endif
            {
                Index body = ent.u.body;
                /* Read before the copy: a GC during it invalidates p */
                int is_tail = !nextnode1(p);
#if defined(JOY_PARALLEL) && defined(NOBDW)
                /* Bodies live in the memory of the root environment */
                if (env->parent_memory)
                    body = parallel_copy_chain(env, env->parent_memory, body);
#endif
                if (is_tail) {
#ifdef NOBDW
                    POP(env->conts);
#endif
                    /* Profiler exit hook for tail call */
                    if (env->profiler.enabled) {
                        profiler_exit(env, index);
                    }
                    n = body;
                    goto start; /* tail call optimization */
                }
                exec_term(env, body); /* subroutine call */
            }
            /* Profiler exit hook for normal return */
            if (env->profiler.enabled) {
                profiler_exit(env, index);
            }
            break;
        case ANON_FUNCT_:
#ifdef COMPILER
            if (env->compiling > 0) {
                index = operindex(env, nodevalue(p).proc);
                ent = vec_at(env->symtab, index);
                /*
                 * Functions that have a template are filled with the
                 * quotations they need. When compiling for Soy, the templates
                 * are not used. The nofun flag is also set when the template
                 * file was not found.
                 */
                nofun = env->compiling == 3;
                if (!nofun && ent.qcode && ent.qcode <= count_quot(env)) {
                    if (instance(env, ent.name, ent.qcode))
                        break;
                    nofun = 1;
                }
                /*
                 * Functions are flagged as used, even when they are
                 * evaluated at compile time.
                 */
                ent = vec_at(env->symtab, index);
                ent.cflags |= IS_USED;
                vec_at(env->symtab, index) = ent;
                /*
                 * An exception needs to be made for dup_ in case the stack
                 * contains a list.
                 */
                if (env->stck && nodetype(env->stck) == LIST_
                    && ent.u.proc == dup_)
                    nofun = 1;
                /*
                 * Functions that cannot be evaluated at compile time
                 * are sent to output. There is no need for a nickname.
                 */
                if (nofun || ent.nofun) {
                    printstack(env);
                    fprintf(env->outfp, "%s_(env);\n", ent.name);
                    break;
                }
            }
#endif
            (*nodevalue(p).proc)(env);
            break;
        case BOOLEAN_:
        case CHAR_:
        case INTEGER_:
        case SET_:
        case STRING_:
        case LIST_:
        case FLOAT_:
        case FILE_:
        case DICT_:
        case LAZY_:
#ifdef JOY_NATIVE_TYPES
        case VECTOR_:
        case MATRIX_:
#endif
            GNULLARY(p);
            break;
        default:
            execerror(env, "valid factor", "exec_term");
        }
#ifdef TRACEGC
        if (env->config.tracegc > 5) {
            printf("exec_term2: ");
            printnode(env, p);
        }
#endif
#ifndef NOBDW
        POP(n);
#endif
    }
#ifdef NOBDW
    POP(env->conts);
#endif
}

/* END of INTERP.C */
