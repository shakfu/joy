/*
 * api.c - Unit tests for Joy public API
 *
 * Tests the public API defined in joy/joy.h to ensure
 * correct behavior for embedders.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "globals.h"
#include "joy/joy.h"

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  Testing %s... ", #name); \
    fflush(stdout); \
    tests_run++; \
    test_##name(); \
    tests_passed++; \
    printf("OK\n"); \
    fflush(stdout); \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("FAILED at line %d: %s\n", __LINE__, #cond); \
        exit(1); \
    } \
} while(0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))
#define ASSERT_NE(a, b) ASSERT((a) != (b))
#define ASSERT_STR_EQ(a, b) ASSERT(strcmp((a), (b)) == 0)

/* Custom I/O test state */
static char output_buffer[1024];
static size_t output_pos = 0;

static void test_write_string(void* user_data, const char* s)
{
    (void)user_data;
    size_t len = strlen(s);
    if (output_pos + len < sizeof(output_buffer)) {
        memcpy(output_buffer + output_pos, s, len);
        output_pos += len;
        output_buffer[output_pos] = '\0';
    }
}

static void test_write_char(void* user_data, int ch)
{
    (void)user_data;
    if (output_pos + 1 < sizeof(output_buffer)) {
        output_buffer[output_pos++] = (char)ch;
        output_buffer[output_pos] = '\0';
    }
}

static JoyIO capture_io = {
    .write_char = test_write_char,
    .write_string = test_write_string,
};

/* A context whose output goes to output_buffer, without autoput */
static JoyContext* capture_create(void)
{
    JoyConfig config = { .io = &capture_io, .enable_autoput = 0 };

    output_buffer[0] = '\0';
    output_pos = 0;
    return joy_create(&config);
}

/* Evaluate source and return what it wrote */
static const char* eval_output(JoyContext* ctx, const char* source)
{
    output_buffer[0] = '\0';
    output_pos = 0;
    ASSERT_EQ(joy_eval_string(ctx, source), JOY_OK);
    return output_buffer;
}

/* Test version API */
TEST(version)
{
    const char* v = joy_version();
    ASSERT(v != NULL);
    ASSERT(strstr(v, "Joy") != NULL);
    ASSERT(strstr(v, JOY_VERSION_STRING) != NULL);

    /* Version macros should be defined */
    ASSERT(JOY_VERSION_MAJOR >= 1);
    ASSERT(JOY_VERSION_MINOR >= 0);
    ASSERT(JOY_VERSION_PATCH >= 0);
}

/* Test context creation and destruction */
TEST(create_destroy)
{
    JoyContext* ctx = joy_create(NULL);
    ASSERT(ctx != NULL);
    joy_destroy(ctx);

    /* Destroy NULL should be safe */
    joy_destroy(NULL);
}

/* Test configuration options */
TEST(config)
{
    JoyConfig config = {
        .initial_memory_size = 0,
        .max_memory_size = 0,
        .enable_gc_trace = 0,
        .enable_autoput = 0,
        .enable_echo = 0,
        .io = NULL
    };

    JoyContext* ctx = joy_create(&config);
    ASSERT(ctx != NULL);

    /* Check autoput was applied */
    ASSERT_EQ(joy_get_autoput(ctx), 0);
    ASSERT_EQ(joy_get_echo(ctx), 0);

    joy_destroy(ctx);
}

/* Test simple evaluation */
TEST(eval_simple)
{
    JoyContext* ctx = capture_create();
    ASSERT(ctx != NULL);

    /* '.' ends the statement; the program is not left on the stack */
    ASSERT_EQ(joy_eval_string(ctx, "2 3 + ."), JOY_OK);
    ASSERT_EQ(joy_stack_depth(ctx), 1);
    ASSERT_STR_EQ(eval_output(ctx, "stack put."), "[5]");

    joy_destroy(ctx);
}

/* With autoput, each statement prints and pops the top of the stack */
TEST(eval_autoput)
{
    JoyConfig config = { .io = &capture_io, .enable_autoput = 1 };
    JoyContext* ctx = joy_create(&config);
    ASSERT(ctx != NULL);

    ASSERT_STR_EQ(eval_output(ctx, "2 3 + . 4 ."), "5\n4\n");
    ASSERT(joy_stack_empty(ctx));

    joy_destroy(ctx);
}

