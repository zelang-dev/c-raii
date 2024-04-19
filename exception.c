/*
 o---------------------------------------------------------------------o
 |
 | Exception in C
 |
 | Copyright (c) 2001+ Laurent Deniau, laurent.deniau@cern.ch
 |
 | For more information, see:
 | http://cern.ch/laurent.deniau/oopc.html
 |
 o---------------------------------------------------------------------o
 |
 | Exception in C is free software; you can redistribute it and/or
 | modify it under the terms of the GNU Lesser General Public License
 | as published by the Free Software Foundation; either version 2.1 of
 | the License, or (at your option) any later version.
 |
 | The C Object System is distributed in the hope that it will be
 | useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 | of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 | Lesser General Public License for more details.
 |
 | You should have received a copy of the GNU Lesser General Public
 | License along with this library; if not, write to the Free
 | Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 | Boston, MA 02110-1301 USA
 |
 o---------------------------------------------------------------------o
 |
 | $Id$
 |
*/

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "exception.h"

/* Some common exception */
EXCEPTION(bad_exit );
EXCEPTION(bad_alloc);
EXCEPTION(bad_type );

static __thread struct ex_cxt cxt0;
__thread struct ex_cxt *ex_cxt = 0;

static void
unwind_stack(struct ex_cxt *cxt)
{
  struct ex_prt *p = cxt->stk;

  cxt->unstk = 1;

  while(p) {
    if (*p->ptr) p->fun(*p->ptr);
    p = p->nxt;
  }

  cxt->unstk = 0;
  cxt->stk   = 0;
}

void
ex_init(void)
{
  ex_cxt = &cxt0;
}

int
ex_uncaught_exception(void)
{
  if (!ex_cxt) ex_init();

  return ex_cxt->unstk;
}

void
ex_terminate(void)
{
  fflush(stdout);
  if (ex_uncaught_exception()) {
    fprintf(stderr,
            "EX-lib: exception %s thrown at (%s:%d) during stack unwinding "
            "leading to an undefined behavior\n",
            ex_cxt->ex, ex_cxt->file, ex_cxt->line);
    abort();
  } else {
    fprintf(stderr,
            "EX-lib: exiting with uncaught exception %s thrown at (%s:%d)\n",
            ex_cxt->ex, ex_cxt->file, ex_cxt->line);
    exit(EXIT_FAILURE);
  }
}

void
ex_throw(const char* ex, const char* file, int line)
{
  struct ex_cxt *cxt = ex_cxt;

  if (!cxt) {
    ex_init();
    cxt = ex_cxt;
  }

  cxt->ex   = ex;
  cxt->file = file;
  cxt->line = line;

  if (cxt->unstk)
    ex_terminate();

  unwind_stack(cxt);

  if (cxt == &cxt0)
    ex_terminate();

  ex_lngjmp(cxt->buf, cxt->st | ex_throw_st);
}

/* --------------------------------------------------------------------------*/

/* Some signal exception */
EXCEPTION(sig_abrt);
EXCEPTION(sig_alrm);
EXCEPTION(sig_bus );
EXCEPTION(sig_fpe );
EXCEPTION(sig_ill );
EXCEPTION(sig_int );
EXCEPTION(sig_quit);
EXCEPTION(sig_segv);
EXCEPTION(sig_term);

enum { max_ex_sig = 32 };

static struct {
  const char *ex;
  int sig;
} ex_sig[max_ex_sig];

static void
ex_handler(int sig)
{
  void (*old)(int) = signal(sig, ex_handler);
  const char *ex = 0;
  int i;

  for (i = 0; i < max_ex_sig; i++)
    if (ex_sig[i].sig == sig) {
      ex = ex_sig[i].ex;
      break;
    }

  if (old == SIG_ERR)
    fprintf(stderr,"EX-lib: cannot reinstall handler for signal no %d (%s)\n",
            sig, ex);

  ex_throw(ex,"unknown",0);
}

void (*
ex_signal(int sig, const char *ex))(int)
{
  void (*old)(int);
  int i;

  for (i = 0; i < max_ex_sig; i++)
    if (!ex_sig[i].ex || ex_sig[i].sig == sig)
      break;

  if (i == max_ex_sig) {
    fprintf(stderr,
            "EX-lib: cannot install exception handler for signal no %d (%s), "
            "too many signal exception handlers installed (max %d)\n",
            sig, ex, max_ex_sig);
    return SIG_ERR;
  }

  old = signal(sig, ex_handler);

  if (old == SIG_ERR)
    fprintf(stderr, "EX-lib: cannot install handler for signal no %d (%s)\n",
            sig, ex);
  else
    ex_sig[i].ex = ex, ex_sig[i].sig = sig;

  return old;
}

void
ex_signal_std(void)
{
#if SIGABRT
  ex_signal(SIGABRT, EX_NAME(sig_abrt));
#endif

#if SIGALRM
  ex_signal(SIGALRM, EX_NAME(sig_alrm));
#endif

#if SIGBUS
  ex_signal(SIGBUS , EX_NAME(sig_bus ));
#elif SIG_BUS
  ex_signal(SIG_BUS, EX_NAME(sig_bus ));
#endif

#if SIGFPE
  ex_signal(SIGFPE , EX_NAME(sig_fpe ));
#endif

#if SIGILL
  ex_signal(SIGILL , EX_NAME(sig_ill ));
#endif

#if SIGINT
  ex_signal(SIGINT , EX_NAME(sig_int ));
#endif

#if SIGQUIT
  ex_signal(SIGQUIT, EX_NAME(sig_quit));
#endif

#if SIGSEGV
  ex_signal(SIGSEGV, EX_NAME(sig_segv));
#endif

#if SIGTERM
  ex_signal(SIGTERM, EX_NAME(sig_term));
#endif
}
