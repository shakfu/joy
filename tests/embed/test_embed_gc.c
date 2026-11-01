/*
 * The conservative GC must scan the embedder-side C stack during an eval: a
 * block held only in a C local must survive a collection. The write callback
 * runs inside the eval, like any builtin with a GC-owned local.
 */
#include <stdio.h>
#include <string.h>

#include <joy/joy.h>

#include "gc.h"

enum { BLOCK_SIZE = 64, FILLERS = 20000 };

static const char PATTERN[] = "still alive";
static int checked, survived;

static void hold_block_across_collection(void)
{
    char* volatile held = GC_malloc_atomic(BLOCK_SIZE);

    strcpy(held, PATTERN);
    GC_gcollect();
    /* A wrongly freed block is handed out again and overwritten */
    for (int i = 0; i < FILLERS; i++)
        memset(GC_malloc_atomic(BLOCK_SIZE), 'x', BLOCK_SIZE);
    checked++;
    survived += strcmp(held, PATTERN) == 0;
}

static void on_write_string(void* user_data, const char* s)
{
    (void)user_data;
    (void)s;
    if (!checked)
        hold_block_across_collection();
}

static void on_write_char(void* user_data, int ch)
{
    (void)ch;
    on_write_string(user_data, "");
}

int main(void)
{
    JoyIO io;
    JoyConfig config;
    JoyContext* ctx;

    memset(&io, 0, sizeof(io));
    io.write_string = on_write_string;
    io.write_char = on_write_char;
    memset(&config, 0, sizeof(config));
    config.io = &io;
    ctx = joy_create(&config);
    if (!ctx) {
        fputs("joy_create failed\n", stderr);
        return 1;
    }
    if (joy_eval_string(ctx, "1 put.") != JOY_OK)
        fprintf(stderr, "eval failed: %s\n", joy_error_message(ctx));
    joy_destroy(ctx);
    printf("checked=%d survived=%d\n", checked, survived);
    return checked == 1 && survived == 1 ? 0 : 1;
}
