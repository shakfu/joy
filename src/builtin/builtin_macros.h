/*
 *  module  : builtin_macros.h
 *  version : 1.0
 *  date    : 01/22/26
 *
 *  Consolidated macro definitions for Joy builtins.
 *  IMPORTANT: Requires globals.h to be included first.
 */
#ifndef BUILTIN_MACROS_H
#define BUILTIN_MACROS_H

/* ======== andorxor.h ======== */
/*
    module  : andorxor.h
    version : 1.3
    date    : 03/21/24
*/
#define ANDORXOR(PROCEDURE, NAME, OPER1, OPER2)                               \
    void PROCEDURE(pEnv env)                                                  \
    {                                                                         \
        TWOPARAMS(NAME);                                                      \
        SAME2TYPES(NAME);                                                     \
        switch (nodetype(env->stck)) {                                        \
        case SET_:                                                            \
            BINARY(SET_NEWNODE,                                               \
                   nodevalue(nextnode1(env->stck))                            \
                       .set OPER1 nodevalue(env->stck)                        \
                       .set);                                                 \
            return;                                                           \
        case BOOLEAN_:                                                        \
        case CHAR_:                                                           \
        case INTEGER_:                                                        \
            BINARY(BOOLEAN_NEWNODE,                                           \
                   nodevalue(nextnode1(env->stck))                            \
                       .num OPER2 nodevalue(env->stck)                        \
                       .num);                                                 \
            return;                                                           \
        default:                                                              \
            BADDATA(NAME);                                                    \
        }                                                                     \
    }

/* ======== bfloat.h ======== */
/*
    module  : bfloat.h
    version : 1.3
    date    : 03/21/24
*/
#define BFLOAT(PROCEDURE, NAME, FUNC)                                         \
    void PROCEDURE(pEnv env)                                                  \
    {                                                                         \
        TWOPARAMS(NAME);                                                      \
        FLOAT2(NAME);                                                         \
        BINARY(FLOAT_NEWNODE, FUNC(FLOATVAL2, FLOATVAL));                     \
    }

/* ======== boolean.h ======== */
/*
    module  : boolean.h
    version : 1.5
    date    : 11/15/24
*/
/*
 * A truth value is expected in a condition. That truth value need not be a
 * boolean, but if it is anything else, it should be converted into one.
 * That is the task of get_boolean. This file should be included whenever
 * a condition is needed.
 */
static int get_boolean(pEnv env, Index node)
{
    int rv = 0;
    switch (nodetype(node)) {
    /*
     * USR_ and ANON_FUNCT_ cannot be 0, so they always evaluate to true.
     */
    case USR_:
    case ANON_FUNCT_:
        rv = 1; /* assume true */
        break;
    case BOOLEAN_:
    case CHAR_:
    case INTEGER_:
        return nodevalue(node).num != 0;
    case SET_:
        return nodevalue(node).set != 0;
    case STRING_:
    case BIGNUM_:
#ifdef NOBDW
        return nodeleng(node) != 0;
#else
        return *nodevalue(node).str != 0;
#endif
    case LIST_:
        return nodevalue(node).lis != 0;
    case FLOAT_:
        return nodevalue(node).dbl != 0;
    case FILE_:
        return nodevalue(node).fil != 0;
    case DICT_:
        return nodevalue(node).dict != 0;
    }
    return rv;
}

