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

mtx_t mtx;
mtx_t mtx2;
cnd_t cnd;
cnd_t cnd2;
tss_t tss;
once_flag once = ONCE_FLAG_INIT;
int flag;
static int test_failed;

thread_storage_create(int, gLocalVar)
void run_thread_test(void);
void run_timed_mtx_test(void);
void run_cnd_test(void);
void run_tss_test(void);
void run_emulated_tls(void);
void run_call_once_test(void);
thread_storage(int, gLocalVar)

/* Thread function: Compile time thread-local storage */
static int thread_test_local_storage(void *aArg) {
    int thread = *(int *)aArg;
    free(aArg);

    int data = thread + rand();
    *gLocalVar() = data;
    thrd_sleep(time_spec(0, 5000), NULL);
    printf("thread #%d, gLocalVar is: %d\n", thread, *gLocalVar());
    assert(*gLocalVar() == data);
    return 0;
}

#define THREAD_COUNT 5

void run_emulated_tls(void) {
    thrd_t t[THREAD_COUNT];
    int i;
    assert(rpmalloc_gLocalVar_tls == 0);
    /* Clear the TLS variable (it should keep this value after all
       threads are finished). */
    *gLocalVar() = 1;
    assert(rpmalloc_gLocalVar_tls == sizeof(int));

    for (i = 0; i < THREAD_COUNT; i++) {
        int *n = malloc(sizeof * n);  // Holds a thread serial number
            *n = i;
        /* Start a child thread that modifies gLocalVar */
        thrd_create(t + i, thread_test_local_storage, n);
    }

    for (i = 0; i < THREAD_COUNT; i++) {
        thrd_join(t[i], NULL);
    }

    /* Check if the TLS variable has changed */
    assert(*gLocalVar() == 1);
}

