/* joy.c -- Tree-sitter based Joy syntax highlighting
 *
 * Uses tree-sitter to parse Joy code and highlight it using query captures.
 */

#include <stdlib.h>
#include <string.h>
#include "syntax/joy.h"
#include "syntax/theme.h"
#include <tree_sitter/api.h>

/* External function from tree-sitter-joy */
extern const TSLanguage *tree_sitter_joy(void);

/* Global state for the highlighter */
static TSParser *parser = NULL;
static TSQuery *query = NULL;
static TSQueryCursor *cursor = NULL;

/* Joy highlighting query.
 * Based on tree-sitter-joy grammar node types. */
static const char *JOY_HIGHLIGHT_QUERY =
    /* Keywords */
    "(library_keyword) @keyword\n"
    "\"==\" @keyword.operator\n"

    /* Literals */
    "(integer) @number\n"
    "(float) @number.float\n"
    "(character) @character\n"
    "(string) @string\n"
    "(interpolated_string) @string.special\n"
    "(boolean) @boolean\n"
    "(null) @constant.builtin\n"

    /* Comments */
    "(line_comment) @comment\n"
    "(block_comment) @comment\n"

    /* Operators */
    "(operator) @operator\n"
    "(cons_operator) @operator\n"

    /* Definition name */
    "(definition name: (symbol) @function.definition)\n"

    /* Symbols/identifiers */
    "(symbol) @variable\n"

    /* Punctuation */
    "[\".\" \";\"] @punctuation.delimiter\n"
    "[\"[\" \"]\"] @punctuation.bracket\n"
    "[\"{\" \"}\"] @punctuation.bracket\n"

    /* Shell escape */
    "(shell_escape) @comment\n"

    /* Native types */
    "(native_vector \"v[\" @punctuation.special)\n"
    "(native_matrix \"m[\" @punctuation.special)\n"
;

/* Map tree-sitter capture names to theme token types.
 * Returns the appropriate color from the current theme. */
static unsigned char get_color_for_capture(const char *name, uint32_t len) {
    /* Keywords */
    if (len >= 7 && strncmp(name, "keyword", 7) == 0) {
        if (len > 8 && strncmp(name + 8, "operator", 8) == 0) {
            return theme_color(TOK_KEYWORD_OPERATOR);
        }
        return theme_color(TOK_KEYWORD);
    }

    /* Comments */
    if (len >= 7 && strncmp(name, "comment", 7) == 0) {
        return theme_color(TOK_COMMENT);
    }

    /* Strings */
    if (len >= 6 && strncmp(name, "string", 6) == 0) {
        if (len > 7 && strncmp(name + 7, "special", 7) == 0) {
            return theme_color(TOK_STRING_SPECIAL);
        }
        return theme_color(TOK_STRING);
    }

    /* Character */
    if (len >= 9 && strncmp(name, "character", 9) == 0) {
        return theme_color(TOK_STRING);
    }

    /* Numbers */
    if (len >= 6 && strncmp(name, "number", 6) == 0) {
        if (len > 7 && strncmp(name + 7, "float", 5) == 0) {
            return theme_color(TOK_NUMBER_FLOAT);
        }
        return theme_color(TOK_NUMBER);
    }

    /* Functions (definition names) */
    if (len >= 8 && strncmp(name, "function", 8) == 0) {
        return theme_color(TOK_FUNCTION);
    }

    /* Booleans */
    if (len >= 7 && strncmp(name, "boolean", 7) == 0) {
        return theme_color(TOK_BOOLEAN);
    }

    /* Constants */
    if (len >= 8 && strncmp(name, "constant", 8) == 0) {
        return theme_color(TOK_CONSTANT_BUILTIN);
    }

    /* Operators */
    if (len >= 8 && strncmp(name, "operator", 8) == 0) {
        return theme_color(TOK_OPERATOR);
    }

    /* Punctuation */
    if (len >= 11 && strncmp(name, "punctuation", 11) == 0) {
        if (len > 12 && strncmp(name + 12, "bracket", 7) == 0) {
            return theme_color(TOK_PUNCTUATION_BRACKET);
        }
        if (len > 12 && strncmp(name + 12, "delimiter", 9) == 0) {
            return theme_color(TOK_PUNCTUATION_DELIMITER);
        }
        if (len > 12 && strncmp(name + 12, "special", 7) == 0) {
            return theme_color(TOK_PUNCTUATION);
        }
        return theme_color(TOK_PUNCTUATION);
    }

    /* Variables */
    if (len >= 8 && strncmp(name, "variable", 8) == 0) {
        return theme_color(TOK_VARIABLE);
    }

    return theme_color(TOK_DEFAULT);
}

int joy_highlight_init(void) {
    uint32_t error_offset;
    TSQueryError error_type;

    if (parser != NULL) {
        return 0; /* Already initialized */
    }

    /* Create parser */
    parser = ts_parser_new();
    if (parser == NULL) {
        return -1;
    }

    /* Set the Joy language */
    if (!ts_parser_set_language(parser, tree_sitter_joy())) {
        ts_parser_delete(parser);
        parser = NULL;
        return -1;
    }

    /* Create the highlighting query */
    query = ts_query_new(
        tree_sitter_joy(),
        JOY_HIGHLIGHT_QUERY,
        (uint32_t)strlen(JOY_HIGHLIGHT_QUERY),
        &error_offset,
        &error_type
    );
    if (query == NULL) {
        ts_parser_delete(parser);
        parser = NULL;
        return -1;
    }

    /* Create a reusable query cursor */
    cursor = ts_query_cursor_new();
    if (cursor == NULL) {
        ts_query_delete(query);
        ts_parser_delete(parser);
        query = NULL;
        parser = NULL;
        return -1;
    }

    return 0;
}

void joy_highlight_free(void) {
    if (cursor != NULL) {
        ts_query_cursor_delete(cursor);
        cursor = NULL;
    }
    if (query != NULL) {
        ts_query_delete(query);
        query = NULL;
    }
    if (parser != NULL) {
        ts_parser_delete(parser);
        parser = NULL;
    }
}

void joy_highlight_callback(const char *buf, char *colors, size_t len) {
    TSTree *tree;
    TSNode root;
    TSQueryMatch match;
    uint32_t capture_index;

    if (parser == NULL || query == NULL || cursor == NULL) {
        return;
    }

    if (len == 0) {
        return;
    }

    /* Parse the input */
    tree = ts_parser_parse_string(parser, NULL, buf, (uint32_t)len);
    if (tree == NULL) {
        return;
    }

    root = ts_tree_root_node(tree);

    /* Execute the query */
    ts_query_cursor_exec(cursor, query, root);

    /* Iterate through captures and apply colors */
    while (ts_query_cursor_next_capture(cursor, &match, &capture_index)) {
        TSQueryCapture capture = match.captures[capture_index];
        uint32_t start = ts_node_start_byte(capture.node);
        uint32_t end = ts_node_end_byte(capture.node);
        uint32_t name_len;
        const char *capture_name;
        unsigned char color;
        uint32_t i;

        /* Get the capture name */
        capture_name = ts_query_capture_name_for_id(query, capture.index, &name_len);
        color = get_color_for_capture(capture_name, name_len);

        /* Apply color to the byte range */
        if (start < len && color != 0) {
            if (end > len) {
                end = (uint32_t)len;
            }
            for (i = start; i < end; i++) {
                /* Only set color if not already set (first match wins for overlapping) */
                if (colors[i] == 0) {
                    colors[i] = (char)color;
                }
            }
        }
    }

    ts_tree_delete(tree);
}
