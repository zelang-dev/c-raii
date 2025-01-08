/*
This benchmark based off **GoLang** version at https://github.com/pkolaczk/async-runtimes-benchmarks/blob/main/go/main.go

```go
package main

import (
    "fmt"
    "os"
    "strconv"
    "sync"
    "time"
)

func main() {
    numRoutines := 100000
    if len(os.Args) > 1 {
        n, err := strconv.Atoi(os.Args[1])
        if err == nil {
            numRoutines = n
        }
    }

    var wg sync.WaitGroup
    for i := 0; i < numRoutines; i++ {
    wg.Add(1)
    go func() {
        defer wg.Done()
        time.Sleep(10 * time.Second)
    }()
    }
    wg.Wait()
    fmt.Println("All goroutines finished.")
}
```
*/
#define USE_CORO
#include "raii.h"

void *func(params_t args) {
    sleepfor(10 * time_Second);
    return 0;
}

int main(int argc, char **argv) {
    u32 numRoutines = 100000, i;
    if (argc > 1)
        numRoutines = (u32)atoi(argv[1]);

    waitgroup_t wg = waitgroup();
    for (i = 0; i < numRoutines; i++) {
        go(func, 0);
    }
    waitfor(wg);

    printf("\nAll coroutines finished.\n");
    return 0;
}
