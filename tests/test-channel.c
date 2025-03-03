#define USE_CORO
#include "channel.h"
#include "test_assert.h"

void *test_receive(params_t arg) {
    channel_t c = arg->object;
    int i;
    ASSERT_EQU(type_of(c), RAII_CHANNEL);

    for (i = 0; i < 10; i++) {
        ASSERT_EQU(i, chan_recv(c).integer);
    }

    delete(c);
    return 0;
}

TEST(chan_send) {
    channel_t c = channel_buf(3);
    int i;

    go(test_receive, 1, c);
    for (i = 0; i < 10; i++) {
        ASSERT_EQ(1, chan_send(c, casting(i)));
    }

    return 0;
}

TEST(list) {
    int result = 0;

    EXEC_TEST(chan_send);
    return result;
}

int main(int argc, char **argv) {
    TEST_FUNC(list());
}
