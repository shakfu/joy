/*
 *  module  : factor.c
 *  version : 1.35
 *  date    : 09/16/24
 */
#include "globals.h"

/*
 * list2set converts a list to a set.
 */
static uint64_t list2set(pEnv env, Index n)
{
    uint64_t set;

    for (set = 0; n; POP(n))
        switch (nodetype(n)) {
        case CHAR_:
        case INTEGER_:
            if (nodevalue(n).num < 0 || nodevalue(n).num >= SETSIZE)
                error(env, "small numeric expected in set");
            else
                set |= (uint64_t)1 << nodevalue(n).set;
            break;
        default:
            error(env, "numeric expected in set");
            break;
        }
    return set;
}

/*
 * readfactor - read a factor from srcfile and push it on the stack.
 * In case of an error nothing is pushed on the stack and rv is set to 0.
 */
int readfactor(pEnv env, int ch, int* rv) /* read a JOY factor */
{
    int index;
    Entry ent;
    uint64_t set;

    *rv = 1; /* assume that a factor will be read */
    switch (env->scanner.sym) {
    case USR_:
        if ((index = lookup(env, env->str)) == 0) {
            error(env, "no such field in module");
            *rv = 0; /* no factor was read */
            break;
        }
        ent = vec_at(env->symtab, index);
        /* execute immediate functions at compile time */
        if (ent.flags == IMMEDIATE) {
            if (ent.is_user)
                exec_term(env, ent.u.body);
            else
                (*ent.u.proc)(env);
        } else if (ent.is_user)
            NULLARY(USR_NEWNODE, index);
        else
            NULLARY(ANON_FUNCT_NEWNODE, ent.u.proc);
        break;

#if 0
    /* A boolean is no longer returned by the scanner */
    case BOOLEAN_:
	NULLARY(BOOLEAN_NEWNODE, env->num);
	break;
#endif

    case CHAR_:
        NULLARY(CHAR_NEWNODE, env->num);
        break;

    case INTEGER_:
        NULLARY(INTEGER_NEWNODE, env->num);
        break;

    case STRING_:
        NULLARY(STRING_NEWNODE, env->str);
        break;

    case FLOAT_:
        NULLARY(FLOAT_NEWNODE, env->dbl);
        break;

    case '{':
        ch = getsym(env, ch);
        ch = readterm(env, ch);
        set = list2set(env, nodevalue(env->stck).lis);
        UNARY(SET_NEWNODE, set);
        if (env->scanner.sym != '}')
            error(env, "'}' expected");
        break;

    case '[':
        ch = getsym(env, ch);
        ch = readterm(env, ch);
        if (env->scanner.sym != ']')
            error(env, "']' expected");
        break;

#ifdef JOY_NATIVE_TYPES
    case VBRACKET: {
        /* Parse vector literal: v[1 2 3] */
        double* values = NULL;
        int capacity = 0, len = 0;
        VectorData* vec;

        ch = getsym(env, ch);
        while (env->scanner.sym != ']') {
            double val;
            if (env->scanner.sym == INTEGER_) {
                val = (double)env->num;
            } else if (env->scanner.sym == FLOAT_) {
                val = env->dbl;
            } else if (env->scanner.sym == '-') {
                /* Handle negative numbers: read next token */
                ch = getsym(env, ch);
                if (env->scanner.sym == INTEGER_) {
                    val = -(double)env->num;
                } else if (env->scanner.sym == FLOAT_) {
                    val = -env->dbl;
                } else {
                    error(env, "number expected after '-' in vector literal");
                    free(values);
                    *rv = 0;
                    return ch;
                }
            } else {
                error(env, "number expected in vector literal");
                free(values);
                *rv = 0;
                return ch;
            }
            if (len >= capacity) {
                capacity = capacity ? capacity * 2 : 8;
                values = realloc(values, capacity * sizeof(double));
            }
            values[len++] = val;
            ch = getsym(env, ch);
        }
        /* Create VectorData */
        vec = GC_CTX_MALLOC_ATOMIC(env, sizeof(VectorData) + len * sizeof(double));
        vec->len = len;
        for (index = 0; index < len; index++)
            vec->data[index] = values[index];
        free(values);
        NULLARY(VECTOR_NEWNODE, vec);
        break;
    }

    case MBRACKET: {
        /* Parse matrix literal: m[[1 2][3 4]]
         * Scanner consumed m[[, so we're already inside the first row.
         * Format: m[[row1][row2]...] where each row is numbers separated by spaces.
         */
        double* values = NULL;
        int capacity = 0, total = 0;
        int rows = 0, cols = -1;
        MatrixData* mat;
        int parsing_row = 1;  /* We start inside the first row */

        ch = getsym(env, ch);

        while (parsing_row) {
            int row_len = 0;

            /* Parse numbers in current row until ] */
            while (env->scanner.sym != ']') {
                double val;
                if (env->scanner.sym == INTEGER_) {
                    val = (double)env->num;
                } else if (env->scanner.sym == FLOAT_) {
                    val = env->dbl;
                } else if (env->scanner.sym == '-') {
                    ch = getsym(env, ch);
                    if (env->scanner.sym == INTEGER_) {
                        val = -(double)env->num;
                    } else if (env->scanner.sym == FLOAT_) {
                        val = -env->dbl;
                    } else {
                        error(env, "number expected after '-' in matrix literal");
                        free(values);
                        *rv = 0;
                        return ch;
                    }
                } else {
                    error(env, "number expected in matrix literal");
                    free(values);
                    *rv = 0;
                    return ch;
                }
                if (total >= capacity) {
                    capacity = capacity ? capacity * 2 : 16;
                    values = realloc(values, capacity * sizeof(double));
                }
                values[total++] = val;
                row_len++;
                ch = getsym(env, ch);
            }

            /* Check row consistency */
            if (cols == -1) {
                cols = row_len;
            } else if (row_len != cols) {
                error(env, "matrix rows must have equal length");
                free(values);
                *rv = 0;
                return ch;
            }
            rows++;

            /* Consume the ] that ended this row */
            ch = getsym(env, ch);

            /* Check what comes next: [ for another row, or ] to end matrix */
            if (env->scanner.sym == '[') {
                /* Start of another row */
                ch = getsym(env, ch);
            } else if (env->scanner.sym == ']') {
                /* End of matrix */
                parsing_row = 0;
            } else {
                error(env, "'[' or ']' expected in matrix literal");
                free(values);
                *rv = 0;
                return ch;
            }
        }

        if (cols == -1) cols = 0;  /* empty matrix */
        /* Create MatrixData */
        mat = GC_CTX_MALLOC_ATOMIC(env, sizeof(MatrixData) + rows * cols * sizeof(double));
        mat->rows = rows;
        mat->cols = cols;
        for (index = 0; index < rows * cols; index++)
            mat->data[index] = values[index];
        free(values);
        NULLARY(MATRIX_NEWNODE, mat);
        break;
    }
#endif /* JOY_NATIVE_TYPES */

    case '(':
        error(env, "'(' not implemented");
        *rv = 0; /* no factor was read */
        break;

    default:
        error(env, "a factor cannot begin with this symbol");
        *rv = 0; /* no factor was read */
        break;
    }
    return ch;
}

