/* FILE : scan.c */
/*
 *  module  : scan.c
 *  version : 1.86
 *  date    : 10/11/24
 */
#include "globals.h"
#include <stdarg.h>

static struct keys {
    char* name;
    Operator sym;
} keywords[] = { { "LIBRA", LIBRA },
                 { "DEFINE", LIBRA },
                 { "HIDE", HIDE },
                 { "IN", IN__ },
                 { "END", '.' },
                 { "MODULE", MODULE_ },
                 { "PRIVATE", PRIVATE },
                 { "PUBLIC", PUBLIC },
                 { "CONST", CONST_ },
                 { "INLINE", CONST_ },
                 /* possibly other uppers here */
                 { "==", EQDEF } };

/* Scanner state is now in Env struct:
 *   env->scanner.srcfile     - FILE pointer to input file
 *   env->scanner.srcfilename - name of input file in messages
 *   env->scanner.linenum     - line number for errors
 *   env->scanner.linepos     - position in line
 *   env->linebuf     - buffered input line
 *   env->scanner.infile[]    - include file stack
 *   env->scanner.ilevel      - index in infile-structure (-1 = empty)
 *   env->scanner.startnum    - line of token start
 *   env->scanner.startpos    - position of token start
 *   env->scanner.endpos      - position of token end
 */
static int stderr_printf_count(const char *fmt, ...);
#ifdef ALLOW_SYSTEM_CALLS
static int command_is_safe(const char *cmd);
#endif

/*
 * getch reads the next character from srcfile.
 */
int getch(pEnv env)
{
    int ch;

again:
    if (vec_size(env->pushback))
        return vec_pop(env->pushback);
    if ((ch = joy_getc(env, env->scanner.srcfile)) == EOF) {
        if (!env->scanner.ilevel)
            abortexecution_(env, ABORT_QUIT);
        fclose(env->scanner.srcfile);
        env->scanner.srcfile = env->scanner.infile[--env->scanner.ilevel].fp;
        env->scanner.linenum = env->scanner.infile[env->scanner.ilevel].line;
        env->scanner.srcfilename = env->scanner.infile[env->scanner.ilevel].name;
        if (env->finclude_busy)
            longjmp(env->finclude, 1); /* back to finclude */
        goto again;
    }
    if (!env->scanner.linepos && ch == SHELLESCAPE) {
        /* Check if this is string interpolation $"..." rather than shell escape */
        int next = joy_getc(env, env->scanner.srcfile);
        if (next == '"') {
            /* It's string interpolation, push back the " and return $ */
            vec_push(env->pushback, next);
            env->scanner.linebuf[env->scanner.linepos++] = ch;
            env->scanner.linebuf[env->scanner.linepos] = 0;
            return ch;
        }
        /* It's a shell escape, continue with shell command processing */
        vec_push(env->string, next);
        while ((ch = joy_getc(env, env->scanner.srcfile)) != '\n' && ch != EOF)
            vec_push(env->string, ch);
        vec_push(env->string, 0);
#ifdef ALLOW_SYSTEM_CALLS
        if (!env->ignore) {
            char *command = &vec_at(env->string, 0);
            if (command_is_safe(command)) {
#ifndef __clang_analyzer__
                (void)system(command);
#else
                (void)command;
#endif
            } else {
                stderr_printf_count("warning: rejected unsafe shell command\n");
            }
        }
#endif
        vec_setsize(env->string, 0);
        goto again;
    }
    if (ch == '\n') {
        if (env->config.echoflag > 2)
            joy_printf(env, "%4d", env->scanner.linenum);
        if (env->config.echoflag > 1)
            joy_putchar(env, '\t');
        if (env->config.echoflag)
            joy_printf(env, "%.*s\n", env->scanner.linepos, env->scanner.linebuf); /* echo line */
        env->scanner.linenum++;
        env->scanner.linepos = 0;
    } else if (env->scanner.linepos < INPLINEMAX)
        env->scanner.linebuf[env->scanner.linepos++] = ch;
    env->scanner.linebuf[env->scanner.linepos] = 0;
    return ch;
}

