/*
An converted version of `Golang` example from https://www.programiz.com/golang/channel

package main
import "fmt"

func main() {
  // create channel
  ch := make(chan string)

  // function call with goroutine
  go sendData(ch)

  // receive channel data
  fmt.Println(<-ch)
}

func sendData(ch chan string) {

  // data sent to the channel
   ch <- "Received. Send Operation Successful"
   fmt.Println("Message sent! Send Operation Complete")
}
*/

#define USE_CORO
#include "channel.h"

void_t sendData(params_t arg) {
    channel_t ch = arg->object;

    // data sent to the channel
    chan_send(ch, "Received. Send Operation Successful");
    puts("Message sent! Send Operation Complete");

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
