/*
 *  module  : aggregate.c
 *  version : 1.0
 *  date    : 01/22/26
 *
 *  Grouped aggregate/list builtins: assign, at, concat, cons, drop, enconcat,
 *  first, list, null, of, rest, size, small, split, swons, take, unassign,
 *  uncons, unswons
 */
#include "globals.h"

/* Include shared helper headers */
#include "boolean.h"
#include "cons_swons.h"
#include "of_at.h"

/* List operations */
/**
Q0  IGNORE_POP  3140  assign  :  V [N]  ->
[IMPURE] Assigns value V to the variable with name N.
*/
void assign_(pEnv env)
{
    int index;
    Entry ent;
    Index lis, node;

    TWOPARAMS("assign");              /* name and value */
    ONEQUOTE("assign");               /* quotation on top */
    lis = nodevalue(env->stck).lis;   /* singleton list */
    CHECKEMPTYLIST(lis, "assign");    /* check non-empty list */
    USERDEF2(lis, "assign");          /* check user defined name */
    index = nodevalue(lis).ent;       /* index user defined name */
    ent = vec_at(env->symtab, index); /* symbol table entry */
    lis = POP(env->stck);             /* value pointer */
    node = newnode2(env, lis, 0);
    POP(env->stck);                /* bump stack again */
    ent.is_root = ent.is_user = 1; /* ensure user defined and root */
    if (!env->variable_busy)       /* test whether this is the first */
        env->variable_busy = ent.is_last = 1;
    ent.u.body = node;                /* insert value in body */
    vec_at(env->symtab, index) = ent; /* update symbol table */
}

/**
Q0  OK  2060  at  :  A I  ->  X
X (= A[I]) is the member of A at position I.
*/
OF_AT(at_, "at", nextnode1(env->stck), env->stck)

/**
Q0  OK  2150  concat  :  S T  ->  U
Sequence U is the concatenation of sequences S and T.
*/
void concat_(pEnv env)
{
    char* str;
    Index temp;
    size_t leng;

    TWOPARAMS("concat");
    SAME2TYPES("concat");
    switch (nodetype(env->stck)) {
    case SET_:
        BINARY(SET_NEWNODE,
               nodevalue(nextnode1(env->stck)).set | nodevalue(env->stck).set);
        break;
    case STRING_:
        leng = nodeleng(nextnode1(env->stck)) + nodeleng(env->stck) + 1;
        str = malloc(leng);
        snprintf(str, leng, "%s%s", (char*)&nodevalue(nextnode1(env->stck)),
                 (char*)&nodevalue(env->stck));
        BINARY(STRING_NEWNODE, str);
        free(str);
        break;
    case LIST_:
        if (!nodevalue(nextnode1(env->stck)).lis) {
            BINARY(LIST_NEWNODE, nodevalue(env->stck).lis);
            return;
        }
        env->dump1 = LIST_NEWNODE(nodevalue(nextnode1(env->stck)).lis,
                                  env->dump1);    /* old */
        env->dump2 = LIST_NEWNODE(0, env->dump2); /* head */
        env->dump3 = LIST_NEWNODE(0, env->dump3); /* last */
        for (; DMP1; DMP1 = nextnode1(DMP1)) {
            temp = newnode2(env, DMP1, 0);
            if (!DMP2) { /* first */
                DMP2 = temp;
                DMP3 = DMP2;
            } else { /* further */
                nextnode1(DMP3) = temp;
                DMP3 = nextnode1(DMP3);
            }
        }
        nextnode1(DMP3) = nodevalue(env->stck).lis;
        BINARY(LIST_NEWNODE, DMP2);
        POP(env->dump1);
        POP(env->dump2);
        POP(env->dump3);
        break;
    default:
        BADAGGREGATE("concat");
    }
}

/**
Q0  OK  2010  cons  :  X A  ->  B
Aggregate B is A with a new member X (first member for sequences).
*/
CONS_SWONS(cons_, "cons", env->stck, nextnode1(env->stck))