/*
 * ungetch unreads a character.
 */
void ungetch(pEnv env, int ch)
{
    if (ch == '\n')
        env->scanner.linenum--; /* about to unread newline */
    ungetc(ch, env->scanner.srcfile);
    if (env->scanner.linepos > 0)
        env->scanner.linepos--; /* read too far, push back */
}

/*
 * error prints a message in case of an error.
 */
static int stderr_printf_count(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    va_list ap_copy;
    va_copy(ap_copy, ap);
    int needed = vsnprintf(NULL, 0, fmt, ap_copy);
    va_end(ap_copy);
    if (needed < 0) {
        va_end(ap);
        return 0;
    }
    size_t bufsize = (size_t)needed + 1;
    char *buffer = malloc(bufsize);
    if (!buffer) {
        va_end(ap);
        return 0;
    }
    vsnprintf(buffer, bufsize, fmt, ap);
    va_end(ap);
    fwrite(buffer, 1, bufsize - 1, stderr);
    free(buffer);
    return needed;
}

void error(pEnv env, char* str)
{
    int leng;

    fflush(stdout);
    leng = stderr_printf_count("%s:%d:", env->scanner.srcfilename, env->scanner.linenum);
    leng += stderr_printf_count("%.*s", env->scanner.linepos, env->scanner.linebuf);
    if (leng > 0) {
        stderr_printf_count("\n%*s^", --leng, "");
        stderr_printf_count("\n%*s%s\n", leng, "", str);
    } else {
        stderr_printf_count("\n^\n%s\n", str);
    }
}

/*
 * redirect - register another file descriptor to read from.
 */
static void redirect(pEnv env, char* str, int j, FILE* fp)
{
    int i;
    char *new_path, *old_path;

    /*
     * If a filename is given with a pathname, then that pathname will be used
     * for additional attempts.
     */
    if (strrchr(str, '/')) {
        new_path = GC_CTX_STRDUP(env, str);
        str = strrchr(new_path, '/');
        *str++ = 0;
        for (i = 0; i < j; i++) {
            old_path = vec_at(env->pathnames, i);
            if (!strcmp(new_path, old_path))
                break;
        }
        if (i == j)
            vec_push(env->pathnames, new_path);
    }
    if (env->scanner.ilevel >= 0)
        env->scanner.infile[env->scanner.ilevel].line = env->scanner.linenum; /* save last line number */
    if (env->scanner.ilevel + 1 == INPSTACKMAX)     /* increase include level */
        execerror(env, "fewer include files", "include");
    env->scanner.infile[++env->scanner.ilevel].fp = env->scanner.srcfile = fp; /* use new file pointer */
    env->scanner.infile[env->scanner.ilevel].line = env->scanner.linenum = 1;  /* start with line 1 */
    strncpy(env->scanner.srcfilename = env->scanner.infile[env->scanner.ilevel].name, str, FILENAMEMAX);
}

/*
 * inilinebuffer - initialise the stack of input files. The filename parameter
 *		   is used in error messages.
 */
void inilinebuffer(pEnv env) { redirect(env, "stdin", 0, stdin); }

/*
 * include - insert the contents of a file in the input.
 *
 * Files are read in the current directory or if that fails from a previous
 * location. Generating an error is left to the call site.
 *
 * Return code is 1 if the file could not be opened for reading.
 */
int include(pEnv env, char* name)
{
    int i, j;
    FILE* fp;
    size_t leng;
    char *path, *str = name; /* str = path/name */

    /*
     * The home directory is added to the list of directories to be searched.
     */
    if (!env->homedir) {               /* should be present */
        env->homedir = getenv("HOME"); /* unix/cygwin */
#ifdef WINDOWS
        if (!env->homedir)
            env->homedir = getenv("HOMEPATH"); /* windows */
#endif
        if (env->homedir)
            vec_push(env->pathnames, env->homedir);
    }
    /*
     * The current directory is tried first.
     * Then all saved directories are tried until there is one that succeeds.
     */
    for (j = i = vec_size(env->pathnames); i >= 0; i--) {
        if (i != vec_size(env->pathnames)) {
            path = vec_at(env->pathnames, i);
            leng = strlen(path) + strlen(name) + 2;
            str = GC_CTX_MALLOC_ATOMIC(env, leng);
            snprintf(str, leng, "%s/%s", path, name);
        }
        if ((fp = fopen(str, "r")) != 0) { /* try to read */
            redirect(env, str, j, fp);     /* stop trying */
            return 0;
        }
    }
    return 1; /* file cannot be opened for reading */
}

