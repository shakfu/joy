/*
 *  module  : pattern.c
 *  version : 1.0
 *  date    : 01/22/26
 *
 *  Pattern matching combinators: match, cases
 *
 *  Pattern syntax:
 *    - Literals (integers, strings, etc.): match exact value
 *    - _ : wildcard, matches anything
 *    - name : variable, matches anything and binds value
 *    - [] : matches empty list
 *    - [x : xs] : cons pattern, matches non-empty list (head x, tail xs)
 *    - [a b c] : exact list pattern, matches list of exactly that length
 */
#include "globals.h"

/* Maximum number of bindings per pattern */
#define MAX_BINDINGS 64

/* Binding: maps a symbol table index to a value */
typedef struct {
    int sym_index;      /* Index into symtab */
    Index value;        /* Value to bind */
    Entry saved_entry;  /* Original entry for restoration */
} Binding;

/* Forward declarations */
static int pattern_match(pEnv env, Index pattern, Index value,
                         Binding* bindings, int* num_bindings);

/*
 * Check if a USR_ node is the wildcard "_"
 */
static int is_wildcard(pEnv env, Index node)
{
    if (nodetype(node) != USR_)
        return 0;
    const char* name = vec_at(env->symtab, nodevalue(node).ent).name;
    return name && name[0] == '_' && name[1] == '\0';
}

/*
 * Check if a USR_ node is the cons separator ":"
 */
static int is_cons_separator(pEnv env, Index node)
{
    if (nodetype(node) != USR_)
        return 0;
    const char* name = vec_at(env->symtab, nodevalue(node).ent).name;
    return name && name[0] == ':' && name[1] == '\0';
}

/*
 * Check if a list pattern contains a cons separator ":"
 * Returns the position of ":" (1-indexed), or 0 if not found
 */
static int find_cons_separator(pEnv env, Index list)
{
    int pos = 1;
    for (Index cur = list; cur; cur = nextnode1(cur), pos++) {
        if (is_cons_separator(env, cur))
            return pos;
    }
    return 0;
}

/*
 * Add a binding (if not wildcard)
 */
static void add_binding(pEnv env, Index pattern_var, Index value,
                        Binding* bindings, int* num_bindings)
{
    if (*num_bindings >= MAX_BINDINGS) {
        execerror(env, "match", "too many pattern variables");
        return;
    }
    int idx = nodevalue(pattern_var).ent;
    bindings[*num_bindings].sym_index = idx;
    bindings[*num_bindings].value = value;
    bindings[*num_bindings].saved_entry = vec_at(env->symtab, idx);
    (*num_bindings)++;
}

/*
 * Match a cons pattern [head_pat : tail_pat] against a value
 */
static int match_cons_pattern(pEnv env, Index pattern_list, int colon_pos,
                              Index value, Binding* bindings, int* num_bindings)
{
    /* Value must be a non-empty list */
    if (nodetype(value) != LIST_ || !nodevalue(value).lis)
        return 0;

    Index value_list = nodevalue(value).lis;
    Index head_value = value_list;
    Index tail_value_list = nextnode1(value_list);

    /* Collect head patterns (before the colon) */
    Index head_patterns[MAX_BINDINGS];
    int num_head_pats = 0;
    Index cur = pattern_list;
    for (int i = 1; i < colon_pos && cur; i++, cur = nextnode1(cur)) {
        head_patterns[num_head_pats++] = cur;
    }

    /* Skip the colon */
    if (cur)
        cur = nextnode1(cur);

    /* Collect tail patterns (after the colon) - should be exactly one */
    Index tail_pattern = cur;
    if (!tail_pattern || nextnode1(tail_pattern)) {
        /* Either no tail pattern or more than one - invalid */
        execerror(env, "match", "cons pattern requires exactly one tail pattern after :");
        return 0;
    }

    /* For simple [h : t] pattern (1 head pattern) */
    if (num_head_pats == 1) {
        /* Match head pattern against first element */
        if (!pattern_match(env, head_patterns[0], head_value, bindings, num_bindings))
            return 0;

        /* Create a list node containing the tail for matching */
        Index tail_value = LIST_NEWNODE(tail_value_list, 0);
        if (!pattern_match(env, tail_pattern, tail_value, bindings, num_bindings))
            return 0;

        return 1;
    }

    /* For patterns like [a b : rest] - match multiple heads */
    Index val_cur = value_list;
    for (int i = 0; i < num_head_pats; i++) {
        if (!val_cur)
            return 0; /* Not enough elements */
        if (!pattern_match(env, head_patterns[i], val_cur, bindings, num_bindings))
            return 0;
        val_cur = nextnode1(val_cur);
    }

    /* Match tail pattern against remaining list */
    Index remaining = LIST_NEWNODE(val_cur, 0);
    if (!pattern_match(env, tail_pattern, remaining, bindings, num_bindings))
        return 0;

    return 1;
}

