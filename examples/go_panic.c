#define USE_CORO
#include "raii.h"

int div_err(int x, int y) {
    return x / y;
}

int mul(int x, int y) {
    return x * y;
}

void func(void *arg) {
    if (catching(catch_message()))
        printf("panic occurred: %s\n", catch_message());
}

void divByZero(void *arg) {
    defer_recover(func, arg);
    printf("%d", div_err(1, 0));
}

int main(int argc, char **argv) {
    launch(divByZero, 0);
    printf("Although panicked. We recovered. We call mul() func\n");
    printf("mul func result: %d\n", mul(5, 10));
    return 1;
}