/**
Q0  OK  2130  drop  :  A N  ->  B
Aggregate B is the result of deleting the first N elements of A.
*/
void drop_(pEnv env)
{
    char* str;
    Index list;
    uint64_t set;
    int i = 0, n;

    TWOPARAMS("drop");
    POSITIVEINDEX(env->stck, "drop");
    n = nodevalue(env->stck).num;
    POP(env->stck);
    switch (nodetype(env->stck)) {
    case SET_:
        for (set = 0; i < SETSIZE; i++)
            if (nodevalue(env->stck).set & ((int64_t)1 << i)) {
                if (n < 1)
                    set |= ((int64_t)1 << i);
                else
                    n--;
            }
        UNARY(SET_NEWNODE, set);
        break;
    case STRING_:
        str = GETSTRING(env->stck);
        while (n-- > 0 && str[i])
            i++;
#ifdef NOBDW
        str = strdup(str + i);
        UNARY(STRING_NEWNODE, str);
        free(str);
#else
        UNARY(STRING_NEWNODE, GC_strdup(str + i));
#endif
        break;
    case LIST_:
        list = nodevalue(env->stck).lis;
        while (n-- > 0 && list)
            list = nextnode1(list);
        UNARY(LIST_NEWNODE, list);
        break;
    default:
        BADAGGREGATE("drop");
    }
}

/**
Q0  OK  2160  enconcat  :  X S T  ->  U
Sequence U is the concatenation of sequences S and T
with X inserted between S and T (== swapd cons concat).
*/
void enconcat_(pEnv env)
{
    THREEPARAMS("enconcat");
    SAME2TYPES("enconcat");
    swapd_(env);
    cons_(env);
    concat_(env);
}

/**
Q0  OK  2030  first  :  A  ->  F
F is the first member of the non-empty aggregate A.
*/
void first_(pEnv env)
{
    int i = 0;
    char* str;

    ONEPARAM("first");
    switch (nodetype(env->stck)) {
    case LIST_:
        CHECKEMPTYLIST(nodevalue(env->stck).lis, "first");
        GUNARY(nodevalue(env->stck).lis);
        break;
    case STRING_:
        str = GETSTRING(env->stck);
        CHECKEMPTYSTRING(str, "first");
        UNARY(CHAR_NEWNODE, *str);
        break;
    case SET_:
        CHECKEMPTYSET(nodevalue(env->stck).set, "first");
        while (!(nodevalue(env->stck).set & ((int64_t)1 << i)))
            i++;
        UNARY(INTEGER_NEWNODE, i);
        break;
    default:
        BADAGGREGATE("first");
    }
}

/**
Q0  OK  2200  null  :  X  ->  B
Tests for empty aggregate X or zero numeric.
*/
void null_(pEnv env)
{
    ONEPARAM("null");
    switch (nodetype(env->stck)) {
#if 0
    case USR_:
	UNARY(BOOLEAN_NEWNODE, (!nodevalue(env->stck).ent));
	break;
    case ANON_FUNCT_:
	UNARY(BOOLEAN_NEWNODE, (!nodevalue(env->stck).proc));
	break;
#endif
    case BOOLEAN_:
    case CHAR_:
    case INTEGER_:
        UNARY(BOOLEAN_NEWNODE, (!nodevalue(env->stck).num));
        break;
    case SET_:
        UNARY(BOOLEAN_NEWNODE, (!nodevalue(env->stck).set));
        break;
    case STRING_:
#ifdef NOBDW
        UNARY(BOOLEAN_NEWNODE, (!nodeleng(env->stck)));
#else
        UNARY(BOOLEAN_NEWNODE, (!*(nodevalue(env->stck).str)));
#endif
        break;
    case LIST_:
        UNARY(BOOLEAN_NEWNODE, (!nodevalue(env->stck).lis));
        break;
    case FLOAT_:
        UNARY(BOOLEAN_NEWNODE, (!nodevalue(env->stck).dbl));
        break;
    case FILE_:
        UNARY(BOOLEAN_NEWNODE, (!nodevalue(env->stck).fil));
        break;
    default:
        UNARY(BOOLEAN_NEWNODE, 0); /* false */
        break;
    }
}

/**
Q0  OK  2070  of  :  I A  ->  X
X (= A[I]) is the I-th member of aggregate A.
*/
OF_AT(of_, "of", env->stck, nextnode1(env->stck))

