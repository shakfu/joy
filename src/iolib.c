/*
 *  module  : iolib.c
 *  version : 1.0
 *  date    : 01/20/26
 *
 *  I/O abstraction layer for embedding support.
 *  Functions check for callbacks and fall back to stdio if not set.
 */
#include "globals.h"
#include <stdarg.h>

/*
 * joy_getchar - read a single character from input.
 * Uses callback if set, otherwise reads from stdin.
 */
int joy_getchar(pEnv env)
{
    if (env->io.read_char) {
        return env->io.read_char(env->io.user_data);
    } else {
        return getchar();
    }
}

/*
 * joy_getc - read a single character from a file.
 * Uses callback if fp is stdin and callback is set, otherwise uses fgetc.
 */
int joy_getc(pEnv env, FILE* fp)
{
    if (fp == stdin && env->io.read_char) {
        return env->io.read_char(env->io.user_data);
    } else {
        return fgetc(fp);
    }
}

/*
 * joy_putchar - write a single character to output.
 * Uses callback if set, otherwise writes to stdout.
 */
void joy_putchar(pEnv env, int ch)
{
    if (env->io.write_char) {
        env->io.write_char(env->io.user_data, ch);
    } else {
        putchar(ch);
    }
}

/*
 * joy_putc - write a single character to a file.
 * Uses callback if fp is stdout and callback is set, otherwise uses putc.
 */
void joy_putc(pEnv env, int ch, FILE* fp)
{
    if (fp == stdout && env->io.write_char) {
        env->io.write_char(env->io.user_data, ch);
    } else {
        putc(ch, fp);
    }
}

/*
 * joy_fputs - write a string to a file.
 * Uses callback if fp is stdout and callback is set, otherwise uses fputs.
 */
void joy_fputs(pEnv env, const char* s, FILE* fp)
{
    if (fp == stdout && env->io.write_string) {
        env->io.write_string(env->io.user_data, s);
    } else {
        fputs(s, fp);
    }
}

/*
 * joy_fwrite - write data to a file.
 * Uses callback if fp is stdout and callback is set, otherwise uses fwrite.
 */
void joy_fwrite(pEnv env, const char* s, size_t len, FILE* fp)
{
    if (fp == stdout && env->io.write_string) {
        /* For callback, we need a null-terminated string */
        char* buf = malloc(len + 1);
        if (buf) {
            memcpy(buf, s, len);
            buf[len] = '\0';
            env->io.write_string(env->io.user_data, buf);
            free(buf);
        }
    } else {
        fwrite(s, 1, len, fp);
    }
}

/*
 * joy_fprintf - formatted output to a file.
 * Uses callback if fp is stdout and callback is set, otherwise uses fprintf.
 */
void joy_fprintf(pEnv env, FILE* fp, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    /* First, determine the size needed */
    va_list ap_copy;
    va_copy(ap_copy, ap);
    int needed = vsnprintf(NULL, 0, fmt, ap_copy);
    va_end(ap_copy);

    if (needed < 0) {
        va_end(ap);
        return;
    }

    /* Allocate buffer and format the string */
    size_t size = (size_t)needed + 1;
    char* buffer = malloc(size);
    if (!buffer) {
        va_end(ap);
        return;
    }

    vsnprintf(buffer, size, fmt, ap);
    va_end(ap);

    /* Output using callback or stdio */
    if (fp == stdout && env->io.write_string) {
        env->io.write_string(env->io.user_data, buffer);
    } else {
        fwrite(buffer, 1, size - 1, fp);
    }

    free(buffer);
}

/*
 * joy_puts - write a string to output (without trailing newline).
 * Uses callback if set, otherwise writes to stdout.
 */
void joy_puts(pEnv env, const char* s)
{
    if (env->io.write_string) {
        env->io.write_string(env->io.user_data, s);
    } else {
        fputs(s, stdout);
    }
}

/*
 * joy_printf - formatted output.
 * Uses callback if set, otherwise writes to stdout.
 */
void joy_printf(pEnv env, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    /* First, determine the size needed */
    va_list ap_copy;
    va_copy(ap_copy, ap);
    int needed = vsnprintf(NULL, 0, fmt, ap_copy);
    va_end(ap_copy);

    if (needed < 0) {
        va_end(ap);
        return;
    }

    /* Allocate buffer and format the string */
    size_t size = (size_t)needed + 1;
    char* buffer = malloc(size);
    if (!buffer) {
        va_end(ap);
        return;
    }

    vsnprintf(buffer, size, fmt, ap);
    va_end(ap);

    /* Output using callback or stdio */
    if (env->io.write_string) {
        env->io.write_string(env->io.user_data, buffer);
    } else {
        fputs(buffer, stdout);
    }

    free(buffer);
}

/*
 * joy_flush - flush output.
 * Only affects stdio; callbacks handle their own buffering.
 */
void joy_flush(pEnv env)
{
    if (!env->io.write_char && !env->io.write_string) {
        fflush(stdout);
    }
    /* Note: If using callbacks, assume the callback handles buffering */
}

/*
 * joy_report_error - report an error via callback or stderr.
 * This is used for error messages, not for fatal errors that abort.
 */
void joy_report_error(pEnv env, int code, const char* msg)
{
    if (env->io.on_error) {
        env->io.on_error(env->io.user_data, code, msg,
                         env->srcfilename, env->linenum, env->linepos);
    } else {
        /* Default: write to stderr */
        fflush(stdout);
        fprintf(stderr, "%s", msg);
    }

    /* Store error info in env for later retrieval */
    strncpy(env->error_message, msg, sizeof(env->error_message) - 1);
    env->error_message[sizeof(env->error_message) - 1] = '\0';
    env->error_line = env->linenum;
    env->error_column = env->linepos;
}