/*
 * special reads a character escape sequence.
 */
static int special(pEnv env)
{
    int ch, i, my_num;

    ch = getch(env);
    switch (ch) {
    case 'b':
        return '\b';
    case 't':
        return '\t';
    case 'n':
        return '\n';
    case 'v':
        return '\v';
    case 'f':
        return '\f';
    case 'r':
        return '\r';
    case '\"':
        return '\"';
    case '\'':
        return '\'';
    case '\\':
        return '\\';
    default:
        if (isdigit(ch)) {
            my_num = ch - '0';
            for (i = 0; i < 2; i++) {
                ch = getch(env);
                if (!isdigit(ch)) {
                    error(env, "digit expected");
                    ungetch(env, ch); /* not a digit, push back */
                    break;
                }
                my_num = 10 * my_num + ch - '0';
            }
            return my_num;
        }
        return ch;
    }
}

/*
 * parse_interpolated_string - parse $"..." string interpolation.
 * Converts $"Hello ${name}!" into: "Hello " name unquoted concat "!" concat
 * Returns the next character after the closing quote.
 */
static int parse_interpolated_string(pEnv env)
{
    int ch, brace_depth;
    Token tok;
    int part_count = 0;

    /* We use a separate vector for building the interpolation tokens */
    vector(Token)* interp_tokens;
    vec_init(interp_tokens);

    /* We're positioned right after $" */
    ch = getch(env);

    vec_setsize(env->string, 0);

    while (ch != '"' && ch != EOF) {
        if (ch == '$') {
            ch = getch(env);
            if (ch == '{') {
                /* Found ${...} - emit current string literal if any */
                if (vec_size(env->string) > 0) {
                    vec_push(env->string, 0);
                    tok.op = STRING_;
                    tok.u.str = GC_CTX_STRDUP(env, &vec_at(env->string, 0));
                    tok.y = env->scanner.startnum;
                    tok.x = env->scanner.startpos;
                    tok.pos = env->scanner.endpos;
                    vec_push(interp_tokens, tok);
                    /* Add concat if we have previous parts */
                    if (part_count > 0) {
                        tok.op = USR_;
                        tok.u.str = GC_CTX_STRDUP(env, "concat");
                        vec_push(interp_tokens, tok);
                    }
                    part_count++;
                    vec_setsize(env->string, 0);
                }

                /* Parse the expression inside ${...} */
                brace_depth = 1;
                ch = getch(env);
                while (brace_depth > 0 && ch != EOF) {
                    if (ch == '{') {
                        brace_depth++;
                        vec_push(env->string, ch);
                    } else if (ch == '}') {
                        brace_depth--;
                        if (brace_depth > 0)
                            vec_push(env->string, ch);
                    } else if (ch == '"') {
                        /* String inside expression */
                        vec_push(env->string, ch);
                        ch = getch(env);
                        while (ch != '"' && ch != EOF) {
                            if (ch == '\\') {
                                vec_push(env->string, ch);
                                ch = getch(env);
                            }
                            vec_push(env->string, ch);
                            ch = getch(env);
                        }
                        vec_push(env->string, ch);
                    } else {
                        vec_push(env->string, ch);
                    }
                    ch = getch(env);
                }

                /* We have the expression in env->string */
                vec_push(env->string, 0);

                /* Parse the expression text into tokens */
                {
                    char* expr = GC_CTX_STRDUP(env, &vec_at(env->string, 0));
                    char* p = expr;
                    /* Push each whitespace-separated token */
                    while (*p) {
                        while (*p && (*p == ' ' || *p == '\t' || *p == '\n'))
                            p++;
                        if (!*p) break;
                        char* start = p;
                        if (*p == '"') {
                            /* String literal */
                            p++;
                            while (*p && *p != '"') {
                                if (*p == '\\' && p[1]) p++;
                                if (*p) p++;
                            }
                            if (*p == '"') p++;
                        } else {
                            while (*p && *p != ' ' && *p != '\t' && *p != '\n')
                                p++;
                        }
                        size_t len = (size_t)(p - start);
                        char* token_str = GC_CTX_MALLOC_ATOMIC(env, len + 1);
                        memcpy(token_str, start, len);
                        token_str[len] = '\0';

                        /* Parse the token */
                        char* end;
                        if (token_str[0] == '"') {
                            /* String literal */
                            tok.op = STRING_;
                            size_t slen = strlen(token_str);
                            if (slen >= 2) {
                                token_str[slen-1] = '\0';
                                tok.u.str = GC_CTX_STRDUP(env, token_str + 1);
                            } else {
                                tok.u.str = GC_CTX_STRDUP(env, "");
                            }
                        } else if ((token_str[0] >= '0' && token_str[0] <= '9') ||
                                   (token_str[0] == '-' && token_str[1] >= '0' && token_str[1] <= '9')) {
                            if (strchr(token_str, '.') || strchr(token_str, 'e') || strchr(token_str, 'E')) {
                                tok.op = FLOAT_;
                                tok.u.dbl = strtod(token_str, &end);
                            } else {
                                tok.op = INTEGER_;
                                tok.u.num = strtoll(token_str, &end, 10);
                            }
                        } else {
                            tok.op = USR_;
                            tok.u.str = token_str;
                        }
                        tok.y = env->scanner.startnum;
                        tok.x = env->scanner.startpos;
                        tok.pos = env->scanner.endpos;
                        vec_push(interp_tokens, tok);
                    }
                }

                /* Add unquoted to convert to string */
                tok.op = USR_;
                tok.u.str = GC_CTX_STRDUP(env, "unquoted");
                tok.y = env->scanner.startnum;
                tok.x = env->scanner.startpos;
                tok.pos = env->scanner.endpos;
                vec_push(interp_tokens, tok);

                /* If we have previous parts, add concat */
                if (part_count > 0) {
                    tok.op = USR_;
                    tok.u.str = GC_CTX_STRDUP(env, "concat");
                    vec_push(interp_tokens, tok);
                }
                part_count++;
                vec_setsize(env->string, 0);
            } else {
                /* Not ${, just a literal $ */
                vec_push(env->string, '$');
                if (ch != '"') {
                    vec_push(env->string, ch);
                    ch = getch(env);
                }
                /* Don't call getch again, let outer loop handle ch */
            }
        } else if (ch == '\\') {
            ch = special(env);
            vec_push(env->string, ch);
            ch = getch(env);
        } else {
            vec_push(env->string, ch);
            ch = getch(env);
        }
    }

    /* Emit final string literal if any */
    if (vec_size(env->string) > 0) {
        vec_push(env->string, 0);
        tok.op = STRING_;
        tok.u.str = GC_CTX_STRDUP(env, &vec_at(env->string, 0));
        tok.y = env->scanner.startnum;
        tok.x = env->scanner.startpos;
        tok.pos = env->scanner.endpos;
        vec_push(interp_tokens, tok);
        if (part_count > 0) {
            tok.op = USR_;
            tok.u.str = GC_CTX_STRDUP(env, "concat");
            vec_push(interp_tokens, tok);
        }
        part_count++;
    }

    /* If empty string, just push empty string */
    if (part_count == 0) {
        tok.op = STRING_;
        tok.u.str = GC_CTX_STRDUP(env, "");
        tok.y = env->scanner.startnum;
        tok.x = env->scanner.startpos;
        tok.pos = env->scanner.endpos;
        vec_push(interp_tokens, tok);
    }

    /* Now push all tokens onto env->tokens in reverse order */
    /* (because env->tokens is processed as a stack) */
    for (int i = vec_size(interp_tokens) - 1; i >= 0; i--) {
        vec_push(env->tokens, vec_at(interp_tokens, i));
    }

    /* Don't pop the first token here - let getsym's final token check
     * handle it. We just need to return with a valid ch value. */
    return getch(env); /* read past closing " */
}