/**
Q0  OK  2040  rest  :  A  ->  R
R is the non-empty aggregate A with its first member removed.
*/
void rest_(pEnv env)
{
    int i = 0;
    char* str;

    ONEPARAM("rest");
    switch (nodetype(env->stck)) {
    case SET_:
        CHECKEMPTYSET(nodevalue(env->stck).set, "rest");
        while (!(nodevalue(env->stck).set & ((int64_t)1 << i)))
            i++;
        UNARY(SET_NEWNODE, nodevalue(env->stck).set & ~((int64_t)1 << i));
        break;
    case STRING_:
        str = GETSTRING(env->stck);
        CHECKEMPTYSTRING(str, "rest");
#ifdef NOBDW
        str = strdup(str + 1);
        UNARY(STRING_NEWNODE, str);
        free(str);
#else
        UNARY(STRING_NEWNODE, GC_strdup(str + 1));
#endif
        break;
    case LIST_:
        CHECKEMPTYLIST(nodevalue(env->stck).lis, "rest");
        UNARY(LIST_NEWNODE, nextnode1(nodevalue(env->stck).lis));
        break;
    default:
        BADAGGREGATE("rest");
    }
}

/**
Q0  OK  2080  size  :  A  ->  I
Integer I is the number of elements of aggregate A.
*/
void size_(pEnv env)
{
    Index list;
    int i, size = 0;

    ONEPARAM("size");
    switch (nodetype(env->stck)) {
    case SET_:
        for (i = 0; i < SETSIZE; i++)
            if (nodevalue(env->stck).set & ((int64_t)1 << i))
                size++;
        break;
    case STRING_:
#ifdef NOBDW
        size = nodeleng(env->stck);
#else
        size = strlen(nodevalue(env->stck).str);
#endif
        break;
    case LIST_:
        for (list = nodevalue(env->stck).lis; list; list = nextnode1(list))
            size++;
        break;
    default:
        BADAGGREGATE("size");
    }
    UNARY(INTEGER_NEWNODE, size);
}

/**
Q0  OK  2210  small  :  X  ->  B
Tests whether aggregate X has 0 or 1 members, or numeric 0 or 1.
*/
void small_(pEnv env)
{
    int i = 0, small = 0;

    ONEPARAM("small");
    switch (nodetype(env->stck)) {
    case BOOLEAN_:
    case CHAR_:
    case INTEGER_:
        small = nodevalue(env->stck).num < 2;
        break;
    case SET_:
        if (nodevalue(env->stck).set == 0)
            small = 1;
        else {
            while (!(nodevalue(env->stck).set & ((int64_t)1 << i)))
                i++;
            small = (nodevalue(env->stck).set & ~((int64_t)1 << i)) == 0;
        }
        break;
    case STRING_:
#ifdef NOBDW
        small = nodeleng(env->stck) < 2;
#else
        small = !*nodevalue(env->stck).str || !nodevalue(env->stck).str[1];
#endif
        break;
    case LIST_:
        small = !nodevalue(env->stck).lis
            || !nextnode1(nodevalue(env->stck).lis);
        break;
    default:
        BADDATA("small");
    }
    UNARY(BOOLEAN_NEWNODE, small);
}