/* Every statement in the string runs, also after a definition */
TEST(eval_statements)
{
    JoyContext* ctx = capture_create();
    ASSERT(ctx != NULL);

    ASSERT_STR_EQ(eval_output(ctx, "1 put. 2 put."), "12");
    ASSERT_STR_EQ(eval_output(ctx, "DEFINE sq == dup * . 3 sq put."), "9");
    /* Input that ends without '.' is an error and runs nothing later */
    ASSERT_EQ(joy_eval_string(ctx, "1 2 +"), JOY_ERROR_SYNTAX);
    joy_stack_clear(ctx);
    ASSERT_STR_EQ(eval_output(ctx, "7 put."), "7");
    /* Blank input and comments are fine */
    ASSERT_STR_EQ(eval_output(ctx, ""), "");
    ASSERT_STR_EQ(eval_output(ctx, " (* comment *) "), "");

    joy_destroy(ctx);
}

/* After an error, the rest of the string is dropped, not run later */
TEST(eval_error_drops_rest)
{
    JoyContext* ctx = capture_create();
    ASSERT(ctx != NULL);

    ASSERT_NE(joy_eval_string(ctx, "1 0 / . 5 put."), JOY_OK);
    joy_stack_clear(ctx);
    ASSERT_STR_EQ(eval_output(ctx, "6 put."), "6");

    joy_destroy(ctx);
}

/* Definitions are kept by the node collector */
TEST(definitions_survive_gc)
{
    JoyContext* ctx = capture_create();
    ASSERT(ctx != NULL);

    ASSERT_EQ(joy_eval_string(ctx, "DEFINE sq == dup * ."), JOY_OK);
    size_t before = joy_gc_count(ctx);
    ASSERT_EQ(joy_eval_string(ctx, "1 20000 [[1 2] [3] concat pop] times pop. gc."), JOY_OK);
    ASSERT(joy_gc_count(ctx) > before);
    ASSERT_STR_EQ(eval_output(ctx, "3 sq put."), "9");

    joy_destroy(ctx);
}

/* Two live contexts keep separate definitions and stacks */
TEST(two_contexts)
{
    JoyContext* a = capture_create();
    JoyContext* b = capture_create();
    ASSERT(a != NULL && b != NULL);

    ASSERT_EQ(joy_eval_string(a, "DEFINE f == 1 + . 10."), JOY_OK);
    ASSERT_EQ(joy_eval_string(b, "DEFINE f == 2 * . 10."), JOY_OK);
    ASSERT_EQ(joy_eval_string(a, "1 30000 [[1] [2] concat pop] times pop."), JOY_OK);
    ASSERT_STR_EQ(eval_output(a, "f put."), "11");
    ASSERT_STR_EQ(eval_output(b, "f put."), "20");
    joy_destroy(a);
    /* b's vectors and strings survive collections after a is gone */
    ASSERT_EQ(joy_eval_string(b, "1 30000 [\"ab\" \"cd\" concat pop] times."), JOY_OK);
    ASSERT_STR_EQ(eval_output(b, "21 f put."), "42");

    joy_destroy(b);
}

/* A file is read to its end and stays open for the caller */
TEST(eval_file)
{
    JoyContext* ctx = capture_create();
    FILE* fp = tmpfile();
    ASSERT(ctx != NULL && fp != NULL);

    fputs("DEFINE sq == dup * .\n3 sq put.\n4 sq put.", fp); /* no final newline */
    rewind(fp);
    ASSERT_EQ(joy_eval_file(ctx, fp, "test"), JOY_OK);
    ASSERT_STR_EQ(output_buffer, "916");
    ASSERT_EQ(fgetc(fp), EOF); /* still open */
    fclose(fp);
    ASSERT_STR_EQ(eval_output(ctx, "5 sq put."), "25");

    joy_destroy(ctx);
}

