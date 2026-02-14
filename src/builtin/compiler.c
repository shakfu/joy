/**
 * compiler.c - Joy to C code generator
 *
 * Compiles Joy quotations to C source code for native execution.
 * The generated code links against libjoycore for runtime support.
 */

#include "globals.h"
#include "builtin.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#if  defined(__APPLE__) || defined(__linux__)
#include <stdarg.h>
#endif

/* String buffer for building output */
typedef struct {
    char* data;
    size_t len;
    size_t cap;
} CompilerBuf;

static void cbuf_init(CompilerBuf* b) {
    b->cap = 4096;
    b->data = GC_malloc(b->cap);
    b->data[0] = '\0';
    b->len = 0;
}

static void cbuf_ensure(CompilerBuf* b, size_t need) {
    if (b->len + need >= b->cap) {
        while (b->len + need >= b->cap)
            b->cap *= 2;
        char* newdata = GC_malloc(b->cap);
        memcpy(newdata, b->data, b->len + 1);
        b->data = newdata;
    }
}

static void cbuf_append(CompilerBuf* b, const char* s) {
    size_t slen = strlen(s);
    cbuf_ensure(b, slen + 1);
    memcpy(b->data + b->len, s, slen + 1);
    b->len += slen;
}

static void cbuf_printf(CompilerBuf* b, const char* fmt, ...) {
    va_list args;
    char tmp[1024];
    va_start(args, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, args);
    va_end(args);
    cbuf_append(b, tmp);
}

/* Track collected user functions */
#define MAX_USER_FUNCS 256

/* Compiler state */
typedef struct {
    pEnv env;
    CompilerBuf out;
    CompilerBuf fwd;      /* Forward declarations */
    CompilerBuf funcs;    /* Function definitions */
    int quot_counter;     /* For unique quotation names */
    int loop_counter;     /* For unique loop variable names */
    int inline_ops;       /* Whether to inline simple ops */
    int fold_constants;   /* Whether to evaluate constant expressions at compile time */
    /* User-defined function tracking */
    int user_funcs[MAX_USER_FUNCS];  /* Entry indices of collected user functions */
    int user_func_count;
    int collecting_deps;  /* Flag to prevent infinite recursion during collection */
} Compiler;

/* Forward declarations */
static void emit_term(Compiler* c, Index n, int indent);
static void emit_quotation_body(Compiler* c, Index list, int indent);
static int try_emit_while(Compiler* c, Index* p, int indent);
static int try_emit_times(Compiler* c, Index* p, int indent);
static int try_fold_constants(Compiler* c, Index* p, int indent);
static void collect_user_func(Compiler* c, int ent_idx);
static void collect_user_funcs_from_body(Compiler* c, Index body);

/* Sanitize Joy name to valid C identifier */
static void sanitize_name(const char* name, char* out, size_t outlen) {
    size_t i = 0, j = 0;

    /* Prefix with joy_word_ */
    const char* prefix = "joy_word_";
    while (*prefix && j < outlen - 1)
        out[j++] = *prefix++;

    while (name[i] && j < outlen - 1) {
        char c = name[i++];
        if (isalnum(c)) {
            out[j++] = c;
        } else if (c == '-') {
            out[j++] = '_';
        } else if (c == '+') {
            const char* s = "_plus";
            while (*s && j < outlen - 1) out[j++] = *s++;
        } else if (c == '*') {
            const char* s = "_star";
            while (*s && j < outlen - 1) out[j++] = *s++;
        } else if (c == '/') {
            const char* s = "_slash";
            while (*s && j < outlen - 1) out[j++] = *s++;
        } else if (c == '<') {
            const char* s = "_lt";
            while (*s && j < outlen - 1) out[j++] = *s++;
        } else if (c == '>') {
            const char* s = "_gt";
            while (*s && j < outlen - 1) out[j++] = *s++;
        } else if (c == '=') {
            const char* s = "_eq";
            while (*s && j < outlen - 1) out[j++] = *s++;
        } else if (c == '?') {
            const char* s = "_p";
            while (*s && j < outlen - 1) out[j++] = *s++;
        } else if (c == '!') {
            const char* s = "_bang";
            while (*s && j < outlen - 1) out[j++] = *s++;
        } else {
            out[j++] = '_';
        }
    }
    out[j] = '\0';
}

/* Check if a user function has already been collected */
static int is_user_func_collected(Compiler* c, int ent_idx) {
    for (int i = 0; i < c->user_func_count; i++) {
        if (c->user_funcs[i] == ent_idx) return 1;
    }
    return 0;
}

/* Collect a user function for later emission */
static void collect_user_func(Compiler* c, int ent_idx) {
    pEnv env = c->env;

    /* Skip if already collected or at capacity */
    if (is_user_func_collected(c, ent_idx)) return;
    if (c->user_func_count >= MAX_USER_FUNCS) return;

    /* Add to collection */
    c->user_funcs[c->user_func_count++] = ent_idx;

    /* Recursively collect dependencies from this function's body */
    Entry ent = vec_at(env->symtab, ent_idx);
    if (ent.is_user && ent.u.body) {
        collect_user_funcs_from_body(c, ent.u.body);
    }
}

/* Collect all user functions referenced in a quotation body */
static void collect_user_funcs_from_body(Compiler* c, Index body) {
    pEnv env = c->env;

    for (Index p = body; p; p = nextnode1(p)) {
        if (nodetype(p) == USR_) {
            int ent_idx = nodevalue(p).ent;
            Entry ent = vec_at(env->symtab, ent_idx);
            if (ent.is_user) {
                collect_user_func(c, ent_idx);
            }
        } else if (nodetype(p) == LIST_) {
            /* Recurse into nested quotations */
            collect_user_funcs_from_body(c, nodevalue(p).lis);
        }
    }
}

/* Emit a user-defined function as C code */
static void emit_user_func(Compiler* c, int ent_idx) {
    pEnv env = c->env;
    Entry ent = vec_at(env->symtab, ent_idx);

    if (!ent.is_user || !ent.u.body) return;

    char cname[256];
    sanitize_name(ent.name, cname, sizeof(cname));

    /* Emit function definition */
    cbuf_printf(&c->funcs, "/* User-defined: %s */\n", ent.name);
    cbuf_printf(&c->funcs, "static void %s(pEnv env)\n{\n", cname);

    /* Save current output buffer and switch to funcs buffer temporarily */
    CompilerBuf saved = c->out;
    c->out = c->funcs;

    emit_quotation_body(c, ent.u.body, 1);

    /* Restore and update funcs buffer */
    c->funcs = c->out;
    c->out = saved;

    cbuf_append(&c->funcs, "}\n\n");
}

