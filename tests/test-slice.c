#include "reflection.h"
#include "test_assert.h"

TEST(slice) {
    map_array_t primes = map_array(of_long, 6, 2, 3, 5, 7, 11, 13);
    ASSERT_TRUE((type_of(primes) == RAII_MAP_STRUCT));
    ASSERT_EQ(6, (int)map_count(primes));
    println(1, primes);

    slice_t part = slice(primes, 2, 5);
    ASSERT_TRUE((type_of(part) == RAII_MAP_STRUCT));
    ASSERT_EQ(3, (int)map_count(part));
    println(1, part);

    ASSERT_EQ(5, slice_get(part, 0).integer);
    ASSERT_EQ(7, slice_get(part, 1).integer);
    ASSERT_EQ(11, slice_get(part, 2).integer);

    return exit_scope();
}

TEST(list) {
    int result = 0;

    EXEC_TEST(slice);

    return result;
}

int main(int argc, char **argv) {
    TEST_FUNC(list());
}
