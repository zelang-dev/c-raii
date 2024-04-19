#include <stdio.h>
#include "../exception.h"

EXCEPTION(division_by_zero);

static int idiv(int a, int b)
{
  if (b == 0)
    THROW(division_by_zero);

  return a/b;
}

int main(void)
{
  TRY {
    idiv(1,0);
    printf("never reached\n");
  }
  CATCH(bad_alloc) {
    printf("catch: exception %s (%s:%d) caught\n", ex, ex_file, ex_line);
  }
  CATCH(division_by_zero) {
    printf("catch: exception %s (%s:%d) caught\n", ex, ex_file, ex_line);
  }
  FINALLY() {
    if (ex) {
      printf("finally: try failed -> %s (%s:%d)\n", ex, ex_file, ex_line);
      RETHROW();
      printf("never reached\n");
    } else
      printf("finally: try succeeded\n");
  }
  ENDTRY

  return 0;
}