/*
 * readterm - read a term from srcfile and push this on the stack as a list.
 */
#ifdef NOBDW
int readterm(pEnv env, int ch)
{
    int rv = 0, first = 1;

    NULLARY(LIST_NEWNODE, 0);
    while (1) {
        if (strchr(".;]}", env->scanner.sym)
            || (env->scanner.sym >= LIBRA && env->scanner.sym <= CONST_))
            break;
        ch = readfactor(env, ch, &rv);
        if (rv) {
            if (first) {
                first = 0;
                nodevalue(nextnode1(env->stck)).lis = env->stck;
                POP(env->stck);
                nextnode1(nodevalue(env->stck).lis) = 0;
                env->dump = LIST_NEWNODE(nodevalue(env->stck).lis, env->dump);
            } else {
                nextnode1(nodevalue(env->dump).lis) = env->stck;
                POP(env->stck);
                nextnode2(nodevalue(env->dump).lis) = 0;
                nodevalue(env->dump).lis = nextnode1(nodevalue(env->dump).lis);
            }
        }
        ch = getsym(env, ch);
    }
    if (!first)
        POP(env->dump);
    return ch;
}
#else
/*
 * readterm - read a term from srcfile and pushes this on the stack as a list.
 */
int readterm(pEnv env, int ch)
{
    int rv = 0;
    Index* dump = 0;

    NULLARY(LIST_NEWNODE, 0);
    dump = &nodevalue(env->stck).lis;
    while (1) {
        if (strchr(".;]}", env->scanner.sym)
            || (env->scanner.sym >= LIBRA && env->scanner.sym <= CONST_))
            break;
        ch = readfactor(env, ch, &rv);
        if (rv) {
            *dump = env->stck;
            dump = &nextnode1(env->stck);
            env->stck = *dump;
            *dump = 0;
        }
        ch = getsym(env, ch);
    }
    return ch;
}
#endif