/*
 * getsym reads the next symbol from code or from srcfile. The return value is
 * the character after the symbol that was read.
 */
static int my_getsym(pEnv env, int ch)
{
    static char *exclude = "\"#'().;[]{}", *include = "-=_";
    char* ptr;
    int i, sign, type;

    vec_setsize(env->string, 0);
start:
    while (ch <= ' ')
        ch = getch(env);
    env->scanner.startnum = env->scanner.linenum; /* line of a token */
    env->scanner.startpos = env->scanner.linepos; /* start position of a token */
    switch (ch) {
    case '(':
        ch = getch(env);
        if (ch == '*') {
            ch = getch(env);
            do {
                while (ch != '*')
                    ch = getch(env);
                ch = getch(env);
            } while (ch != ')');
            ch = getch(env);
            goto start;
        }
        env->scanner.sym = '(';
        return ch;

    case '#':
        do
            ch = getch(env);
        while (ch != '\n');
        goto start;

    case ')':
    case '[':
    case ']':
    case '{':
    case '}':
    case '.':
    case ';':
        env->scanner.sym = ch;
        env->scanner.endpos = env->scanner.linepos;
        return getch(env); /* read past ch */

    case '\'':
        ch = getch(env);
        if (ch == '\\')
            ch = special(env);
        env->num = ch;
        env->scanner.sym = CHAR_;
        env->scanner.endpos = env->scanner.linepos;
        return getch(env); /* read past ch */

    case '"':
        ch = getch(env);
        while (ch != '"') {
            if (ch == '\\')
                ch = special(env);
            vec_push(env->string, ch);
            ch = getch(env);
        }
        vec_push(env->string, 0);
        env->str = GC_CTX_STRDUP(env, &vec_at(env->string, 0));
        env->scanner.sym = STRING_;
        env->scanner.endpos = env->scanner.linepos;
        return getch(env); /* read past " */

    case '$':
        ch = getch(env);
        if (ch == '"') {
            /* String interpolation: $"Hello ${expr}!" */
            return parse_interpolated_string(env);
        }
        /* Not interpolation - check if $ is standalone or part of identifier */
        if (ch <= ' ' || strchr(exclude, ch)) {
            /* $ followed by whitespace or delimiter - standalone symbol */
            ungetch(env, ch);
            env->str = GC_CTX_STRDUP(env, "$");
            env->scanner.sym = USR_;
            return getch(env);
        }
        /* $ followed by identifier char - treat as start of identifier */
        vec_push(env->string, '$');
        goto identifier;

    default:
    identifier:
        vec_push(env->string, ch);
        sign = ch; /* possible sign */
        ch = getch(env);
        type = isdigit(sign) || (sign == '-' && isdigit(ch)); /* numeric */
        while (ch > ' ' && !strchr(exclude, ch)) {
            vec_push(env->string, ch);
            ch = getch(env);
        }
        if (ch == '.') {
            ch = getch(env);
            if (type) {
                if (!isdigit(ch)) { /* test float */
                    ungetch(env, ch);    /* read too far, push back */
                    ch = '.';       /* restore full stop */
                    goto einde;
                }
                type = 2; /* floating point */
            } else if (!isalnum(ch) && !strchr(include, ch)) { /* member */
                ungetch(env, ch); /* read too far, push back */
                ch = '.';    /* restore full stop */
                goto einde;
            }
            vec_push(env->string, '.');
            while (ch > ' ' && !strchr(exclude, ch)) {
                vec_push(env->string, ch);
                ch = getch(env);
            }
        }
    einde:
        vec_push(env->string, 0);
        ptr = &vec_at(env->string, 0);
        env->scanner.endpos = env->scanner.startpos + strlen(ptr) - 1;
        if (type) {
            if (type == 2) {
                env->dbl = strtod(&vec_at(env->string, 0), &ptr);
                env->scanner.sym = FLOAT_;
            } else {
                if (sign == '-')
                    env->num = strtoll(&vec_at(env->string, 1), &ptr, 0);
                else
                    env->num = strtoll(&vec_at(env->string, 0), &ptr, 0);
                if (env->num == MAXINT_) {
                    env->dbl = strtod(&vec_at(env->string, 0), &ptr);
                    env->scanner.sym = FLOAT_;
                } else {
                    if (sign == '-')
                        env->num = -env->num;
                    env->scanner.sym = INTEGER_;
                }
            }
            if (*ptr) {
                env->scanner.endpos -= strlen(ptr);
                vec_push(env->pushback, ch);
                for (i = strlen(ptr) - 1; i >= 0; i--)
                    vec_push(env->pushback, ptr[i]);
                return getch(env);
            }
            return ch;
        }
        type = ptr[1];
        if (isupper(type) || type == '=') {
            type = sizeof(keywords) / sizeof(keywords[0]);
            for (sign = 0; sign < type; sign++)
                if (!strcmp(ptr, keywords[sign].name)) {
                    env->scanner.sym = keywords[sign].sym;
                    return ch;
                }
        }
#ifdef JOY_NATIVE_TYPES
        /* Check for vector literal: v[ */
        if (strcmp(ptr, "v") == 0 && ch == '[') {
            env->scanner.sym = VBRACKET;
            return getch(env);  /* consume [ */
        }
        /* Check for matrix literal: m[[ */
        if (strcmp(ptr, "m") == 0 && ch == '[') {
            int next = getch(env);
            if (next == '[') {
                env->scanner.sym = MBRACKET;
                return getch(env);  /* consume second [ */
            }
            ungetch(env, next);  /* not m[[, push back */
        }
#endif
        env->str = GC_CTX_STRDUP(env, ptr);
        env->scanner.sym = USR_;
    }
    return ch;
}

