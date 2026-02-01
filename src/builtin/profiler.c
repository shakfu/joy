/*
 *  module  : profiler.c
 *  version : 1.0
 *  date    : 02/01/26
 *
 *  Profiler builtins: profile, profile-start, profile-stop,
 *  profile-report, profile-data, profile-reset
 */
#include "globals.h"
#include "runtime.h"

/*
 * Comparison function for qsort - sort by self_time descending
 */
typedef struct ProfileSortEntry {
    int sym_idx;
    int64_t call_count;
    int64_t total_time_ns;
    int64_t self_time_ns;
} ProfileSortEntry;

static int profile_cmp_self_time(const void* a, const void* b)
{
    const ProfileSortEntry* pa = (const ProfileSortEntry*)a;
    const ProfileSortEntry* pb = (const ProfileSortEntry*)b;
    /* Sort descending by self_time */
    if (pb->self_time_ns > pa->self_time_ns)
        return 1;
    if (pb->self_time_ns < pa->self_time_ns)
        return -1;
    return 0;
}

/*
 * profiler_reset - Clear all profiling data
 */
static void profiler_reset(pEnv env)
{
    EnvProfiler* prof = &env->profiler;
    if (prof->entries) {
        vec_setsize(prof->entries, 0);
    }
    prof->call_depth = 0;
    prof->enabled = 0;
}

/*
 * profiler_report - Format and print profiling report to stdout
 */
static void profiler_report(pEnv env)
{
    EnvProfiler* prof = &env->profiler;
    size_t n, i, count;
    ProfileSortEntry* sorted;
    int64_t total_time = 0;

    if (!prof->entries || vec_size(prof->entries) == 0) {
        printf("No profiling data collected.\n");
        return;
    }

    n = vec_size(prof->entries);

    /* Count non-zero entries */
    count = 0;
    for (i = 0; i < n; i++) {
        if (vec_at(prof->entries, i).call_count > 0) {
            count++;
            total_time += vec_at(prof->entries, i).self_time_ns;
        }
    }

    if (count == 0) {
        printf("No profiling data collected.\n");
        return;
    }

    /* Build sortable array */
    sorted = malloc(count * sizeof(ProfileSortEntry));
    if (!sorted) {
        printf("Memory allocation failed.\n");
        return;
    }

    count = 0;
    for (i = 0; i < n; i++) {
        ProfileEntry* pe = &vec_at(prof->entries, i);
        if (pe->call_count > 0) {
            sorted[count].sym_idx = (int)i;
            sorted[count].call_count = pe->call_count;
            sorted[count].total_time_ns = pe->total_time_ns;
            sorted[count].self_time_ns = pe->self_time_ns;
            count++;
        }
    }

    /* Sort by self_time descending */
    qsort(sorted, count, sizeof(ProfileSortEntry), profile_cmp_self_time);

    /* Print report */
    printf("=== Profile Report ===\n");
    printf("Total: %.2f ms\n\n", total_time / 1000000.0);
    printf("%20s   %8s   %10s   %10s\n", "Symbol", "Calls", "Total(ms)", "Self(ms)");
    printf("------------------------------------------------------------\n");

    for (i = 0; i < count; i++) {
        Entry ent = vec_at(env->symtab, sorted[i].sym_idx);
        printf("%20s   %8" PRId64 "   %10.2f   %10.2f\n",
               ent.name,
               sorted[i].call_count,
               sorted[i].total_time_ns / 1000000.0,
               sorted[i].self_time_ns / 1000000.0);
    }

    free(sorted);
}

/**
Q1  OK  3950  profile  :  [P]  ->  ...
Executes P with profiling enabled and prints a timing report.
*/
void profile_(pEnv env)
{
    ONEPARAM("profile");
    ONEQUOTE("profile");
    SAVESTACK;
    POP(env->stck);

    /* Reset and enable profiler */
    profiler_reset(env);
    env->profiler.enabled = 1;

    /* Execute the quotation */
    exec_term(env, nodevalue(SAVED1).lis);

    /* Stop profiling and print report */
    env->profiler.enabled = 0;
    profiler_report(env);

    POP(env->dump);
}

/**
Q0  OK  3960  profile-start\0profile_start  :  ->
Starts collecting profiling data (clears previous data).
*/
void profile_start_(pEnv env)
{
    profiler_reset(env);
    env->profiler.enabled = 1;
}

/**
Q0  OK  3970  profile-stop\0profile_stop  :  ->
Stops collecting profiling data.
*/
void profile_stop_(pEnv env)
{
    env->profiler.enabled = 0;
}

/**
Q0  OK  3980  profile-report\0profile_report  :  ->
Prints the current profiling data as a formatted report.
*/
void profile_report_(pEnv env)
{
    profiler_report(env);
}

/**
Q0  OK  3990  profile-data\0profile_data  :  ->  L
Pushes profiling data as a list of [name calls total-ns self-ns] lists.
*/
void profile_data_(pEnv env)
{
    EnvProfiler* prof = &env->profiler;
    Index result = 0;
    size_t n, i;

    if (!prof->entries || vec_size(prof->entries) == 0) {
        NULLARY(LIST_NEWNODE, result);
        return;
    }

    n = vec_size(prof->entries);

    /* Build list in reverse order so first entries end up first */
    for (i = n; i > 0; i--) {
        ProfileEntry* pe = &vec_at(prof->entries, i - 1);
        if (pe->call_count > 0) {
            Entry ent = vec_at(env->symtab, (int)(i - 1));
            Index inner = 0;

            /* Build inner list: [name calls total-ns self-ns] */
            inner = INTEGER_NEWNODE(pe->self_time_ns, inner);
            inner = INTEGER_NEWNODE(pe->total_time_ns, inner);
            inner = INTEGER_NEWNODE(pe->call_count, inner);
            inner = STRING_NEWNODE(GC_strdup(ent.name), inner);

            result = LIST_NEWNODE(inner, result);
        }
    }

    NULLARY(LIST_NEWNODE, result);
}

/**
Q0  OK  4000  profile-reset\0profile_reset  :  ->
Clears all profiling data.
*/
void profile_reset_(pEnv env)
{
    profiler_reset(env);
}
