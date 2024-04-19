#include <stdio.h>
#include "exception.h"

EXCEPTION(division_by_zero);

static int idiv(int a, int b)
{
  if (b == 0)
    THROW(division_by_zero);

  return a/b;
}

int main(void)
{
  idiv(1,0);
  printf("never reached\n");

  return 0;
}