/* Emit forward declarations for all collected user functions */
static void emit_user_func_declarations(Compiler* c) {
    pEnv env = c->env;

    if (c->user_func_count == 0) return;

    cbuf_append(&c->fwd, "/* User-defined function declarations */\n");
    for (int i = 0; i < c->user_func_count; i++) {
        Entry ent = vec_at(env->symtab, c->user_funcs[i]);
        char cname[256];
        sanitize_name(ent.name, cname, sizeof(cname));
        cbuf_printf(&c->fwd, "static void %s(pEnv env);\n", cname);
    }
    cbuf_append(&c->fwd, "\n");
}

/* Emit all collected user function definitions */
static void emit_user_func_definitions(Compiler* c) {
    for (int i = 0; i < c->user_func_count; i++) {
        emit_user_func(c, c->user_funcs[i]);
    }
}

/* Map Joy builtin names to C function names */
static const char* builtin_to_c(const char* name) {
    /* Stack operations */
    if (strcmp(name, "dup") == 0) return "dup_";
    if (strcmp(name, "pop") == 0) return "pop_";
    if (strcmp(name, "swap") == 0) return "swap_";
    if (strcmp(name, "rollup") == 0) return "rollup_";
    if (strcmp(name, "rolldown") == 0) return "rolldown_";
    if (strcmp(name, "rotate") == 0) return "rotate_";
    if (strcmp(name, "dupd") == 0) return "dupd_";
    if (strcmp(name, "popd") == 0) return "popd_";
    if (strcmp(name, "swapd") == 0) return "swapd_";
    if (strcmp(name, "over") == 0) return "over_";
    if (strcmp(name, "pick") == 0) return "pick_";

    /* Arithmetic */
    if (strcmp(name, "+") == 0) return "plus_";
    if (strcmp(name, "-") == 0) return "minus_";
    if (strcmp(name, "*") == 0) return "mul_";
    if (strcmp(name, "/") == 0) return "divide_";
    if (strcmp(name, "rem") == 0) return "rem_";
    if (strcmp(name, "div") == 0) return "div_";
    if (strcmp(name, "neg") == 0) return "neg_";
    if (strcmp(name, "abs") == 0) return "abs_";
    if (strcmp(name, "succ") == 0) return "succ_";
    if (strcmp(name, "pred") == 0) return "pred_";
    if (strcmp(name, "max") == 0) return "max_";
    if (strcmp(name, "min") == 0) return "min_";

    /* Comparison */
    if (strcmp(name, "<") == 0) return "less_";
    if (strcmp(name, "<=") == 0) return "leql_";
    if (strcmp(name, ">") == 0) return "greater_";
    if (strcmp(name, ">=") == 0) return "geql_";
    if (strcmp(name, "=") == 0) return "equal_";
    if (strcmp(name, "!=") == 0) return "neql_";

    /* Logic */
    if (strcmp(name, "and") == 0) return "and_";
    if (strcmp(name, "or") == 0) return "or_";
    if (strcmp(name, "not") == 0) return "not_";
    if (strcmp(name, "xor") == 0) return "xor_";

    /* Combinators */
    if (strcmp(name, "i") == 0) return "i_";
    if (strcmp(name, "x") == 0) return "x_";
    if (strcmp(name, "dip") == 0) return "dip_";
    if (strcmp(name, "dipd") == 0) return "dipd_";
    if (strcmp(name, "dipdd") == 0) return "dipdd_";
    if (strcmp(name, "app1") == 0) return "app1_";
    if (strcmp(name, "app2") == 0) return "app2_";
    if (strcmp(name, "app3") == 0) return "app3_";
    if (strcmp(name, "map") == 0) return "map_";
    if (strcmp(name, "fold") == 0) return "fold_";
    if (strcmp(name, "filter") == 0) return "filter_";
    if (strcmp(name, "times") == 0) return "times_";
    if (strcmp(name, "while") == 0) return "while_";
    if (strcmp(name, "ifte") == 0) return "ifte_";
    if (strcmp(name, "cond") == 0) return "cond_";
    if (strcmp(name, "branch") == 0) return "branch_";
    if (strcmp(name, "choice") == 0) return "choice_";
    if (strcmp(name, "nullary") == 0) return "nullary_";
    if (strcmp(name, "unary") == 0) return "unary_";
    if (strcmp(name, "binary") == 0) return "binary_";
    if (strcmp(name, "ternary") == 0) return "ternary_";
    if (strcmp(name, "primrec") == 0) return "primrec_";
    if (strcmp(name, "linrec") == 0) return "linrec_";
    if (strcmp(name, "binrec") == 0) return "binrec_";
    if (strcmp(name, "genrec") == 0) return "genrec_";
    if (strcmp(name, "tailrec") == 0) return "tailrec_";
    if (strcmp(name, "step") == 0) return "step_";
    if (strcmp(name, "infra") == 0) return "infra_";
    if (strcmp(name, "cleave") == 0) return "cleave_";

    /* List operations */
    if (strcmp(name, "first") == 0) return "first_";
    if (strcmp(name, "rest") == 0) return "rest_";
    if (strcmp(name, "cons") == 0) return "cons_";
    if (strcmp(name, "swons") == 0) return "swons_";
    if (strcmp(name, "uncons") == 0) return "uncons_";
    if (strcmp(name, "unswons") == 0) return "unswons_";
    if (strcmp(name, "null") == 0) return "null_";
    if (strcmp(name, "size") == 0) return "size_";
    if (strcmp(name, "concat") == 0) return "concat_";
    if (strcmp(name, "reverse") == 0) return "reverse_";
    if (strcmp(name, "at") == 0) return "at_";
    if (strcmp(name, "of") == 0) return "of_";
    if (strcmp(name, "take") == 0) return "take_";
    if (strcmp(name, "drop") == 0) return "drop_";
    if (strcmp(name, "enconcat") == 0) return "enconcat_";

    /* I/O */
    if (strcmp(name, "put") == 0) return "put_";
    if (strcmp(name, "putch") == 0) return "putch_";
    if (strcmp(name, "get") == 0) return "get_";
    if (strcmp(name, ".") == 0) return "put_";

    /* Type checking */
    if (strcmp(name, "integer") == 0) return "integer_";
    if (strcmp(name, "float") == 0) return "float_";
    if (strcmp(name, "string") == 0) return "string_";
    if (strcmp(name, "list") == 0) return "list_";
    if (strcmp(name, "logical") == 0) return "logical_";
    if (strcmp(name, "char") == 0) return "char_";
    if (strcmp(name, "set") == 0) return "set_";

    /* Misc */
    if (strcmp(name, "id") == 0) return "id_";
    if (strcmp(name, "stack") == 0) return "stack_";
    if (strcmp(name, "unstack") == 0) return "unstack_";
    if (strcmp(name, "newstack") == 0) return "newstack_";
    if (strcmp(name, "unit") == 0) return "unit_";
    if (strcmp(name, "small") == 0) return "small_";
    if (strcmp(name, "name") == 0) return "name_";
    if (strcmp(name, "body") == 0) return "body_";
    if (strcmp(name, "intern") == 0) return "intern_";
    if (strcmp(name, "type") == 0) return "type_";
    if (strcmp(name, "typeof") == 0) return "typeof_";

    return NULL;
}