/* ======== compare.h ======== */
/*
    module  : compare.h
    version : 1.22
    date    : 10/18/24
*/
static int is_null(pEnv env, Index node)
{
    switch (nodetype(node)) {
    case USR_:
    case ANON_FUNCT_:
        break;
    case BOOLEAN_:
    case CHAR_:
    case INTEGER_:
        return !nodevalue(node).num;
    case SET_:
        return !nodevalue(node).set;
    case STRING_:
    case BIGNUM_:
#ifdef NOBDW
        return !nodeleng(node);
#else
        return !*nodevalue(node).str;
#endif
    case LIST_:
        return !nodevalue(node).lis;
    case FLOAT_:
        return !nodevalue(node).dbl;
    case FILE_:
        return !nodevalue(node).fil;
    case DICT_: {
        khash_t(Dict)* d = (khash_t(Dict)*)nodevalue(node).dict;
        return !d || kh_size(d) == 0;
    }
    }
    return 0;
}
int Compare(pEnv env, Index first, Index second)
{
    FILE *fp1, *fp2;
    int type1, type2;
    double dbl1 = 0, dbl2 = 0;
    int64_t num1 = 0, num2 = 0;
    const char *name1 = 0, *name2 = 0;
    if (is_null(env, first) && is_null(env, second)) /* only one nothing */
        return 0;
    type1 = nodetype(first);
    type2 = nodetype(second);
    switch (type1) {
    case USR_:
        name1 = vec_at(env->symtab, nodevalue(first).ent).name;
        switch (type2) {
        case USR_:
            name2 = vec_at(env->symtab, nodevalue(second).ent).name;
            goto cmpstr;
        case ANON_FUNCT_:
            name2 = nickname(operindex(env, nodevalue(second).proc));
            goto cmpstr;
        case STRING_:
        case BIGNUM_:
            name2 = GETSTRING(second);
            goto cmpstr;
        }
        break;
    case ANON_FUNCT_:
        name1 = nickname(operindex(env, nodevalue(first).proc));
        switch (type2) {
        case USR_:
            name2 = vec_at(env->symtab, nodevalue(second).ent).name;
            goto cmpstr;
        case ANON_FUNCT_:
            name2 = nickname(operindex(env, nodevalue(second).proc));
            goto cmpstr;
        case STRING_:
        case BIGNUM_:
            name2 = GETSTRING(second);
            goto cmpstr;
        }
        break;
    case BOOLEAN_:
        num1 = nodevalue(first).num;
        switch (type2) {
        case BOOLEAN_:
        case CHAR_:
        case INTEGER_:
        case SET_:
            num2 = nodevalue(second).num;
            goto cmpnum;
        case FLOAT_:
            dbl1 = num1;
            dbl2 = nodevalue(second).dbl;
            goto cmpdbl;
        }
        break;
    case CHAR_:
        num1 = nodevalue(first).num;
        switch (type2) {
        case BOOLEAN_:
        case CHAR_:
        case INTEGER_:
        case SET_:
            num2 = nodevalue(second).num;
            goto cmpnum;
        case FLOAT_:
            dbl1 = num1;
            dbl2 = nodevalue(second).dbl;
            goto cmpdbl;
        }
        break;
    case INTEGER_:
        num1 = nodevalue(first).num;
        switch (type2) {
        case BOOLEAN_:
        case CHAR_:
        case INTEGER_:
        case SET_:
            num2 = nodevalue(second).num;
            goto cmpnum;
        case FLOAT_:
            dbl1 = num1;
            dbl2 = nodevalue(second).dbl;
            goto cmpdbl;
        }
        break;
    case SET_:
        num1 = nodevalue(first).set;
        switch (type2) {
        case BOOLEAN_:
        case CHAR_:
        case INTEGER_:
        case SET_:
        case FLOAT_:
            num2 = nodevalue(second).num;
            goto cmpnum;
        }
        break;
    case LIST_:
        break;
    case STRING_:
    case BIGNUM_:
        name1 = GETSTRING(first);
        switch (type2) {
        case USR_:
            name2 = vec_at(env->symtab, nodevalue(second).ent).name;
            goto cmpstr;
        case ANON_FUNCT_:
            name2 = nickname(operindex(env, nodevalue(second).proc));
            goto cmpstr;
        case STRING_:
        case BIGNUM_:
            name2 = GETSTRING(second);
            goto cmpstr;
        }
        break;
    case FLOAT_:
        dbl1 = nodevalue(first).dbl;
        switch (type2) {
        case BOOLEAN_:
        case CHAR_:
        case INTEGER_:
            dbl2 = nodevalue(second).num;
            goto cmpdbl;
        case SET_:
            num1 = nodevalue(first).num;
            num2 = nodevalue(second).num;
            goto cmpnum;
        case FLOAT_:
            dbl2 = nodevalue(second).dbl;
            goto cmpdbl;
        }
        break;
    case FILE_:
        fp1 = nodevalue(first).fil;
        switch (type2) {
        case FILE_:
            fp2 = nodevalue(second).fil;
            return fp1 < fp2 ? -1 : fp1 > fp2;
        }
        break;
    case DICT_:
        if (type2 == DICT_) {
            void* d1 = nodevalue(first).dict;
            void* d2 = nodevalue(second).dict;
            return d1 < d2 ? -1 : d1 > d2;
        }
        break;
    }
    return 1; /* unequal */
cmpnum:
    return num1 < num2 ? -1 : num1 > num2;
cmpdbl:
    return dbl1 < dbl2 ? -1 : dbl1 > dbl2;
cmpstr:
    if (!name1)
        name1 = "";
    if (!name2)
        name2 = "";
    num1 = strcmp(name1, name2);
    return num1 < 0 ? -1 : num1 > 0;
}

