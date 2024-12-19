#include "raii.h"
#include "test_assert.h"

TEST(vector_init) {
    vectors(vec, 4, "Bonjour", "tout", "le", "monde");

    ASSERT_TRUE((vec_len(vec) == 4));
    ASSERT_STR("Bonjour", $(vec, 0).char_ptr);

    foreach(item in vec)
        ASSERT_NOTNULL(item.char_ptr);

    vec_del(vec, 3);
    vec_del(vec, 2);
    vec_del(vec, 1);

    ASSERT_TRUE((vec_len(vec) == 1));

    vec_set(vec, 0, "Hello");
    ASSERT_STR("Hello", $(vec, 0).char_ptr);

    vec_push(vec, "World");
    ASSERT_STR("World", $(vec, 1).char_ptr);

    return 0;
}

TEST(list) {
    int result = 0;

    EXEC_TEST(vector_init);

    return result;
}

int main(int argc, char **argv) {
    TEST_FUNC(list());
}
