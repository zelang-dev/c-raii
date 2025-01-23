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

TEST(list) {
    int result = 0;

    EXEC_TEST(println);

    return result;
}

int main(int argc, char **argv) {
    TEST_FUNC(list());
}
