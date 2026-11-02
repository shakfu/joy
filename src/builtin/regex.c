/*
 *  module  : regex.c
 *  version : 1.0
 *  date    : 01/31/26
 *
 *  Regular expression operations for Joy using POSIX regex.
 */
#include "globals.h"
#include "runtime.h"
#include "builtin_macros.h"

#include <ctype.h>
#include <regex.h>

/* Maximum number of capture groups */
#define MAX_GROUPS 32

/* Perl-style shortcut expansions - standalone vs inside character class */
#define EXPAND_d       "[0-9]"
#define EXPAND_d_CLASS "0-9"
#define EXPAND_D       "[^0-9]"
#define EXPAND_w       "[a-zA-Z0-9_]"
#define EXPAND_w_CLASS "a-zA-Z0-9_"
#define EXPAND_W       "[^a-zA-Z0-9_]"
#define EXPAND_s       "[ \t\n\r\f\v]"
#define EXPAND_s_CLASS " \t\n\r\f\v"
#define EXPAND_S       "[^ \t\n\r\f\v]"

typedef struct {
    char* s;
    size_t len, cap;
    int oom;
} PatBuf;

static void pat_put(PatBuf* b, const char* s, size_t n)
{
    if (b->oom)
        return;
    if (b->len + n + 1 > b->cap) {
        size_t cap = (b->len + n + 1) * 2;
        char* t = realloc(b->s, cap);
        if (!t) {
            b->oom = 1;
            return;
        }
        b->s = t;
        b->cap = cap;
    }
    memcpy(b->s + b->len, s, n);
    b->len += n;
    b->s[b->len] = '\0';
}

static void pat_putc(PatBuf* b, char c) { pat_put(b, &c, 1); }

/* Byte membership for a shortcut letter (d, w, s and their negations) */
static int shortcut_has(char k, int c)
{
    int in;
    switch (k | 0x20) {
    case 'd': in = c >= '0' && c <= '9'; break;
    case 'w': in = isalnum(c) || c == '_'; break;
    default:  in = c == ' ' || (c >= '\t' && c <= '\r'); break;
    }
    return (k & 0x20) ? in : !in; /* upper case negates */
}

/* Byte membership for a POSIX class name such as "alpha"; -1 if unknown */
static int posix_class_has(const char* name, size_t n, int c)
{
    static const struct { const char* name; int (*fn)(int); } classes[] = {
        {"alnum", isalnum}, {"alpha", isalpha}, {"blank", isblank},
        {"cntrl", iscntrl}, {"digit", isdigit}, {"graph", isgraph},
        {"lower", islower}, {"print", isprint}, {"punct", ispunct},
        {"space", isspace}, {"upper", isupper}, {"xdigit", isxdigit},
    };
    size_t i;
    for (i = 0; i < sizeof(classes) / sizeof(classes[0]); i++)
        if (strlen(classes[i].name) == n && !strncmp(classes[i].name, name, n))
            return classes[i].fn(c) != 0;
    return -1;
}

/*
 * Rewrites the bracket members [p, end) as an explicit byte set. POSIX has
 * no way to put a negated class such as \D inside a bracket, so the set is
 * computed here. Returns 0 on success, -1 for unsupported members.
 */