static void dumptok(pEnv env, int y, int x, int pos)
{
    joy_printf(env, "(%d,%d:%d) ", y, x, pos);
    switch (env->scanner.sym) {
    case USR_:
        joy_printf(env, "%s", env->str);
        break;
    case CHAR_:
        joy_printf(env, "%d", (int)env->num);
        break;
    case INTEGER_:
        joy_printf(env, "%" PRId64, env->num);
        break;
    case STRING_:
        joy_printf(env, "\"%s\"", env->str);
        break;
    case FLOAT_:
        joy_printf(env, "%g", env->dbl);
        break;
    case '[':
        joy_puts(env, "LBRACK");
        break;
    case ']':
        joy_puts(env, "RBRACK");
        break;
    case '{':
        joy_puts(env, "LBRACE");
        break;
    case '}':
        joy_puts(env, "RBRACE");
        break;
    case '(':
        joy_puts(env, "LPAREN");
        break;
    case ')':
        joy_puts(env, "RPAREN");
        break;
    case '.':
        joy_puts(env, "PERIOD");
        break;
    case ';':
        joy_puts(env, "SEMICOL");
        break;
    case LIBRA:
        joy_puts(env, "LIBRA");
        break;
    case EQDEF:
        joy_puts(env, "EQDEF");
        break;
    case HIDE:
        joy_puts(env, "HIDE");
        break;
    case IN__:
        joy_puts(env, "IN");
        break;
    case MODULE_:
        joy_puts(env, "MODULE");
        break;
    case PRIVATE:
        joy_puts(env, "PRIVATE");
        break;
    case PUBLIC:
        joy_puts(env, "PUBLIC");
        break;
    case CONST_:
        joy_puts(env, "CONST");
        break;
    }
    joy_puts(env, "\n");
}