/* joy_load_stdlib reads the whole library before returning */
TEST(load_stdlib)
{
    const char* path = "api_stdlib.tmp";
    FILE* fp = fopen(path, "w");
    ASSERT(fp != NULL);
    fputs("DEFINE triple == 3 * .\n", fp);
    fclose(fp);

    JoyContext* ctx = capture_create();
    ASSERT(ctx != NULL);
    ASSERT_EQ(joy_load_stdlib(ctx, path), JOY_OK);
    remove(path);
    ASSERT_STR_EQ(eval_output(ctx, "5 triple put."), "15");
    ASSERT_EQ(joy_load_stdlib(ctx, "no-such-library.joy"), JOY_ERROR_IO);

    joy_destroy(ctx);
}

/* An eval from an output callback reads only its own input */
static JoyContext* nested_ctx;
static const char* nested_source;
static JoyResult nested_result;
static int nested_done;

static void nested_eval_once(void)
{
    if (!nested_done) {
        nested_done = 1;
        nested_result = joy_eval_string(nested_ctx, nested_source);
    }
}

static void nested_write_string(void* user_data, const char* s)
{
    test_write_string(user_data, s);
    nested_eval_once();
}

static void nested_write_char(void* user_data, int ch)
{
    test_write_char(user_data, ch);
    nested_eval_once();
}

TEST(nested_eval)
{
    JoyIO io = { .write_char = nested_write_char, .write_string = nested_write_string };
    JoyConfig config = { .io = &io, .enable_autoput = 0 };

    output_buffer[0] = '\0';
    output_pos = 0;
    nested_done = 0;
    nested_source = "3 put.";
    nested_ctx = joy_create(&config);
    ASSERT(nested_ctx != NULL);
    ASSERT_EQ(joy_eval_string(nested_ctx, "1 put. 2 put."), JOY_OK);
    ASSERT_EQ(nested_result, JOY_OK);
    ASSERT_STR_EQ(output_buffer, "132");
    /* The outer error handler is restored after the nested eval */
    ASSERT_NE(joy_eval_string(nested_ctx, "+ ."), JOY_OK);

    joy_destroy(nested_ctx);
}

/* A nested eval rejects a definition: it would reset the outer statement */
static void ignore_error(void* user_data, JoyResult result, const char* message,
                         const char* file, int line, int column)
{
    (void)user_data;
    (void)result;
    (void)message;
    (void)file;
    (void)line;
    (void)column;
}

TEST(nested_define)
{
    JoyIO io = { .write_char = nested_write_char,
                 .write_string = nested_write_string,
                 .on_error = ignore_error };
    JoyConfig config = { .io = &io, .enable_autoput = 0 };

    output_buffer[0] = '\0';
    output_pos = 0;
    nested_done = 0;
    nested_source = "DEFINE f == 4. 5 put.";
    nested_ctx = joy_create(&config);
    ASSERT(nested_ctx != NULL);
    ASSERT_EQ(joy_eval_string(nested_ctx, "10 [1 2 3] [put] step 20 put."), JOY_OK);
    ASSERT_EQ(nested_result, JOY_ERROR_RUNTIME);
    ASSERT_STR_EQ(output_buffer, "12320");
    ASSERT_EQ(joy_stack_depth(nested_ctx), 1);
    /* Definitions still work at the outermost level */
    ASSERT_EQ(joy_eval_string(nested_ctx, "DEFINE f == 4. f put."), JOY_OK);

    joy_destroy(nested_ctx);
}

/* Test stack operations */
TEST(stack_ops)
{
    JoyContext* ctx = capture_create();
    ASSERT(ctx != NULL);

    /* Stack starts empty */
    ASSERT(joy_stack_empty(ctx));
    ASSERT_EQ(joy_stack_depth(ctx), 0);

    /* Push some values */
    joy_eval_string(ctx, "1 2 3 .");
    ASSERT(!joy_stack_empty(ctx));
    ASSERT_EQ(joy_stack_depth(ctx), 3);

    /* Clear stack */
    joy_stack_clear(ctx);
    ASSERT(joy_stack_empty(ctx));
    ASSERT_EQ(joy_stack_depth(ctx), 0);

    joy_destroy(ctx);
}

