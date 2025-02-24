/*
An converted version of `Golang` example from https://www.golinuxcloud.com/goroutines-golang/

package main

import (
   "fmt"
   "time"
)

func main() {
   fmt.Println("Start of main Goroutine")
   go greetings("John")
   go greetings("Mary")
   time.Sleep(time.Second * 10)
   fmt.Println("End of main Goroutine")

}

func greetings(name string) {
   for i := 0; i < 3; i++ {
       fmt.Println(i, "==>", name)
       time.Sleep(time.Millisecond)
  }
}
*/

#define USE_CORO
#include "raii.h"

void *greetings(params_t args) {
    char *name = args[0].char_ptr;
    int i;
    for (i = 0; i < 3; i++) {
        printf("%d ==> %s\n", i, name);
        sleepfor(1);
    }
    return 0;
}

int main(int argc, char **argv) {
    puts("Start of main Goroutine");
    go(greetings, 1, "John");
    go(greetings, 1, "Mary");
    sleepfor(1000);
    puts("\nEnd of main Goroutine");
    return 0;
}
