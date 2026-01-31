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

/* Helper: preprocess pattern to expand Perl-style shortcuts to POSIX */
static char* preprocess_pattern(const char* pattern)
{
    /* First pass: calculate output size */
    size_t out_len = 0;
    const char* p = pattern;
    int in_class = 0;  /* Track if inside [...] */

    while (*p) {
        if (*p == '[' && !in_class) {
            in_class = 1;
            out_len++;
            p++;
        } else if (*p == ']' && in_class) {
            in_class = 0;
            out_len++;
            p++;
        } else if (*p == '\\' && p[1]) {
            switch (p[1]) {
                case 'd':
                    out_len += in_class ? sizeof(EXPAND_d_CLASS) - 1 : sizeof(EXPAND_d) - 1;
                    p += 2;
                    break;
                case 'D':
                    /* \D inside class is complex - just use the standalone form */
                    out_len += sizeof(EXPAND_D) - 1;
                    p += 2;
                    break;
                case 'w':
                    out_len += in_class ? sizeof(EXPAND_w_CLASS) - 1 : sizeof(EXPAND_w) - 1;
                    p += 2;
                    break;
                case 'W':
                    out_len += sizeof(EXPAND_W) - 1;
                    p += 2;
                    break;
                case 's':
                    out_len += in_class ? sizeof(EXPAND_s_CLASS) - 1 : sizeof(EXPAND_s) - 1;
                    p += 2;
                    break;
                case 'S':
                    out_len += sizeof(EXPAND_S) - 1;
                    p += 2;
                    break;
                case '\\':
                    out_len += 2;
                    p += 2;
                    break;
                default:
                    out_len += 2;
                    p += 2;
                    break;
            }
        } else {
            out_len++;
            p++;
        }
    }

    /* Allocate and build output */
    char* result = GC_malloc_atomic(out_len + 1);
    char* out = result;
    p = pattern;
    in_class = 0;

    while (*p) {
        if (*p == '[' && !in_class) {
            in_class = 1;
            *out++ = *p++;
        } else if (*p == ']' && in_class) {
            in_class = 0;
            *out++ = *p++;
        } else if (*p == '\\' && p[1]) {
            const char* expand = NULL;
            switch (p[1]) {
                case 'd': expand = in_class ? EXPAND_d_CLASS : EXPAND_d; break;
                case 'D': expand = EXPAND_D; break;
                case 'w': expand = in_class ? EXPAND_w_CLASS : EXPAND_w; break;
                case 'W': expand = EXPAND_W; break;
                case 's': expand = in_class ? EXPAND_s_CLASS : EXPAND_s; break;
                case 'S': expand = EXPAND_S; break;
            }
            if (expand) {
                size_t len = strlen(expand);
                memcpy(out, expand, len);
                out += len;
                p += 2;
            } else {
                /* Keep other escape sequences as-is */
                *out++ = *p++;
                *out++ = *p++;
            }
        } else {
            *out++ = *p++;
        }
    }
    *out = '\0';

    return result;
}

