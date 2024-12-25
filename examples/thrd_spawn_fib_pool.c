#include "raii.h"


void_t fib(args_t args) {
    int n = get_arg(args).integer;
    if (n < 2) {
        return thrd_value(n);
    } else {
        result_t x, y;
        future_t f = thrd_scope();

        x = thrd_spawn(fib, thrd_value(n - 1));
        y = thrd_spawn(fib, thrd_value(n - 2));

        RAII_HERE;
        thrd_sync(f);

        RAII_HERE;
        return thrd_value(thrd_result(x).integer + thrd_result(y).integer);
    }
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: fib <options> <n>\n");
        exit(1);
    }

    int n = atoi(argv[1]);
    thrd_init(25);
    future_t fut = thrd_scope();
    result_t results = thrd_spawn(fib, thrd_value(n));

    thrd_sync(fut);
    printf("Result: %d\n", thrd_result(results).integer);

    return 0;
}

