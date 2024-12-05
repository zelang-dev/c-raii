#include "raii.h"

void_t fib(args_t args) {
    int n = get_arg(args).integer;
    if (n < 2) {
        return thrd_data(n);
    } else {
        result_t x, y;
        future_t *f = thrd_scope();

        x = thrd_spawn(fib, thrd_value(n - 1));
        y = thrd_spawn(fib, thrd_value(n - 2));

        thrd_sync(f);

        return thrd_value(thrd_result(x).integer + thrd_result(y).integer);
    }
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: fib <options> <n>\n");
        exit(1);
    }

    result_t results;
    future_t *fut = thrd_scope();
    int n = atoi(argv[1]);

    results = thrd_spawn(fib, thrd_data(n - 2));

    thrd_sync(fut);
    printf("Result: %d\n", thrd_result(results).integer);
    thrd_destroy(fut);

    return 0;
}

