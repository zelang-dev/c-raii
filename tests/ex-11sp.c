#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "../exception.h"

enum { n_loop = 100000000 };

static void test(void)
{
  char *p = 0; PRT(p,free);

  UNPRT(p);
}

int main(void)
{
  double t0,t1;
  int i;

  t0 = clock();

  for (i = 0; i<n_loop; i++)
    test();

  t1 = clock();
  printf("timing: %9d loops = %.2f sec\n", n_loop, (t1-t0)/CLOCKS_PER_SEC);

  return 0;
}
