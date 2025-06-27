#define USE_CORO
#include "raii.h"
#include "test_assert.h"

void_t hello_world(params_t args) {
    yielding("Hello");
    return "world";
}

TEST(yield_for) {
    generator_t gen = generator(hello_world, 0);
    ASSERT_TRUE(is_type(gen, RAII_YIELD));
    ASSERT_STR("Hello", yield_for(gen).char_ptr);
    ASSERT_NULL(yield_for(gen).object);
    ASSERT_STR("world", result_for(yield_id()).char_ptr);

    return 0;
}

TEST(list) {
    int result = 0;

    EXEC_TEST(yield_for);

    return result;
}

int main(int argc, char **argv) {
    TEST_FUNC(list());
}