/**
Q1  OK  2840  split  :  A [B]  ->  A1 A2
Uses test B to split aggregate A into sametype aggregates A1 and A2.
*/
void split_(pEnv env)
{
    Index temp;
    uint64_t yes_set = 0, no_set = 0;
    char *str, *yesstring, *nostring;
    int i = 0, yesptr = 0, noptr = 0, result = 0;

    TWOPARAMS("split");
    ONEQUOTE("split");
    SAVESTACK;
    switch (nodetype(SAVED2)) {
    case SET_:
        for (; i < SETSIZE; i++)
            if (nodevalue(SAVED2).set & ((int64_t)1 << i)) {
                env->stck = INTEGER_NEWNODE(i, SAVED3);
                exec_term(env, nodevalue(SAVED1).lis);
                CHECKSTACK("split");
                result = get_boolean(env, env->stck);
                if (result)
                    yes_set |= ((int64_t)1 << i);
                else
                    no_set |= ((int64_t)1 << i);
            }
        env->stck = SET_NEWNODE(yes_set, SAVED3);
        NULLARY(SET_NEWNODE, no_set);
        break;
    case STRING_:
        yesstring = malloc(nodeleng(SAVED2) + 1);
        nostring = malloc(nodeleng(SAVED2) + 1);
        for (str = strdup((char*)&nodevalue(SAVED2)); str[i]; i++) {
            env->stck = CHAR_NEWNODE(str[i], SAVED3);
            exec_term(env, nodevalue(SAVED1).lis);
            CHECKSTACK("split");
            result = get_boolean(env, env->stck);
            if (result)
                yesstring[yesptr++] = str[i];
            else
                nostring[noptr++] = str[i];
        }
        yesstring[yesptr] = 0;
        nostring[noptr] = 0;
        env->stck = STRING_NEWNODE(yesstring, SAVED3);
        NULLARY(STRING_NEWNODE, nostring);
        free(str);
        free(nostring);
        free(yesstring);
        break;
    case LIST_:
        env->dump1 = LIST_NEWNODE(nodevalue(SAVED2).lis, env->dump1);
        env->dump2 = LIST_NEWNODE(0, env->dump2); /* head true */
        env->dump3 = LIST_NEWNODE(0, env->dump3); /* last true */
        env->dump4 = LIST_NEWNODE(0, env->dump4); /* head false */
        env->dump5 = LIST_NEWNODE(0, env->dump5); /* last false */
        for (; DMP1; DMP1 = nextnode1(DMP1)) {
            env->stck = newnode2(env, DMP1, SAVED3);
            exec_term(env, nodevalue(SAVED1).lis);
            CHECKSTACK("split");
            temp = newnode2(env, DMP1, 0);
            result = get_boolean(env, env->stck);
            if (result) {    /* pass */
                if (!DMP2) { /* first */
                    DMP2 = temp;
                    DMP3 = DMP2;
                } else { /* further */
                    nextnode1(DMP3) = temp;
                    DMP3 = nextnode1(DMP3);
                }
            } else {         /* fail */
                if (!DMP4) { /* first */
                    DMP4 = temp;
                    DMP5 = DMP4;
                } else { /* further */
                    nextnode1(DMP5) = temp;
                    DMP5 = nextnode1(DMP5);
                }
            }
        }
        env->stck = LIST_NEWNODE(DMP2, SAVED3);
        NULLARY(LIST_NEWNODE, DMP4);
        POP(env->dump5);
        POP(env->dump4);
        POP(env->dump3);
        POP(env->dump2);
        POP(env->dump1);
        break;
    default:
        BADAGGREGATE("split");
    }
    POP(env->dump);
}

/**
Q0  OK  2020  swons  :  A X  ->  B
Aggregate B is A with a new member X (first member for sequences).
*/
CONS_SWONS(swons_, "swons", nextnode1(env->stck), env->stck)

/**
Q0  OK  2140  take  :  A N  ->  B
Aggregate B is the result of retaining just the first N elements of A.
*/
void take_(pEnv env)
{
    char* str;
    Index temp;
    int i = 0, n;
    uint64_t set;

    TWOPARAMS("take");
    POSITIVEINDEX(env->stck, "take");
    n = nodevalue(env->stck).num;
    POP(env->stck);
    switch (nodetype(env->stck)) {
    case SET_:
        for (set = 0; i < SETSIZE && n; i++)
            if (nodevalue(env->stck).set & ((int64_t)1 << i)) {
                set |= ((int64_t)1 << i);
                n--;
            }
        UNARY(SET_NEWNODE, set);
        break;
    case STRING_:
        if (n >= (int)nodeleng(env->stck))
            return; /* the old string unchanged */
        str = strdup((char*)&nodevalue(env->stck));
        str[n] = 0; /* end the string */
        UNARY(STRING_NEWNODE, str);
        free(str);
        break;
    case LIST_:
        env->dump1 = LIST_NEWNODE(nodevalue(env->stck).lis, env->dump1);
        env->dump2 = LIST_NEWNODE(0, env->dump2); /* head */
        env->dump3 = LIST_NEWNODE(0, env->dump3); /* last */
        for (; DMP1 && n; DMP1 = nextnode1(DMP1), n--) {
            temp = newnode2(env, DMP1, 0);
            if (!DMP2) { /* first */
                DMP2 = temp;
                DMP3 = DMP2;
            } else { /* further */
                nextnode1(DMP3) = temp;
                DMP3 = nextnode1(DMP3);
            }
        }
        UNARY(LIST_NEWNODE, DMP2);
        POP(env->dump1);
        POP(env->dump2);
        POP(env->dump3);
        break;
    default:
        BADAGGREGATE("take");
    }
}

