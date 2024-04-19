#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include "exception.h"

enum { n_th = 4, n_loop = 25000000 };

EXCEPTION(division_by_zero);

static void tst(void)
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

static void* test(void *_)
{
  int i;

  (void)_;

  for (i = 0; i<n_loop/n_th; i++)
    tst();

  return 0;
}

int main(void)
{
  pthread_attr_t attr;
  pthread_t th[n_th];
  double t0,t1;
  int i;

  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_JOINABLE);

  t0 = clock();
  
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

  t1 = clock();
  printf("timing: %9d loops = %.2f sec\n", n_loop, (t1-t0)/CLOCKS_PER_SEC);

  pthread_attr_destroy(&attr);

  return 0;
}