/* ======== comprel.h ======== */
/*
    module  : comprel.h
    version : 1.9
    date    : 09/17/24
*/
#define COMPREL(PROCEDURE, NAME, CONSTRUCTOR, OPR, SETCMP)                    \
    void PROCEDURE(pEnv env)                                                  \
    {                                                                         \
        uint64_t i, j;                                                        \
        int comp = 0;                                                         \
        TWOPARAMS(NAME);                                                      \
        if (nodetype(env->stck) == SET_                                       \
            || nodetype(nextnode1(env->stck)) == SET_) {                      \
            i = nodevalue(nextnode1(env->stck)).num;                          \
            j = nodevalue(env->stck).num;                                     \
            comp = SETCMP;                                                    \
        } else                                                                \
            comp = Compare(env, nextnode1(env->stck), env->stck) OPR 0;       \
        env->stck = CONSTRUCTOR(comp, nextnode2(env->stck));                  \
    }

/* ======== comprel2.h ======== */
/*
    module  : comprel2.h
    version : 1.1
    date    : 04/05/24
*/
#define COMPREL2(PROCEDURE, NAME, CONSTRUCTOR, OPR)                           \
    void PROCEDURE(pEnv env)                                                  \
    {                                                                         \
        int comp;                                                             \
        TWOPARAMS(NAME);                                                      \
        comp = Compare(env, nextnode1(env->stck), env->stck) OPR 0;           \
        env->stck = CONSTRUCTOR(comp, nextnode2(env->stck));                  \
    }

/* ======== cons_swons.h ======== */
/*
    module  : cons_swons.h
    version : 1.11
    date    : 09/17/24
*/
#define CONS_SWONS(PROCEDURE, NAME, AGGR, ELEM)                               \
    void PROCEDURE(pEnv env)                                                  \
    {                                                                         \
        Index temp;                                                           \
        char* str;                                                            \
        TWOPARAMS(NAME);                                                      \
        switch (nodetype(AGGR)) {                                             \
        case LIST_:                                                           \
            temp = newnode2(env, ELEM, nodevalue(AGGR).lis);                  \
            BINARY(LIST_NEWNODE, temp);                                       \
            break;                                                            \
        case SET_:                                                            \
            CHECKSETMEMBER(ELEM, NAME);                                       \
            BINARY(SET_NEWNODE,                                               \
                   nodevalue(AGGR).set                                        \
                       | ((int64_t)1 << nodevalue(ELEM).num));                \
            break;                                                            \
        case STRING_:                                                         \
            CHECKCHARACTER(ELEM, NAME);                                       \
            str = malloc(nodeleng(AGGR) + 2);                                 \
            str[0] = (char)nodevalue(ELEM).num;                               \
            strcpy(str + 1, (char*)&nodevalue(AGGR));                         \
            BINARY(STRING_NEWNODE, str);                                      \
            free(str);                                                        \
            break;                                                            \
        default:                                                              \
            BADAGGREGATE(NAME);                                               \
        }                                                                     \
    }

/* ======== decode.h ======== */
/*
    module  : decode.h
    version : 1.6
    date    : 11/11/24
*/
static void decode_time(pEnv env, struct tm* t)
{
    Index p;
    t->tm_year = t->tm_mon = t->tm_mday = t->tm_hour = t->tm_min = t->tm_sec
        = t->tm_isdst = t->tm_yday = t->tm_wday = 0;
    p = nodevalue(env->stck).lis;
    if (p && nodetype(p) == INTEGER_) {
        t->tm_year = nodevalue(p).num - 1900;
        POP(p);
    }
    if (p && nodetype(p) == INTEGER_) {
        t->tm_mon = nodevalue(p).num - 1;
        POP(p);
    }
    if (p && nodetype(p) == INTEGER_) {
        t->tm_mday = nodevalue(p).num;
        POP(p);
    }
    if (p && nodetype(p) == INTEGER_) {
        t->tm_hour = nodevalue(p).num;
        POP(p);
    }
    if (p && nodetype(p) == INTEGER_) {
        t->tm_min = nodevalue(p).num;
        POP(p);
    }
    if (p && nodetype(p) == INTEGER_) {
        t->tm_sec = nodevalue(p).num;
        POP(p);
    }
    if (p && nodetype(p) == BOOLEAN_) {
        t->tm_isdst = nodevalue(p).num;
        POP(p);
    }
    if (p && nodetype(p) == INTEGER_) {
        t->tm_yday = nodevalue(p).num;
        POP(p);
    }
    if (p && nodetype(p) == INTEGER_) {
        t->tm_wday = nodevalue(p).num % 7;
        POP(p);
    }
}

