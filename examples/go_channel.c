#define USE_CORO
#include "channel.h"

void_t sendData(params_t arg) {
    channel_t ch = arg->object;

    // data sent to the channel
    chan_send(ch, "Received. Send Operation Successful");
    puts("No receiver! Send Operation Blocked");

    return 0;
}

int main(int argc, char **argv) {
    // create channel
    channel_t ch = channel();
    defer(delete, ch);

    // function call with goroutine
    go(sendData, 1, ch);
    // receive channel data
    printf("%s\n", chan_recv(ch).char_ptr);

    return 0;
}
