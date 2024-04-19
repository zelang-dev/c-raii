#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "../exception.h"

enum { n_th = 4 };

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

static void* test(void *_)
{
  (void)_;

  TRY {
    char *p = 0; PRT(p,pfree);

    p = malloc(sizeof("p"));
    if (p) strcpy(p,"p");

    free(p);

    p = malloc(sizeof("p2")); /* still protected */
    if (p) strcpy(p,"p2");

    idiv(1,0);
    printf("never reached\n");

    free(p); UNPRT(p);
  }
  CATCH_ANY() {
    printf("catch_any: exception %s (%s:%d) caught\n", ex, ex_file, ex_line);
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
