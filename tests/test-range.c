#include "raii.h"
#include "test_assert.h"

TEST(range) {
    int x = 0, i = 10;
    arrays_t list = range(10, 15);
    ASSERT_TRUE(($size(list) == 5));
    foreach(item in list) {
        ASSERT_EQ(x, iitem);
        ASSERT_EQ(i, item.integer);
        i++;
        x++;
    }

    return 0;
}

TEST(rangeing) {
    int x = 0, i = 5;
    arrays_t list = rangeing(5, 10, 2);
    ASSERT_TRUE(($size(list) == 3));
    foreach(item in list) {
        printf("%d ", item.integer);
        ASSERT_EQ(x, iitem);
        ASSERT_EQ(i, item.integer);
        i = i + 2;
        x++;
    }

    return 0;
}

TEST(range_char) {
    string_t hello = "hello world";
    int i = 0;
    arrays_t list = range_char("hello world");
    ASSERT_TRUE(($size(list) == strlen(hello)));
    foreach(item in list) {
        printf("%c ", item.schar);
        ASSERT_EQ(i, iitem);
        ASSERT_CHAR(*hello++, item.schar);
        i++;
    }

    return 0;
}

TEST(list) {
    int result = 0;

    EXEC_TEST(range);
    EXEC_TEST(rangeing);
    EXEC_TEST(range_char);

    return result;
}

int main(int argc, char **argv) {
    TEST_FUNC(list());
}