/**
Q0  IGNORE_POP  3230  unassign  :  [N]  ->
[IMPURE] Sets the body of the name N to uninitialized.
*/
void unassign_(pEnv env)
{
    Index lis;
    int index;
    Entry ent;

    ONEPARAM(__FILE__);               /* name */
    ONEQUOTE("unassign");             /* quotation on top */
    lis = nodevalue(env->stck).lis;   /* singleton list */
    CHECKEMPTYLIST(lis, "unassign");  /* check non-empty list */
    USERDEF2(lis, "unassign");        /* check user defined name */
    index = nodevalue(lis).ent;       /* index user defined name */
    ent = vec_at(env->symtab, index); /* symbol table entry */
    POP(env->stck);                   /* bump stack again */
    ent.is_root = ent.is_user = 1;    /* ensure user defined and root */
    ent.u.body = 0;                   /* ensure empty body */
    vec_at(env->symtab, index) = ent; /* update symbol table */
}

/**
Q0  OK  2110  uncons  :  A  ->  F R
F and R are the first and the rest of non-empty aggregate A.
*/
void uncons_(pEnv env)
{
    int i = 0;
    char* str;
    uint64_t set;

    ONEPARAM("uncons");
    switch (nodetype(env->stck)) {
    case SET_:
        set = nodevalue(env->stck).set;
        CHECKEMPTYSET(set, "uncons");
        while (!(set & ((int64_t)1 << i)))
            i++;
        UNARY(INTEGER_NEWNODE, i);
        NULLARY(SET_NEWNODE, set & ~((int64_t)1 << i));
        break;
    case STRING_:
        str = strdup((char*)&nodevalue(env->stck));
        CHECKEMPTYSTRING(str, "uncons");
        UNARY(CHAR_NEWNODE, *str);
        NULLARY(STRING_NEWNODE, str + 1);
        free(str);
        break;
    case LIST_:
        SAVESTACK;
        CHECKEMPTYLIST(nodevalue(SAVED1).lis, "uncons");
        GUNARY(nodevalue(SAVED1).lis);
        NULLARY(LIST_NEWNODE, nextnode1(nodevalue(SAVED1).lis));
        POP(env->dump);
        break;
    default:
        BADAGGREGATE("uncons");
    }
}

/**
Q0  OK  2120  unswons  :  A  ->  R F
R and F are the rest and the first of non-empty aggregate A.
*/
void unswons_(pEnv env)
{
    int i = 0;
    char* str;
    uint64_t set;

    ONEPARAM("unswons");
    switch (nodetype(env->stck)) {
    case SET_:
        set = nodevalue(env->stck).set;
        CHECKEMPTYSET(set, "unswons");
        while (!(set & ((int64_t)1 << i)))
            i++;
        UNARY(SET_NEWNODE, set & ~((int64_t)1 << i));
        NULLARY(INTEGER_NEWNODE, i);
        break;
    case STRING_:
        str = strdup((char*)&nodevalue(env->stck));
        CHECKEMPTYSTRING(str, "unswons");
        UNARY(STRING_NEWNODE, str + 1);
        NULLARY(CHAR_NEWNODE, *str);
        free(str);
        break;
    case LIST_:
        SAVESTACK;
        CHECKEMPTYLIST(nodevalue(SAVED1).lis, "unswons");
        UNARY(LIST_NEWNODE, nextnode1(nodevalue(SAVED1).lis));
        GNULLARY(nodevalue(SAVED1).lis);
        POP(env->dump);
        break;
    default:
        BADAGGREGATE("unswons");
    }
}
