#include "raii.h"

static int div_err(int a, int b) {
    if (b == 0)
        throw(division_by_zero);

    return a / b;
}

int main(int argc, char **argv) {
    try {
        div_err(1, 0);
        printf("never reached\n");
    } catch (out_of_memory) {
        printf("catch: exception %s (%s:%d) caught\n", err.name, err.file, err.line);
    } finally {
        if (err.name)
            printf("finally: try failed -> %s (%s:%d)\n", err.name, err.file, err.line);
        else
            printf("finally: try succeeded\n");
    }

    return 0;
}
