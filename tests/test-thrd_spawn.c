#include "raii.h"
#include "test_assert.h"

void *is_prime(args_t arg) {
    int x = get_args(arg, 0).integer;
    ASSERT_TRUE((x == 194232491));
    usleep(10);
    int **result = try_calloc(2, sizeof(int));
    result[1] = x;
    result[0] = true;
    return thrd_returning(arg, result);
}

void_t check_primes(void_t result, size_t id, values_type iter) {
    ASSERT_TRUE(iter.array_int[0]);
    ASSERT_TRUE((iter.array_int[1] == 194232491));
}

TEST(thrd_spawn) {
    values_type data[1];
    int prime = 194232491;
    future_t *fut = thrd_scope();
    ASSERT_TRUE(is_type(fut, RAII_SPAWN));

    ASSERT_TRUE(is_type(thrd_spawn_ex(is_prime, "d", prime), RAII_VALUE));
    ASSERT_TRUE(is_type(thrd_spawn_ex(is_prime, "d", prime), RAII_VALUE));
    ASSERT_TRUE(is_type(thrd_spawn_ex(is_prime, "d", prime), RAII_VALUE));
    ASSERT_TRUE(is_type(thrd_spawn_ex(is_prime, "d", prime), RAII_VALUE));

    ASSERT_FALSE(thrd_is_finish(fut));

    thrd_then(check_primes, thrd_sync(fut), data);
    thrd_destroy(fut);

    return 0;
}

TEST(list) {
    int result = 0;

    EXEC_TEST(thrd_spawn);

    return result;
}

int main(int argc, char **argv) {
    TEST_FUNC(list());
}
