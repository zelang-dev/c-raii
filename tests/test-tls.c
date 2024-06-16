/* Test program for cthread. */

#include "raii.h"

#include <stddef.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>

#define CHK_THRD_EXPECTED(a, b) assert_thrd_expected(a, b, __FILE__, __LINE__, #a, #b)
#define CHK_THRD(a) CHK_THRD_EXPECTED(a, thrd_success)
#define CHK_EXPECTED(a, b) assert_expected(a, b, __FILE__, __LINE__, #a, #b)
#define NUM_THREADS 8

thrd_local_create(int, gLocalVar)
thrd_local(int, gLocalVar)

/* Thread function: Compile time thread-local storage */
static int thread_test_local_storage(void *aArg) {
    int thread = *(int *)aArg;
    C11_FREE(aArg);
    srand(thread);
    int data = thread + rand();
    *gLocalVar() = data;
    printf("thread #%d, gLocalVar is: %d\n", thread, *gLocalVar());
    assert(*gLocalVar() == data);
    return 0;
}

#define THREAD_COUNT 5

void run_tls(void) {
    thrd_t t[THREAD_COUNT];
    /* Clear the TLS variable (it should keep this value after all
       threads are finished). */
    *gLocalVar() = 1;

    for (int i = 0; i < THREAD_COUNT; i++) {
        int *n = C11_MALLOC(sizeof * n);  // Holds a thread serial number
            *n = i;
        /* Start a child thread that modifies gLocalVar */
        thrd_create(t + i, thread_test_local_storage, n);
    }

    for (int i = 0; i < THREAD_COUNT; i++) {
        thrd_join(t[i], NULL);
    }

    /* Check if the TLS variable has changed */
    assert(*gLocalVar() == 1);
}

int main(void) {
    puts("start thread-local test");
    run_tls();
    puts("end thread-local test\n");

    return 0;
}

void assert_thrd_expected(int thrd_status, int expected, const char *file, unsigned int line, const char *expr, const char *expected_str) {
    const char *thrd_status_str;

    if (thrd_status != expected) {
        fflush(stdout);

        switch (thrd_status) {
            case thrd_success:	thrd_status_str = "thrd_success"; break;
            case thrd_timedout:	thrd_status_str = "thrd_timedout"; break;
            case thrd_busy:		thrd_status_str = "thrd_busy"; break;
            case thrd_error:	thrd_status_str = "thrd_error"; break;
            case thrd_nomem:	thrd_status_str = "thrd_nomem"; break;
            default:
                fprintf(stderr, "%s:%u: %s: error %d, expected %s\n", file, line, expr, thrd_status, expected_str);
                abort();
        }

        fprintf(stderr, "%s:%u: %s: error %s, expected %s\n", file, line, expr, thrd_status_str, expected_str);
        abort();
    }
}

void assert_expected(int res, int expected, const char *file, unsigned int line, const char *expr, const char *expected_str) {
    if (res != expected) {
        fflush(stdout);
        fprintf(stderr, "%s:%u: %s: error %d, expected %s\n", file, line, expr, res, expected_str);
        abort();
    }
}