static int emit_byte_set(PatBuf* b, const char* p, const char* end, int neg)
{
    unsigned char set[256] = { 0 };
    int c, lo, n = 0;

    while (p < end) {
        if (p[0] == '[' && p[1] == ':') {
            const char* name = p + 2;
            const char* close = strstr(name, ":]");
            if (!close || close >= end)
                return -1;
            for (c = 1; c < 256; c++) {
                int in = posix_class_has(name, (size_t)(close - name), c);
                if (in < 0)
                    return -1;
                set[c] |= in;
            }
            p = close + 2;
        } else if (p[0] == '[' && (p[1] == '.' || p[1] == '=')) {
            return -1;
        } else if (p[0] == '\\' && p + 1 < end && strchr("dDwWsS", p[1])) {
            for (c = 1; c < 256; c++)
                set[c] |= shortcut_has(p[1], c);
            p += 2;
        } else if (p + 2 < end && p[1] == '-' && p[2] != ']') {
            for (c = (unsigned char)p[0]; c <= (unsigned char)p[2]; c++)
                set[c] = 1;
            p += 3;
        } else {
            set[(unsigned char)*p++] = 1;
        }
    }
    set[0] = 0; /* patterns are NUL-terminated */
    if (neg)
        for (c = 1; c < 256; c++)
            set[c] = !set[c];
    for (c = 1; c < 256; c++)
        n += set[c];
    if (n == 0)
        return -1;
    if (n == 1 && set['^']) {
        pat_put(b, "\\^", 2);
        return 0;
    }
    /* ']' must come first, '^' must not, '-' must come last */
    pat_putc(b, '[');
    if (set[']'])
        pat_putc(b, ']');
    for (c = 1; c < 256; c++) {
        if (!set[c] || c == ']' || c == '^' || c == '-')
            continue;
        lo = c;
        while (c + 1 < 256 && set[c + 1] && c + 1 != ']' && c + 1 != '^'
               && c + 1 != '-')
            c++;
        pat_putc(b, (char)lo);
        if (c - lo >= 2)
            pat_putc(b, '-');
        if (c > lo)
            pat_putc(b, (char)c);
    }
    if (set['^'])
        pat_putc(b, '^');
    if (set['-'])
        pat_putc(b, '-');
    pat_putc(b, ']');
    return 0;
}

/* True if the bracket members [p, end) include \D, \W or \S */
static int has_negated_shortcut(const char* p, const char* end)
{
    for (; p + 1 < end; p++)
        if (*p == '\\') {
            if (strchr("DWS", p[1]))
                return 1;
            p++;
        }
    return 0;
}

/* Returns the closing ']' of the bracket opening at p, or NULL */
static const char* bracket_end(const char* p)
{
    const char* q = p + 1;
    if (*q == '^')
        q++;
    if (*q == ']')
        q++; /* a leading ']' is a member */
    while (*q && *q != ']') {
        if (q[0] == '[' && (q[1] == ':' || q[1] == '.' || q[1] == '=')) {
            char close[3] = { q[1], ']', '\0' };
            const char* e = strstr(q + 2, close);
            if (!e)
                return NULL;
            q = e + 2;
        } else if (q[0] == '\\' && q[1]) {
            q += 2;
        } else {
            q++;
        }
    }
    return *q ? q : NULL;
}

/*
 * Expands Perl-style shortcuts (\d \w \s and negations) to POSIX ERE.
 * Returns a malloc'd pattern, or NULL with *err set.
 */
static char* preprocess_pattern(const char* pattern, const char** err)
{
    PatBuf b = { NULL, 0, 0, 0 };
    const char* p = pattern;

    *err = "memory";
    pat_put(&b, "", 0);
    while (*p && !b.oom) {
        if (*p == '[') {
            const char* end = bracket_end(p);
            const char* q = p + 1;
            int neg = *q == '^';
            if (!end) { /* unterminated: let regcomp report it */
                pat_put(&b, p, strlen(p));
                break;
            }
            q += neg;
            if (has_negated_shortcut(q, end)) {
                if (emit_byte_set(&b, q, end, neg) < 0) {
                    free(b.s);
                    *err = "supported bracket with \\D, \\W or \\S";
                    return NULL;
                }
            } else {
                pat_put(&b, p, (size_t)(q - p));
                while (q < end) {
                    const char* x = NULL;
                    if (q[0] == '\\' && q[1] == 'd') x = EXPAND_d_CLASS;
                    else if (q[0] == '\\' && q[1] == 'w') x = EXPAND_w_CLASS;
                    else if (q[0] == '\\' && q[1] == 's') x = EXPAND_s_CLASS;
                    if (x) {
                        pat_put(&b, x, strlen(x));
                        q += 2;
                    } else {
                        pat_putc(&b, *q++);
                    }
                }
                pat_putc(&b, ']');
            }
            p = end + 1;
        } else if (*p == '\\' && p[1]) {
            const char* x = NULL;
            switch (p[1]) {
            case 'd': x = EXPAND_d; break;
            case 'D': x = EXPAND_D; break;
            case 'w': x = EXPAND_w; break;
            case 'W': x = EXPAND_W; break;
            case 's': x = EXPAND_s; break;
            case 'S': x = EXPAND_S; break;
            }
            if (x)
                pat_put(&b, x, strlen(x));
            else
                pat_put(&b, p, 2); /* keep other escapes as-is */
            p += 2;
        } else {
            pat_putc(&b, *p++);
        }
    }
    if (b.oom) {
        free(b.s);
        return NULL;
    }
    return b.s;
}

