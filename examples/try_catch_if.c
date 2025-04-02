#include "raii.h"

static int idiv(int a, int b) {
    if (b == 0)
        throw(division_by_zero);

    return a / b;
}

int main(int argc, char **argv){
    try {
        idiv(1, 0);
        printf("never reached\n");
    } catch_if {
        if (caught(bad_alloc)) {
            printf("catch: exception %s (%s:%d) caught\n", err.name, err.file, err.line);
        } else if (caught(division_by_zero)) {
            printf("catch: exception %s (%s:%d) caught\n", err.name, err.file, err.line);
        }
    } finally {
        if (err.name)
            printf("finally: try succeeded -> %s (%s:%d)\n", err.name, err.file, err.line);
        else
            printf("finally: try failed to `catch()`\n");
    }

    return 0;
}
