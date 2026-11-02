/*
 *  module  : transport.c
 *  version : 1.0
 *  date    : 02/11/26
 *
 *  JSON-RPC transport layer for LSP (Content-Length framing).
 *  Reads from stdin, writes to stdout. Debug output goes to stderr.
 */
#include "transport.h"
#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_HEADER_LINE 65536

/* Reads one header line without its terminator. Returns NULL on EOF;
 * a line longer than MAX_HEADER_LINE is truncated. */
static char *read_header_line(char *buf, size_t cap) {
    size_t n = 0;
    int ch;

    while ((ch = getchar()) != EOF && ch != '\n')
        if (n + 1 < cap)
            buf[n++] = (char)ch;
    if (ch == EOF && n == 0)
        return NULL;
    if (n > 0 && buf[n - 1] == '\r')
        n--;
    buf[n] = '\0';
    return buf;
}

/* Finds "content-length:" in line, ignoring case (header names are
 * case-insensitive). Searching the whole line resynchronises after a
 * body that was not consumed. */
static const char *find_content_length(const char *line) {
    static const char key[] = "content-length:";
    size_t k = sizeof(key) - 1;

    for (; *line; line++) {
        size_t i = 0;
        while (i < k && line[i] && tolower((unsigned char)line[i]) == key[i])
            i++;
        if (i == k)
            return line + k;
    }
    return NULL;
}

/* Parses a decimal length with optional surrounding whitespace */
static int parse_length(const char *s, size_t *out) {
    char *end;
    unsigned long long v;

    while (*s == ' ' || *s == '\t')
        s++;
    if (!isdigit((unsigned char)*s))
        return 0;
    errno = 0;
    v = strtoull(s, &end, 10);
    while (*end == ' ' || *end == '\t')
        end++;
    if (*end || errno == ERANGE || v > SIZE_MAX - 1)
        return 0;
    *out = (size_t)v;
    return 1;
}

char *transport_read(size_t *out_len) {
    static char line[MAX_HEADER_LINE];

    for (;;) {
        size_t content_length = 0;
        int found_length = 0;
        const char *value;

        /* Read headers until empty line */
        for (;;) {
            if (!read_header_line(line, sizeof(line)))
                return NULL; /* EOF */
            if (line[0] == '\0')
                break;
            if ((value = find_content_length(line)) != NULL)
                found_length = parse_length(value, &content_length);
        }

        /* A bad header loses one message, not the session */
        if (!found_length) {
            fprintf(stderr, "joy-lsp: missing or invalid Content-Length\n");
            continue;
        }
        if (content_length == 0)
            continue;

        char *body = malloc(content_length + 1);
        size_t total = 0;
        while (total < content_length) {
            char discard[4096];
            size_t want = content_length - total;
            size_t n;
            if (body)
                n = fread(body + total, 1, want, stdin);
            else
                n = fread(discard, 1, want < sizeof(discard) ? want : sizeof(discard), stdin);
            if (n == 0) {
                free(body);
                return NULL; /* EOF during body */
            }
            total += n;
        }
        if (!body) {
            fprintf(stderr, "joy-lsp: no memory for %zu-byte message\n", content_length);
            continue;
        }
        body[content_length] = '\0';
        *out_len = content_length;
        return body;
    }
}

void transport_write(const char *body, size_t body_len) {
    fprintf(stdout, "Content-Length: %zu\r\n\r\n", body_len);
    fwrite(body, 1, body_len, stdout);
    fflush(stdout);
}
