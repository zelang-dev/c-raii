/*
An expanded/converted version of `Golang` example from https://gobyexample.com/waitgroups

package main

import (
    "fmt"
    "sync"
    "time"
)

func worker(id int) {
    fmt.Printf("Worker %d starting\n", id)

    time.Sleep(time.Second)

    fmt.Printf("Worker %d done\n", id)
}

func main() {

    var wg sync.WaitGroup

    for i := 1; i <= 5; i++ {
        wg.Add(1)

        i := i

        go func() {
            defer wg.Done()
            worker(i)
        }()
    }

    wg.Wait()
*/

#define USE_CORO
#include "raii.h"

void *worker(params_t args) {
    int wid = args[0].integer + 1;
    int r = 32, id = coro_id();

    printf("Worker %d starting\n", wid);
    coro_info_active();
    sleepfor(1000);
    printf("Worker %d done\n", wid);
    coro_info_active();

    if (id == 4)
        return casting(r);
    else if (id == 3)
        return "hello world";

    return 0;
}

int main(int argc, char **argv) {
    int cid[50], i;

    waitgroup_t wg = waitgroup();
    for (i = 0; i < 50; i++) {
        cid[i] = go(worker, 1, i);
    }
    waitresult_t wgr = waitfor(wg);

    printf("\nWorker # %d returned: %d\n", cid[3], result_for(cid[3]).integer);
    printf("Worker # %d returned: %s\n", cid[2], result_for(cid[2]).char_ptr);
    return 0;
}
