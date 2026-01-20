/*
 * embed_custom_io.c - Joy embedding with custom I/O
 *
 * This example demonstrates how to capture Joy's output using
 * custom I/O callbacks. This is useful for:
 *   - GUI applications that need to redirect output to widgets
 *   - Web servers that need to capture output as strings
 *   - Testing where output needs to be verified
 *
 * Compile with:
 *   gcc -o embed_custom_io embed_custom_io.c -I../include -L../build -ljoycore_static -lgc
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <joy/joy.h>

/* Simple string buffer for capturing output */
typedef struct {
    char* data;
    size_t len;
    size_t cap;
} StringBuffer;

static void buffer_init(StringBuffer* buf)
{
    buf->cap = 256;
    buf->len = 0;
    buf->data = malloc(buf->cap);
    if (buf->data) buf->data[0] = '\0';
}

static void buffer_free(StringBuffer* buf)
{
    free(buf->data);
    buf->data = NULL;
    buf->len = buf->cap = 0;
}

static void buffer_append_char(StringBuffer* buf, int ch)
{
    if (buf->len + 2 > buf->cap) {
        buf->cap *= 2;
        buf->data = realloc(buf->data, buf->cap);
    }
    if (buf->data) {
        buf->data[buf->len++] = (char)ch;
        buf->data[buf->len] = '\0';
    }
}

static void buffer_append_string(StringBuffer* buf, const char* s)
{
    size_t slen = strlen(s);
    if (buf->len + slen + 1 > buf->cap) {
        while (buf->len + slen + 1 > buf->cap)
            buf->cap *= 2;
        buf->data = realloc(buf->data, buf->cap);
    }
    if (buf->data) {
        memcpy(buf->data + buf->len, s, slen + 1);
        buf->len += slen;
    }
}

/* I/O callbacks that write to our buffer */
static void my_write_char(void* user_data, int ch)
{
    StringBuffer* buf = (StringBuffer*)user_data;
    buffer_append_char(buf, ch);
}

static void my_write_string(void* user_data, const char* s)
{
    StringBuffer* buf = (StringBuffer*)user_data;
    buffer_append_string(buf, s);
}

static void my_on_error(void* user_data, JoyResult code, const char* msg,
                        const char* filename, int line, int column)
{
    StringBuffer* buf = (StringBuffer*)user_data;
    char errbuf[512];
    snprintf(errbuf, sizeof(errbuf), "[ERROR %d at %s:%d:%d] %s",
             code, filename ? filename : "<string>", line, column, msg);
    buffer_append_string(buf, errbuf);
}

/* Helper to evaluate and return captured output */
static char* eval_and_capture(JoyContext* ctx, StringBuffer* buf, const char* code)
{
    buf->len = 0;
    if (buf->data) buf->data[0] = '\0';

    joy_eval_string(ctx, code);

    /* Return a copy of the captured output */
    return buf->data ? strdup(buf->data) : NULL;
}

int main(int argc, char* argv[])
{
    StringBuffer output;
    buffer_init(&output);

    /* Set up custom I/O */
    JoyIO io = {
        .user_data = &output,
        .read_char = NULL,  /* Not capturing input in this example */
        .write_char = my_write_char,
        .write_string = my_write_string,
        .on_error = my_on_error
    };

    JoyConfig config = {
        .io = &io,
        .enable_autoput = 0,  /* We control output manually */
        .enable_echo = 0
    };

    JoyContext* ctx = joy_create(&config);
    if (!ctx) {
        fprintf(stderr, "Failed to create Joy context\n");
        buffer_free(&output);
        return 1;
    }

    printf("Joy Library with Custom I/O Example\n");
    printf("====================================\n\n");

    /* Example 1: Capture arithmetic result */
    char* result = eval_and_capture(ctx, &output, "2 3 + 4 * .");
    printf("Code: 2 3 + 4 *\n");
    printf("Captured output: [%s]\n\n", result ? result : "(null)");
    free(result);

    joy_stack_clear(ctx);

    /* Example 2: Capture list output */
    result = eval_and_capture(ctx, &output, "[1 2 3 4 5] dup reverse concat .");
    printf("Code: [1 2 3 4 5] dup reverse concat\n");
    printf("Captured output: [%s]\n\n", result ? result : "(null)");
    free(result);

    joy_stack_clear(ctx);

    /* Example 3: Capture error message */
    result = eval_and_capture(ctx, &output, "pop");  /* Stack underflow */
    printf("Code: pop (with empty stack)\n");
    printf("Captured output: [%s]\n\n", result ? result : "(null)");
    free(result);

    /* Example 4: Multiple outputs */
    output.len = 0;
    if (output.data) output.data[0] = '\0';

    joy_eval_string(ctx, "\"Hello\" putchars \" \" putchars \"World\" putchars \"!\" putchars");
    printf("Code: Multiple putchars\n");
    printf("Captured output: [%s]\n\n", output.data ? output.data : "(null)");

    /* Clean up */
    joy_destroy(ctx);
    buffer_free(&output);

    printf("Done!\n");
    return 0;
}
