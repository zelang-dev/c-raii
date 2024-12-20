#include "raii.h"
#include "test_assert.h"

TEST(vector_local) {
    vector(vec, 4, "Bonjour", "tout", "le", "monde");

    ASSERT_TRUE((vec_len(vec) == 4));
    ASSERT_STR("Bonjour", $(vec, 0).char_ptr);

    foreach(item in vec)
        ASSERT_NOTNULL(item.char_ptr);

    vec_del(vec, 3);
    vec_del(vec, 2);
    vec_del(vec, 1);

    ASSERT_TRUE((vec_len(vec) == 1));

    $set(vec, 0, "Hello");
    ASSERT_STR("Hello", $(vec, 0).char_ptr);

    vec_push(vec, "World");
    ASSERT_STR("World", $(vec, 1).char_ptr);

    $del(vec, 1);
    $del(vec, 0);
    ASSERT_TRUE(($size(vec) == 0));

    $$(vec, 0);
    $$(vec, 1);
    $$(vec, 2);
    ASSERT_TRUE(($size(vec) == 3));

    foreach(ii in vec)
        printf("%d ", ii.integer);
    printf("\n");

    vector_clear(vec);
    ASSERT_TRUE(($size(vec) == 0));

    return 0;
}

TEST(vector_values) {
    vectorize(v);

    $push(v, strdup("hello"));
    ASSERT_STR("hello", v[0].char_ptr);

    $push(v, strdup("world"));
    ASSERT_STR("world", v[1].char_ptr);

    return 0;
}

TEST(list) {
    int result = 0;

    EXEC_TEST(vector_local);
    EXEC_TEST(vector_values);

    return result;
}

int main(int argc, char **argv) {
    TEST_FUNC(list());
}