/* Get inline code for simple operations */
static const char* get_inline_op(const char* name) {
    if (strcmp(name, "dup") == 0)
        return "{ ONEPARAM(\"dup\"); GNULLARY(env->stck); }";
    if (strcmp(name, "pop") == 0)
        return "{ ONEPARAM(\"pop\"); POP(env->stck); }";
    if (strcmp(name, "swap") == 0)
        return "{ TWOPARAMS(\"swap\"); SAVESTACK; GBINARY(SAVED1); GNULLARY(SAVED2); POP(env->dump); }";
    if (strcmp(name, "+") == 0)
        return "if (nodetype(env->stck) == INTEGER_ && nodetype(nextnode1(env->stck)) == INTEGER_) {\n"
               "        BINARY(INTEGER_NEWNODE, nodevalue(nextnode1(env->stck)).num + nodevalue(env->stck).num);\n"
               "    } else { plus_(env); }";
    if (strcmp(name, "-") == 0)
        return "if (nodetype(env->stck) == INTEGER_ && nodetype(nextnode1(env->stck)) == INTEGER_) {\n"
               "        BINARY(INTEGER_NEWNODE, nodevalue(nextnode1(env->stck)).num - nodevalue(env->stck).num);\n"
               "    } else { minus_(env); }";
    if (strcmp(name, "*") == 0)
        return "if (nodetype(env->stck) == INTEGER_ && nodetype(nextnode1(env->stck)) == INTEGER_) {\n"
               "        BINARY(INTEGER_NEWNODE, nodevalue(nextnode1(env->stck)).num * nodevalue(env->stck).num);\n"
               "    } else { mul_(env); }";
    if (strcmp(name, "<") == 0)
        return "if (nodetype(env->stck) == INTEGER_ && nodetype(nextnode1(env->stck)) == INTEGER_) {\n"
               "        BINARY(BOOLEAN_NEWNODE, nodevalue(nextnode1(env->stck)).num < nodevalue(env->stck).num);\n"
               "    } else { less_(env); }";
    if (strcmp(name, ">") == 0)
        return "if (nodetype(env->stck) == INTEGER_ && nodetype(nextnode1(env->stck)) == INTEGER_) {\n"
               "        BINARY(BOOLEAN_NEWNODE, nodevalue(nextnode1(env->stck)).num > nodevalue(env->stck).num);\n"
               "    } else { greater_(env); }";
    if (strcmp(name, "pred") == 0)
        return "if (nodetype(env->stck) == INTEGER_) {\n"
               "        UNARY(INTEGER_NEWNODE, nodevalue(env->stck).num - 1);\n"
               "    } else { pred_(env); }";
    if (strcmp(name, "succ") == 0)
        return "if (nodetype(env->stck) == INTEGER_) {\n"
               "        UNARY(INTEGER_NEWNODE, nodevalue(env->stck).num + 1);\n"
               "    } else { succ_(env); }";
    if (strcmp(name, "rollup") == 0)
        return "{ THREEPARAMS(\"rollup\"); SAVESTACK;\n"
               "      GTERNARY(SAVED1); GNULLARY(SAVED3); GNULLARY(SAVED2); POP(env->dump); }";
    if (strcmp(name, "rolldown") == 0)
        return "{ THREEPARAMS(\"rolldown\"); SAVESTACK;\n"
               "      GTERNARY(SAVED2); GNULLARY(SAVED1); GNULLARY(SAVED3); POP(env->dump); }";
    return NULL;
}

/* Emit indentation */
static void emit_indent(Compiler* c, int indent) {
    for (int i = 0; i < indent; i++)
        cbuf_append(&c->out, "    ");
}

/* Emit code for pushing a quotation onto the stack */
static void emit_quotation_push(Compiler* c, Index list, int indent) {
    pEnv env = c->env;  /* Required for node macros */

    emit_indent(c, indent);

    if (!list) {
        cbuf_append(&c->out, "NULLARY(LIST_NEWNODE, 0);  /* empty quotation */\n");
        return;
    }

    /* Count terms */
    int count = 0;
    for (Index p = list; p; p = nextnode1(p))
        count++;

    cbuf_printf(&c->out, "/* push quotation [%d terms] */\n", count);
    emit_indent(c, indent);
    cbuf_append(&c->out, "{\n");
    emit_indent(c, indent + 1);
    cbuf_append(&c->out, "Index _q = 0;\n");

    /* Build in reverse order */
    Index* terms = GC_malloc(count * sizeof(Index));
    int i = 0;
    for (Index p = list; p; p = nextnode1(p))
        terms[i++] = p;

    for (i = count - 1; i >= 0; i--) {
        Index p = terms[i];
        emit_indent(c, indent + 1);

        switch (nodetype(p)) {
        case INTEGER_:
            cbuf_printf(&c->out, "_q = INTEGER_NEWNODE(%lldLL, _q);\n",
                       (long long)nodevalue(p).num);
            break;

        case FLOAT_:
            cbuf_printf(&c->out, "_q = FLOAT_NEWNODE(%g, _q);\n", nodevalue(p).dbl);
            break;

        case BOOLEAN_:
            cbuf_printf(&c->out, "_q = BOOLEAN_NEWNODE(%d, _q);\n",
                       nodevalue(p).num ? 1 : 0);
            break;

        case CHAR_:
            if (nodevalue(p).num == '\n')
                cbuf_append(&c->out, "_q = CHAR_NEWNODE('\\n', _q);\n");
            else if (nodevalue(p).num == '\t')
                cbuf_append(&c->out, "_q = CHAR_NEWNODE('\\t', _q);\n");
            else if (nodevalue(p).num == '\\')
                cbuf_append(&c->out, "_q = CHAR_NEWNODE('\\\\', _q);\n");
            else if (nodevalue(p).num == '\'')
                cbuf_append(&c->out, "_q = CHAR_NEWNODE('\\'', _q);\n");
            else
                cbuf_printf(&c->out, "_q = CHAR_NEWNODE('%c', _q);\n",
                           (char)nodevalue(p).num);
            break;

        case STRING_: {
            cbuf_append(&c->out, "_q = STRING_NEWNODE(\"");
            const char* s = (const char*)&nodevalue(p);
            while (*s) {
                if (*s == '"') cbuf_append(&c->out, "\\\"");
                else if (*s == '\\') cbuf_append(&c->out, "\\\\");
                else if (*s == '\n') cbuf_append(&c->out, "\\n");
                else if (*s == '\t') cbuf_append(&c->out, "\\t");
                else cbuf_printf(&c->out, "%c", *s);
                s++;
            }
            cbuf_append(&c->out, "\", _q);\n");
            break;
        }

        case USR_: {
            int ent_idx = nodevalue(p).ent;
            Entry ent = vec_at(c->env->symtab, ent_idx);
            const char* cfunc = builtin_to_c(ent.name);
            if (cfunc) {
                cbuf_printf(&c->out, "_q = ANON_FUNCT_NEWNODE(%s, _q);\n", cfunc);
            } else if (ent.is_user) {
                /* Collect this user function for emission */
                collect_user_func(c, ent_idx);
                char cname[256];
                sanitize_name(ent.name, cname, sizeof(cname));
                cbuf_printf(&c->out, "_q = ANON_FUNCT_NEWNODE(%s, _q);\n", cname);
            } else {
                cbuf_printf(&c->out, "_q = USR_NEWNODE(lookup(env, \"%s\"), _q);\n",
                           ent.name);
            }
            break;
        }

        case ANON_FUNCT_: {
            /* Find the function name */
            const char* fname = NULL;
            for (int j = 0; j < (int)vec_size(c->env->symtab); j++) {
                Entry ent = vec_at(c->env->symtab, j);
                if (!ent.is_user && ent.u.proc == nodevalue(p).proc) {
                    fname = builtin_to_c(ent.name);
                    if (!fname) fname = ent.name;
                    break;
                }
            }
            if (fname)
                cbuf_printf(&c->out, "_q = ANON_FUNCT_NEWNODE(%s, _q);\n", fname);
            else
                cbuf_append(&c->out, "/* unknown function */\n");
            break;
        }

        case LIST_:
            cbuf_append(&c->out, "/* nested quotation - TODO */\n");
            break;

        case SET_:
            cbuf_printf(&c->out, "_q = SET_NEWNODE(%lluULL, _q);\n",
                       (unsigned long long)nodevalue(p).set);
            break;

        default:
            cbuf_append(&c->out, "/* unknown term type */\n");
            break;
        }
    }

    emit_indent(c, indent + 1);
    cbuf_append(&c->out, "NULLARY(LIST_NEWNODE, _q);\n");
    emit_indent(c, indent);
    cbuf_append(&c->out, "}\n");
}

