#include "raii.h"
#include "test_assert.h"

void *is_prime(args_t arg) {
    int x = arg[0].integer;
    ASSERT_THREAD(is_args(arg));
    ASSERT_THREAD((arg[0].integer == 194232491));
    usleep(10);

    return $$(true, x);
}

void_t check_primes(void_t result, size_t id, vectors_t object) {
    bool y = object[0].boolean;
    int prime = object[1].integer;
    ASSERT_TRUE((prime == 194232491));
    ASSERT_EQ(y, true);

    *(int *)result = 0;
    return result;
}

TEST(thrd_spawn) {
    int data = 1;
    int prime = 194232491;
    future_t fut = thrd_scope();
    ASSERT_TRUE(is_type(fut, RAII_SPAWN));

    ASSERT_TRUE(is_type(thrd_spawn(is_prime, 1, prime), RAII_VALUE));
    ASSERT_TRUE(is_type(thrd_spawn(is_prime, 1, prime), RAII_VALUE));
    ASSERT_TRUE(is_type(thrd_spawn(is_prime, 1, prime), RAII_VALUE));
    ASSERT_TRUE(is_type(thrd_spawn(is_prime, 1, prime), RAII_VALUE));

    ASSERT_FALSE(thrd_is_finish(fut));

    thrd_then(check_primes, thrd_sync(fut), &data);
    if (!data)
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
