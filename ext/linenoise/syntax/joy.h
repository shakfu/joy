/* joy.h -- Tree-sitter based Joy syntax highlighting for linenoise
 *
 * This module provides a syntax highlighting callback that uses tree-sitter
 * to parse Joy code and colorize it.
 */

#ifndef HIGHLIGHT_JOY_H
#define HIGHLIGHT_JOY_H

#include <stddef.h>
#include "syntax/theme.h"

/* Nord palette with Joy-specific categories: visible brackets and
 * delimiters, keywords distinct from numbers, builtins, and errors. */
extern const syntax_theme_t theme_joy_nord;

/* Register a function that tells whether a word is a builtin; such words
 * get the theme's TOK_FUNCTION_BUILTIN colour. NULL disables the check. */
void joy_highlight_set_builtin_lookup(int (*is_builtin)(const char *word, size_t len));

/* Initialize the Joy highlighter.
 * Must be called before using joy_highlight_callback.
 * Returns 0 on success, -1 on failure. */
int joy_highlight_init(void);

/* Free resources used by the Joy highlighter.
 * Should be called when highlighting is no longer needed. */
void joy_highlight_free(void);

/* Syntax highlighting callback for linenoise.
 * Can be passed to linenoise_set_highlight_callback().
 *
 * Color values used (mapped via theme system):
 *   keywords - purple/magenta
 *   strings - green
 *   numbers - yellow/orange
 *   comments - gray
 *   operators - cyan
 *   definitions - blue
 *   booleans - orange
 */
void joy_highlight_callback(const char *buf, char *colors, size_t len,
                            size_t pos, void *udata);

#endif /* HIGHLIGHT_JOY_H */
