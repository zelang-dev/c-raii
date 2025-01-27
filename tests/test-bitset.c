/* Converted from https://www.boost.org/doc/libs/1_87_0/libs/dynamic_bitset/example/example1.cpp */

#include "bitset.h"
#include "test_assert.h"

TEST(bitset_test) {
    i32 i;
    bits_t x = bitset_create(5); // all 0's by default
    ASSERT_EQ(5, bitset_size(x));

    bitset_set(x, 0);
    bitset_set(x, 1);
    bitset_set(x, 4);
    for (i = 0; i < bitset_size(x); ++i)
        printf("%d", bitset_test(x, i));
    puts("\n");

    ASSERT_STR("10011", bitset_to_string(x));
    ASSERT_EQ(3, bitset_count(x));

    bitset_free(x);
    return 0;
}

TEST(list) {
    int result = 0;

    EXEC_TEST(bitset_test);
    return result;
}

int main(int argc, char **argv) {
    TEST_FUNC(list());
}