/* ======== dipped.h ======== */
/*
    module  : dipped.h
    version : 1.5
    date    : 09/17/24
*/
#define DIPPED(PROCEDURE, NAME, PARAMCOUNT, ARGUMENT)                         \
    void PROCEDURE(pEnv env)                                                  \
    {                                                                         \
        PARAMCOUNT(NAME);                                                     \
        SAVESTACK;                                                            \
        POP(env->stck);                                                       \
        ARGUMENT(env);                                                        \
        GNULLARY(SAVED1);                                                     \
        POP(env->dump);                                                       \
    }

/* ======== fileget.h ======== */
/*
    module  : fileget.h
    version : 1.4
    date    : 09/17/24
*/
#define FILEGET(PROCEDURE, NAME, CONSTRUCTOR, EXPR)                           \
    void PROCEDURE(pEnv env)                                                  \
    {                                                                         \
        ONEPARAM(NAME);                                                       \
        ISFILE(NAME);                                                         \
        NULLARY(CONSTRUCTOR, EXPR);                                           \
    }

/* ======== help.h ======== */
/*
    module  : help.h
    version : 1.5
    date    : 04/11/24
*/
#define HELP(PROCEDURE, REL)                                                  \
    void PROCEDURE(pEnv env)                                                  \
    {                                                                         \
        int i = vec_size(env->symtab);                                        \
        int column = 0;                                                       \
        int name_length;                                                      \
        Entry ent;                                                            \
        while (i) {                                                           \
            ent = vec_at(env->symtab, --i);                                   \
            if (strchr("#0123456789_", ent.name[0]) REL 0) {                  \
                name_length = strlen(ent.name) + 1;                           \
                if (column + name_length > HELPLINEMAX) {                     \
                    printf("\n");                                             \
                    column = 0;                                               \
                }                                                             \
                printf("%s ", ent.name);                                      \
                column += name_length;                                        \
            }                                                                 \
        }                                                                     \
        printf("\n");                                                         \
    }

/* ======== if_type.h ======== */
/*
    module  : if_type.h
    version : 1.5
    date    : 09/17/24
*/
#define IF_TYPE(PROCEDURE, NAME, TYP)                                         \
    void PROCEDURE(pEnv env)                                                  \
    {                                                                         \
        THREEPARAMS(NAME);                                                    \
        TWOQUOTES(NAME);                                                      \
        SAVESTACK;                                                            \
        env->stck = SAVED3;                                                   \
        exec_term(env,                                                          \
                nodetype(env->stck) == TYP ? nodevalue(SAVED2).lis            \
                                           : nodevalue(SAVED1).lis);          \
        POP(env->dump);                                                       \
    }

/* ======== inhas.h ======== */
/*
    module  : inhas.h
    version : 1.9
    date    : 09/17/24
*/
#define INHAS(PROCEDURE, NAME, AGGR, ELEM)                                    \
    void PROCEDURE(pEnv env)                                                  \
    {                                                                         \
        int found = 0;                                                        \
        char* str;                                                            \
        Index node;                                                           \
        TWOPARAMS(NAME);                                                      \
        switch (nodetype(AGGR)) {                                             \
        case SET_:                                                            \
            CHECKSETMEMBER(ELEM, NAME);                                       \
            found = ((nodevalue(AGGR).set)                                    \
                     & ((int64_t)1 << nodevalue(ELEM).num))                   \
                > 0;                                                          \
            break;                                                            \
        case STRING_:                                                         \
            for (str = (char*)&nodevalue(AGGR);                               \
                 *str && *str != nodevalue(ELEM).num; str++)                  \
                ;                                                             \
            found = *str != 0;                                                \
            break;                                                            \
        case LIST_:                                                           \
            node = nodevalue(AGGR).lis;                                       \
            while (node && Compare(env, node, ELEM))                          \
                node = nextnode1(node);                                       \
            found = node != 0;                                                \
            break;                                                            \
        default:                                                              \
            BADAGGREGATE(NAME);                                               \
        }                                                                     \
        BINARY(BOOLEAN_NEWNODE, found);                                       \
    }

