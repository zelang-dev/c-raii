#include "raii.h"
#include "test_assert.h"

int some_func(int args) {
    return args * 2;
}

int some_args(args_t args) {
    string arg1 = get_args(args, 0).char_ptr;
    string arg2 = get_args(args, 1).char_ptr;
    int num = get_args(args, 2).integer;
    raii_callable_t numfunc = (raii_callable_t)get_args(args, 3).func;

    ASSERT_EQ(is_str_eq(arg1, "no no"), true);
    ASSERT_EQ(is_str_eq(arg2, "hello world"), true);
    ASSERT_EQ(num, 64);
    ASSERT_XEQ(numfunc(num), 128);

    return 0;
}

TEST(args_for) {
    args_t d = raii_args_for(raii_init(), "ssi", "hello", "world", 32);
    string arg1 = args_in(d, 0).char_ptr;
    string arg2 = args_in(d, 1).char_ptr;
    int num = args_in(d, 2).integer;

    ASSERT_EQ(is_str_eq(arg1, "hello"), true);
    ASSERT_EQ(is_str_eq(arg2, "world"), true);
    ASSERT_EQ(num, 32);
    args_free(d);

    return some_args(args_for("ssdx", "no no", "hello world", 64, some_func));
}

TEST(list) {
    int result = 0;

    EXEC_TEST(args_for);

    raii_deferred_clean();
    return result;
}

int main(int argc, char **argv) {
    TEST_FUNC(list());
}
