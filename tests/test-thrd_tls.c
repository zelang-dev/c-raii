#include "raii.h"
#include "test_assert.h"

int some_function(void) {
    int *foo = thrd_get();
    ASSERT_NOTNULL(foo);
    ASSERT_EQ(10, *foo);
}

int some_defer_function(void *ptr) {
    printf("thread #%d, running defer \n", *(int *)ptr);
    int *foo = thrd_get();
    ASSERT_NULL(foo);
    ASSERT_NOTNULL(ptr);
    ASSERT_EQ(true, *(int *)ptr >= 0);
    puts("");
}

int run(void *arg) {
    int *foo = thrd_unique(8);
    thrd_defer((func_t)some_defer_function, arg);
    ASSERT_NOTNULL(foo);

    *foo = 10;
    usleep(500);
    some_function();

    *foo++;

    printf("thread #%d, exiting\n\n", *(int *)arg);
    return 0;
}

#define THREAD_COUNT 5

int main(void) {
    thrd_t t[THREAD_COUNT];
    int i;
    thrd_init();

    ASSERT_NULL(thrd_get());
    for (i = 0; i < THREAD_COUNT; i++) {
        int *n = thrd_alloc(sizeof * n);  // Holds a thread serial number
        *n = i;
        thrd_create(t + i, run, n);
    }

    for (i = 0; i < THREAD_COUNT; i++) {
        thrd_join(t[i], NULL);
    }

    return 0;
}
