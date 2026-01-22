/*
 *  module  : json.c
 *  version : 1.0
 *  date    : 01/22/26
 *
 *  JSON parsing and emitting for Joy.
 *  Converts between JSON strings and Joy values (dict, list, etc.)
 */
#include "globals.h"
#include "runtime.h"
#include "builtin_macros.h"

/* Forward declarations for recursive parser */
static Index parse_value(pEnv env, const char **p);
static Index parse_object(pEnv env, const char **p);
static Index parse_array(pEnv env, const char **p);
static Index parse_string(pEnv env, const char **p);
static Index parse_number(pEnv env, const char **p);

/* Helper: skip whitespace */
static void skip_ws(const char **p)
{
    while (**p == ' ' || **p == '\t' || **p == '\n' || **p == '\r')
        (*p)++;
}

/* Helper: parse a JSON string (assumes we're at the opening quote) */
static char* parse_string_value(pEnv env, const char **p)
{
    const char *start;
    char *result, *dst;
    size_t len;

    (void)env;
    if (**p != '"')
        return NULL;
    (*p)++;  /* skip opening quote */
    start = *p;

    /* First pass: count length (accounting for escapes) */
    len = 0;
    while (**p && **p != '"') {
        if (**p == '\\') {
            (*p)++;
            if (**p == 'u') {
                (*p) += 4;  /* \uXXXX */
                len++;  /* simplified: just count as 1 char */
            } else {
                (*p)++;
                len++;
            }
        } else {
            (*p)++;
            len++;
        }
    }

    /* Allocate and copy */
    result = GC_malloc_atomic(len + 1);
    *p = start;
    dst = result;

    while (**p && **p != '"') {
        if (**p == '\\') {
            (*p)++;
            switch (**p) {
            case '"':  *dst++ = '"';  break;
            case '\\': *dst++ = '\\'; break;
            case '/':  *dst++ = '/';  break;
            case 'b':  *dst++ = '\b'; break;
            case 'f':  *dst++ = '\f'; break;
            case 'n':  *dst++ = '\n'; break;
            case 'r':  *dst++ = '\r'; break;
            case 't':  *dst++ = '\t'; break;
            case 'u':
                /* Simplified: just skip \uXXXX and insert '?' */
                (*p) += 4;
                *dst++ = '?';
                break;
            default:
                *dst++ = **p;
            }
            (*p)++;
        } else {
            *dst++ = **p;
            (*p)++;
        }
    }
    *dst = '\0';

    if (**p == '"')
        (*p)++;  /* skip closing quote */

    return result;
}

/* Parse a JSON number */
static Index parse_number(pEnv env, const char **p)
{
    const char *start = *p;
    int is_float = 0;
    int64_t int_val;
    double float_val;
    char *end;

    /* Skip leading minus */
    if (**p == '-')
        (*p)++;

    /* Skip digits */
    while (**p >= '0' && **p <= '9')
        (*p)++;

    /* Check for decimal point */
    if (**p == '.') {
        is_float = 1;
        (*p)++;
        while (**p >= '0' && **p <= '9')
            (*p)++;
    }

    /* Check for exponent */
    if (**p == 'e' || **p == 'E') {
        is_float = 1;
        (*p)++;
        if (**p == '+' || **p == '-')
            (*p)++;
        while (**p >= '0' && **p <= '9')
            (*p)++;
    }

    if (is_float) {
        float_val = strtod(start, &end);
        return FLOAT_NEWNODE(float_val, 0);
    } else {
        int_val = strtoll(start, &end, 10);
        return INTEGER_NEWNODE(int_val, 0);
    }
}

/* Parse a JSON string into a STRING_ node */
static Index parse_string(pEnv env, const char **p)
{
    char *str = parse_string_value(env, p);
    if (!str)
        return 0;
    return STRING_NEWNODE(str, 0);
}

