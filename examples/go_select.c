/*
An expanded/converted version of `Golang` example from https://go.dev/tour/concurrency/5

package main

import "fmt"

func fibonacci(c, quit chan int) {
    x, y := 0, 1
    for {
        select {
        case c <- x:
            x, y = y, x+y
        case <-quit:
            fmt.Println("quit")
            return
        }
    }
}

func main() {
    c := make(chan int)
    quit := make(chan int)
    go func() {
        for i := 0; i < 10; i++ {
            fmt.Println(<-c)
        }
        quit <- 0
    }()
    fibonacci(c, quit)
}
*/

#define USE_CORO
#include "channel.h"

int fibonacci(channel_t c, channel_t quit) {
    int x = 0;
    int y = 1;
    for_select {
        _send(c, casting(x))
            unsigned long tmp = x + y;
            x = y;
            y = tmp;
        _else_if _recv(quit, na)
            puts("quit");
            return 0;
        _break;
    } selected;
}

void *func(params_t args) {
    channel_t c = args[0].object;
    channel_t quit = args[1].object;
    int i;

    defer(delete, c);
    for (i = 0; i < 10; i++) {
        printf("%d\n", chan_recv(c).integer);
    }
    chan_send(quit, 0);

    printf("\nChannel `quit` type is: %d, validity: %d\n", type_of(quit), is_instance(quit));

    delete(c);
    delete(quit);

    printf("Channel `quit` freed, validity: %d\n", is_instance(quit));
    printf("Channel `c` type is: %d accessed after freed!\n\n", type_of(c));
    delete(quit);

    return 0;
}

int main(int argc, char **argv) {
    vectors_t params = vector_for(nullptr, 2, channel(), channel());
    go(func, 2, params[0].object, params[1].object);
    return fibonacci(params[0].object, params[1].object);
}
