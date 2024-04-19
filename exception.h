#ifndef EXCEPTION_H
#define EXCEPTION_H

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

/* exception config
 */
#if defined(__GNUC__) && (!defined(_WIN32) || !defined(_WIN64))
    #define _GNU_SOURCE
#endif

#if defined(_MSC_VER)
    #if !defined(__cplusplus)
        #define __STDC__ 1
    #endif
#endif

#if !defined(thread_local) /* User can override thread_local for obscure compilers */
     /* Running in multi-threaded environment */
    #if defined(__STDC__) /* Compiling as C Language */
      #if defined(_MSC_VER) /* Don't rely on MSVC's C11 support */
        #define thread_local __declspec(thread)
      #elif __STDC_VERSION__ < 201112L /* If we are on C90/99 */
        #if defined(__clang__) || defined(__GNUC__) /* Clang and GCC */
          #define thread_local __thread
        #else /* Otherwise, we ignore the directive (unless user provides their own) */
          #define thread_local
        #endif
      #elif __APPLE__ && __MACH__
        #define thread_local __thread
      #else /* C11 and newer define thread_local in threads.h */
        #include <threads.h>
      #endif
    #elif defined(__cplusplus) /* Compiling as C++ Language */
      #if __cplusplus < 201103L /* thread_local is a C++11 feature */
        #if defined(_MSC_VER)
          #define thread_local __declspec(thread)
        #elif defined(__clang__) || defined(__GNUC__)
          #define thread_local __thread
        #else /* Otherwise, we ignore the directive (unless user provides their own) */
          #define thread_local
        #endif
      #else /* In C++ >= 11, thread_local in a builtin keyword */
        /* Don't do anything */
      #endif
    #endif
#endif

/* exception keywords
 */
#ifdef  EX_DISABLE_ALL
#define EX_DISABLE_EXCEPTION
#define EX_DISABLE_TRY
#define EX_DISABLE_CATCH
#define EX_DISABLE_CATCH_ANY
#define EX_DISABLE_FINALLY
#define EX_DISABLE_ENDTRY
#define EX_DISABLE_THROW
#define EX_DISABLE_THROWLOC
#define EX_DISABLE_RETHROW
#define EX_DISABLE_PRT
#define EX_DISABLE_UNPRT
#endif

#ifndef EX_DISABLE_EXCEPTION
#define EXCEPTION(E) EX_EXCEPTION(E)
#endif

#ifndef EX_DISABLE_TRY
#define TRY EX_TRY
#endif

#ifndef EX_DISABLE_CATCH
#define CATCH(E) EX_CATCH(E)
#endif

#ifndef EX_DISABLE_CATCH_ANY
#define CATCH_ANY() EX_CATCH_ANY()
#endif

#ifndef EX_DISABLE_FINALLY
#define FINALLY() EX_FINALLY()
#endif

#ifndef EX_DISABLE_ENDTRY
#define ENDTRY EX_ENDTRY
#endif

#ifndef EX_DISABLE_THROW
#define THROW(E) EX_THROW(E)
#endif

#ifndef EX_DISABLE_THROWLOC
#define THROWLOC(E,F,L) EX_THROWLOC(E,F,L)
#endif

#ifndef EX_DISABLE_RETHROW
#define RETHROW() EX_RETHROW()
#endif

#ifndef EX_DISABLE_PRT
#define PRT(P,F) EX_PRT(P,F)
#endif

#ifndef EX_DISABLE_UNPRT
#define UNPRT(P) EX_UNPRT(P)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* low-level api
 */
void   ex_throw(const char* ex, const char* file, int line);
int    ex_uncaught_exception(void);
void   ex_terminate(void);
void (*ex_signal(int sig, const char *ex))(int);
void   ex_signal_std(void);

/***********************************************************
 * Implementation
 */

#include <setjmp.h>

/* states
*/
enum { ex_try_st, ex_throw_st, ex_catch_st };

/* macros
 */
#define EX_EXCEPTION(E) \
        const char EX_NAME(E)[] = EX_STR(E)

