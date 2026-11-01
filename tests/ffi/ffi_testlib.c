/* Functions for tests/ffi/ffi.joy: one signature per marshalling path. */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

static int counter;

int add_int(int a, int b) { return a + b; }
unsigned int max_uint(void) { return 4294967295u; }
int negative_int(void) { return -7; }
int64_t add_int64(int64_t a, int64_t b) { return a + b; }
uint64_t big_uint64(void) { return UINT64_MAX; }
size_t size_of_string(const char* s) { return strlen(s); }
char next_char(char c) { return (char)(c + 1); }
bool negate(bool b) { return !b; }
float half_float(float x) { return x / 2; }
double half_double(double x) { return x / 2; }
const char* greeting(void) { return "hello"; }
const char* null_string(void) { return NULL; }
void* null_pointer(void) { return NULL; }
void* same_pointer(void* p) { return p; }
const char* skip_one(const char* s) { return s + 1; }
void bump(void) { counter++; }
int count(void) { return counter; }

/* Order and register overflow: 9 integers and 9 doubles exceed the registers */
double mixed(int a, double b, int c, double d, int e, double f, int g, double h,
             int i, double j, int k, double l, int m, double n, int o, double p)
{
    return a + 2 * b + 3 * c + 4 * d + 5 * e + 6 * f + 7 * g + 8 * h
        + 9 * i + 10 * j + 11 * k + 12 * l + 13 * m + 14 * n + 15 * o + 16 * p;
}

/* Writes into its argument: Joy's own string must not change */
int clobber(char* s)
{
    memset(s, 'x', strlen(s));
    return 0;
}
