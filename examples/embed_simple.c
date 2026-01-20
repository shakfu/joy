/*
 * embed_simple.c - Simple Joy embedding example
 *
 * This example demonstrates basic embedding of the Joy interpreter
 * into a C program. It shows how to:
 *   - Create an interpreter context
 *   - Evaluate Joy code from strings
 *   - Check for errors
 *   - Clean up resources
 *
 * Compile with:
 *   gcc -o embed_simple embed_simple.c -I../include -L../build -ljoycore_static -lgc
 *
 * Or use CMake (see examples/CMakeLists.txt)
 */
#include <stdio.h>
#include <stdlib.h>
#include <joy/joy.h>

int main(int argc, char* argv[])
{
    JoyResult result;

    /* Print version info */
    printf("Joy Library version: %s\n", joy_version());
    printf("Header version: %d.%d.%d\n\n",
           JOY_VERSION_MAJOR, JOY_VERSION_MINOR, JOY_VERSION_PATCH);

    /* Create interpreter with default configuration */
    JoyContext* ctx = joy_create(NULL);
    if (!ctx) {
        fprintf(stderr, "Failed to create Joy context\n");
        return 1;
    }

    /* Disable autoput for cleaner output control */
    joy_set_autoput(ctx, 0);

    /* Example 1: Simple arithmetic */
    printf("Example 1: 2 3 + 4 *\n");
    result = joy_eval_string(ctx, "2 3 + 4 *");
    if (result == JOY_OK) {
        /* Print the result manually */
        joy_eval_string(ctx, ".");
        printf("\n");
    } else {
        fprintf(stderr, "Error: %s\n", joy_result_string(result));
    }

    /* Clear stack for next example */
    joy_stack_clear(ctx);

    /* Example 2: Factorial using recursion */
    printf("\nExample 2: Define and use factorial\n");
    result = joy_eval_string(ctx,
        "DEFINE factorial == [0 =] [pop 1] [dup 1 - factorial *] ifte.");

    if (result == JOY_OK) {
        printf("5 factorial = ");
        joy_eval_string(ctx, "5 factorial .");
        printf("\n");

        printf("10 factorial = ");
        joy_eval_string(ctx, "10 factorial .");
        printf("\n");
    } else {
        fprintf(stderr, "Error: %s\n", joy_error_message(ctx));
    }

    joy_stack_clear(ctx);

    /* Example 3: List operations */
    printf("\nExample 3: List operations\n");
    printf("[1 2 3 4 5] [dup *] map = ");
    joy_eval_string(ctx, "[1 2 3 4 5] [dup *] map .");
    printf("\n");

    joy_stack_clear(ctx);

    /* Example 4: Error handling */
    printf("\nExample 4: Error handling\n");
    result = joy_eval_string(ctx, "+ ");  /* Stack underflow */
    if (result != JOY_OK) {
        printf("Caught error: %s\n", joy_result_string(result));
        printf("Message: %s\n", joy_error_message(ctx));
    }

    /* Print statistics */
    printf("\nStatistics:\n");
    printf("  Memory used: %zu nodes\n", joy_memory_used(ctx));
    printf("  Memory max:  %zu nodes\n", joy_memory_max(ctx));
    printf("  GC count:    %zu\n", joy_gc_count(ctx));

    /* Clean up */
    joy_destroy(ctx);

    return 0;
}
