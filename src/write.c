/*
 *  module  : write.c
 *  version : 1.2
 *  date    : 10/11/24
 */
#include "globals.h"
#include <stdarg.h>

static void safe_fprintf(FILE *fp, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    va_list ap_copy;
    va_copy(ap_copy, ap);
    int needed = vsnprintf(NULL, 0, fmt, ap_copy);
    va_end(ap_copy);
    if (needed < 0) {
        va_end(ap);
        return;
    }
    size_t size = (size_t)needed + 1;
    char *buffer = malloc(size);
    if (!buffer) {
        va_end(ap);
        return;
    }
    vsnprintf(buffer, size, fmt, ap);
    va_end(ap);
    fwrite(buffer, 1, size - 1, fp);
    free(buffer);
}

/*
 * writefactor - print a factor in readable format to stdout.
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
        fputs(vec_at(env->symtab, nodevalue(n).ent).name, fp);
        break;

    case ANON_FUNCT_:
        fputs(opername(operindex(env, nodevalue(n).proc)), fp);
        break;

    case BOOLEAN_:
        fputs(nodevalue(n).num ? "true" : "false", fp);
        break;

    case CHAR_:
        if (nodevalue(n).num >= 8 && nodevalue(n).num <= 13)
            safe_fprintf(fp, "'\\%c", "btnvfr"[nodevalue(n).num - 8]);
        else if (iscntrl(nodevalue(n).num) || nodevalue(n).num == 32)
            safe_fprintf(fp, "'\\%03d", (int)nodevalue(n).num);
        else
            safe_fprintf(fp, "'%c", (int)nodevalue(n).num);
        break;

    case INTEGER_:
        safe_fprintf(fp, "%" PRId64, nodevalue(n).num);
        break;

    case SET_:
        putc('{', fp);
        for (i = 0, j = 1, set = nodevalue(n).set; i < SETSIZE; i++, j <<= 1)
            if (set & j) {
                safe_fprintf(fp, "%d", i);
                set &= ~j;
                if (set)
                    putc(' ', fp);
            }
        putc('}', fp);
        break;

    case STRING_:
        putc('"', fp);
#ifdef NOBDW
        for (ptr = (char*)&nodevalue(n); *ptr; ptr++)
#else
        for (ptr = nodevalue(n).str; *ptr; ptr++)
#endif
            if (*ptr == '"')
                fputs("\\\"", fp);
            else if (*ptr >= 8 && *ptr <= 13)
                safe_fprintf(fp, "\\%c", "btnvfr"[*ptr - 8]);
            else if (iscntrl((int)*ptr))
                safe_fprintf(fp, "\\%03d", *ptr);
            else
                putc(*ptr, fp);
        putc('"', fp);
        break;

    case LIST_:
        putc('[', fp);
        writeterm(env, nodevalue(n).lis, fp);
        putc(']', fp);
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
        fputs(buf, fp);
        break;

    case FILE_:
        if (!nodevalue(n).fil)
            fputs("NULL", fp);
        else if (nodevalue(n).fil == stdin)
            fputs("stdin", fp);
        else if (nodevalue(n).fil == stdout)
            fputs("stdout", fp);
        else if (nodevalue(n).fil == stderr)
            fputs("stderr", fp);
        else
            safe_fprintf(fp, "%p", (void*)nodevalue(n).fil);
        break;

    case BIGNUM_:
#ifdef NOBDW
        fputs((char*)&nodevalue(n), fp);
#else
        fputs(nodevalue(n).str, fp);
#endif
        break;

    default:
        error("a factor cannot begin with this symbol");
        break;
    }
}

/*
 * writeterm - print the contents of a list in readable format to fp.
 */
void writeterm(pEnv env, Index n, FILE* fp)
{
    while (n) {
        writefactor(env, n, fp);
        POP(n);
        if (n)
            putc(' ', fp);
    }
}
