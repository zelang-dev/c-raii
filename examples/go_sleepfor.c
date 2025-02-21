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
