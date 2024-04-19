#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "exception.h"

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
  char *p = 0; PRT(p,pfree);

  p = malloc(sizeof("p"));
  if (p) strcpy(p,"p");
  idiv(1,0);
  printf("never reached\n");

  free(p); UNPRT(p);

  return 0;
}
