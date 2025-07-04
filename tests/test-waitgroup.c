#define USE_CORO
#include "raii.h"
#include "test_assert.h"

void *worker(params_t args) {
    int id = coro_id();
    ASSERT_EQU(coro_id(), args[0].integer + 1);

    sleepfor(1000);
    if (id == 4)
        return casting(32);
    else if (id == 3)
        return "hello world";

    return 0;
}

TEST(waitfor) {
    int cid[10], i;

    waitgroup_t wg = waitgroup();
    ASSERT_TRUE(is_type(wg, RAII_HASH));
    for (i = 0; i < 10; i++) {
        cid[i] = go(worker, 1, i);
        ASSERT_EQ(cid[i], i + 1);
    }
    ASSERT_TRUE((hash_count(wg) == 10));
    waitresult_t wgr = waitfor(wg);
    ASSERT_TRUE(is_array(wgr));
    ASSERT_TRUE(($size(wgr) == 2));

    ASSERT_TRUE((32 == result_for(cid[3]).integer));
    ASSERT_STR("hello world", result_for(cid[2]).char_ptr);

    return 0;
}

TEST(list) {
    int result = 0;

    EXEC_TEST(waitfor);

    return result;
}

int main(int argc, char **argv) {
    TEST_FUNC(list());
}
