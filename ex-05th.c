#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "exception.h"

enum { n_th = 4 };

EXCEPTION(division_by_zero);

static int idiv(int a, int b)
{
  if (b == 0)
    THROW(division_by_zero);

  return a/b;
}

static void* test(void *_)
{
  (void)_;

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
    if (ex)
      printf("finally: try failed -> %s (%s:%d)\n", ex, ex_file, ex_line);
    else
      printf("finally: try succeeded\n");
  }
  ENDTRY

  return 0;
}

int main(void)
{
  pthread_attr_t attr;
  pthread_t th[n_th];
  int i;

  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_JOINABLE);

  for (i = 0; i < n_th; i++) {
    /* fprintf(stderr, "Starting thread %d\n", i); */
    if (pthread_create(&th[i],&attr,test,0)) {
      fprintf(stderr, "unable to create thread %d\n", i);
      exit(EXIT_FAILURE);
    }
  }

  for (i = 0; i < n_th; i++) {
    /* fprintf(stderr, "Waiting thread %d\n", i); */
    if (pthread_join(th[i],NULL)) {
      fprintf(stderr, "unable to join thread %d\n", i);
      exit(EXIT_FAILURE);
    }
  }

  pthread_attr_destroy(&attr);

  return 0;
}