/* Check if node is a builtin with given name (handles both USR_ and ANON_FUNCT_) */
static int is_builtin_name(Compiler* c, Index n, const char* name) {
    pEnv env = c->env;

    if (nodetype(n) == USR_) {
        Entry ent = vec_at(env->symtab, nodevalue(n).ent);
        return strcmp(ent.name, name) == 0;
    }

    if (nodetype(n) == ANON_FUNCT_) {
        /* Search for the function pointer in the symbol table */
        void (*proc)(pEnv) = nodevalue(n).proc;
        for (int i = 0; i < (int)vec_size(env->symtab); i++) {
            Entry ent = vec_at(env->symtab, i);
            if (!ent.is_user && ent.u.proc == proc) {
                return strcmp(ent.name, name) == 0;
            }
        }
    }

    return 0;
}

/* Try to emit inlined while loop: [cond] [body] while
 * Returns 1 if pattern matched and emitted, 0 otherwise.
 * Updates *pp to point past the consumed nodes.
 */
static int try_emit_while(Compiler* c, Index* pp, int indent) {
    pEnv env = c->env;
    Index p = *pp;

    /* Need at least 3 nodes: [cond] [body] while */
    if (!p || !nextnode1(p) || !nextnode2(p)) return 0;

    Index cond_node = p;
    Index body_node = nextnode1(p);
    Index while_node = nextnode2(p);

    /* Check pattern: LIST, LIST, while */
    if (nodetype(cond_node) != LIST_) return 0;
    if (nodetype(body_node) != LIST_) return 0;
    if (!is_builtin_name(c, while_node, "while")) return 0;

    /* Pattern matched - emit inline while loop */
    int loop_id = ++c->loop_counter;
    Index cond_body = nodevalue(cond_node).lis;
    Index loop_body = nodevalue(body_node).lis;

    emit_indent(c, indent);
    cbuf_printf(&c->out, "/* inline while loop */\n");
    emit_indent(c, indent);
    cbuf_append(&c->out, "{\n");
    emit_indent(c, indent + 1);
    cbuf_printf(&c->out, "Index _while_stck_%d = env->stck;\n", loop_id);
    emit_indent(c, indent + 1);
    cbuf_append(&c->out, "while (1) {\n");

    /* Restore stack and emit condition */
    emit_indent(c, indent + 2);
    cbuf_printf(&c->out, "env->stck = _while_stck_%d;\n", loop_id);
    emit_quotation_body(c, cond_body, indent + 2);

    /* Check result and break if false */
    emit_indent(c, indent + 2);
    cbuf_append(&c->out, "{\n");
    emit_indent(c, indent + 3);
    cbuf_printf(&c->out, "int _while_result_%d = nodevalue(env->stck).num;\n", loop_id);
    emit_indent(c, indent + 3);
    cbuf_append(&c->out, "POP(env->stck);\n");
    emit_indent(c, indent + 3);
    cbuf_printf(&c->out, "if (!_while_result_%d) break;\n", loop_id);
    emit_indent(c, indent + 2);
    cbuf_append(&c->out, "}\n");

    /* Restore stack and emit body */
    emit_indent(c, indent + 2);
    cbuf_printf(&c->out, "env->stck = _while_stck_%d;\n", loop_id);
    emit_quotation_body(c, loop_body, indent + 2);

    /* Save new stack state */
    emit_indent(c, indent + 2);
    cbuf_printf(&c->out, "_while_stck_%d = env->stck;\n", loop_id);
    emit_indent(c, indent + 1);
    cbuf_append(&c->out, "}\n");

    /* Restore final stack state */
    emit_indent(c, indent + 1);
    cbuf_printf(&c->out, "env->stck = _while_stck_%d;\n", loop_id);
    emit_indent(c, indent);
    cbuf_append(&c->out, "}\n");

    /* Advance past all three nodes */
    *pp = nextnode3(p);
    return 1;
}

/* Try to emit inlined times loop: N [body] times
 * Returns 1 if pattern matched and emitted, 0 otherwise.
 * Updates *pp to point past the consumed nodes.
 */
