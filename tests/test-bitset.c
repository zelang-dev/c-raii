/* Converted from boost

https://www.boost.org/doc/libs/1_87_0/libs/dynamic_bitset/example/example1.cpp
https://www.boost.org/doc/libs/1_87_0/libs/dynamic_bitset/example/example2.cpp

*/

#include "bitset.h"
#include "test_assert.h"

TEST(bitset_test) {
    i32 i;
    bits_t x = bitset_create(5); // all 0's by default
    ASSERT_EQ(5, bitset_size(x));
    ASSERT_EQ(0, bitset_count(x));

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

TEST(bitset) {
    bits_t b0 = bitset(2, 0ul);
    ASSERT_TRUE((type_of(b0) == RAII_BITSET));
    ASSERT_STR("00", bitset_to_string(b0));

    ASSERT_STR("01", bitset_to_string(bitset(2, 1ul)));

    bits_t b2 = bitset(2, 2ul);
    ASSERT_STR("10", bitset_to_string(b2));

    bits_t b3 = bitset(2, 3ul);
    ASSERT_STR("11", bitset_to_string(b3));

    return 0;
}

TEST(list) {
    int result = 0;

    EXEC_TEST(bitset_test);
    EXEC_TEST(bitset);
    return result;
}

int main(int argc, char **argv) {
    TEST_FUNC(list());
}
