#ifdef __linux__
#define _GNU_SOURCE
#endif

#include "ub_queue.h"
#include <assert.h>

/* Usage example */

#define N 20000000

int volatile g_start = 0;

/** Get thread id as integer */
uint64_t thread_get_id() {
#if defined(_WIN32)
    return *(uint64_t *)thrd_current().p;
#else
    return (uint64_t)thrd_current();
#endif
}

/** Get CPU timestamp counter */
static FORCEINLINE uint64_t rdtscp() {
    uint64_t tsc;
#ifdef _WIN32
    tsc = __rdtsc();
#else
    /** @todo not recommended to use rdtsc on multicore machine */
    __asm__("rdtsc;"
            "shl $32, %%rdx;"
            "or %%rdx,%%rax"
            : "=a" (tsc)
            :
            : "%rcx", "%rdx", "memory");
#endif
    return tsc;
}

/** Sleep, do nothing */
static FORCEINLINE void nop() {
#ifdef _WIN32
    usleep(0);
#else
    __asm__("rep nop;");
#endif
}

/* Static global storage */
int fibs[N];

int producer(void *ctx) {
    ub_queue_t *q = (ub_queue_t *)ctx;
    size_t i;
    unsigned long count, n1, n2;

    srand((unsigned)time(0) + thread_get_id());
    size_t pause = rand() % 1000;

    /* Wait for global start signal */
    while (g_start == 0)
        thrd_yield();

    /* Wait for a random time */
    for (i = 0; i != pause; i += 1)
        nop();

    /* Enqueue */
    for (count = 0, n1 = 0, n2 = 1; count < N; count++) {
        fibs[count] = n1 + n2;

        void *fib_ptr = (void *)&fibs[count];
        assert(ub_enqueue(q, fib_ptr));

        n1 = n2;
        n2 = fibs[count];
    }

    return 0;
}

int consumer(void *ctx) {
    ub_queue_t *q = (ub_queue_t *)ctx;
    unsigned long count, n1, n2;
    size_t i;

    srand((unsigned)time(0) + thread_get_id());
    size_t pause = rand() % 1000;

    /* Wait for global start signal */
    while (g_start == 0)
        thrd_yield();

    /* Wait for a random time */
    for (i = 0; i != pause; i += 1)
        nop();

    /* Dequeue */
    for (count = 0, n1 = 0, n2 = 1; count < N; count++) {
        int fib = n1 + n2;
        int *pulled;

        while (!ub_dequeue(q, (void **)&pulled)) {
            continue;
        }

        assert(*pulled == fib);
        n1 = n2;
        n2 = fib;
    }

    return 0;
}

int test_multi(ub_queue_t *q) {
    thrd_t thrp, thrc;
    int resp, resc;

    g_start = 0;

    thrd_create(&thrp, consumer, q);
    thrd_create(&thrc, producer, q);

    usleep(1);

    uint64_t start_tsc_time, end_tsc_time;

    start_tsc_time = rdtscp();
    g_start = 1;

    thrd_join(thrp, &resp);
    thrd_join(thrc, &resc);

    end_tsc_time = rdtscp();

    assert(resc == 0 && resp == 0);
    printf("Two-thread Test Complete\n");
    printf("cycles/op for rdtsc %lu\n", (end_tsc_time - start_tsc_time) / N);

    assert(ub_is_empty(q));
    assert(ub_count(q) == 0);
    int ret = ub_destroy(q);
    if (ret)
        printf("Failed to destroy queue: %d\n", ret);

    assert(ub_destroy(q) == 0);

    return 0;
}

int main() {

    test_multi(ub_raii(Mb(1)));

    return 0;
}