int main(int argc, char **argv) {
    puts("start thread test");
    run_thread_test();
    puts("end thread test\n");

#if !defined(__arm__)
    puts("start timed mutex test");
    run_timed_mtx_test();
    puts("end timed mutex test\n");
#endif

    puts("start condvar test");
    run_cnd_test();
    puts("end condvar test\n");

    puts("start thread-specific storage test");
    run_tss_test();
    puts("end thread-specific storage test\n");

    puts("start emulate thread-local storage test");
    run_emulated_tls();
    puts("end emulate thread-local storage test\n");

    puts("start call once test");
    run_call_once_test();
    puts("end call once test\n");

    puts("\ntests finished");

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

int tfunc(void *arg) {
    int num;
    struct timespec dur;

    num = (int)(size_t)arg;

    printf("hello from thread %d\n", num);

    dur.tv_sec = 1;
    dur.tv_nsec = 0;
    CHK_EXPECTED(thrd_sleep(&dur, NULL), 0);

    printf("thread %d done\n", num);
    return 0;
}

void run_thread_test(void) {
    int i;
    thrd_t threads[NUM_THREADS];

    for (i = 0; i < NUM_THREADS; i++) {
        CHK_THRD(thrd_create(threads + i, tfunc, (void *)(size_t)i));
    }
    for (i = 0; i < NUM_THREADS; i++) {
        CHK_THRD(thrd_join(threads[i], NULL));
    }
}

#if !defined(_WIN32) || defined(C11THREADS_PTHREAD_WIN32) || !defined(C11THREADS_OLD_WIN32API)
int hold_mutex_for_one_second(void *arg) {
    struct timespec dur;

    (void)arg;

    CHK_THRD(mtx_lock(&mtx));

    CHK_THRD(mtx_lock(&mtx2));
    flag = 1;
    CHK_THRD(cnd_signal(&cnd));
    CHK_THRD(mtx_unlock(&mtx2));

    dur.tv_sec = 1;
    dur.tv_nsec = 0;
    CHK_EXPECTED(thrd_sleep(&dur, NULL), 0);

    CHK_THRD(mtx_unlock(&mtx));

    return 0;
}

void run_timed_mtx_test(void) {
    thrd_t thread;
    struct timespec ts;
    struct timespec dur;

    CHK_THRD(mtx_init(&mtx, mtx_timed));
    CHK_THRD(mtx_init(&mtx2, mtx_plain));
    CHK_THRD(cnd_init(&cnd));
    flag = 0;

    CHK_THRD(thrd_create(&thread, hold_mutex_for_one_second, NULL));

    CHK_THRD(mtx_lock(&mtx2));
    while (!flag) {
        CHK_THRD(cnd_wait(&cnd, &mtx2));
    }
    CHK_THRD(mtx_unlock(&mtx2));
    mtx_destroy(&mtx2);
    cnd_destroy(&cnd);

    CHK_EXPECTED(timespec_get(&ts, TIME_UTC), TIME_UTC);
    ts.tv_nsec += 500000000;
    if (ts.tv_nsec >= 1000000000) {
        ++ts.tv_sec;
        ts.tv_nsec -= 1000000000;
    }
    CHK_THRD_EXPECTED(mtx_timedlock(&mtx, &ts), thrd_timedout);
    puts("thread has locked mutex & we timed out waiting for it");

    dur.tv_sec = 1;
    dur.tv_nsec = 0;
    CHK_EXPECTED(thrd_sleep(&dur, NULL), 0);

    CHK_EXPECTED(timespec_get(&ts, TIME_UTC), TIME_UTC);
    ts.tv_nsec += 500000000;
    if (ts.tv_nsec >= 1000000000) {
        ++ts.tv_sec;
        ts.tv_nsec -= 1000000000;
    }
    CHK_THRD(mtx_timedlock(&mtx, &ts));
    puts("thread no longer has mutex & we grabbed it");
    CHK_THRD(mtx_unlock(&mtx));
    mtx_destroy(&mtx);
    CHK_THRD(thrd_join(thread, NULL));
}
#endif

int my_cnd_thread_func(void *arg) {
    int thread_num;
    thread_num = (int)(size_t)arg;
    CHK_THRD(mtx_lock(&mtx));
    ++flag;
    CHK_THRD(cnd_signal(&cnd2));
    do {
        CHK_THRD(cnd_wait(&cnd, &mtx));
        printf("thread %d: woke up\n", thread_num);
    } while (flag <= NUM_THREADS);
    printf("thread %d: flag > NUM_THREADS; incrementing flag and exiting\n", thread_num);
    ++flag;
    CHK_THRD(cnd_signal(&cnd2));
    CHK_THRD(mtx_unlock(&mtx));
    return 0;
}

void run_cnd_test(void) {
    struct timespec dur;
    int i;
    thrd_t threads[NUM_THREADS];

    flag = 0;
    dur.tv_sec = 0;
    dur.tv_nsec = 500000000;
    CHK_THRD(mtx_init(&mtx, mtx_plain));
    CHK_THRD(cnd_init(&cnd));
    CHK_THRD(cnd_init(&cnd2));

    for (i = 0; i < NUM_THREADS; i++) {
        CHK_THRD(thrd_create(threads + i, my_cnd_thread_func, (void *)(size_t)i));
    }

    CHK_THRD(mtx_lock(&mtx));
    while (flag != NUM_THREADS) {
        CHK_THRD(cnd_wait(&cnd2, &mtx));
    }
    CHK_THRD(mtx_unlock(&mtx));
    puts("main thread: threads are ready");

    /* No guarantees, but this might unblock a thread. */
    puts("main thread: cnd_signal()");
    CHK_THRD(cnd_signal(&cnd));
    CHK_THRD(thrd_sleep(&dur, NULL));

    /* No guarantees, but this might unblock all threads. */
    puts("main thread: cnd_broadcast()");
    CHK_THRD(cnd_broadcast(&cnd));
    CHK_THRD(thrd_sleep(&dur, NULL));

    CHK_THRD(mtx_lock(&mtx));
    flag = NUM_THREADS + 1;
    CHK_THRD(mtx_unlock(&mtx));
    puts("main thread: set flag to NUM_THREADS + 1");

    /* No guarantees, but this might unblock two threads. */
    puts("main thread: sending cnd_signal() twice");
    CHK_THRD(cnd_signal(&cnd));
    CHK_THRD(cnd_signal(&cnd));
    CHK_THRD(thrd_sleep(&dur, NULL));

    CHK_THRD(mtx_lock(&mtx));
    while (flag == NUM_THREADS + 1) {
        CHK_THRD(cnd_wait(&cnd2, &mtx));
    }
    CHK_THRD(mtx_unlock(&mtx));

    puts("main thread: woke up, flag != NUM_THREADS + 1; sending cnd_broadcast() and joining threads");
    CHK_THRD(cnd_broadcast(&cnd));
    for (i = 0; i < NUM_THREADS; i++) {
        CHK_THRD(thrd_join(threads[i], NULL));
    }

    cnd_destroy(&cnd2);
    cnd_destroy(&cnd);
    mtx_destroy(&mtx);
}

void my_tss_dtor(void *arg) {
    printf("dtor: content of tss: %d\n", (int)(size_t)arg);
    CHK_EXPECTED((int)(size_t)arg, 42);
}

int my_tss_thread_func(void *arg) {
    void *tss_content;

    (void)arg;

    tss_content = tss_get(tss);
    printf("thread func: initial content of tss: %d\n", (int)(size_t)tss_content);
    CHK_THRD(tss_set(tss, (void *)42));
    tss_content = tss_get(tss);
    printf("thread func: content of tss now: %d\n", (int)(size_t)tss_content);
    CHK_EXPECTED((int)(size_t)tss_content, 42);
    return 0;
}

void run_tss_test(void) {
    thrd_t thread;

    CHK_THRD(tss_create(&tss, my_tss_dtor));
    CHK_THRD(thrd_create(&thread, my_tss_thread_func, NULL));
    CHK_THRD(thrd_join(thread, NULL));
    tss_delete(tss);
}

void my_call_once_func(void) {
    puts("my_call_once_func() was called");
    ++flag;
}

int my_call_once_thread_func(void *arg) {
    (void)arg;
    puts("my_call_once_thread_func() was called");
    call_once(&once, my_call_once_func);
    return 0;
}

void run_call_once_test(void) {
    int i;
    thrd_t threads[NUM_THREADS];

    flag = 0;

    for (i = 0; i < NUM_THREADS; i++) {
        CHK_THRD(thrd_create(threads + i, my_call_once_thread_func, NULL));
    }

    for (i = 0; i < NUM_THREADS; i++) {
        CHK_THRD(thrd_join(threads[i], NULL));
    }

    printf("content of flag: %d\n", flag);

    CHK_EXPECTED(flag, 1);
}
