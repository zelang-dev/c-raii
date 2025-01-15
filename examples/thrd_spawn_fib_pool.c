#include "raii.h"


void_t fib(args_t args) {
    int n = args[0].integer;
    if (n < 2) {
        return $(n);
    } else {
        rid_t x, y;
        future_t f = thrd_scope();

        x = thrd_spawn(fib, 1, (n - 1));
        y = thrd_spawn(fib, 1, (n - 2));

        thrd_sync(f);

        return $(thrd_result(x).integer + thrd_result(y).integer);
    }
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: fib <options> <n>\n");
        exit(1);
    }

    int n = atoi(argv[1]);
    coro_thread_init(25);
    future_t fut = thrd_scope();
    rid_t results = thrd_spawn(fib, 1, (n));

    thrd_sync(fut);
    printf("Result: %d\n", thrd_result(results).integer);

    return 0;
}