static int try_emit_times(Compiler* c, Index* pp, int indent) {
    pEnv env = c->env;
    Index p = *pp;

    /* Need at least 3 nodes: N [body] times */
    if (!p || !nextnode1(p) || !nextnode2(p)) return 0;

    Index count_node = p;
    Index body_node = nextnode1(p);
    Index times_node = nextnode2(p);

    /* Check pattern: INTEGER, LIST, USR_("times") */
    if (nodetype(count_node) != INTEGER_) return 0;
    if (nodetype(body_node) != LIST_) return 0;
    if (!is_builtin_name(c, times_node, "times")) return 0;

    /* Pattern matched - emit inline times loop */
    int loop_id = ++c->loop_counter;
    long long count = nodevalue(count_node).num;
    Index loop_body = nodevalue(body_node).lis;

    emit_indent(c, indent);
    cbuf_printf(&c->out, "/* inline times loop (%lld iterations) */\n", count);
    emit_indent(c, indent);
    cbuf_append(&c->out, "{\n");
    emit_indent(c, indent + 1);
    cbuf_printf(&c->out, "int64_t _times_i_%d;\n", loop_id);
    emit_indent(c, indent + 1);
    cbuf_printf(&c->out, "for (_times_i_%d = 0; _times_i_%d < %lldLL; _times_i_%d++) {\n",
               loop_id, loop_id, count, loop_id);

    /* Emit body */
    emit_quotation_body(c, loop_body, indent + 2);

    emit_indent(c, indent + 1);
    cbuf_append(&c->out, "}\n");
    emit_indent(c, indent);
    cbuf_append(&c->out, "}\n");

    /* Advance past all three nodes */
    *pp = nextnode3(p);
    return 1;
}

/*
 * Constant folding - evaluate constant expressions at compile time
 *
 * Handles patterns like:
 *   - INTEGER INTEGER BINOP  (e.g., 2 3 + -> 5)
 *   - INTEGER UNARYOP (e.g., 5 neg -> -5)
 *   - INTEGER dup * (squaring, e.g., 5 dup * -> 25)
 *   - FLOAT FLOAT BINOP
 *   - Chained operations (e.g., 2 3 + 4 * -> 20)
 *
 * Returns 1 if folding occurred and *pp is updated, 0 otherwise.
 */

/* Get the name of a builtin operation from a node */
static const char* get_op_name(Compiler* c, Index n) {
    pEnv env = c->env;
    if (!n) return NULL;

    if (nodetype(n) == USR_) {
        Entry ent = vec_at(env->symtab, nodevalue(n).ent);
        return ent.name;
    }

    if (nodetype(n) == ANON_FUNCT_) {
        void (*proc)(pEnv) = nodevalue(n).proc;
        for (int i = 0; i < (int)vec_size(env->symtab); i++) {
            Entry ent = vec_at(env->symtab, i);
            if (!ent.is_user && ent.u.proc == proc) {
                return ent.name;
            }
        }
    }

    return NULL;
}

/* Check if an operation is a foldable binary operation */
static int is_foldable_binop(const char* name) {
    if (!name) return 0;
    return strcmp(name, "+") == 0 || strcmp(name, "-") == 0 ||
           strcmp(name, "*") == 0 || strcmp(name, "/") == 0 ||
           strcmp(name, "rem") == 0 || strcmp(name, "div") == 0 ||
           strcmp(name, "max") == 0 || strcmp(name, "min") == 0 ||
           strcmp(name, "<") == 0 || strcmp(name, "<=") == 0 ||
           strcmp(name, ">") == 0 || strcmp(name, ">=") == 0 ||
           strcmp(name, "=") == 0 || strcmp(name, "!=") == 0 ||
           strcmp(name, "and") == 0 || strcmp(name, "or") == 0 ||
           strcmp(name, "xor") == 0;
}

/* Check if an operation is a foldable unary operation */
static int is_foldable_unaryop(const char* name) {
    if (!name) return 0;
    return strcmp(name, "neg") == 0 || strcmp(name, "abs") == 0 ||
           strcmp(name, "succ") == 0 || strcmp(name, "pred") == 0 ||
           strcmp(name, "not") == 0 || strcmp(name, "sign") == 0;
}

/* Emit a folded constant result */
static void emit_folded_constant(Compiler* c, int is_float, double dval, int64_t ival,
                                  int is_bool, int indent) {
    emit_indent(c, indent);
    if (is_bool) {
        cbuf_printf(&c->out, "NULLARY(BOOLEAN_NEWNODE, %d);  /* folded */\n", ival ? 1 : 0);
    } else if (is_float) {
        cbuf_printf(&c->out, "NULLARY(FLOAT_NEWNODE, %.17g);  /* folded */\n", dval);
    } else {
        cbuf_printf(&c->out, "NULLARY(INTEGER_NEWNODE, %lldLL);  /* folded */\n", (long long)ival);
    }
}

/* Apply a binary operation to two values, return result.
 * Returns 1 on success, 0 on failure (e.g., division by zero). */
static int apply_binop(const char* op,
                       int a_is_float, double a_dbl, int64_t a_int,
                       int b_is_float, double b_dbl, int64_t b_int,
                       int* r_is_float, double* r_dbl, int64_t* r_int,
                       int* r_is_bool) {
    double a = a_is_float ? a_dbl : (double)a_int;
    double b = b_is_float ? b_dbl : (double)b_int;
    int use_float = a_is_float || b_is_float;

    *r_is_bool = 0;

    if (strcmp(op, "+") == 0) {
        if (use_float) { *r_is_float = 1; *r_dbl = a + b; }
        else { *r_is_float = 0; *r_int = a_int + b_int; }
    } else if (strcmp(op, "-") == 0) {
        if (use_float) { *r_is_float = 1; *r_dbl = a - b; }
        else { *r_is_float = 0; *r_int = a_int - b_int; }
    } else if (strcmp(op, "*") == 0) {
        if (use_float) { *r_is_float = 1; *r_dbl = a * b; }
        else { *r_is_float = 0; *r_int = a_int * b_int; }
    } else if (strcmp(op, "/") == 0) {
        if (use_float) {
            if (b == 0.0) return 0;  /* division by zero */
            *r_is_float = 1; *r_dbl = a / b;
        } else {
            if (b_int == 0) return 0;  /* division by zero */
            *r_is_float = 0; *r_int = a_int / b_int;
        }
    } else if (strcmp(op, "rem") == 0) {
        if (use_float) return 0;  /* rem only for integers */
        if (b_int == 0) return 0;
        *r_is_float = 0; *r_int = a_int % b_int;
    } else if (strcmp(op, "div") == 0) {
        if (use_float) return 0;  /* div only for integers */
        if (b_int == 0) return 0;
        *r_is_float = 0; *r_int = a_int / b_int;
    } else if (strcmp(op, "max") == 0) {
        if (use_float) { *r_is_float = 1; *r_dbl = a > b ? a : b; }
        else { *r_is_float = 0; *r_int = a_int > b_int ? a_int : b_int; }
    } else if (strcmp(op, "min") == 0) {
        if (use_float) { *r_is_float = 1; *r_dbl = a < b ? a : b; }
        else { *r_is_float = 0; *r_int = a_int < b_int ? a_int : b_int; }
    } else if (strcmp(op, "<") == 0) {
        *r_is_bool = 1; *r_is_float = 0; *r_int = a < b;
    } else if (strcmp(op, "<=") == 0) {
        *r_is_bool = 1; *r_is_float = 0; *r_int = a <= b;
    } else if (strcmp(op, ">") == 0) {
        *r_is_bool = 1; *r_is_float = 0; *r_int = a > b;
    } else if (strcmp(op, ">=") == 0) {
        *r_is_bool = 1; *r_is_float = 0; *r_int = a >= b;
    } else if (strcmp(op, "=") == 0) {
        *r_is_bool = 1; *r_is_float = 0; *r_int = a == b;
    } else if (strcmp(op, "!=") == 0) {
        *r_is_bool = 1; *r_is_float = 0; *r_int = a != b;
    } else if (strcmp(op, "and") == 0) {
        *r_is_bool = 1; *r_is_float = 0; *r_int = (a_int != 0) && (b_int != 0);
    } else if (strcmp(op, "or") == 0) {
        *r_is_bool = 1; *r_is_float = 0; *r_int = (a_int != 0) || (b_int != 0);
    } else if (strcmp(op, "xor") == 0) {
        *r_is_bool = 1; *r_is_float = 0; *r_int = ((a_int != 0) != (b_int != 0));
    } else {
        return 0;  /* unknown op */
    }
    return 1;
}

