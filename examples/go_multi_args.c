#define USE_CORO
#include "raii.h"

void *worker(params_t args) {
    int i, count = args[0].integer;
    char *text = args[1].char_ptr;

    for (i = 0; i < count; i++) {
        printf("%s\n", text);
        sleepfor(10);
    }
    return 0;
}

int main(int argc, char **argv) {
    go(worker, 2, 4, "a");
    go(worker, 2, 2, "b");
    go(worker, 2, 3, "c");

    sleepfor(100);

    return 0;
}