/* Helper: compile regex with error handling */
static int compile_regex(pEnv env, const char* pattern, regex_t* re, const char* op)
{
    const char* err;
    char* expanded = preprocess_pattern(pattern, &err);
    int ret;

    if (!expanded) {
        execerror(env, (char*)err, (char*)op);
        return -1;
    }
    ret = regcomp(re, expanded, REG_EXTENDED | REG_NEWLINE);
    free(expanded);
    if (ret != 0) {
        char errbuf[256];
        regerror(ret, re, errbuf, sizeof(errbuf));
        execerror(env, errbuf, (char*)op);
        return -1;
    }
    return 0;
}

/* Helper: extract substring from match */
static char* extract_match(pEnv env, const char* str, regmatch_t* match)
{
    if (match->rm_so == -1)
        return NULL;

    size_t len = (size_t)(match->rm_eo - match->rm_so);
    char* result = GC_malloc_atomic(len + 1);
    memcpy(result, str + match->rm_so, len);
    result[len] = '\0';
    return result;
}

/*
 * Finds the next match at or after *pos, with offsets relative to str.
 * After an empty match the search resumes one character later, so that
 * iteration terminates and no input is skipped (the JS/Python rule).
 */
static int next_match(const regex_t* re, const char* str, size_t len,
                      size_t* pos, size_t nmatch, regmatch_t* m)
{
    size_t i;
    int flags;

    if (*pos > len)
        return 0;
    /* `^` must not match mid-string, except after a newline (REG_NEWLINE) */
    flags = (*pos == 0 || str[*pos - 1] == '\n') ? 0 : REG_NOTBOL;
    if (regexec(re, str + *pos, nmatch, m, flags) != 0)
        return 0;
    for (i = 0; i < nmatch; i++)
        if (m[i].rm_so != -1) {
            m[i].rm_so += (regoff_t)*pos;
            m[i].rm_eo += (regoff_t)*pos;
        }
    *pos = (size_t)m[0].rm_eo + (m[0].rm_eo == m[0].rm_so);
    return 1;
}

/**
Q0  OK  3300  regex-match\0regex_match  :  S S  ->  B
Tests if the string (second on stack) matches the regex pattern (top of stack).
Returns true if the pattern matches anywhere in the string.
Uses POSIX Extended Regular Expressions (ERE).
*/
void regex_match_(pEnv env)
{
    char* pattern;
    char* str;
    regex_t re;
    int result;

    TWOPARAMS("regex-match");
    STRING("regex-match");
    pattern = GETSTRING(env->stck);
    POP(env->stck);

    STRING("regex-match");
    str = GETSTRING(env->stck);
    POP(env->stck);

    if (compile_regex(env, pattern, &re, "regex-match") < 0)
        return;

    result = regexec(&re, str, 0, NULL, 0);
    regfree(&re);

    NULLARY(BOOLEAN_NEWNODE, result == 0 ? 1 : 0);
}

