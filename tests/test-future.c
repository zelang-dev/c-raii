#include "raii.h"
#include "test_assert.h"

int some_worker(int args) {
    return args * 2;
}

void *is_future(args_t args) {
    string arg1 = get_args(args, 0).char_ptr;
    int num = get_args(args, 1).integer;
    raii_callable_t worker = (raii_callable_t)get_args(args, 2).func;
    usleep(10);

    ASSERT_THREAD(is_str_eq(arg1, "hello world"));
    ASSERT_THREAD((num == 128));
    ASSERT_THREAD((worker(num) == 256));

    return thrd_value(true);
}

TEST(thrd_for) {
    future *fut = thrd_for(is_future, args_for("six", "hello world", 128, some_worker));

    ASSERT_TRUE(is_type(fut, RAII_FUTURE));
    ASSERT_FALSE(thrd_is_done(fut));
    ASSERT_TRUE(thrd_get(fut).boolean);

    return 0;
}

TEST(list) {
    int result = 0;

    EXEC_TEST(thrd_for);

    return result;
}

int main(int argc, char **argv) {
    TEST_FUNC(list());
    raii_destroy();
}