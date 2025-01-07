#define USE_CORO
#include "raii.h"
#include "test_assert.h"

void *worker(params_t args) {
    int id = coro_id();
    ASSERT_EQ(coro_id(), args[0].integer + 1);

    sleepfor(1000);
    if (id == 4)
        return $$$(32);
    else if (id == 3)
        return "hello world";

    return 0;
}

TEST(waitfor) {
    int cid[5], i;

    waitgroup_t wg = waitgroup();
    ASSERT_TRUE(is_array(wg));
    for (i = 0; i < 5; i++) {
        cid[i] = go_for(worker, 1, i);
        ASSERT_EQ(cid[i], i + 1);
    }
    ASSERT_TRUE(($size(wg) == 5));
    waitresult_t wgr = waitfor(wg);
    ASSERT_TRUE(is_array(wgr));
    ASSERT_TRUE(($size(wgr) == 2));

    ASSERT_TRUE((wgr[1].integer == get_result(cid[3]).integer));
    ASSERT_STR(wgr[0].char_ptr, get_result(cid[2]).char_ptr);

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