/*
 * push_sym - push a symbol into the tokenlist.
 */
static void push_sym(pEnv env)
{
    Token node;

    switch (node.op = env->scanner.sym) {
    case CHAR_:
    case INTEGER_:
        node.u.num = env->num;
        break;
    case FLOAT_:
        node.u.dbl = env->dbl;
        break;
    case USR_:
    case STRING_:
        node.u.str = env->str;
        break;
    }
    node.y = env->scanner.startnum;
    node.x = env->scanner.startpos;
    node.pos = env->scanner.endpos;
    vec_push(env->tokens, node);
}

/*
 * getsym - wrapper around my_getsym, storing tokens read, reading from the
 *	    store or just calling my_getsym itself. This allows tokens to be
 *	    read twice.
 *
 * After reading MODULE or PRIVATE, read all tokens upto END, and include them
 * in the tokenlist. All symbols preceding "==" are declared.
 */
int getsym(pEnv env, int ch)
{
    Token node;
    int module = 0, private = 0, hide = 0, modl = 0, hcnt = 0;

    /*
     * If there is a tokenlist, extract tokens from there.
     */
    if (vec_size(env->tokens)) {
    begin:
        node = vec_pop(env->tokens);
        switch (env->scanner.sym = node.op) {
        case CHAR_:
        case INTEGER_:
            env->num = node.u.num;
            break;
        case FLOAT_:
            env->dbl = node.u.dbl;
            break;
        case USR_:
        case STRING_:
            env->str = node.u.str;
            break;
        }
        if (env->printing)
            dumptok(env, node.y, node.x, node.pos); /* tokens from tokenlist */
        return ch;
    }
    /*
     * There is no tokenlist, use the normal procedure to get one.
     */
    ch = my_getsym(env, ch);
    /*
     * There is a token available, do some extra processing, in case the token
     * is MODULE or HIDE: MODULE .. END or HIDE .. END.
     */
    if (env->scanner.sym == MODULE_ || env->scanner.sym == HIDE) {
        /*
         * Copy the global variables of modl.c into local variables.
         */
        node.op = env->scanner.sym;
        savemod(&hide, &modl, &hcnt);
        do {
            switch (env->scanner.sym) {
            case MODULE_:
                push_sym(env);
                ch = my_getsym(env, ch);
                if (env->scanner.sym == USR_) {
                    initmod(env, env->str);
                    module++;
                } else
                    error(env, "atom expected as name of module");
                break;
            case HIDE:
            case PRIVATE:
                initpriv(env);
                if (!module)
                    private++;
                break;
            case IN__:
            case PUBLIC:
                stoppriv();
                break;
            case EQDEF:
                if (node.op == USR_) {
                    if (!strchr(env->str, '.'))
                        env->str = classify(env, env->str);
                    enteratom(env, env->str);
                }
                break;
            case '.':
                if (module) {
                    exitmod();
                    module--;
                } else if (private) {
                    exitpriv();
                    private--;
                }
                if (!module && !private)
                    goto einde;
                break;
            }
            node.op = env->scanner.sym; /* previous symbol */
            push_sym(env);
            ch = my_getsym(env, ch);
        } while (1);
    /*
     * Restore the global variables in module.c from the local copies.
     */
    einde:
        undomod(hide, modl, hcnt);
        push_sym(env); /* store the last symbol that was read */
        push_sym(env); /* extra sym for the benefit of reverse */
        vec_reverse(env->tokens);
    }
    /*
     * If there is a tokenlist, extract tokens from there.
     */
    if (vec_size(env->tokens))
        goto begin;
    if (env->printing)
        dumptok(env, env->scanner.startnum, env->scanner.startpos, env->scanner.endpos); /* tokens read directly */
    return ch;
}
#ifdef ALLOW_SYSTEM_CALLS
static int command_is_safe(const char *cmd)
{
    while (*cmd) {
        if (!(isalnum((unsigned char)*cmd) || strchr(" ._/-", *cmd)))
            return 0;
        cmd++;
    }
    return 1;
}
#endif
