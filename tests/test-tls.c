/* Test program for cthread. */

#include "raii.h"

#include <stddef.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>
#define NUM_THREADS 8

int flag;
static int test_failed;
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

static int
test_fail_cb(const char *reason, const char *file, int line) {
    fprintf(stderr, "FAIL: %s @ %s:%d\n", reason, file, line);
    fflush(stderr);
    test_failed = 1;
    return -1;
}

#define test_fail(msg) test_fail_cb(msg, __FILE__, __LINE__)

static int got_error = 0;

static void test_error_callback(const char *message) {
    printf("%s\n", message);
    (void)sizeof(message);
    got_error = 1;
}

static void test_memory(void) {
    rpmalloc_config_t config = {0};
    config.error_callback = test_error_callback;
    rpmalloc_initialize_config(&config);

    void *ptr = malloc(10);
    free(ptr);
    //rpmalloc_finalize();

     if (!got_error) {
         //printf("Leak not detected and reported as expected\n");
         return;
     }
}

int main(void) {
    puts("start thread-local test");
    run_tls();
    puts("end thread-local test\n");

    puts("start memory test");
    test_memory();
    puts("end memory test\n");

    return 0;
}