/**
Q0  OK  3301  regex-find\0regex_find  :  S S  ->  S | B
Finds the first match of the regex pattern in the string.
Returns the matched substring, or false if no match.
If the pattern contains groups, returns the first group match.
*/
void regex_find_(pEnv env)
{
    char* pattern;
    char* str;
    regex_t re;
    regmatch_t matches[MAX_GROUPS];
    int result;
    char* found;

    TWOPARAMS("regex-find");
    STRING("regex-find");
    pattern = GETSTRING(env->stck);
    POP(env->stck);

    STRING("regex-find");
    str = GETSTRING(env->stck);
    POP(env->stck);

    if (compile_regex(env, pattern, &re, "regex-find") < 0)
        return;

    result = regexec(&re, str, MAX_GROUPS, matches, 0);

    if (result != 0) {
        regfree(&re);
        NULLARY(BOOLEAN_NEWNODE, 0);  /* false - no match */
        return;
    }

    /* If there's a capture group, return it; otherwise return full match */
    if (re.re_nsub > 0 && matches[1].rm_so != -1) {
        found = extract_match(env, str, &matches[1]);
    } else {
        found = extract_match(env, str, &matches[0]);
    }

    regfree(&re);

    if (found) {
        NULLARY(STRING_NEWNODE, found);
    } else {
        NULLARY(BOOLEAN_NEWNODE, 0);
    }
}

/**
Q0  OK  3302  regex-find-all\0regex_find_all  :  S S  ->  [S...]
Finds all matches of the regex pattern in the string.
Returns a list of all matched substrings.
If the pattern contains groups, returns the first group from each match.
*/
void regex_find_all_(pEnv env)
{
    char* pattern;
    char* str;
    regex_t re;
    regmatch_t matches[MAX_GROUPS];
    size_t pos, len;
    Index list_head = 0;
    Index list_tail = 0;
    int count = 0;

    TWOPARAMS("regex-find-all");
    STRING("regex-find-all");
    STRING2("regex-find-all");
    /* Both stay on the stack until the reservation: it may run a GC */
    pattern = GETSTRING(env->stck);
    str = GETSTRING(nextnode1(env->stck));

    if (compile_regex(env, pattern, &re, "regex-find-all") < 0)
        return;

    /* Count matches first for ensure_capacity */
    len = strlen(str);
    pos = 0;
    while (next_match(&re, str, len, &pos, 1, matches))
        count++;

#ifdef NOBDW
    ensure_capacity(env, count * 2 + (int)(strlen(str) / sizeof(Node)) + 2);
    str = GETSTRING(nextnode1(env->stck));
    env->stck = nextnode2(env->stck);
#endif

    /* Now collect matches */
    pos = 0;
    while (next_match(&re, str, len, &pos, MAX_GROUPS, matches)) {
        char* found;
        Index node;

        /* If there's a capture group, use it; otherwise use full match */
        if (re.re_nsub > 0 && matches[1].rm_so != -1) {
            found = extract_match(env, str, &matches[1]);
        } else {
            found = extract_match(env, str, &matches[0]);
        }

        if (found) {
            node = STRING_NEWNODE(found, 0);
            if (!list_head) {
                list_head = list_tail = node;
            } else {
#ifdef NOBDW
                env->memory[list_tail].next = node;
#else
                list_tail->next = node;
#endif
                list_tail = node;
            }
        }
    }

    regfree(&re);
    NULLARY(LIST_NEWNODE, list_head);
}