/* Apply a unary operation to a value, return result. */
static int apply_unaryop(const char* op,
                         int a_is_float, double a_dbl, int64_t a_int,
                         int* r_is_float, double* r_dbl, int64_t* r_int,
                         int* r_is_bool) {
    *r_is_bool = 0;

    if (strcmp(op, "neg") == 0) {
        if (a_is_float) { *r_is_float = 1; *r_dbl = -a_dbl; }
        else { *r_is_float = 0; *r_int = -a_int; }
    } else if (strcmp(op, "abs") == 0) {
        if (a_is_float) { *r_is_float = 1; *r_dbl = a_dbl < 0 ? -a_dbl : a_dbl; }
        else { *r_is_float = 0; *r_int = a_int < 0 ? -a_int : a_int; }
    } else if (strcmp(op, "succ") == 0) {
        if (a_is_float) { *r_is_float = 1; *r_dbl = a_dbl + 1; }
        else { *r_is_float = 0; *r_int = a_int + 1; }
    } else if (strcmp(op, "pred") == 0) {
        if (a_is_float) { *r_is_float = 1; *r_dbl = a_dbl - 1; }
        else { *r_is_float = 0; *r_int = a_int - 1; }
    } else if (strcmp(op, "not") == 0) {
        *r_is_bool = 1; *r_is_float = 0; *r_int = a_int == 0;
    } else if (strcmp(op, "sign") == 0) {
        if (a_is_float) {
            *r_is_float = 0;
            *r_int = a_dbl < 0 ? -1 : (a_dbl > 0 ? 1 : 0);
        } else {
            *r_is_float = 0;
            *r_int = a_int < 0 ? -1 : (a_int > 0 ? 1 : 0);
        }
    } else {
        return 0;  /* unknown op */
    }
    return 1;
}

/* Try to fold constant expressions at the current position.
 * Returns 1 if folding occurred and *pp is updated, 0 otherwise.
 *
 * This function uses a simple virtual stack to simulate execution
 * of constant operations, then emits the result.
 */
static int try_fold_constants(Compiler* c, Index* pp, int indent) {
    pEnv env = c->env;
    Index p = *pp;

    if (!p) return 0;

    /* Virtual stack for folding - stores up to 8 values */
    #define MAX_FOLD_STACK 8
    struct {
        int is_float;
        int is_bool;
        double dbl;
        int64_t num;
    } vstack[MAX_FOLD_STACK];
    int vsp = 0;  /* stack pointer */
    int ops_folded = 0;

    /* Try to accumulate constants and fold operations */
    while (p) {
        int typ = nodetype(p);

        if (typ == INTEGER_) {
            if (vsp >= MAX_FOLD_STACK) break;
            vstack[vsp].is_float = 0;
            vstack[vsp].is_bool = 0;
            vstack[vsp].num = nodevalue(p).num;
            vsp++;
            p = nextnode1(p);
            continue;
        }

        if (typ == FLOAT_) {
            if (vsp >= MAX_FOLD_STACK) break;
            vstack[vsp].is_float = 1;
            vstack[vsp].is_bool = 0;
            vstack[vsp].dbl = nodevalue(p).dbl;
            vsp++;
            p = nextnode1(p);
            continue;
        }

        if (typ == BOOLEAN_) {
            if (vsp >= MAX_FOLD_STACK) break;
            vstack[vsp].is_float = 0;
            vstack[vsp].is_bool = 1;
            vstack[vsp].num = nodevalue(p).num;
            vsp++;
            p = nextnode1(p);
            continue;
        }

        /* Check for foldable operations */
        const char* opname = get_op_name(c, p);
        if (!opname) break;

        /* Check for dup followed by binary op (e.g., 5 dup * -> 25) */
        if (strcmp(opname, "dup") == 0 && vsp >= 1) {
            if (vsp >= MAX_FOLD_STACK) break;
            vstack[vsp] = vstack[vsp - 1];  /* duplicate top */
            vsp++;
            p = nextnode1(p);
            ops_folded++;
            continue;
        }

        /* Binary operations */
        if (is_foldable_binop(opname)) {
            if (vsp < 2) break;  /* need 2 operands */

            int r_is_float, r_is_bool;
            double r_dbl;
            int64_t r_int;

            /* Stack: ... a b  ->  ... (a op b)
             * b is on top (vsp-1), a is below (vsp-2) */
            if (!apply_binop(opname,
                            vstack[vsp-2].is_float, vstack[vsp-2].dbl, vstack[vsp-2].num,
                            vstack[vsp-1].is_float, vstack[vsp-1].dbl, vstack[vsp-1].num,
                            &r_is_float, &r_dbl, &r_int, &r_is_bool)) {
                break;  /* operation failed (e.g., div by zero) */
            }

            vsp--;  /* pop b */
            vstack[vsp-1].is_float = r_is_float;
            vstack[vsp-1].is_bool = r_is_bool;
            vstack[vsp-1].dbl = r_dbl;
            vstack[vsp-1].num = r_int;
            p = nextnode1(p);
            ops_folded++;
            continue;
        }

        /* Unary operations */
        if (is_foldable_unaryop(opname)) {
            if (vsp < 1) break;  /* need 1 operand */

            int r_is_float, r_is_bool;
            double r_dbl;
            int64_t r_int;

            if (!apply_unaryop(opname,
                              vstack[vsp-1].is_float, vstack[vsp-1].dbl, vstack[vsp-1].num,
                              &r_is_float, &r_dbl, &r_int, &r_is_bool)) {
                break;  /* operation failed */
            }

            vstack[vsp-1].is_float = r_is_float;
            vstack[vsp-1].is_bool = r_is_bool;
            vstack[vsp-1].dbl = r_dbl;
            vstack[vsp-1].num = r_int;
            p = nextnode1(p);
            ops_folded++;
            continue;
        }

        /* Not a foldable operation, stop here */
        break;
    }

    /* Only emit folded result if we actually folded something */
    if (ops_folded == 0) return 0;

    /* Emit the folded results */
    emit_indent(c, indent);
    cbuf_append(&c->out, "/* constant-folded expression */\n");

    for (int i = 0; i < vsp; i++) {
        emit_folded_constant(c, vstack[i].is_float, vstack[i].dbl, vstack[i].num,
                            vstack[i].is_bool, indent);
    }

    *pp = p;
    return 1;

    #undef MAX_FOLD_STACK
}