/* Parse a JSON array into a LIST_ node */
static Index parse_array(pEnv env, const char **p)
{
    Index head = 0, tail = 0, elem;

    if (**p != '[')
        return 0;
    (*p)++;  /* skip '[' */

    skip_ws(p);
    if (**p == ']') {
        (*p)++;
        return LIST_NEWNODE(0, 0);  /* empty list */
    }

    while (1) {
        skip_ws(p);
        elem = parse_value(env, p);
        if (!elem)
            return 0;

        if (!head) {
            head = elem;
            tail = elem;
        } else {
            nextnode1(tail) = elem;
            tail = elem;
        }

        skip_ws(p);
        if (**p == ']') {
            (*p)++;
            break;
        }
        if (**p == ',') {
            (*p)++;
        } else {
            return 0;  /* parse error */
        }
    }

    return LIST_NEWNODE(head, 0);
}

/* Parse a JSON object into a DICT_ node */
static Index parse_object(pEnv env, const char **p)
{
    khash_t(Dict)* d;
    char* key;
    Index value;
    khint_t k;
    int ret;

    if (**p != '{')
        return 0;
    (*p)++;  /* skip '{' */

    d = kh_init(Dict);

    skip_ws(p);
    if (**p == '}') {
        (*p)++;
        return DICT_NEWNODE(d, 0);  /* empty dict */
    }

    while (1) {
        skip_ws(p);
        if (**p != '"') {
            return 0;  /* keys must be strings */
        }

        key = parse_string_value(env, p);
        if (!key)
            return 0;

        skip_ws(p);
        if (**p != ':')
            return 0;
        (*p)++;  /* skip ':' */

        skip_ws(p);
        value = parse_value(env, p);
        if (!value)
            return 0;

        k = kh_put(Dict, d, key, &ret);
        kh_value(d, k) = value;

        skip_ws(p);
        if (**p == '}') {
            (*p)++;
            break;
        }
        if (**p == ',') {
            (*p)++;
        } else {
            return 0;  /* parse error */
        }
    }

    return DICT_NEWNODE(d, 0);
}

/* Parse any JSON value */
static Index parse_value(pEnv env, const char **p)
{
    int index;

    skip_ws(p);

    switch (**p) {
    case '{':
        return parse_object(env, p);
    case '[':
        return parse_array(env, p);
    case '"':
        return parse_string(env, p);
    case 't':  /* true */
        if (strncmp(*p, "true", 4) == 0) {
            *p += 4;
            return BOOLEAN_NEWNODE(1, 0);
        }
        return 0;
    case 'f':  /* false */
        if (strncmp(*p, "false", 5) == 0) {
            *p += 5;
            return BOOLEAN_NEWNODE(0, 0);
        }
        return 0;
    case 'n':  /* null */
        if (strncmp(*p, "null", 4) == 0) {
            *p += 4;
            /* Return a USR_ node for "null" symbol */
            index = enteratom(env, "null");
            return USR_NEWNODE(index, 0);
        }
        return 0;
    case '-':
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        return parse_number(env, p);
    default:
        return 0;  /* parse error */
    }
}

/**
Q0  OK  3820  json>\0fromjson  :  S  ->  V
V is a Joy value parsed from JSON string S.
JSON objects become dictionaries, arrays become lists.
*/
void fromjson_(pEnv env)
{
    char* str;
    const char* p;
    Index result;

    ONEPARAM("json>");
    STRING("json>");

    str = GETSTRING(env->stck);
    p = str;

    result = parse_value(env, &p);
    if (!result) {
        execerror(env, "valid JSON", "json>");
        return;
    }

    GUNARY(result);
}

/* Simple dynamic string buffer for JSON emission */
typedef struct {
    char* data;
    size_t len;
    size_t cap;
} JsonBuf;

static void jbuf_init(JsonBuf* b)
{
    b->cap = 256;
    b->len = 0;
    b->data = malloc(b->cap);
}

static void jbuf_push(JsonBuf* b, char c)
{
    if (b->len >= b->cap - 1) {
        b->cap *= 2;
        b->data = realloc(b->data, b->cap);
    }
    b->data[b->len++] = c;
}

static void jbuf_str(JsonBuf* b, const char* s)
{
    while (*s)
        jbuf_push(b, *s++);
}

static char* jbuf_finish(JsonBuf* b)
{
    char* result;
    jbuf_push(b, '\0');
    result = GC_strdup(b->data);
    free(b->data);
    return result;
}

/* Forward declaration for recursive emit */
static void emit_value(pEnv env, Index node, JsonBuf* out);

