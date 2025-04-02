#include "raii.h"

static void pfree(void *p) {
    printf("freeing protected memory pointed by '%s'\n", (char *)p);
    free(p);
}

int main(int argc, char **argv) {
    try {
        int a = 1;
        int b = 0.00000;
        char *p = 0;
        p = malloc_full(raii_init(), 3, pfree);
        strcpy(p, "p");

        *(int *)0 = 0;
        printf("never reached\n");
        printf("%d\n", (a / b));
        raise(SIGINT);
    } catch_if {
        if (caught(bad_alloc))
            printf("catch: exception %s (%s:%d) caught\n", err.name, err.file, err.line);
        else if (caught(division_by_zero))
            printf("catch: exception %s (%s:%d) caught\n", err.name, err.file, err.line);
        else if (caught(sig_fpe))
            printf("catch: exception %s (%s:%d) caught\n", err.name, err.file, err.line);
        else if (caught(sig_ill))
            printf("catch: exception %s (%s:%d) caught\n", err.name, err.file, err.line);
        else if (caught(sig_int))
            printf("catch: exception %s (%s:%d) caught\n", err.name, err.file, err.line);
        else if (caught(sig_segv))
            printf("catch: exception %s (%s:%d) caught\n", err.name, err.file, err.line);
    } finally {
        if (err.is_caught) {
            printf("finally: try failed, but succeeded to catch -> %s (%s:%d)\n", err.name, err.file, err.line);
        } else {
            printf("finally: try failed to `catch()`\n");
            ex_backtrace(err.backtrace);
        }
    }

    return 0;
}