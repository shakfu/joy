/*
 *  module  : document.c
 *  version : 1.0
 *  date    : 02/11/26
 *
 *  Per-document state for the LSP server.
 *  Uses khash for URI -> Document mapping.
 */
#include "document.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "khash.h"

/* Declare khash type for string -> Document* */
KHASH_MAP_INIT_STR(DocMap, Document *)

static khash_t(DocMap) *doc_map;
static TSParser *ts_parser;
static TSQuery *def_query;
static TSQueryCursor *def_cursor;

/* Tree-sitter query for definitions:
 * (definition name: (symbol) @name) @def */
static const char *DEF_QUERY_STR =
    "(definition name: (symbol) @name) @def";

void doc_store_init(TSParser *parser) {
    uint32_t error_offset;
    TSQueryError error_type;

    doc_map = kh_init(DocMap);
    ts_parser = parser;

    def_query = ts_query_new(
        ts_parser_language(parser),
        DEF_QUERY_STR,
        (uint32_t)strlen(DEF_QUERY_STR),
        &error_offset,
        &error_type
    );
    if (!def_query) {
        fprintf(stderr, "joy-lsp: failed to compile definition query at offset %u\n",
                error_offset);
    }
    def_cursor = ts_query_cursor_new();
}

static void doc_free_defs(Document *doc) {
    for (size_t i = 0; i < doc->defs_len; i++) {
        free(doc->defs[i].name);
        free(doc->defs[i].body_text);
    }
    free(doc->defs);
    doc->defs = NULL;
    doc->defs_len = 0;
    doc->defs_cap = 0;
}

static void doc_destroy(Document *doc) {
    if (!doc) return;
    free(doc->uri);
    free(doc->text);
    if (doc->tree) ts_tree_delete(doc->tree);
    doc_free_defs(doc);
    free(doc);
}

void doc_store_free(void) {
    if (!doc_map) return;
    khint_t k;
    for (k = kh_begin(doc_map); k != kh_end(doc_map); ++k) {
        if (kh_exist(doc_map, k)) {
            doc_destroy(kh_value(doc_map, k));
            free((char *)kh_key(doc_map, k));
        }
    }
    kh_destroy(DocMap, doc_map);
    doc_map = NULL;
    if (def_cursor) { ts_query_cursor_delete(def_cursor); def_cursor = NULL; }
    if (def_query)  { ts_query_delete(def_query); def_query = NULL; }
}

static void doc_extract_defs(Document *doc) {
    doc_free_defs(doc);
    if (!def_query || !def_cursor || !doc->tree) return;

    TSNode root = ts_tree_root_node(doc->tree);
    ts_query_cursor_exec(def_cursor, def_query, root);

    TSQueryMatch match;
    while (ts_query_cursor_next_match(def_cursor, &match)) {
        /* We expect captures: @name (index 0) and @def (index 1) */
        TSNode name_node;
        TSNode def_node;
        memset(&name_node, 0, sizeof(name_node));
        memset(&def_node, 0, sizeof(def_node));
        for (uint16_t i = 0; i < match.capture_count; i++) {
            uint32_t name_len;
            const char *cname = ts_query_capture_name_for_id(
                def_query, match.captures[i].index, &name_len);
            if (name_len == 4 && memcmp(cname, "name", 4) == 0)
                name_node = match.captures[i].node;
            else if (name_len == 3 && memcmp(cname, "def", 3) == 0)
                def_node = match.captures[i].node;
        }

        if (ts_node_is_null(name_node)) continue;

        uint32_t ns = ts_node_start_byte(name_node);
        uint32_t ne = ts_node_end_byte(name_node);
        if (ne > doc->text_len) continue;

        /* Grow array if needed */
        if (doc->defs_len >= doc->defs_cap) {
            doc->defs_cap = doc->defs_cap ? doc->defs_cap * 2 : 16;
            doc->defs = realloc(doc->defs, sizeof(DocDefinition) * doc->defs_cap);
        }

        DocDefinition *d = &doc->defs[doc->defs_len];
        d->name = malloc(ne - ns + 1);
        memcpy(d->name, doc->text + ns, ne - ns);
        d->name[ne - ns] = '\0';

        d->name_start_byte = ns;
        d->name_end_byte = ne;

        TSPoint sp = ts_node_start_point(name_node);
        TSPoint ep = ts_node_end_point(name_node);
        d->start_row = sp.row;
        d->start_col = sp.column;
        d->end_row = ep.row;
        d->end_col = ep.column;

        /* Extract body text from the full definition node */
        if (!ts_node_is_null(def_node)) {
            uint32_t ds = ts_node_start_byte(def_node);
            uint32_t de = ts_node_end_byte(def_node);
            if (de <= doc->text_len) {
                size_t blen = de - ds;
                d->body_text = malloc(blen + 1);
                memcpy(d->body_text, doc->text + ds, blen);
                d->body_text[blen] = '\0';
            } else {
                d->body_text = NULL;
            }
        } else {
            d->body_text = NULL;
        }
        doc->defs_len++;
    }
}