/**
Q0  OK  3303  regex-split\0regex_split  :  S S  ->  [S...]
Splits the string by the regex pattern.
Returns a list of substrings between matches.
*/
void regex_split_(pEnv env)
{
    char* pattern;
    char* str;
    regex_t re;
    regmatch_t matches[1];
    size_t pos, len;
    const char* segment_start;
    Index list_head = 0;
    Index list_tail = 0;

    TWOPARAMS("regex-split");
    STRING("regex-split");
    STRING2("regex-split");
    /* Both stay on the stack until the reservation: it may run a GC */
    pattern = GETSTRING(env->stck);
    str = GETSTRING(nextnode1(env->stck));

    if (compile_regex(env, pattern, &re, "regex-split") < 0)
        return;

#ifdef NOBDW
    ensure_capacity(env, (int)strlen(str) * 2 + (int)(strlen(str) / sizeof(Node)) + 6);
    str = GETSTRING(nextnode1(env->stck));
    env->stck = nextnode2(env->stck);
#endif

    /* Collect segments */
    len = strlen(str);
    pos = 0;
    segment_start = str;

    while (next_match(&re, str, len, &pos, 1, matches)) {
        /* An empty match splits only between two characters */
        if (matches[0].rm_so == matches[0].rm_eo
            && (str + matches[0].rm_so == segment_start
                || (size_t)matches[0].rm_so == len))
            continue;

        /* Extract segment before this match */
        size_t seg_len = (size_t)((str + matches[0].rm_so) - segment_start);
        char* segment = GC_malloc_atomic(seg_len + 1);
        memcpy(segment, segment_start, seg_len);
        segment[seg_len] = '\0';

        Index node = STRING_NEWNODE(segment, 0);
        if (!list_head) {
            list_head = list_tail = node;
        } else {
#ifdef NOBDW
            env->memory[list_tail].next = node;
#else
            list_tail->next = node;
#endif
            list_tail = node;
        }

        segment_start = str + matches[0].rm_eo;
    }

    /* Add final segment */
    {
        size_t seg_len = strlen(segment_start);
        char* segment = GC_malloc_atomic(seg_len + 1);
        memcpy(segment, segment_start, seg_len);
        segment[seg_len] = '\0';

        Index node = STRING_NEWNODE(segment, 0);
        if (!list_head) {
            list_head = list_tail = node;
        } else {
#ifdef NOBDW
            env->memory[list_tail].next = node;
#else
            list_tail->next = node;
#endif
            list_tail = node;
        }
    }

    regfree(&re);
    NULLARY(LIST_NEWNODE, list_head);
}

/**
Q0  OK  3304  regex-sub\0regex_sub  :  S S S  ->  S
Substitutes the first match of the pattern with the replacement string.
Stack: string pattern replacement -> result
*/
void regex_sub_(pEnv env)
{
    char* replacement;
    char* pattern;
    char* str;
    regex_t re;
    regmatch_t matches[1];
    char* result;
    size_t result_len;

    THREEPARAMS("regex-sub");
    STRING("regex-sub");
    replacement = GETSTRING(env->stck);
    POP(env->stck);

    STRING("regex-sub");
    pattern = GETSTRING(env->stck);
    POP(env->stck);

    STRING("regex-sub");
    str = GETSTRING(env->stck);
    POP(env->stck);

    if (compile_regex(env, pattern, &re, "regex-sub") < 0)
        return;

    if (regexec(&re, str, 1, matches, 0) != 0) {
        /* No match - return original string */
        regfree(&re);
        NULLARY(STRING_NEWNODE, GC_strdup(str));
        return;
    }

    /* Calculate result length */
    size_t prefix_len = (size_t)matches[0].rm_so;
    size_t suffix_len = strlen(str + matches[0].rm_eo);
    size_t repl_len = strlen(replacement);
    result_len = prefix_len + repl_len + suffix_len;

    result = GC_malloc_atomic(result_len + 1);
    memcpy(result, str, prefix_len);
    memcpy(result + prefix_len, replacement, repl_len);
    memcpy(result + prefix_len + repl_len, str + matches[0].rm_eo, suffix_len);
    result[result_len] = '\0';

    regfree(&re);
    NULLARY(STRING_NEWNODE, result);
}

