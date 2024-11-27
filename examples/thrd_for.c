#include "raii.h"

void *is_prime(args_t arg) {
    int i, x = get_args(arg, 0).integer;
    printf("\nThread #%zx, received: %d\n", thrd_self(), x);
    for (i = 2; i < x; ++i) if (x % i == 0) return thrd_value(false);

    return thrd_value(true);
}

void_t check_primes(void_t result, size_t id, values_type iter) {
    if (iter.boolean)
        printf("Number %zu is prime.\n", id);
    else
        printf("Number %zu is not prime.\n", id);

    return iter.object;
}

int main(int argc, char **argv) {
    values_type result[1];
    int prime = 194232491;
    future_t *fut = thrd_for(is_prime, 4, "d", prime);

    if (!thrd_is_finish(fut))
        printf("checking...\n");

    result->object = thrd_then(check_primes, thrd_sync(fut), result).object;

    return 0;
}
