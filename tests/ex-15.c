#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* #include <signal.h> */
#include "exception.h"

static void pfree(void *p)
{
  printf("freeing protected memory pointed by '%s'\n", (char*)p);
  free(p);
}

int main(void)
{
  /*
    extern const char EX_NAME(sig_segv)[];
    ex_signal(SIGSEGV,EX_NAME(sig_segv));
  */
  ex_signal_std();

  TRY {
    char *p = 0; PRT(p,pfree);

    p = malloc(sizeof("p"));
    if (p) strcpy(p,"p");

    *(int*)0 = 0;
    printf("never reached\n");

    free(p); UNPRT(p);
  }
  CATCH(sig_segv) {
    printf("catch: exception %s (%s:%d) caught\n", ex, ex_file, ex_line);
  }
  CATCH_ANY() {
    printf("catch_any: exception %s (%s:%d) caught\n", ex, ex_file, ex_line);
  }
  ENDTRY

  return 0;
}
