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
    } catch (division_by_zero) {
        printf("catch: exception %s (%s:%d) caught\n", err.name, err.file, err.line);
    } 

    return 0;
}