/*
 * Match an exact list pattern [p1 p2 ... pn] against a value
 */
static int match_exact_list(pEnv env, Index pattern_list, Index value,
                            Binding* bindings, int* num_bindings)
{
    /* Value must be a list */
    if (nodetype(value) != LIST_)
        return 0;

    Index val_cur = nodevalue(value).lis;
    Index pat_cur = pattern_list;

    /* Match each element */
    while (pat_cur && val_cur) {
        if (!pattern_match(env, pat_cur, val_cur, bindings, num_bindings))
            return 0;
        pat_cur = nextnode1(pat_cur);
        val_cur = nextnode1(val_cur);
    }

    /* Both must be exhausted */
    return !pat_cur && !val_cur;
}

/*
 * Main pattern matching function
 *
 * Returns 1 on match, 0 on failure
 * Populates bindings array with variable->value pairs
 */
static int pattern_match(pEnv env, Index pattern, Index value,
                         Binding* bindings, int* num_bindings)
{
    Operator pat_type = nodetype(pattern);

    switch (pat_type) {
    case USR_: {
        /* Could be wildcard "_", or a variable name */
        if (is_wildcard(env, pattern)) {
            /* Wildcard matches anything, binds nothing */
            return 1;
        }
        /* Variable: matches anything, binds the value */
        add_binding(env, pattern, value, bindings, num_bindings);
        return 1;
    }

    case LIST_: {
        Index pat_list = nodevalue(pattern).lis;

        /* Empty list pattern matches empty list */
        if (!pat_list) {
            if (nodetype(value) != LIST_)
                return 0;
            return nodevalue(value).lis == 0;
        }

        /* Check for cons pattern */
        int colon_pos = find_cons_separator(env, pat_list);
        if (colon_pos > 0) {
            return match_cons_pattern(env, pat_list, colon_pos, value,
                                      bindings, num_bindings);
        }

        /* Exact list pattern */
        return match_exact_list(env, pat_list, value, bindings, num_bindings);
    }

    /* Literals: match by equality */
    case INTEGER_:
    case FLOAT_:
    case STRING_:
    case CHAR_:
    case BOOLEAN_:
    case SET_:
        return Compare(env, pattern, value) == 0;

    default:
        /* Unsupported pattern type */
        return 0;
    }
}

/*
 * Apply bindings to symbol table
 */
static void apply_bindings(pEnv env, Binding* bindings, int num_bindings)
{
    for (int i = 0; i < num_bindings; i++) {
        Entry ent = bindings[i].saved_entry;
        ent.is_user = 1;
        ent.u.body = newnode2(env, bindings[i].value, 0);
        vec_at(env->symtab, bindings[i].sym_index) = ent;
    }
}

/*
 * Restore original symbol table entries
 */
static void restore_bindings(pEnv env, Binding* bindings, int num_bindings)
{
    for (int i = 0; i < num_bindings; i++) {
        vec_at(env->symtab, bindings[i].sym_index) = bindings[i].saved_entry;
    }
}

