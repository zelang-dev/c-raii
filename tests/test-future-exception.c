#include "raii.h"
#include "test_assert.h"

int some_worker(int args) {
    throw(division_by_zero);
    return args / 0;
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

TEST(thrd_async) {
    future fut = thrd_async(is_future, args_for("six", "hello world", 128, some_worker));
    raii_defer((func_t)thrd_delete, fut);

    ASSERT_TRUE(is_type(fut, RAII_FUTURE));
    ASSERT_FALSE(thrd_is_done(fut));

    try {
        ASSERT_TRUE(thrd_get(fut).boolean);
    } catch (division_by_zero) {
        ASSERT_STR(err, "division_by_zero");
    } end_trying;

    return 0;
}

TEST(list) {
    int result = 0;

    EXEC_TEST(thrd_async);

    return result;
}

int main(int argc, char **argv) {
    TEST_FUNC(list());
}
