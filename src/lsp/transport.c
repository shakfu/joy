/*
 *  module  : transport.c
 *  version : 1.0
 *  date    : 02/11/26
 *
 *  JSON-RPC transport layer for LSP (Content-Length framing).
 *  Reads from stdin, writes to stdout. Debug output goes to stderr.
 */
#include "transport.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *transport_read(size_t *out_len) {
    char line[256];
    size_t content_length = 0;
    int found_length = 0;

    /* Read headers until empty line */
    for (;;) {
        if (!fgets(line, sizeof(line), stdin))
            return NULL; /* EOF */

        /* Empty line (just \r\n or \n) marks end of headers */
        if (strcmp(line, "\r\n") == 0 || strcmp(line, "\n") == 0)
            break;

        if (strncmp(line, "Content-Length:", 15) == 0) {
            content_length = (size_t)atol(line + 15);
            found_length = 1;
        }
    }

    if (!found_length || content_length == 0)
        return NULL;

    char *body = malloc(content_length + 1);
    if (!body)
        return NULL;

    size_t total = 0;
    while (total < content_length) {
        size_t n = fread(body + total, 1, content_length - total, stdin);
        if (n == 0) {
            free(body);
            return NULL; /* EOF during body */
        }
        total += n;
    }
    body[content_length] = '\0';
    *out_len = content_length;
    return body;
}

void transport_write(const char *body, size_t body_len) {
    fprintf(stdout, "Content-Length: %zu\r\n\r\n", body_len);
    fwrite(body, 1, body_len, stdout);
    fflush(stdout);
}
