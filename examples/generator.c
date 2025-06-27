
#define USE_CORO
#include "raii.h"

void_t fibonacci_coro(params_t args) {
    /* Retrieve max value. */
    unsigned long max = args[0].u_long;
    unsigned long m = 1;
    unsigned long n = 1;

    while (1) {
        /* Yield the next Fibonacci number. */
        yielding(casting(m));

        unsigned long tmp = m + n;
        m = n;
        n = tmp;
        if (m >= max)
            break;
    }

    /* Yield the last Fibonacci number. */
    yielding(casting(m));

    return "hello world";
}

int main(int argc, char **argv) {
    /* Set storage. */
    unsigned long maximum = 1000000000;

    /* Create the coroutine. */
    generator_t gen = generator(fibonacci_coro, 1, casting(maximum));

    int counter = 1;
    unsigned long ret = 0;
    while (ret < maximum) {
        /* Resume the coroutine. */
        /* Retrieve storage set in last coroutine yield. */
        ret = yield_for(gen).u_long;

        printf("fib %d = %li\t\n", counter, ret);
        counter = counter + 1;
    }

    if (is_empty(yield_for(gen).object))
        printf("\n\n%s\n", result_for(yield_id()).char_ptr);

    return 0;
}
