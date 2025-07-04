#include "map.h"
#include "test_assert.h"

static string _value_1 = "value1";
static string _value_2 = "value2";
static string _value_3 = "value3";

TEST(map_for) {
    int i = 0, index1 = 1, index2 = 2, index4 = 21;
    string index3 = "hellohellohellohellohellohellohellohellohellohellohellohellohello";
    string text = "hellohello";

    map_t list = map_for(3,
                         kv_signed("__1_", index1),
                         kv_double("two", index2),
                         kv_string("3", index3)
    );

    ASSERT_TRUE((type_of(list) == RAII_MAP_STRUCT));
    ASSERT_UEQ(3, map_count(list));
    ASSERT_EQ(index1, map_get(list, "__1_").integer);
    ASSERT_DOUBLE(2.0, map_get(list, "two").precision);
    ASSERT_STR(index3, map_get(list, "3").char_ptr);

    map_put(list, "4", &index4);
    ASSERT_PTR(&index4, map_get(list, "4").object);
    ASSERT_UEQ(4, map_count(list));

    map_put(list, "3", casting(3));
    ASSERT_EQ(3, map_get(list, "3").integer);
    ASSERT_UEQ(4, map_count(list));

    index4 = 99;
    foreach_map(item in list) {
        if (i == 0) {
            ASSERT_TRUE((iter_type(item) == RAII_LLONG));
            printf("item key is %s, and value is %d\n", indic(item), has(item).integer);
        } else if (i == 1) {
            ASSERT_TRUE((iter_type(item) == RAII_DOUBLE));
            printf("item key is %s, and value is %.1f\n", indic(item), has(item).precision);
        } else if (i == 2) {
            ASSERT_TRUE((iter_type(item) == RAII_OBJ));
            printf("item key is %s, and value is %d\n", indic(item), has(item).integer);
        } else if (i == 3) {
            ASSERT_TRUE((iter_type(item) == RAII_OBJ));
            printf("item key is %s, and value is %d\n", indic(item), *has(item).int_ptr);
        }
        i++;
    }

    map_free(list);

    list = map_insert(maps(), kv_string("check", text));
    ASSERT_STR(text, map_get(list, "check").char_ptr);
    text = index3;
    ASSERT_STR("hellohello", map_get(list, "check").char_ptr);

    return 0;
}


TEST(map_push) {
    map_t list = map_create();
    string value;

    map_push(list, _value_1);
    ASSERT_XEQ(map_count(list), 1);
    value = map_pop(list).char_ptr;
    ASSERT_TRUE(is_equal(value, _value_1));
    ASSERT_XEQ(map_count(list), 0);

    map_push(list, _value_2);
    ASSERT_XEQ(map_count(list), 1);
    map_push(list, _value_3);
    ASSERT_XEQ(map_count(list), 2);
    value = map_pop(list).object;
    ASSERT_TRUE(is_equal(value, _value_3));
    ASSERT_XEQ(map_count(list), 1);
    value = map_pop(list).object;
    ASSERT_TRUE(is_equal(value, _value_2));
    ASSERT_XEQ(map_count(list), 0);

    map_free(list);
    return 0;
}

TEST(map_shift) {
    map_t list = map_create();
    string value;

    map_shift(list, _value_1);
    ASSERT_XEQ(map_count(list), 1);
    value = map_unshift(list).object;
    ASSERT_TRUE(is_equal(value, _value_1));
    ASSERT_XEQ(map_count(list), 0);

    map_shift(list, _value_2);
    ASSERT_XEQ(map_count(list), 1);
    map_shift(list, _value_3);
    ASSERT_XEQ(map_count(list), 2);
    value = map_unshift(list).object;
    ASSERT_TRUE(is_equal(value, _value_3));
    ASSERT_XEQ(map_count(list), 1);
    value = map_unshift(list).object;
    ASSERT_TRUE(is_equal(value, _value_2));
    ASSERT_XEQ(map_count(list), 0);

    map_free(list);
    return 0;
}

TEST(list) {
    int result = 0;

    EXEC_TEST(map_for);
    EXEC_TEST(map_push);
    EXEC_TEST(map_shift);

    return result;
}

int main(int argc, char **argv) {
    TEST_FUNC(list());
}
