#include "raii.h"

static int segfault(int a, int b) {
    return *(int *)0 = b;
}

int main(int argc, char **argv) {
    try {
        segfault(1, 0);
        printf("never reached\n");
    } catch_any{
        printf("catch_any: exception %s (%s:%d) caught\n", err.name, err.file, err.line);
    } _tried;

    return 0;
}
