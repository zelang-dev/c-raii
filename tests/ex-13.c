#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../exception.h"

EXCEPTION(division_by_zero);

static int idiv(int a, int b)
{
  if (b == 0)
    THROW(division_by_zero);

  return a/b;
}

static void pfree(void *p)
{
  printf("freeing protected memory pointed by '%s'\n", (char*)p);
  free(p);
}

int main(void)
{
  TRY {
    char *p1 = 0; PRT(p1,pfree);
    char *p2 = 0; PRT(p2,pfree);

    p1 = malloc(sizeof("p1"));
    if (p1) strcpy(p1,"p1");

    p2 = malloc(sizeof("p2"));
    if (p2) strcpy(p2,"p2");

    free(p1); free(p2); UNPRT(p1); /* unprotect p2 too */

    idiv(1,0);
    printf("never reached\n");
  }
  CATCH_ANY() {
    printf("catch_any: exception %s (%s:%d) caught\n", ex, ex_file, ex_line);
  }
  ENDTRY

  return 0;
}