/* Emit code for a single term */
static void emit_term(Compiler* c, Index n, int indent) {
    pEnv env = c->env;  /* Required for node macros */

    emit_indent(c, indent);

    switch (nodetype(n)) {
    case INTEGER_:
        cbuf_printf(&c->out, "NULLARY(INTEGER_NEWNODE, %lldLL);\n",
                   (long long)nodevalue(n).num);
        break;

    case FLOAT_:
        cbuf_printf(&c->out, "NULLARY(FLOAT_NEWNODE, %g);\n", nodevalue(n).dbl);
        break;

    case BOOLEAN_:
        cbuf_printf(&c->out, "NULLARY(BOOLEAN_NEWNODE, %d);\n",
                   nodevalue(n).num ? 1 : 0);
        break;

    case CHAR_:
        if (nodevalue(n).num == '\n')
            cbuf_append(&c->out, "NULLARY(CHAR_NEWNODE, '\\n');\n");
        else if (nodevalue(n).num == '\t')
            cbuf_append(&c->out, "NULLARY(CHAR_NEWNODE, '\\t');\n");
        else if (nodevalue(n).num == '\\')
            cbuf_append(&c->out, "NULLARY(CHAR_NEWNODE, '\\\\');\n");
        else if (nodevalue(n).num == '\'')
            cbuf_append(&c->out, "NULLARY(CHAR_NEWNODE, '\\'');\n");
        else
            cbuf_printf(&c->out, "NULLARY(CHAR_NEWNODE, '%c');\n",
                       (char)nodevalue(n).num);
        break;

    case STRING_: {
        cbuf_append(&c->out, "NULLARY(STRING_NEWNODE, \"");
        const char* s = (const char*)&nodevalue(n);
        while (*s) {
            if (*s == '"') cbuf_append(&c->out, "\\\"");
            else if (*s == '\\') cbuf_append(&c->out, "\\\\");
            else if (*s == '\n') cbuf_append(&c->out, "\\n");
            else if (*s == '\t') cbuf_append(&c->out, "\\t");
            else cbuf_printf(&c->out, "%c", *s);
            s++;
        }
        cbuf_append(&c->out, "\");\n");
        break;
    }

    case SET_:
        cbuf_printf(&c->out, "NULLARY(SET_NEWNODE, %lluULL);\n",
                   (unsigned long long)nodevalue(n).set);
        break;

    case LIST_:
        emit_quotation_push(c, nodevalue(n).lis, indent);
        break;

    case USR_: {
        int ent_idx = nodevalue(n).ent;
        Entry ent = vec_at(c->env->symtab, ent_idx);
        const char* cfunc = builtin_to_c(ent.name);

        if (c->inline_ops) {
            const char* inline_code = get_inline_op(ent.name);
            if (inline_code) {
                cbuf_printf(&c->out, "/* %s */\n", ent.name);
                emit_indent(c, indent + 1);
                cbuf_append(&c->out, inline_code);
                cbuf_append(&c->out, "\n");
                return;
            }
        }

        if (cfunc) {
            cbuf_printf(&c->out, "%s(env);\n", cfunc);
        } else if (ent.is_user) {
            /* Collect this user function for emission */
            collect_user_func(c, ent_idx);
            char cname[256];
            sanitize_name(ent.name, cname, sizeof(cname));
            cbuf_printf(&c->out, "%s(env);\n", cname);
        } else {
            /* Fallback: lookup at runtime */
            cbuf_printf(&c->out, "/* %s (runtime lookup) */\n", ent.name);
            emit_indent(c, indent);
            cbuf_printf(&c->out, "{ Entry _e = vec_at(env->symtab, lookup(env, \"%s\"));\n",
                       ent.name);
            emit_indent(c, indent + 1);
            cbuf_append(&c->out, "if (_e.is_user) exec_term(env, _e.u.body);\n");
            emit_indent(c, indent + 1);
            cbuf_append(&c->out, "else (*_e.u.proc)(env); }\n");
        }
        break;
    }

    case ANON_FUNCT_: {
        /* Anonymous function - find its name */
        const char* fname = NULL;
        for (int i = 0; i < (int)vec_size(c->env->symtab); i++) {
            Entry ent = vec_at(c->env->symtab, i);
            if (!ent.is_user && ent.u.proc == nodevalue(n).proc) {
                fname = builtin_to_c(ent.name);
                if (!fname) {
                    /* Unknown builtin, call by name */
                    cbuf_printf(&c->out, "%s_(env);\n", ent.name);
                    return;
                }
                break;
            }
        }
        if (fname)
            cbuf_printf(&c->out, "%s(env);\n", fname);
        else
            cbuf_append(&c->out, "/* unknown anonymous function */\n");
        break;
    }

    default:
        cbuf_printf(&c->out, "/* unhandled node type %d */\n", nodetype(n));
        break;
    }
}

/* Emit code for a quotation body (sequence of terms) */
static void emit_quotation_body(Compiler* c, Index list, int indent) {
    pEnv env = c->env;  /* Required for node macros */

    Index p = list;
    while (p) {
        /* Try inline loop patterns first */
        if (c->inline_ops) {
            if (try_emit_while(c, &p, indent)) continue;
            if (try_emit_times(c, &p, indent)) continue;
        }

        /* Try constant folding */
        if (c->fold_constants) {
            if (try_fold_constants(c, &p, indent)) continue;
        }

        /* Regular term emission */
        emit_term(c, p, indent);
        p = nextnode1(p);
    }
}