/**
Q0  OK  3305  regex-sub-all\0regex_sub_all  :  S S S  ->  S
Substitutes all matches of the pattern with the replacement string.
Stack: string pattern replacement -> result
*/
void regex_sub_all_(pEnv env)
{
    char* replacement;
    char* pattern;
    char* str;
    regex_t re;
    regmatch_t matches[1];
    size_t repl_len;
    size_t str_len;
    size_t pos, last;
    size_t match_count = 0;
    size_t total_match_len = 0;
    size_t result_len;
    char* result;
    char* out;

    THREEPARAMS("regex-sub-all");
    STRING("regex-sub-all");
    replacement = GETSTRING(env->stck);
    POP(env->stck);

    STRING("regex-sub-all");
    pattern = GETSTRING(env->stck);
    POP(env->stck);

    STRING("regex-sub-all");
    str = GETSTRING(env->stck);
    POP(env->stck);

    if (compile_regex(env, pattern, &re, "regex-sub-all") < 0)
        return;

    repl_len = strlen(replacement);
    str_len = strlen(str);

    /* First pass: count matches and total matched length */
    pos = 0;
    while (next_match(&re, str, str_len, &pos, 1, matches)) {
        match_count++;
        total_match_len += (size_t)(matches[0].rm_eo - matches[0].rm_so);
    }

    if (match_count == 0) {
        /* No matches - return original string */
        regfree(&re);
        NULLARY(STRING_NEWNODE, GC_strdup(str));
        return;
    }

    /* Calculate result size: original - matched + replacements */
    result_len = str_len - total_match_len + (match_count * repl_len);
    result = GC_malloc_atomic(result_len + 1);
    out = result;

    /* Second pass: build result */
    pos = last = 0;
    while (next_match(&re, str, str_len, &pos, 1, matches)) {
        size_t prefix_len = (size_t)matches[0].rm_so - last;

        /* Copy prefix (text before match) */
        memcpy(out, str + last, prefix_len);
        out += prefix_len;

        /* Copy replacement */
        memcpy(out, replacement, repl_len);
        out += repl_len;
        last = (size_t)matches[0].rm_eo;
    }

    /* Copy remaining text after last match */
    memcpy(out, str + last, str_len - last);
    out += str_len - last;

    *out = '\0';

    regfree(&re);
    NULLARY(STRING_NEWNODE, result);
}

/**
Q0  OK  3306  regex-groups\0regex_groups  :  S S  ->  [S...]
Finds the first match and returns all capture groups as a list.
The first element is the full match, followed by each group.
Returns empty list if no match.
*/
void regex_groups_(pEnv env)
{
    char* pattern;
    char* str;
    regex_t re;
    regmatch_t matches[MAX_GROUPS];
    int result;
    Index list_head = 0;
    Index list_tail = 0;
    size_t i;
    int count = 0;

    TWOPARAMS("regex-groups");
    STRING("regex-groups");
    STRING2("regex-groups");
    /* Both stay on the stack until the reservation: it may run a GC */
    pattern = GETSTRING(env->stck);
    str = GETSTRING(nextnode1(env->stck));

    if (compile_regex(env, pattern, &re, "regex-groups") < 0)
        return;

    result = regexec(&re, str, MAX_GROUPS, matches, 0);

    if (result != 0) {
        regfree(&re);
        env->stck = nextnode2(env->stck);
        NULLARY(LIST_NEWNODE, 0);  /* Empty list - no match */
        return;
    }

    /* Count valid groups for ensure_capacity */
    for (i = 0; i <= re.re_nsub && i < MAX_GROUPS; i++) {
        if (matches[i].rm_so != -1)
            count++;
    }

#ifdef NOBDW
    ensure_capacity(env, count * ((int)(strlen(str) / sizeof(Node)) + 2) + 2);
    str = GETSTRING(nextnode1(env->stck));
    env->stck = nextnode2(env->stck);
#endif

    /* Build list of groups (full match + capture groups) */
    for (i = 0; i <= re.re_nsub && i < MAX_GROUPS; i++) {
        if (matches[i].rm_so == -1)
            continue;

        char* found = extract_match(env, str, &matches[i]);
        if (found) {
            Index node = STRING_NEWNODE(found, 0);
            if (!list_head) {
                list_head = list_tail = node;
            } else {
#ifdef NOBDW
                env->memory[list_tail].next = node;
#else
                list_tail->next = node;
#endif
                list_tail = node;
            }
        }
    }

    regfree(&re);
    NULLARY(LIST_NEWNODE, list_head);
}