/**
Q2  OK  2395  match  :  X [pattern] [action]  ->  result | false
Pattern matching combinator. If X matches pattern, binds pattern variables
and executes action, pushing the result. If no match, pushes false.

Patterns:
  - Literals (0, "hi", true): match exact value
  - _ : wildcard, matches anything
  - name : variable, binds X to name
  - [] : matches empty list
  - [h : t] : cons pattern, h = head, t = tail
  - [a b c] : exact list, matches 3-element list

Example: 5 [n] [n n *] match  =>  25
Example: [1 2 3] [[h : t]] [h] match  =>  1
*/
void match_(pEnv env)
{
    Index pattern, action, value;
    Binding bindings[MAX_BINDINGS];
    int num_bindings = 0;

    THREEPARAMS("match");
    TWOQUOTES("match");

    /* Get action and pattern quotations */
    action = nodevalue(env->stck).lis;
    pattern = nodevalue(nextnode1(env->stck)).lis;
    value = nextnode2(env->stck);
    env->stck = nextnode3(env->stck);

    /* Empty pattern matches anything */
    if (!pattern) {
        exec_term(env, action);
        return;
    }

    /* Pattern should be a single element for matching */
    Index pat = pattern;

    /* Attempt pattern match */
    if (pattern_match(env, pat, value, bindings, &num_bindings)) {
        /* Match succeeded: apply bindings, execute action, restore */
        apply_bindings(env, bindings, num_bindings);
        exec_term(env, action);
        restore_bindings(env, bindings, num_bindings);
    } else {
        /* Match failed: push false */
        NULLARY(BOOLEAN_NEWNODE, 0);
    }
}

/**
Q2  OK  2396  cases  :  X [[pat1] [act1]] [[pat2] [act2]] ...  ->  result
Multi-pattern dispatch combinator. Tries each pattern in order against X.
Executes the action of the first matching pattern.
Errors with "no matching pattern" if no pattern matches.

Example:
  DEFINE fact == [[[0] [pop 1]] [[n] [n 1 - fact n *]]] cases.
  5 fact  =>  120

Example:
  [1 2 3] [[[[]] [pop 0]] [[[h : _]] [h]]] cases  =>  1
*/
void cases_(pEnv env)
{
    Index cases_list, value, cur;
    Binding bindings[MAX_BINDINGS];
    int num_bindings;

    TWOPARAMS("cases");
    ONEQUOTE("cases");

    /* Get cases list */
    cases_list = nodevalue(env->stck).lis;
    value = nextnode1(env->stck);
    env->stck = nextnode2(env->stck);

    /* Try each case */
    for (cur = cases_list; cur; cur = nextnode1(cur)) {
        Index case_pair;
        Index pattern, action;

        /* Each case should be a list [pattern action] */
        if (nodetype(cur) != LIST_) {
            execerror(env, "cases", "each case must be a quotation");
            return;
        }

        case_pair = nodevalue(cur).lis;
        if (!case_pair || nodetype(case_pair) != LIST_) {
            execerror(env, "cases", "case must be [[pattern] [action]]");
            return;
        }

        /* Get pattern and action */
        pattern = nodevalue(case_pair).lis;
        if (!nextnode1(case_pair) || nodetype(nextnode1(case_pair)) != LIST_) {
            execerror(env, "cases", "case must have [pattern] and [action]");
            return;
        }
        action = nodevalue(nextnode1(case_pair)).lis;

        /* Reset bindings for this attempt */
        num_bindings = 0;

        /* Empty pattern matches anything */
        if (!pattern) {
            exec_term(env, action);
            return;
        }

        /* Attempt pattern match */
        if (pattern_match(env, pattern, value, bindings, &num_bindings)) {
            /* Match succeeded */
            apply_bindings(env, bindings, num_bindings);
            exec_term(env, action);
            restore_bindings(env, bindings, num_bindings);
            return;
        }
    }

    /* No pattern matched */
    execerror(env, "cases", "no matching pattern");
}
