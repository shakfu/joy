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

#ifdef JOY_PARALLEL
/*
 * Copy a single node (not the next chain) from parent memory to child memory.
 * Returns the new node with next=0.
 */
static Index copy_body_from_parent(pEnv env, Index node);

static Index copy_single_body_node(pEnv env, Index node)
{
#ifdef NOBDW
    Node* pmem = env->parent_memory;
    Types u;
    Operator op = pmem[node].op;

    switch (op) {
    case INTEGER_:
    case BOOLEAN_:
    case CHAR_:
    case SET_:
    case FLOAT_:
        u = pmem[node].u;
        break;
    case STRING_:
    case BIGNUM_:
        u.str = GC_CTX_STRDUP(env, (char*)&pmem[node].u);
        break;
    case LIST_:
        /* Recursively copy list contents (bounded by nesting depth) */
        u.lis = copy_body_from_parent(env, pmem[node].u.lis);
        break;
#ifdef JOY_NATIVE_TYPES
    case VECTOR_:
        /* Deep-copy VectorData */
        if (pmem[node].u.vec) {
            VectorData* old_vec = pmem[node].u.vec;
            size_t size = sizeof(VectorData) + old_vec->len * sizeof(double);
            VectorData* new_vec = GC_CTX_MALLOC_ATOMIC(env, size);
            memcpy(new_vec, old_vec, size);
            u.vec = new_vec;
        } else {
            u.vec = NULL;
        }
        break;
    case MATRIX_:
        /* Deep-copy MatrixData */
        if (pmem[node].u.mat) {
            MatrixData* old_mat = pmem[node].u.mat;
            size_t size = sizeof(MatrixData) + old_mat->rows * old_mat->cols * sizeof(double);
            MatrixData* new_mat = GC_CTX_MALLOC_ATOMIC(env, size);
            memcpy(new_mat, old_mat, size);
            u.mat = new_mat;
        } else {
            u.mat = NULL;
        }
        break;
#endif /* JOY_NATIVE_TYPES */
    case USR_:
        /* Symbol references stay as-is */
        u = pmem[node].u;
        break;
    default:
        u = pmem[node].u;
        break;
    }

    return newnode(env, op, u, 0);
#else
    return node;
#endif
}

/*
 * Copy a node list from parent memory to child memory for parallel execution.
 * This is needed because symbol bodies live in parent memory.
 *
 * IMPORTANT: Uses iteration for the next-chain to avoid stack overflow on
 * long lists. Only LIST_ contents use recursion (bounded by nesting depth).
 */
static Index copy_body_from_parent(pEnv env, Index node)
{
    if (!node || !env->parent_memory)
        return node;  /* Not in parallel context or empty */

#ifdef NOBDW
    Node* pmem = env->parent_memory;
    Index current = node;

    /*
     * GC protection: Push protection nodes onto dump4 (head) and dump5 (tail).
     * These are GC roots, so values stored in them survive garbage collection.
     */
    {
        Types u_prot;
        u_prot.lis = 0;
        env->dump4 = newnode(env, LIST_, u_prot, env->dump4);
        env->dump5 = newnode(env, LIST_, u_prot, env->dump5);
    }

    while (current) {
        /* Copy this single node (with recursive list copy for LIST_ type) */
        Index new_node = copy_single_body_node(env, current);

        if (!env->memory[env->dump4].u.lis) {
            /* First node becomes head and tail */
            env->memory[env->dump4].u.lis = new_node;
            env->memory[env->dump5].u.lis = new_node;
        } else {
            /* Link previous tail to new node, update tail */
            Index tail = env->memory[env->dump5].u.lis;
            env->memory[tail].next = new_node;
            env->memory[env->dump5].u.lis = new_node;
        }

        current = pmem[current].next;
    }

    /* Extract head before popping */
    Index head = env->memory[env->dump4].u.lis;

    /* Pop the protection nodes */
    env->dump5 = env->memory[env->dump5].next;
    env->dump4 = env->memory[env->dump4].next;

    return head;
#else
    return node;
#endif
}
#endif /* JOY_PARALLEL */

static void writestack(pEnv env, Index n)
{
    if (n) {
        writestack(env, nextnode1(n));
        if (nextnode1(n))
            putchar(' ');
        writefactor(env, n, stdout);
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
        if (env->config.debugging) {
            writestack(env, env->stck);
            if (env->config.debugging == 2) {
                printf(" : ");
                writeterm(env, p, stdout);
            }
            putchar('\n');
            fflush(stdout);
        }
        switch (nodetype(p)) {
        case ILLEGAL_:
        case COPIED_:
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
                break;
            }
#endif
            {
                Index body = ent.u.body;
#if defined(JOY_PARALLEL) && defined(NOBDW)
                /* In parallel context, copy body from parent memory */
                if (env->parent_memory)
                    body = copy_body_from_parent(env, body);
#endif
                if (!nextnode1(p)) {
#ifdef NOBDW
                    POP(env->conts);
#endif
                    n = body;
                    goto start; /* tail call optimization */
                }
                exec_term(env, body); /* subroutine call */
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
