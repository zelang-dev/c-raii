#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "../exception.h"

enum { n_th = 4 };

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

int main(void)
{
  pthread_attr_t attr;
  pthread_t th[n_th];
  int i;

  ex_signal_std();

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
