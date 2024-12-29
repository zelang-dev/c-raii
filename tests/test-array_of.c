#include "raii.h"
#include "test_assert.h"


int some_func(int args) {
    return args * 2;
}

TEST(array_of) {
    memory_t *s = unique_init();
    arrays_t d = array_of(s, 8, "hello", "world", 32,
                          "hello world", 123, some_func,
                          "four", 600);
    ASSERT_TRUE(is_array(d));
    array_deferred_set(d, s);
    _defer(raii_delete, s);

    string ar1 = d[0].char_ptr;
    string ar2 = d[1].char_ptr;
    int num = d[2].integer;

    ASSERT_EQ(is_str_eq(ar1, "hello"), true);
    ASSERT_EQ(is_str_eq(ar2, "world"), true);
    ASSERT_EQ(num, 32);


    string data = "hello again!";
    ASSERT_STR("hello world", d[3].char_ptr);
    ASSERT_XEQ(246, ((raii_callable_t)d[5].func)(d[4].integer));
    ASSERT_STR("four", d[6].char_ptr);
    ASSERT_EQ(600, d[7].integer);

    d[4].char_ptr = data;
    printf("vec[4].char_ptr = data\n");
    ASSERT_STR("hello again!", d[4].char_ptr);

    d[7].char_ptr = "string 600";
    ASSERT_STR("string 600", d[7].char_ptr);

    $append(d, 256);
    ASSERT_EQ(256, d[8].integer);

    return 0;
}

TEST(list) {
    int result = 0;

    EXEC_TEST(array_of);

    return result;
}

int main(int argc, char **argv) {
    TEST_FUNC(list());
}