/* ======== maxmin.h ======== */
/*
    module  : maxmin.h
    version : 1.3
    date    : 03/21/24
*/
#define MAXMIN(PROCEDURE, NAME, OPER)                                         \
    void PROCEDURE(pEnv env)                                                  \
    {                                                                         \
        TWOPARAMS(NAME);                                                      \
        if (FLOATABLE2) {                                                     \
            BINARY(FLOAT_NEWNODE,                                             \
                   FLOATVAL OPER FLOATVAL2 ? FLOATVAL2 : FLOATVAL);           \
            return;                                                           \
        }                                                                     \
        SAME2TYPES(NAME);                                                     \
        NUMERICTYPE(NAME);                                                    \
        if (nodetype(env->stck) == CHAR_)                                     \
            BINARY(CHAR_NEWNODE,                                              \
                   nodevalue(env->stck)                                       \
                           .num OPER nodevalue(nextnode1(env->stck))          \
                           .num                                               \
                       ? nodevalue(nextnode1(env->stck)).num                  \
                       : nodevalue(env->stck).num);                           \
        else                                                                  \
            BINARY(INTEGER_NEWNODE,                                           \
                   nodevalue(env->stck)                                       \
                           .num OPER nodevalue(nextnode1(env->stck))          \
                           .num                                               \
                       ? nodevalue(nextnode1(env->stck)).num                  \
                       : nodevalue(env->stck).num);                           \
    }

/* ======== n_ary.h ======== */
/*
    module  : n_ary.h
    version : 1.6
    date    : 09/17/24
*/
#define N_ARY(PROCEDURE, NAME, PARAMCOUNT, TOP)                               \
    void PROCEDURE(pEnv env)                                                  \
    {                                                                         \
        PARAMCOUNT(NAME);                                                     \
        ONEQUOTE(NAME);                                                       \
        SAVESTACK;                                                            \
        POP(env->stck);                                                       \
        exec_term(env, nodevalue(SAVED1).lis);                                  \
        CHECKVALUE(NAME);                                                     \
        env->stck = newnode2(env, env->stck, TOP);                            \
        POP(env->dump);                                                       \
    }

/* ======== of_at.h ======== */
/*
    module  : of_at.h
    version : 1.11
    date    : 09/17/24
*/
#define OF_AT(PROCEDURE, NAME, AGGR, INDEX)                                   \
    void PROCEDURE(pEnv env)                                                  \
    {                                                                         \
        Index n;                                                              \
        char* str;                                                            \
        int i, indx;                                                          \
        TWOPARAMS(NAME);                                                      \
        POSITIVEINDEX(INDEX, NAME);                                           \
        indx = nodevalue(INDEX).num;                                          \
        switch (nodetype(AGGR)) {                                             \
        case SET_:                                                            \
            CHECKEMPTYSET(nodevalue(AGGR).set, NAME);                         \
            for (i = 0; i < SETSIZE; i++) {                                   \
                if (nodevalue(AGGR).set & ((int64_t)1 << i)) {                \
                    if (indx == 0) {                                          \
                        BINARY(INTEGER_NEWNODE, i);                           \
                        return;                                               \
                    }                                                         \
                    indx--;                                                   \
                }                                                             \
            }                                                                 \
            INDEXTOOLARGE(NAME);                                              \
            break;                                                            \
        case STRING_:                                                         \
            if (indx >= (int)nodeleng(AGGR))                                  \
                INDEXTOOLARGE(NAME);                                          \
            str = (char*)&nodevalue(AGGR);                                    \
            BINARY(CHAR_NEWNODE, str[indx]);                                  \
            break;                                                            \
        case LIST_:                                                           \
            n = nodevalue(AGGR).lis;                                          \
            CHECKEMPTYLIST(n, NAME);                                          \
            while (indx > 0) {                                                \
                if (!nextnode1(n))                                            \
                    INDEXTOOLARGE(NAME);                                      \
                n = nextnode1(n);                                             \
                indx--;                                                       \
            }                                                                 \
            GBINARY(n);                                                       \
            break;                                                            \
        default:                                                              \
            BADAGGREGATE(NAME);                                               \
        }                                                                     \
    }