/* Test error handling */
TEST(errors)
{
    JoyContext* ctx = joy_create(NULL);
    ASSERT(ctx != NULL);

    /* Stack underflow - note: Joy needs terminator, and error happens during eval */
    JoyResult r = joy_eval_string(ctx, "+ .");
    ASSERT_NE(r, JOY_OK);

    /* Error message should be set */
    const char* msg = joy_error_message(ctx);
    ASSERT(msg != NULL);
    ASSERT(strlen(msg) > 0);

    joy_destroy(ctx);
}

/* Test result string conversion */
TEST(result_strings)
{
    ASSERT_STR_EQ(joy_result_string(JOY_OK), "OK");
    ASSERT_STR_EQ(joy_result_string(JOY_ERROR_SYNTAX), "Syntax error");
    ASSERT_STR_EQ(joy_result_string(JOY_ERROR_RUNTIME), "Runtime error");
    ASSERT_STR_EQ(joy_result_string(JOY_ERROR_TYPE), "Type error");
    ASSERT_STR_EQ(joy_result_string(JOY_ERROR_STACK_UNDERFLOW), "Stack underflow");
    ASSERT_STR_EQ(joy_result_string(JOY_ERROR_OUT_OF_MEMORY), "Out of memory");
    ASSERT_STR_EQ(joy_result_string(JOY_ERROR_IO), "I/O error");
}

/* Test autoput setting */
TEST(autoput)
{
    JoyContext* ctx = joy_create(NULL);
    ASSERT(ctx != NULL);

    /* Default is on */
    joy_set_autoput(ctx, 1);
    ASSERT_EQ(joy_get_autoput(ctx), 1);

    joy_set_autoput(ctx, 0);
    ASSERT_EQ(joy_get_autoput(ctx), 0);

    joy_destroy(ctx);
}

/* Test echo setting */
TEST(echo)
{
    JoyContext* ctx = joy_create(NULL);
    ASSERT(ctx != NULL);

    joy_set_echo(ctx, 1);
    ASSERT_EQ(joy_get_echo(ctx), 1);

    joy_set_echo(ctx, 0);
    ASSERT_EQ(joy_get_echo(ctx), 0);

    joy_destroy(ctx);
}

/* Test memory statistics */
TEST(memory_stats)
{
    JoyContext* ctx = joy_create(NULL);
    ASSERT(ctx != NULL);

    /* Should have some memory available */
    ASSERT(joy_memory_max(ctx) > 0);

    /* After some operations, memory used should be > 0 */
    joy_eval_string(ctx, "[1 2 3 4 5] dup concat .");
    ASSERT(joy_memory_used(ctx) > 0);
    ASSERT(joy_memory_used(ctx) <= joy_memory_max(ctx));

    /* In-use nodes fall after a collection; a cumulative count would not */
    joy_eval_string(ctx, "0 [100000 <] [succ] while pop.");
    size_t before = joy_memory_used(ctx);
    joy_eval_string(ctx, "gc.");
    ASSERT(joy_memory_used(ctx) < before);

    joy_destroy(ctx);
}

/* Contexts can be created and destroyed repeatedly */
TEST(multiple_contexts)
{
    for (int i = 0; i < 20; i++) {
        JoyContext* ctx = capture_create();
        ASSERT(ctx != NULL);
        ASSERT_EQ(joy_eval_string(ctx, "1 2 3 ."), JOY_OK);
        joy_destroy(ctx);
    }
}

/* Test definitions persist */
TEST(definitions)
{
    JoyContext* ctx = joy_create(NULL);
    ASSERT(ctx != NULL);
    joy_set_autoput(ctx, 0);

    /* Define a function */
    JoyResult r = joy_eval_string(ctx, "DEFINE square == dup * .");
    ASSERT_EQ(r, JOY_OK);

    /* Use it */
    joy_stack_clear(ctx);
    r = joy_eval_string(ctx, "5 square .");
    ASSERT_EQ(r, JOY_OK);
    ASSERT_EQ(joy_stack_depth(ctx), 1);

    joy_destroy(ctx);
}

