/*
 *  module  : tostring.c
 *  version : 1.0
 *  date    : 01/22/26
 *
 *  toString operator - converts any Joy value to its string representation.
 */
#include "globals.h"
#include "runtime.h"
#include "builtin_macros.h"

/* Simple dynamic string buffer */
typedef struct {
    char* data;
    size_t len;
    size_t cap;
} StrBuf;

static void sbuf_init(StrBuf* b)
{
    b->cap = 64;
    b->len = 0;
    b->data = malloc(b->cap);
}

static void sbuf_push(StrBuf* b, char c)
{
    if (b->len >= b->cap - 1) {
        b->cap *= 2;
        b->data = realloc(b->data, b->cap);
    }
    b->data[b->len++] = c;
}

static void sbuf_str(StrBuf* b, const char* s)
{
    while (*s)
        sbuf_push(b, *s++);
}

static char* sbuf_finish(StrBuf* b)
{
    char* result;
    sbuf_push(b, '\0');
    result = GC_strdup(b->data);
    free(b->data);
    return result;
}

/* Forward declaration for recursive stringify */
static void stringify_value(pEnv env, Index node, StrBuf* out);
static void stringify_term(pEnv env, Index node, StrBuf* out);

/* Stringify a term (list of nodes) */
static void stringify_term(pEnv env, Index node, StrBuf* out)
{
    int first = 1;
    while (node) {
        if (!first)
            sbuf_push(out, ' ');
        first = 0;
        stringify_value(env, node, out);
        node = nextnode1(node);
    }
}

/* Stringify a single value */
static void stringify_value(pEnv env, Index node, StrBuf* out)
{
    char buf[64];
    char* ptr;
    khash_t(Dict)* d;
    khint_t k;
    int first;

    switch (nodetype(node)) {
    case USR_:
        sbuf_str(out, vec_at(env->symtab, nodevalue(node).ent).name);
        break;

    case BOOLEAN_:
        sbuf_str(out, nodevalue(node).num ? "true" : "false");
        break;

    case CHAR_:
        sbuf_push(out, '\'');
        if (nodevalue(node).num == '\n')
            sbuf_str(out, "\\n");
        else if (nodevalue(node).num == '\t')
            sbuf_str(out, "\\t");
        else if (nodevalue(node).num == '\\')
            sbuf_str(out, "\\\\");
        else if (nodevalue(node).num == '\'')
            sbuf_str(out, "\\'");
        else
            sbuf_push(out, (char)nodevalue(node).num);
        break;

    case INTEGER_:
        snprintf(buf, sizeof(buf), "%" PRId64, nodevalue(node).num);
        sbuf_str(out, buf);
        break;

    case SET_: {
        uint64_t set = nodevalue(node).set;
        int i;
        sbuf_push(out, '{');
        first = 1;
        for (i = 0; i < SETSIZE; i++) {
            if (set & ((uint64_t)1 << i)) {
                if (!first)
                    sbuf_push(out, ' ');
                first = 0;
                snprintf(buf, sizeof(buf), "%d", i);
                sbuf_str(out, buf);
            }
        }
        sbuf_push(out, '}');
        break;
    }

    case STRING_:
        sbuf_push(out, '"');
        for (ptr = GETSTRING(node); *ptr; ptr++) {
            if (*ptr == '"')
                sbuf_str(out, "\\\"");
            else if (*ptr == '\\')
                sbuf_str(out, "\\\\");
            else if (*ptr == '\n')
                sbuf_str(out, "\\n");
            else if (*ptr == '\t')
                sbuf_str(out, "\\t");
            else
                sbuf_push(out, *ptr);
        }
        sbuf_push(out, '"');
        break;

    case LIST_:
        sbuf_push(out, '[');
        stringify_term(env, nodevalue(node).lis, out);
        sbuf_push(out, ']');
        break;

    case FLOAT_: {
        double val = nodevalue(node).dbl;
        snprintf(buf, sizeof(buf), "%g", val);
        sbuf_str(out, buf);
        /* Add .0 if no decimal point */
        if (!strchr(buf, '.') && !strchr(buf, 'e') && !strchr(buf, 'E')) {
            sbuf_str(out, ".0");
        }
        break;
    }

    case FILE_:
        if (nodevalue(node).fil == stdin)
            sbuf_str(out, "stdin");
        else if (nodevalue(node).fil == stdout)
            sbuf_str(out, "stdout");
        else if (nodevalue(node).fil == stderr)
            sbuf_str(out, "stderr");
        else if (nodevalue(node).fil == NULL)
            sbuf_str(out, "NULL");
        else {
            snprintf(buf, sizeof(buf), "%p", (void*)nodevalue(node).fil);
            sbuf_str(out, buf);
        }
        break;

    case BIGNUM_:
#ifdef NOBDW
        sbuf_str(out, (char*)&nodevalue(node));
#else
        sbuf_str(out, nodevalue(node).str);
#endif
        break;

    case DICT_:
        sbuf_push(out, '{');
        d = (khash_t(Dict)*)nodevalue(node).dict;
        first = 1;
        if (d) {
            for (k = kh_begin(d); k != kh_end(d); ++k) {
                if (kh_exist(d, k)) {
                    if (!first)
                        sbuf_push(out, ' ');
                    first = 0;
                    sbuf_push(out, '"');
                    sbuf_str(out, kh_key(d, k));
                    sbuf_str(out, "\": ");
                    stringify_value(env, kh_value(d, k), out);
                }
            }
        }
        sbuf_push(out, '}');
        break;

    default:
        sbuf_str(out, "?");
        break;
    }
}

/**
Q0  OK  3830  toString  :  X  ->  S
S is the string representation of X.
*/
void toString_(pEnv env)
{
    StrBuf out;

    ONEPARAM("toString");

    /* If already a string, just return it without quotes */
    if (nodetype(env->stck) == STRING_) {
        /* Already a string - don't add quotes */
        return;
    }

    sbuf_init(&out);
    stringify_value(env, env->stck, &out);

    UNARY(STRING_NEWNODE, sbuf_finish(&out));
}

/**
Q0  OK  3831  unquoted  :  X  ->  S
S is the unquoted string representation of X.
For strings, returns the string without surrounding quotes.
For other types, same as toString.
*/
void unquoted_(pEnv env)
{
    StrBuf out;

    ONEPARAM("unquoted");

    /* If already a string, just return it */
    if (nodetype(env->stck) == STRING_) {
        return;
    }

    sbuf_init(&out);

    /* For characters, don't include the quote marks */
    if (nodetype(env->stck) == CHAR_) {
        char c = (char)nodevalue(env->stck).num;
        sbuf_push(&out, c);
    } else {
        stringify_value(env, env->stck, &out);
    }

    UNARY(STRING_NEWNODE, sbuf_finish(&out));
}