/* Helper: compile regex with error handling */
static int compile_regex(pEnv env, const char* pattern, regex_t* re, const char* op)
{
    /* Preprocess to expand Perl-style shortcuts */
    char* expanded = preprocess_pattern(pattern);

    int ret = regcomp(re, expanded, REG_EXTENDED | REG_NEWLINE);
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
    int result;
    const char* cursor;
    Index list_head = 0;
    Index list_tail = 0;
    int count = 0;

    TWOPARAMS("regex-find-all");
    STRING("regex-find-all");
    pattern = GETSTRING(env->stck);
    POP(env->stck);

    STRING("regex-find-all");
    str = GETSTRING(env->stck);
    POP(env->stck);

    if (compile_regex(env, pattern, &re, "regex-find-all") < 0)
        return;

    /* Count matches first for ensure_capacity */
    cursor = str;
    while (regexec(&re, cursor, 1, matches, 0) == 0) {
        count++;
        if (matches[0].rm_eo == 0)
            break;  /* Prevent infinite loop on zero-width match */
        cursor += matches[0].rm_eo;
        if (*cursor == '\0')
            break;
    }

#ifdef NOBDW
    if (count > 0)
        ensure_capacity(env, count);
#endif

    /* Now collect matches */
    cursor = str;
    while ((result = regexec(&re, cursor, MAX_GROUPS, matches, 0)) == 0) {
        char* found;
        Index node;

        /* If there's a capture group, use it; otherwise use full match */
        if (re.re_nsub > 0 && matches[1].rm_so != -1) {
            found = extract_match(env, cursor, &matches[1]);
        } else {
            found = extract_match(env, cursor, &matches[0]);
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

        /* Move cursor past this match */
        if (matches[0].rm_eo == 0)
            break;  /* Prevent infinite loop on zero-width match */
        cursor += matches[0].rm_eo;
        if (*cursor == '\0')
            break;
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
    const char* cursor;
    const char* segment_start;
    Index list_head = 0;
    Index list_tail = 0;
    int count = 0;

    TWOPARAMS("regex-split");
    STRING("regex-split");
    pattern = GETSTRING(env->stck);
    POP(env->stck);

    STRING("regex-split");
    str = GETSTRING(env->stck);
    POP(env->stck);

    if (compile_regex(env, pattern, &re, "regex-split") < 0)
        return;

    /* Count segments first */
    cursor = str;
    while (regexec(&re, cursor, 1, matches, 0) == 0) {
        count++;
        if (matches[0].rm_eo == 0)
            break;
        cursor += matches[0].rm_eo;
        if (*cursor == '\0')
            break;
    }
    count++;  /* Final segment after last match */

#ifdef NOBDW
    ensure_capacity(env, count);
#endif

    /* Collect segments */
    cursor = str;
    segment_start = str;

    while (regexec(&re, cursor, 1, matches, 0) == 0) {
        /* Extract segment before this match */
        size_t seg_len = (size_t)((cursor + matches[0].rm_so) - segment_start);
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

        /* Move past the match */
        if (matches[0].rm_eo == 0) {
            cursor++;
            segment_start = cursor;
        } else {
            cursor += matches[0].rm_eo;
            segment_start = cursor;
        }

        if (*cursor == '\0')
            break;
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
    const char* cursor;
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
    cursor = str;
    while (regexec(&re, cursor, 1, matches, 0) == 0) {
        match_count++;
        total_match_len += (size_t)(matches[0].rm_eo - matches[0].rm_so);

        if (matches[0].rm_eo == 0) {
            cursor++;
        } else {
            cursor += matches[0].rm_eo;
        }
        if (*cursor == '\0')
            break;
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
    cursor = str;
    while (regexec(&re, cursor, 1, matches, 0) == 0) {
        size_t prefix_len = (size_t)matches[0].rm_so;

        /* Copy prefix (text before match) */
        memcpy(out, cursor, prefix_len);
        out += prefix_len;

        /* Copy replacement */
        memcpy(out, replacement, repl_len);
        out += repl_len;

        /* Move cursor past match */
        if (matches[0].rm_eo == 0) {
            cursor++;
        } else {
            cursor += matches[0].rm_eo;
        }

        if (*cursor == '\0')
            break;
    }

    /* Copy remaining text after last match */
    {
        size_t remaining = strlen(cursor);
        memcpy(out, cursor, remaining);
        out += remaining;
    }

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
    pattern = GETSTRING(env->stck);
    POP(env->stck);

    STRING("regex-groups");
    str = GETSTRING(env->stck);
    POP(env->stck);

    if (compile_regex(env, pattern, &re, "regex-groups") < 0)
        return;

    result = regexec(&re, str, MAX_GROUPS, matches, 0);

    if (result != 0) {
        regfree(&re);
        NULLARY(LIST_NEWNODE, 0);  /* Empty list - no match */
        return;
    }

    /* Count valid groups for ensure_capacity */
    for (i = 0; i <= re.re_nsub && i < MAX_GROUPS; i++) {
        if (matches[i].rm_so != -1)
            count++;
    }

#ifdef NOBDW
    if (count > 0)
        ensure_capacity(env, count);
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