/* Test custom I/O callbacks */
TEST(custom_io)
{
    output_buffer[0] = '\0';
    output_pos = 0;

    JoyIO io = {
        .user_data = NULL,
        .read_char = NULL,
        .write_char = test_write_char,
        .write_string = test_write_string,
        .on_error = NULL
    };

    JoyConfig config = {
        .io = &io,
        .enable_autoput = 0
    };

    JoyContext* ctx = joy_create(&config);
    ASSERT(ctx != NULL);

    joy_eval_string(ctx, "42 put.");
    ASSERT_STR_EQ(output_buffer, "42");

    joy_destroy(ctx);
}

/* Character and string output builtins go through the callbacks */
TEST(output_builtins)
{
    JoyContext* ctx = capture_create();
    ASSERT(ctx != NULL);

    ASSERT_STR_EQ(eval_output(ctx, "\"ab\" putchars 'c putch."), "abc");
    ASSERT_STR_EQ(eval_output(ctx, "stdout \"de\" fputchars 'f fputch pop."),
                  "def");
    ASSERT_STR_EQ(eval_output(ctx, "stdout \"gh\" fputstring pop."), "gh");
    ASSERT_STR_EQ(eval_output(ctx, "[dup] helpdetail."),
                  "\ndup\t:  X  ->  X X.\nPushes an extra copy of X onto stack.\n");

    joy_destroy(ctx);
}

/* Each error class has its own result code, also passed to on_error */
static JoyResult reported;

static void record_error(void* user_data, JoyResult result, const char* message,
                         const char* file, int line, int column)
{
    (void)user_data;
    (void)message;
    (void)file;
    (void)line;
    (void)column;
    reported = result;
}

TEST(result_codes)
{
    JoyIO io = { .on_error = record_error };
    JoyConfig config = { .io = &io };
    JoyContext* ctx = joy_create(&config);
    ASSERT(ctx != NULL);

    ASSERT_EQ(joy_eval_string(ctx, "+ ."), JOY_ERROR_STACK_UNDERFLOW);
    ASSERT_EQ(reported, JOY_ERROR_STACK_UNDERFLOW);
    ASSERT_EQ(joy_eval_string(ctx, "\"a\" 1 + ."), JOY_ERROR_TYPE);
    ASSERT_EQ(reported, JOY_ERROR_TYPE);
    ASSERT_EQ(joy_eval_string(ctx, "1 0 / ."), JOY_ERROR_RUNTIME);
    ASSERT_EQ(reported, JOY_ERROR_RUNTIME);
    ASSERT_EQ(joy_eval_string(ctx, "abort ."), JOY_ERROR_ABORT);
    joy_stack_clear(ctx);
    /* A syntax error is reported and the rest of the input still runs */
    ASSERT_EQ(joy_eval_string(ctx, "[1 2 . 3 ."), JOY_ERROR_SYNTAX);
    ASSERT_EQ(reported, JOY_ERROR_SYNTAX);
    ASSERT_EQ(joy_stack_depth(ctx), 2);
    ASSERT_EQ(joy_eval_string(ctx, "3 ."), JOY_OK);

    joy_destroy(ctx);
}

int main(void)
{
    printf("Joy API Tests\n");
    printf("=============\n");
    fflush(stdout);

    RUN_TEST(version);
    RUN_TEST(result_strings);
    RUN_TEST(create_destroy);
    RUN_TEST(config);
    RUN_TEST(eval_simple);
    RUN_TEST(eval_autoput);
    RUN_TEST(eval_statements);
    RUN_TEST(eval_error_drops_rest);
    RUN_TEST(stack_ops);
    RUN_TEST(errors);
    RUN_TEST(result_codes);
    RUN_TEST(autoput);
    RUN_TEST(echo);
    RUN_TEST(memory_stats);
    RUN_TEST(multiple_contexts);
    RUN_TEST(definitions);
    RUN_TEST(definitions_survive_gc);
    RUN_TEST(two_contexts);
    RUN_TEST(eval_file);
    RUN_TEST(load_stdlib);
    RUN_TEST(nested_eval);
    RUN_TEST(nested_define);
    RUN_TEST(custom_io);
    RUN_TEST(output_builtins);

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
