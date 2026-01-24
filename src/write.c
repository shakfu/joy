/*
 *  module  : write.c
 *  version : 1.3
 *  date    : 01/20/26
 */
#include "globals.h"

/*
 * writefactor - print a factor in readable format to fp.
 * Uses I/O abstraction for stdout output when callbacks are set.
 */
void writefactor(pEnv env, Index n, FILE* fp)
{
    int i;
    uint64_t set, j;
    char *ptr, buf[BUFFERMAX], tmp[MAXNUM];

#if 0
/*
 * This cannot happen. writefactor has a small number of customers: writeterm,
 * main, put, fput. They all check that the stack is not empty, so this code
 * only serves as a reminder for future customers.
 */
    if (!n)
	execerror(env, "non-empty stack", "writefactor");
#endif
    switch (nodetype(n)) {
    case USR_:
        joy_fputs(env, vec_at(env->symtab, nodevalue(n).ent).name, fp);
        break;

    case ANON_FUNCT_:
        joy_fputs(env, opername(operindex(env, nodevalue(n).proc)), fp);
        break;

    case BOOLEAN_:
        joy_fputs(env, nodevalue(n).num ? "true" : "false", fp);
        break;

    case CHAR_:
        if (nodevalue(n).num >= 8 && nodevalue(n).num <= 13)
            joy_fprintf(env, fp, "'\\%c", "btnvfr"[nodevalue(n).num - 8]);
        else if (iscntrl(nodevalue(n).num) || nodevalue(n).num == 32)
            joy_fprintf(env, fp, "'\\%03d", (int)nodevalue(n).num);
        else
            joy_fprintf(env, fp, "'%c", (int)nodevalue(n).num);
        break;

    case INTEGER_:
        joy_fprintf(env, fp, "%" PRId64, nodevalue(n).num);
        break;

    case SET_:
        joy_putc(env, '{', fp);
        for (i = 0, j = 1, set = nodevalue(n).set; i < SETSIZE; i++, j <<= 1)
            if (set & j) {
                joy_fprintf(env, fp, "%d", i);
                set &= ~j;
                if (set)
                    joy_putc(env, ' ', fp);
            }
        joy_putc(env, '}', fp);
        break;

    case STRING_:
        joy_putc(env, '"', fp);
#ifdef NOBDW
        for (ptr = (char*)&nodevalue(n); *ptr; ptr++)
#else
        for (ptr = nodevalue(n).str; *ptr; ptr++)
#endif
            if (*ptr == '"')
                joy_fputs(env, "\\\"", fp);
            else if (*ptr >= 8 && *ptr <= 13)
                joy_fprintf(env, fp, "\\%c", "btnvfr"[*ptr - 8]);
            else if (iscntrl((int)*ptr))
                joy_fprintf(env, fp, "\\%03d", *ptr);
            else
                joy_putc(env, *ptr, fp);
        joy_putc(env, '"', fp);
        break;

    case LIST_:
        joy_putc(env, '[', fp);
        writeterm(env, nodevalue(n).lis, fp);
        joy_putc(env, ']', fp);
        break;

    case FLOAT_:
        snprintf(buf, BUFFERMAX, "%g", nodevalue(n).dbl); /* exponent char e */
        ptr = strchr(buf, '.');               /* locate decimal point */
        if (!ptr) {
            char *exp_ptr = strchr(buf, 'e'); /* locate start of exponent */
            if (!exp_ptr) {
                i = buf[strlen(buf) - 1];
                if (isdigit(i))        /* check digit present */
                    strcat(buf, ".0"); /* add decimal point and 0 */
            } else {
                strcpy(tmp, exp_ptr);  /* save exponent */
                strcpy(exp_ptr, ".0"); /* add decimal point and 0 */
                strcat(buf, tmp);  /* restore exponent */
            }
        }
        joy_fputs(env, buf, fp);
        break;

    case FILE_:
        if (!nodevalue(n).fil)
            joy_fputs(env, "NULL", fp);
        else if (nodevalue(n).fil == stdin)
            joy_fputs(env, "stdin", fp);
        else if (nodevalue(n).fil == stdout)
            joy_fputs(env, "stdout", fp);
        else if (nodevalue(n).fil == stderr)
            joy_fputs(env, "stderr", fp);
        else
            joy_fprintf(env, fp, "%p", (void*)nodevalue(n).fil);
        break;

    case BIGNUM_:
#ifdef NOBDW
        joy_fputs(env, (char*)&nodevalue(n), fp);
#else
        joy_fputs(env, nodevalue(n).str, fp);
#endif
        break;

    case DICT_: {
        khash_t(Dict)* d = (khash_t(Dict)*)nodevalue(n).dict;
        khint_t k;
        int first = 1;
        joy_fputs(env, "{", fp);
        if (d) {
            for (k = kh_begin(d); k != kh_end(d); ++k) {
                if (kh_exist(d, k)) {
                    if (!first)
                        joy_fputs(env, " ", fp);
                    first = 0;
                    /* Print key */
                    joy_putc(env, '"', fp);
                    joy_fputs(env, kh_key(d, k), fp);
                    joy_fputs(env, "\": ", fp);
                    /* Print value */
                    writefactor(env, kh_value(d, k), fp);
                }
            }
        }
        joy_fputs(env, "}", fp);
        break;
    }

#ifdef JOY_NATIVE_TYPES
    case VECTOR_: {
        VectorData* v = nodevalue(n).vec;
        joy_fputs(env, "v[", fp);
        if (v) {
            for (i = 0; i < v->len; i++) {
                if (i > 0)
                    joy_putc(env, ' ', fp);
                snprintf(buf, BUFFERMAX, "%g", v->data[i]);
                /* Ensure decimal point for whole numbers */
                ptr = strchr(buf, '.');
                if (!ptr && !strchr(buf, 'e')) {
                    if (isdigit(buf[strlen(buf) - 1]))
                        strcat(buf, ".0");
                }
                joy_fputs(env, buf, fp);
            }
        }
        joy_putc(env, ']', fp);
        break;
    }

    case MATRIX_: {
        MatrixData* m = nodevalue(n).mat;
        int r, c;
        joy_fputs(env, "m[", fp);
        if (m) {
            for (r = 0; r < m->rows; r++) {
                joy_putc(env, '[', fp);
                for (c = 0; c < m->cols; c++) {
                    if (c > 0)
                        joy_putc(env, ' ', fp);
                    snprintf(buf, BUFFERMAX, "%g", m->data[r * m->cols + c]);
                    ptr = strchr(buf, '.');
                    if (!ptr && !strchr(buf, 'e')) {
                        if (isdigit(buf[strlen(buf) - 1]))
                            strcat(buf, ".0");
                    }
                    joy_fputs(env, buf, fp);
                }
                joy_putc(env, ']', fp);
            }
        }
        joy_putc(env, ']', fp);
        break;
    }
#endif /* JOY_NATIVE_TYPES */

    default:
        error(env, "a factor cannot begin with this symbol");
        break;
    }
}

/*
 * writeterm - print the contents of a list in readable format to fp.
 * Uses I/O abstraction for stdout output when callbacks are set.
 */
void writeterm(pEnv env, Index n, FILE* fp)
{
    while (n) {
        writefactor(env, n, fp);
        POP(n);
        if (n)
            joy_putc(env, ' ', fp);
    }
}
