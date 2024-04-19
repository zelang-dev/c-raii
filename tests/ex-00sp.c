#include <stdio.h>
#include <time.h>
#include "../exception.h"

enum { n_loop = 25000000 };

EXCEPTION(division_by_zero);

static void test(void)
{
  TRY {
  }
  CATCH(bad_alloc) {
  }
  CATCH(division_by_zero) {
  }
  FINALLY() {
  }
  ENDTRY
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
