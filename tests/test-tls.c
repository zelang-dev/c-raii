/* Test program for cthread. */

#include "raii.h"

#include <stddef.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>

#define CHK_EXPECTED(a, b) assert_expected(a, b, __FILE__, __LINE__, #a, #b)
#define NUM_THREADS 8

thrd_static(int, gLocalVar, 0)

/* Thread function: Compile time thread-local storage */
static int thread_test_local_storage(void *aArg) {
    int thread = *(int *)aArg;
    srand(thread);
    int data = thread + rand();
    *gLocalVar() = data;
    usleep(500);
    printf("thread #%d, gLocalVar is: %d\n", thread, *gLocalVar());
    assert(*gLocalVar() == data);
    free(aArg);
    return 0;
}

void run_tls(void) {
    thrd_t t[NUM_THREADS];
    int i;
    /* Clear the TLS variable (it should keep this value after all
       threads are finished). */
    *gLocalVar() = 1;

    for (i = 0; i < NUM_THREADS; i++) {
        int *n = malloc(sizeof * n);  // Holds a thread serial number
            *n = i;
        /* Start a child thread that modifies gLocalVar */
        thrd_create(t + i, thread_test_local_storage, n);
    }

    for (i = 0; i < NUM_THREADS; i++) {
        thrd_join(t[i], NULL);
    }

    /* Check if the TLS variable has changed */
    assert(*gLocalVar() == 1);
    assert(++*gLocalVar() == 2);
    assert(--*gLocalVar() == 1);
}

int main(void) {
    puts("start thread-local test");
    run_tls();
    puts("end thread-local test\n");

    return 0;
}

void assert_expected(int res, int expected, const char *file, unsigned int line, const char *expr, const char *expected_str) {
    if (res != expected) {
        fflush(stdout);
        fprintf(stderr, "%s:%u: %s: error %d, expected %s\n", file, line, expr, res, expected_str);
        abort();
    }
}
