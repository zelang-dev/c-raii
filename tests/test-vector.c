#include "raii.h"
#include "test_assert.h"

TEST(vector_of) {
    vector(vec, 4, "Bonjour", "tout", "le", "monde");

    ASSERT_TRUE(($size(vec) == 4));
    ASSERT_STR("Bonjour", vec[0].char_ptr);

    foreach(item in vec)
        ASSERT_NOTNULL(item.char_ptr);

    $erase(vec, 3);
    $erase(vec, 2);
    $erase(vec, 1);

    ASSERT_TRUE(($size(vec) == 1));

    $insert(vec, 0, "Hello");
    ASSERT_STR("Hello", vec[0].char_ptr);

    ASSERT_STR("Bonjour", vec[1].char_ptr);

    $push_back(vec, "World");
    ASSERT_STR("World", vec[2].char_ptr);

    ASSERT_TRUE(($size(vec) == 3));

    $clear(vec);
    ASSERT_TRUE(($size(vec) == 0));

    $push_back(vec, 0);
    $push_back(vec, 1);
    $push_back(vec, 2);
    ASSERT_TRUE(($size(vec) == 3));

    foreach(ii in vec)
        printf("%d ", ii.integer);
    printf("\n");

    return 0;
}

TEST(vector_any) {
    vectorize(v);

    $push_back(v, strdup("hello"));
    ASSERT_STR("hello", v[0].char_ptr);

    $push_back(v, strdup("world"));
    ASSERT_STR("world", v[1].char_ptr);

    vector_of(v, 4, "Bonjour", "tout", "le", "monde");
    ASSERT_STR("Bonjour", v[2].char_ptr);
    ASSERT_STR("tout", v[3].char_ptr);
    ASSERT_STR("le", v[4].char_ptr);
    ASSERT_STR("monde", v[5].char_ptr);

    vectors_t vec = vector_of(nil, 2, "mixed", 123);
    ASSERT_STR("mixed", vec[0].char_ptr);
    ASSERT_EQ(123, vec[1].integer);

    return 0;
}

TEST(list) {
    int result = 0;

    EXEC_TEST(vector_of);
    EXEC_TEST(vector_any);

    return result;
}

int main(int argc, char **argv) {
    TEST_FUNC(list());
}