#define EX_TRY                            \
  {                                       \
    /* local context */                   \
    struct ex_cxt ex_lcxt;                \
    if (!ex_cxt) ex_init();               \
    ex_lcxt.nxt   = ex_cxt;               \
    ex_lcxt.stk   = 0;                    \
    ex_lcxt.ex    = 0;                    \
    ex_lcxt.unstk = 0;                    \
    /* gobal context updated */           \
    ex_cxt = &ex_lcxt;                    \
    /* save jump location */              \
    ex_lcxt.st = ex_setjmp(ex_lcxt.buf);  \
    if (ex_lcxt.st == ex_try_st) {{

#define EX_CATCH(E)                       \
    }}                                    \
    if (ex_lcxt.st == ex_throw_st) {      \
      extern const char EX_NAME(E)[];     \
      if (ex_lcxt.ex == EX_NAME(E)) {     \
        EX_MAKE();                        \
        ex_lcxt.st = ex_catch_st;

#define EX_CATCH_ANY()                    \
    }}                                    \
    if (ex_lcxt.st == ex_throw_st) {{     \
      EX_MAKE();                          \
      ex_lcxt.st = ex_catch_st;

#define EX_FINALLY()                      \
    }} {{                                 \
      EX_MAKE();                          \
      /* gobal context updated */         \
      ex_cxt = ex_lcxt.nxt;

#define EX_ENDTRY                         \
    }}                                    \
    if (ex_cxt == &ex_lcxt)               \
      /* gobal context updated */         \
      ex_cxt = ex_lcxt.nxt;               \
    if ((ex_lcxt.st & ex_throw_st) != 0)  \
      EX_RETHROW();                       \
  }

#define EX_THROW(E) \
        EX_THROWLOC(E,__FILE__,__LINE__)

#define EX_RETHROW() \
        ex_throw(ex_lcxt.ex,ex_lcxt.file,ex_lcxt.line)

#define EX_THROWLOC(E,F,L)                  \
        do {                                \
          extern const char EX_NAME(E)[];   \
          ex_throw(EX_NAME(E),F,L);         \
        } while(0)

#define EX_MAKE() \
  const char* const ex      = ((void)ex     ,ex_lcxt.ex  ); \
  const char* const ex_file = ((void)ex_file,ex_lcxt.file); \
  const int         ex_line = ((void)ex_line,ex_lcxt.line)

/* some useful macros
*/
#define EX_CAT( a,b) EX_CAT_(a,b)
#define EX_CAT_(a,b) a ## b

#define EX_STR( a) EX_STR_(a)
#define EX_STR_(a) # a

#define EX_NAME(e)  EX_CAT(ex_exception_,e)
#define EX_PNAME(p) EX_CAT(ex_protected_,p)

/* context savings
*/
#ifdef sigsetjmp
#define ex_jmpbuf         sigjmp_buf
#define ex_setjmp(buf)    sigsetjmp (buf,1)
#define ex_lngjmp(buf,st) siglongjmp(buf,st)
#else
#define ex_jmpbuf         jmp_buf
#define ex_setjmp(buf)    setjmp (buf)
#define ex_lngjmp(buf,st) longjmp(buf,st)
#endif

/* types
*/
struct ex_cxt {
  struct ex_cxt* nxt;
  struct ex_prt* stk;
  const char* volatile ex;
  const char* volatile file;
  int volatile line;
  int volatile st;
  int unstk;
  ex_jmpbuf buf;
};

struct ex_prt {
  struct ex_prt* nxt;
  void (*fun)(void*);
  void** ptr;
};

/* extern declaration
*/
extern thread_local struct ex_cxt *ex_cxt;
extern void ex_init(void);

/* pointer protection
*/
#define EX_PRT(p,f) \
        struct ex_prt EX_PNAME(p) =	ex_prt(&EX_PNAME(p),&p,f)

#define EX_UNPRT(p) \
        (ex_cxt->stk = EX_PNAME(p).nxt)

static struct ex_prt
ex_prt(struct ex_prt *pp, void *p, void (*f)(void*))
{
  if (!ex_cxt) ex_init();
  pp->nxt = ex_cxt->stk;
  pp->fun = f;
  pp->ptr = p;
  ex_cxt->stk = pp;
  return *pp;
  (void)ex_prt;
}

#ifdef __cplusplus
}
#endif

#endif /* EXCEPTION_H */
