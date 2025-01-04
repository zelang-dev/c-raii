#include "raii.h"
#include "test_assert.h"

int some_worker(int args) {
    return args * 2;
}

void *is_future(args_t args) {
    string arg1 = args[0].char_ptr;
    int num = args[1].integer;
    raii_callable_t worker = (raii_callable_t)args[2].func;
    usleep(10);

    ASSERT_THREAD(is_str_eq(arg1, "hello world"));
    ASSERT_THREAD((num == 128));
    ASSERT_THREAD((worker(num) == 256));

    return thrd_value(true);
}

TEST(thrd_async_ex) {
    memory_t *s = unique_init();
    future fut = thrd_async_ex(s, is_future, args_for(3, "hello world", 128, some_worker));
    _defer(raii_delete, s);

    ASSERT_TRUE(is_type(fut, RAII_FUTURE));
    ASSERT_FALSE(thrd_is_done(fut));
    ASSERT_TRUE(thrd_get(fut).boolean);

    return 0;
}

TEST(list) {
    int result = 0;

    EXEC_TEST(thrd_async_ex);

    return result;
}

int main(int argc, char **argv) {
    TEST_FUNC(list());
}