/* ======== ordchr.h ======== */
/*
    module  : ordchr.h
    version : 1.3
    date    : 03/21/24
*/
#define ORDCHR(PROCEDURE, NAME, RESULTTYP)                                    \
    void PROCEDURE(pEnv env)                                                  \
    {                                                                         \
        ONEPARAM(NAME);                                                       \
        NUMERICTYPE(NAME);                                                    \
        UNARY(RESULTTYP, nodevalue(env->stck).num);                           \
    }

/* ======== plusminus.h ======== */
/*
    module  : plusminus.h
    version : 1.3
    date    : 03/21/24
*/
#define PLUSMINUS(PROCEDURE, NAME, OPER)                                      \
    void PROCEDURE(pEnv env)                                                  \
    {                                                                         \
        TWOPARAMS(NAME);                                                      \
        FLOAT_I(OPER);                                                        \
        INTEGER(NAME);                                                        \
        NUMERIC2(NAME);                                                       \
        if (nodetype(nextnode1(env->stck)) == CHAR_)                          \
            BINARY(CHAR_NEWNODE,                                              \
                   nodevalue(nextnode1(env->stck))                            \
                       .num OPER nodevalue(env->stck)                         \
                       .num);                                                 \
        else                                                                  \
            BINARY(INTEGER_NEWNODE,                                           \
                   nodevalue(nextnode1(env->stck))                            \
                       .num OPER nodevalue(env->stck)                         \
                       .num);                                                 \
    }

/* ======== predsucc.h ======== */
/*
    module  : predsucc.h
    version : 1.3
    date    : 03/21/24
*/
#define PREDSUCC(PROCEDURE, NAME, OPER)                                       \
    void PROCEDURE(pEnv env)                                                  \
    {                                                                         \
        ONEPARAM(NAME);                                                       \
        NUMERICTYPE(NAME);                                                    \
        if (nodetype(env->stck) == CHAR_)                                     \
            UNARY(CHAR_NEWNODE, nodevalue(env->stck).num OPER 1);             \
        else                                                                  \
            UNARY(INTEGER_NEWNODE, nodevalue(env->stck).num OPER 1);          \
    }

/* ======== push.h ======== */
/*
    module  : push.h
    version : 1.3
    date    : 03/21/24
*/
#define PUSH(PROCEDURE, CONSTRUCTOR, VALUE)                                   \
    void PROCEDURE(pEnv env) { NULLARY(CONSTRUCTOR, VALUE); }

/* ======== someall.h ======== */
/*
    module  : someall.h
    version : 1.10
    date    : 11/06/24
*/
#define SOMEALL(PROCEDURE, NAME, INITIAL)                                     \
    void PROCEDURE(pEnv env)                                                  \
    {                                                                         \
        int i = 0, result, end_result = INITIAL;                              \
        char* str;                                                            \
        TWOPARAMS(NAME);                                                      \
        ONEQUOTE(NAME);                                                       \
        SAVESTACK;                                                            \
        switch (nodetype(SAVED2)) {                                           \
        case SET_:                                                            \
            for (; i < SETSIZE; i++)                                          \
                if (nodevalue(SAVED2).set & ((int64_t)1 << i)) {              \
                    env->stck = INTEGER_NEWNODE(i, SAVED3);                   \
                    exec_term(env, nodevalue(SAVED1).lis);                      \
                    CHECKSTACK(NAME);                                         \
                    result = get_boolean(env, env->stck);                     \
                    if (result != INITIAL) {                                  \
                        end_result = 1 - INITIAL;                             \
                        break;                                                \
                    }                                                         \
                }                                                             \
            break;                                                            \
        case STRING_:                                                         \
            for (str = strdup((char*)&nodevalue(SAVED2)); str[i]; i++) {      \
                env->stck = CHAR_NEWNODE(str[i], SAVED3);                     \
                exec_term(env, nodevalue(SAVED1).lis);                          \
                CHECKSTACK(NAME);                                             \
                result = get_boolean(env, env->stck);                         \
                if (result != INITIAL) {                                      \
                    end_result = 1 - INITIAL;                                 \
                    break;                                                    \
                }                                                             \
            }                                                                 \
            free(str);                                                        \
            break;                                                            \
        case LIST_:                                                           \
            env->dump1 = LIST_NEWNODE(nodevalue(SAVED2).lis, env->dump1);     \
            for (; DMP1; DMP1 = nextnode1(DMP1)) {                            \
                env->stck = newnode2(env, DMP1, SAVED3);                      \
                exec_term(env, nodevalue(SAVED1).lis);                          \
                CHECKSTACK(NAME);                                             \
                result = get_boolean(env, env->stck);                         \
                if (result != INITIAL) {                                      \
                    end_result = 1 - INITIAL;                                 \
                    break;                                                    \
                }                                                             \
            }                                                                 \
            POP(env->dump1);                                                  \
            break;                                                            \
        default:                                                              \
            BADAGGREGATE(NAME);                                               \
        }                                                                     \
        env->stck = BOOLEAN_NEWNODE(end_result, SAVED3);                      \
        POP(env->dump);                                                       \
    }

