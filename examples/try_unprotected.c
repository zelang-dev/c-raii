#include "raii.h"

static int segfault(int a, int b) {
    return *(int *)0 = b;
}

static void pfree(void *p) {
    printf("freeing protected memory pointed by '%s'\n", (char *)p);
    free(p);
}

int main(int argc, char **argv) {
    try {
        char *p1 = 0;
        protected(p1, pfree);
        char *p2 = 0;
        protected(p2, pfree);

        p1 = malloc(sizeof("p1"));
        if (p1)
            strcpy(p1, "p1");

        p2 = malloc(sizeof("p2"));
        if (p2)
            strcpy(p2, "p2");

        free(p1);
        free(p2);
        unprotected(p1); /* unprotected p2 too */

        segfault(1, 0);
        printf("never reached\n");
        (void)ex_protected_p2;
    } catch_any {
        printf("catch_any: exception %s (%s:%d) caught\n", err.name, err.file, err.line);
    } 

    return 0;
}
