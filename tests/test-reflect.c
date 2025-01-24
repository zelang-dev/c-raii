#include "reflection.h"
#include "test_assert.h"

TEST(println) {
    as_reflect(kind, worker_t, NULL);
    ASSERT_UEQ((size_t)7, reflect_num_fields(kind));
    ASSERT_STR("worker_t", reflect_type_of(kind));
    ASSERT_UEQ(sizeof(worker_t), reflect_type_size(kind));

    size_t packed_size = sizeof(raii_type) + sizeof(int) + sizeof(unique_t *)
        + sizeof(void_t) + sizeof(thrd_func_t) + sizeof(promise *) + sizeof(raii_deque_t *);
    ASSERT_UEQ(packed_size, reflect_packed_size(kind));

    println(1, kind);

    return 0;
}

TEST(reflect_var_t) {
    char out[SCRAPE_SIZE] = {0};
    ranges_t value, list = range_char("hello world!");
    as_range_char_ref(kind, ranges_t, list);
    println(1, kind);
    ASSERT_FALSE(is_array(kind));
    ASSERT_TRUE((type_of(kind) == RAII_RANGE_CHAR));
    ASSERT_TRUE(is_array(list));

    ASSERT_TRUE((type_of(kind_r) == RAII_STRUCT));
    ASSERT_UEQ((size_t)2, reflect_num_fields(kind_r));
    ASSERT_STR("var_t", reflect_type_of(kind_r));
    ASSERT_UEQ(sizeof(var_t), reflect_type_size(kind_r));
    size_t packed_size = sizeof(raii_type) + sizeof(var_t *);
    ASSERT_UEQ(packed_size, reflect_packed_size(kind_r));
    println(1, kind_r);

    reflect_get_field(kind_r, 1, out);
    ASSERT_TRUE(is_array(c_ptr(out)));
    value = c_ptr(out);
    foreach(item in value)
        printf("%c ", item.schar);
    puts("");

    shuttingdown();
    return 0;
}

TEST(list) {
    int result = 0;

    EXEC_TEST(println);
    EXEC_TEST(reflect_var_t);

    return result;
}

int main(int argc, char **argv) {
    TEST_FUNC(list());
}