/* ======== type.h ======== */
/*
    module  : type.h
    version : 1.3
    date    : 03/21/24
*/
#define TYPE(PROCEDURE, NAME, REL, TYP)                                       \
    void PROCEDURE(pEnv env)                                                  \
    {                                                                         \
        ONEPARAM(NAME);                                                       \
        UNARY(BOOLEAN_NEWNODE, (nodetype(env->stck) REL TYP));                \
    }

/* ======== ufloat.h ======== */
/*
    module  : ufloat.h
    version : 1.3
    date    : 03/21/24
*/
#define UFLOAT(PROCEDURE, NAME, FUNC)                                         \
    void PROCEDURE(pEnv env)                                                  \
    {                                                                         \
        ONEPARAM(NAME);                                                       \
        FLOAT(NAME);                                                          \
        UNARY(FLOAT_NEWNODE, FUNC(FLOATVAL));                                 \
    }

/* ======== unmktime.h ======== */
/*
    module  : unmktime.h
    version : 1.11
    date    : 09/17/24
*/
#define UNMKTIME(PROCEDURE, NAME, FUNC)                                       \
    void PROCEDURE(pEnv env)                                                  \
    {                                                                         \
        int wday;                                                             \
        Index temp;                                                           \
        struct tm* t;                                                         \
        time_t timval;                                                        \
        ONEPARAM(NAME);                                                       \
        INTEGER(NAME);                                                        \
        timval = nodevalue(env->stck).num;                                    \
        t = FUNC(&timval);                                                    \
        wday = t->tm_wday;                                                    \
        if (wday == 0)                                                        \
            wday = 7;                                                         \
        env->dump1 = LIST_NEWNODE(0, env->dump1);                             \
        temp = INTEGER_NEWNODE(wday, DMP1);                                   \
        DMP1 = temp;                                                          \
        temp = INTEGER_NEWNODE(t->tm_yday, DMP1);                             \
        DMP1 = temp;                                                          \
        temp = BOOLEAN_NEWNODE(t->tm_isdst, DMP1);                            \
        DMP1 = temp;                                                          \
        temp = INTEGER_NEWNODE(t->tm_sec, DMP1);                              \
        DMP1 = temp;                                                          \
        temp = INTEGER_NEWNODE(t->tm_min, DMP1);                              \
        DMP1 = temp;                                                          \
        temp = INTEGER_NEWNODE(t->tm_hour, DMP1);                             \
        DMP1 = temp;                                                          \
        temp = INTEGER_NEWNODE(t->tm_mday, DMP1);                             \
        DMP1 = temp;                                                          \
        temp = INTEGER_NEWNODE((t->tm_mon + 1), DMP1);                        \
        DMP1 = temp;                                                          \
        temp = INTEGER_NEWNODE((t->tm_year + 1900), DMP1);                    \
        UNARY(LIST_NEWNODE, temp);                                            \
        POP(env->dump1);                                                      \
    }

/* ======== usetop.h ======== */
/*
    module  : usetop.h
    version : 1.3
    date    : 03/21/24
*/
#define USETOP(PROCEDURE, NAME, TYPE, BODY)                                   \
    void PROCEDURE(pEnv env)                                                  \
    {                                                                         \
        ONEPARAM(NAME);                                                       \
        TYPE(NAME);                                                           \
        BODY;                                                                 \
        POP(env->stck);                                                       \
    }

#endif /* BUILTIN_MACROS_H */
