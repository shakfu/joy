/*
 *  module  : document.h
 *  version : 1.0
 *  date    : 02/11/26
 *
 *  Per-document state for the LSP server.
 */
#ifndef LSP_DOCUMENT_H
#define LSP_DOCUMENT_H

#include <stddef.h>
#include <stdint.h>
#include <tree_sitter/api.h>

/* A definition found in the document (e.g., "foo == ...") */
typedef struct {
    char *name;
    uint32_t name_start_byte;
    uint32_t name_end_byte;
    uint32_t start_row;
    uint32_t start_col;
    uint32_t end_row;
    uint32_t end_col;
    char *body_text; /* text of the definition body, for hover */
} DocDefinition;

typedef struct {
    char *uri;
    char *text;
    size_t text_len;
    TSTree *tree;
    DocDefinition *defs;
    size_t defs_len;
    size_t defs_cap;
} Document;

/* Initialize the document store (call once at startup) */
void doc_store_init(TSParser *parser);

/* Clean up the document store */
void doc_store_free(void);

/* Open/update/close documents */
void doc_open(const char *uri, const char *text, size_t text_len);
void doc_update(const char *uri, const char *text, size_t text_len);
void doc_close(const char *uri);

/* Look up a document by URI. Returns NULL if not found. */
Document *doc_get(const char *uri);

/* Get the symbol text at a given position (row, col).
 * Returns a malloc'd string, or NULL. */
char *doc_symbol_at(Document *doc, uint32_t row, uint32_t col);

/* Get the word prefix ending at a given position for completion.
 * Returns a malloc'd string, or NULL. */
char *doc_word_at(Document *doc, uint32_t row, uint32_t col);

#endif /* LSP_DOCUMENT_H */
