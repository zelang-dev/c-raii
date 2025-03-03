#include "raii.h"
#include "test_assert.h"

int some_func(int args) {
    return args * 2;
}

int some_args(args_t args) {
    args_destructor_set(args);
    string arg1 = args[0].char_ptr;
    string arg2 = args[1].char_ptr;
    int num = args[2].integer;
    raii_callable_t numfunc = (raii_callable_t)args[3].func;

    ASSERT_EQ(is_str_eq(arg1, "no no"), true);
    ASSERT_EQ(is_str_eq(arg2, "hello world"), true);
    ASSERT_EQ(num, 64);
    ASSERT_XEQ(numfunc(num), 128);

    return 0;
}

TEST(args_for) {
    memory_t *s = unique_init();
    args_t d = args_for_ex(s, 3, "hello", "world", 32);
    args_deferred_set(d, s);
    deferring(raii_delete, s);

    string arg1 = d[0].char_ptr;
    string arg2 = d[1].char_ptr;
    int num = d[2].integer;

    ASSERT_EQ(is_str_eq(arg1, "hello"), true);
    ASSERT_EQ(is_str_eq(arg2, "world"), true);
    ASSERT_EQ(num, 32);

    return some_args(args_for(4, "no no", "hello world", 64, some_func));
}

TEST(list) {
    int result = 0;

    EXEC_TEST(args_for);

    return result;
}

int main(int argc, char **argv) {
    TEST_FUNC(list());
}