/* Helper: append an escaped JSON string */
static void emit_json_string(pEnv env, const char* s, JsonBuf* out)
{
    (void)env;
    jbuf_push(out, '"');
    while (*s) {
        switch (*s) {
        case '"':  jbuf_str(out, "\\\""); break;
        case '\\': jbuf_str(out, "\\\\"); break;
        case '\b': jbuf_str(out, "\\b");  break;
        case '\f': jbuf_str(out, "\\f");  break;
        case '\n': jbuf_str(out, "\\n");  break;
        case '\r': jbuf_str(out, "\\r");  break;
        case '\t': jbuf_str(out, "\\t");  break;
        default:
            if ((unsigned char)*s < 32) {
                char buf[8];
                snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)*s);
                jbuf_str(out, buf);
            } else {
                jbuf_push(out, *s);
            }
        }
        s++;
    }
    jbuf_push(out, '"');
}

/* Emit a Joy value as JSON */
static void emit_value(pEnv env, Index node, JsonBuf* out)
{
    char buf[64];
    Index elem;
    int first;
    khash_t(Dict)* d;
    khint_t k;

    switch (nodetype(node)) {
    case BOOLEAN_:
        jbuf_str(out, nodevalue(node).num ? "true" : "false");
        break;

    case INTEGER_:
        snprintf(buf, sizeof(buf), "%" PRId64, nodevalue(node).num);
        jbuf_str(out, buf);
        break;

    case FLOAT_:
        snprintf(buf, sizeof(buf), "%g", nodevalue(node).dbl);
        jbuf_str(out, buf);
        break;

    case STRING_:
        emit_json_string(env, GETSTRING(node), out);
        break;

    case LIST_:
        jbuf_push(out, '[');
        first = 1;
        elem = nodevalue(node).lis;
        while (elem) {
            if (!first)
                jbuf_push(out, ',');
            first = 0;
            emit_value(env, elem, out);
            elem = nextnode1(elem);
        }
        jbuf_push(out, ']');
        break;

    case DICT_:
        jbuf_push(out, '{');
        d = (khash_t(Dict)*)nodevalue(node).dict;
        first = 1;
        if (d) {
            for (k = kh_begin(d); k != kh_end(d); ++k) {
                if (kh_exist(d, k)) {
                    if (!first)
                        jbuf_push(out, ',');
                    first = 0;
                    emit_json_string(env, kh_key(d, k), out);
                    jbuf_push(out, ':');
                    emit_value(env, kh_value(d, k), out);
                }
            }
        }
        jbuf_push(out, '}');
        break;

    case USR_:
        /* Check if it's the "null" symbol */
        if (strcmp(vec_at(env->symtab, nodevalue(node).ent).name, "null") == 0) {
            jbuf_str(out, "null");
        } else {
            /* Other symbols become strings */
            emit_json_string(env, vec_at(env->symtab, nodevalue(node).ent).name, out);
        }
        break;

    case CHAR_:
        /* Character becomes a single-char string */
        buf[0] = (char)nodevalue(node).num;
        buf[1] = '\0';
        emit_json_string(env, buf, out);
        break;

    case SET_:
        /* Sets become arrays of integers */
        {
            uint64_t set = nodevalue(node).set;
            int i;
            jbuf_push(out, '[');
            first = 1;
            for (i = 0; i < SETSIZE; i++) {
                if (set & ((uint64_t)1 << i)) {
                    if (!first)
                        jbuf_push(out, ',');
                    first = 0;
                    snprintf(buf, sizeof(buf), "%d", i);
                    jbuf_str(out, buf);
                }
            }
            jbuf_push(out, ']');
        }
        break;

    default:
        /* Other types become null */
        jbuf_str(out, "null");
        break;
    }
}

/**
Q0  OK  3821  >json\0tojson  :  V  ->  S
S is a JSON string representation of Joy value V.
Dictionaries become JSON objects, lists become arrays.
*/
void tojson_(pEnv env)
{
    JsonBuf out;

    ONEPARAM(">json");

    jbuf_init(&out);
    emit_value(env, env->stck, &out);

    UNARY(STRING_NEWNODE, jbuf_finish(&out));
}