/* Emit file header */
static void emit_header(Compiler* c) {
    cbuf_append(&c->out,
        "/**\n"
        " * Joy program compiled to C\n"
        " * Generated by compile-to-c builtin\n"
        " *\n"
        " * Compile with:\n"
        " *   gcc -O3 -DNOBDW -I<joy>/include -I<joy>/build/generated -I<joy> program.c \\\n"
        " *       <joy>/build/libjoycore_static.a -lm -lsqlite3 -o program\n"
        " */\n\n"
        "#include \"globals.h\"\n"
        "#include \"builtin.h\"\n\n"
        "/* Stack manipulation macros */\n"
        "#define SAVESTACK env->dump = LIST_NEWNODE(env->stck, env->dump)\n"
        "#define DMP nodevalue(env->dump).lis\n"
        "#define SAVED1 DMP\n"
        "#define SAVED2 nextnode1(DMP)\n"
        "#define SAVED3 nextnode2(DMP)\n"
        "#define SAVED4 nextnode3(DMP)\n"
        "#define SAVED5 nextnode4(DMP)\n\n"
        "/* Parameter validation macros */\n"
        "#ifndef NCHECK\n"
        "#define ONEPARAM(NAME) if (!env->stck) { execerror(env, \"one parameter\", NAME); return; }\n"
        "#define TWOPARAMS(NAME) if (!env->stck || !nextnode1(env->stck)) { execerror(env, \"two parameters\", NAME); return; }\n"
        "#define THREEPARAMS(NAME) if (!env->stck || !nextnode1(env->stck) || !nextnode2(env->stck)) { execerror(env, \"three parameters\", NAME); return; }\n"
        "#else\n"
        "#define ONEPARAM(NAME)\n"
        "#define TWOPARAMS(NAME)\n"
        "#define THREEPARAMS(NAME)\n"
        "#endif\n\n"
    );
}

/* Emit entry point */
static void emit_entry_point(Compiler* c) {
    cbuf_append(&c->out,
        "\n/* Print remaining stack */\n"
        "static void print_final_stack(pEnv env)\n"
        "{\n"
        "    if (env->stck) {\n"
        "        Index p = env->stck;\n"
        "        while (p) {\n"
        "            writefactor(env, p, stdout);\n"
        "            if (nextnode1(p)) printf(\" \");\n"
        "            POP(p);\n"
        "        }\n"
        "        printf(\"\\n\");\n"
        "    }\n"
        "}\n\n"
        "/* Entry point */\n"
        "int main(int argc, char* argv[])\n"
        "{\n"
        "    Env _env;\n"
        "    pEnv env = &_env;\n\n"
        "    GC_INIT();\n"
        "    memset(env, 0, sizeof(Env));\n\n"
        "    vec_init(env->pathnames);\n"
        "    vec_init(env->string);\n"
        "    vec_init(env->pushback);\n"
        "    vec_init(env->tokens);\n"
        "    vec_init(env->symtab);\n\n"
        "    inisymboltable(env);\n"
        "    inimem1(env, 0);\n"
        "    inimem2(env);\n\n"
        "    env->config.autoput = 1;\n"
        "    env->config.undeferror = 1;\n"
        "    env->g_argc = argc;\n"
        "    env->g_argv = argv;\n\n"
        "    if (setjmp(env->error_jmp) == 0) {\n"
        "        _joy_compiled_entry_(env);\n"
        "        print_final_stack(env);\n"
        "    }\n\n"
        "    return 0;\n"
        "}\n"
    );
}

/**
Q1  OK  3900  compile-to-c\0compile_to_c  :  [P]  ->  S
[COMPILER] Compiles quotation P to C source code.
The resulting string S is a complete C program that can be
compiled with gcc and linked against libjoycore.
*/
void compile_to_c_(pEnv env)
{
    Compiler c;

    ONEPARAM("compile-to-c");
    LIST("compile-to-c");

    c.env = env;
    c.quot_counter = 0;
    c.loop_counter = 0;
    c.inline_ops = 1;
    c.fold_constants = 1;
    c.user_func_count = 0;
    c.collecting_deps = 0;
    cbuf_init(&c.out);
    cbuf_init(&c.fwd);
    cbuf_init(&c.funcs);

    Index prog = nodevalue(env->stck).lis;

    /* First pass: collect user functions from the main program */
    collect_user_funcs_from_body(&c, prog);

    /* Emit header */
    emit_header(&c);

    /* Emit forward declarations */
    cbuf_append(&c.out, "/* Forward declarations */\n");
    cbuf_append(&c.out, "void _joy_compiled_entry_(pEnv env);\n");

    /* Add user function forward declarations */
    if (c.user_func_count > 0) {
        emit_user_func_declarations(&c);
        cbuf_append(&c.out, c.fwd.data);
    }
    cbuf_append(&c.out, "\n");

    /* Emit user function definitions */
    if (c.user_func_count > 0) {
        emit_user_func_definitions(&c);
        cbuf_append(&c.out, c.funcs.data);
    }

    /* Emit main function */
    cbuf_append(&c.out, "/* Main program */\n");
    cbuf_append(&c.out, "void _joy_compiled_entry_(pEnv env)\n{\n");
    emit_quotation_body(&c, prog, 1);
    cbuf_append(&c.out, "}\n");

    /* Emit entry point */
    emit_entry_point(&c);

    /* Replace quotation on stack with result string */
    UNARY(STRING_NEWNODE, c.out.data);
}

/**
Q1  OK  3905  compile-to-file\0compile_to_file  :  [P] "filename"  ->
[COMPILER] Compiles quotation P to C source code and writes it to filename.
*/
void compile_to_file_(pEnv env)
{
    char* filename;
    FILE* fp;
    Compiler c;

    TWOPARAMS("compile-to-file");
    STRING("compile-to-file");
    filename = GETSTRING(env->stck);
    POP(env->stck);
    LIST("compile-to-file");

    /* Open output file */
    fp = fopen(filename, "w");
    if (!fp) {
        execerror(env, "cannot open output file", "compile-to-file");
        return;
    }

    /* Initialize compiler */
    c.env = env;
    c.quot_counter = 0;
    c.loop_counter = 0;
    c.inline_ops = 1;
    c.fold_constants = 1;
    c.user_func_count = 0;
    c.collecting_deps = 0;
    cbuf_init(&c.out);
    cbuf_init(&c.fwd);
    cbuf_init(&c.funcs);

    Index prog = nodevalue(env->stck).lis;

    /* First pass: collect user functions from the main program */
    collect_user_funcs_from_body(&c, prog);

    /* Emit header */
    emit_header(&c);

    /* Emit forward declarations */
    cbuf_append(&c.out, "/* Forward declarations */\n");
    cbuf_append(&c.out, "void _joy_compiled_entry_(pEnv env);\n");

    /* Add user function forward declarations */
    if (c.user_func_count > 0) {
        emit_user_func_declarations(&c);
        cbuf_append(&c.out, c.fwd.data);
    }
    cbuf_append(&c.out, "\n");

    /* Emit user function definitions */
    if (c.user_func_count > 0) {
        emit_user_func_definitions(&c);
        cbuf_append(&c.out, c.funcs.data);
    }

    /* Emit main function */
    cbuf_append(&c.out, "/* Main program */\n");
    cbuf_append(&c.out, "void _joy_compiled_entry_(pEnv env)\n{\n");
    emit_quotation_body(&c, prog, 1);
    cbuf_append(&c.out, "}\n");

    /* Emit entry point */
    emit_entry_point(&c);

    /* Write to file and close */
    fputs(c.out.data, fp);
    fflush(fp);
    fclose(fp);

    /* Remove quotation from stack */
    POP(env->stck);
}