static void doc_parse(Document *doc) {
    if (doc->tree) {
        ts_tree_delete(doc->tree);
        doc->tree = NULL;
    }
    doc->tree = ts_parser_parse_string(ts_parser, NULL, doc->text, (uint32_t)doc->text_len);
    doc_extract_defs(doc);
}

void doc_open(const char *uri, const char *text, size_t text_len) {
    int ret;
    khint_t k = kh_put(DocMap, doc_map, strdup(uri), &ret);

    Document *doc = calloc(1, sizeof(Document));
    doc->uri = strdup(uri);
    doc->text = malloc(text_len + 1);
    memcpy(doc->text, text, text_len);
    doc->text[text_len] = '\0';
    doc->text_len = text_len;

    if (ret == 0) {
        /* Key already existed — free old doc and old key */
        doc_destroy(kh_value(doc_map, k));
    }
    kh_value(doc_map, k) = doc;
    doc_parse(doc);
}

void doc_update(const char *uri, const char *text, size_t text_len) {
    Document *doc = doc_get(uri);
    if (!doc) {
        doc_open(uri, text, text_len);
        return;
    }
    free(doc->text);
    doc->text = malloc(text_len + 1);
    memcpy(doc->text, text, text_len);
    doc->text[text_len] = '\0';
    doc->text_len = text_len;
    doc_parse(doc);
}

void doc_close(const char *uri) {
    khint_t k = kh_get(DocMap, doc_map, uri);
    if (k == kh_end(doc_map)) return;
    doc_destroy(kh_value(doc_map, k));
    free((char *)kh_key(doc_map, k));
    kh_del(DocMap, doc_map, k);
}

Document *doc_get(const char *uri) {
    khint_t k = kh_get(DocMap, doc_map, uri);
    if (k == kh_end(doc_map)) return NULL;
    return kh_value(doc_map, k);
}

char *doc_symbol_at(Document *doc, uint32_t row, uint32_t col) {
    if (!doc || !doc->tree) return NULL;

    TSNode root = ts_tree_root_node(doc->tree);
    TSPoint pt = { row, col };
    TSNode node = ts_node_named_descendant_for_point_range(root, pt, pt);
    if (ts_node_is_null(node)) return NULL;

    const char *type = ts_node_type(node);
    if (strcmp(type, "symbol") != 0 && strcmp(type, "operator") != 0)
        return NULL;

    uint32_t start = ts_node_start_byte(node);
    uint32_t end = ts_node_end_byte(node);
    if (end > doc->text_len) return NULL;

    size_t len = end - start;
    char *name = malloc(len + 1);
    memcpy(name, doc->text + start, len);
    name[len] = '\0';
    return name;
}

char *doc_word_at(Document *doc, uint32_t row, uint32_t col) {
    if (!doc || !doc->text) return NULL;

    /* Find the byte offset of (row, col) */
    uint32_t cur_row = 0, offset = 0;
    while (cur_row < row && offset < doc->text_len) {
        if (doc->text[offset] == '\n') cur_row++;
        offset++;
    }
    offset += col;
    if (offset > doc->text_len) offset = (uint32_t)doc->text_len;

    /* Scan backwards to find word start */
    uint32_t start = offset;
    while (start > 0 && doc->text[start - 1] != ' ' && doc->text[start - 1] != '\t' &&
           doc->text[start - 1] != '\n' && doc->text[start - 1] != '\r' &&
           doc->text[start - 1] != '[' && doc->text[start - 1] != ']' &&
           doc->text[start - 1] != '{' && doc->text[start - 1] != '}') {
        start--;
    }

    if (start >= offset) return NULL;

    size_t len = offset - start;
    char *word = malloc(len + 1);
    memcpy(word, doc->text + start, len);
    word[len] = '\0';
    return word;
}
