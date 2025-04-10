
#ifndef _CORO_H
#define _CORO_H

#include "future.h"
#include "hashtable.h"

/* Coroutine states. */
typedef enum {
    CORO_EVENT_DEAD = RAII_ERR, /* The coroutine has ended it's Event Loop routine, is uninitialized or deleted. */
    CORO_DEAD, /* The coroutine is uninitialized or deleted. */
    CORO_NORMAL,   /* The coroutine is active but not running (that is, it has switch to another coroutine, suspended). */
    CORO_RUNNING,  /* The coroutine is active and running. */
    CORO_SUSPENDED, /* The coroutine is suspended (in a startup, or it has not started running yet). */
    CORO_EVENT, /* The coroutine is in an Event Loop callback. */
    CORO_ERRED, /* The coroutine has erred. */
} coro_states;

typedef enum {
    CORO_RUN_NORMAL,
    CORO_RUN_MAIN,
    CORO_RUN_THRD,
    CORO_RUN_SYSTEM,
    CORO_RUN_EVENT,
    CORO_RUN_ASYNC,
} run_mode;

typedef struct routine_s routine_t;
typedef struct hash_s *waitgroup_t;
typedef int (*coro_sys_func)(u32, void_t);
typedef struct awaitable_s {
    raii_type type;
    rid_t cid;
    waitgroup_t wg;
} _awaitable_t, *awaitable_t;

#define USE_UCONTEXT
#if ((defined(__clang__) || defined(__GNUC__)) && defined(__i386__)) || (defined(_MSC_VER) && defined(_M_IX86))
#   undef USE_UCONTEXT
#elif ((defined(__clang__) || defined(__GNUC__)) && defined(__amd64__)) || (defined(_MSC_VER) && defined(_M_AMD64))
#   undef USE_UCONTEXT
#elif (defined(__clang__) || defined(__GNUC__)) && (defined(__arm__) || defined(__aarch64__)|| defined(__powerpc64__) || defined(__ARM_EABI__) || defined(__riscv))
#   undef USE_UCONTEXT
#endif

#if defined(USE_UCONTEXT)
#define _BSD_SOURCE
#if __APPLE__ && __MACH__
#   include <sys/ucontext.h>
#elif defined(_WIN32) || defined(_WIN64) || defined(_MSC_VER)
#   include <windows.h>
#   if defined(_X86_)
#       define DUMMYARGS
#   else
#       define DUMMYARGS long dummy0, long dummy1, long dummy2, long dummy3,
#   endif
    typedef struct __stack {
        void *ss_sp;
        size_t ss_size;
        int ss_flags;
    } stack_t;

    typedef CONTEXT mcontext_t;
    typedef unsigned long __sigset_t;

    typedef struct __ucontext {
        unsigned long int	uc_flags;
        struct __ucontext *uc_link;
        stack_t				uc_stack;
        mcontext_t			uc_mcontext;
        __sigset_t			uc_sigmask;
    } ucontext_t;

    C_API int getcontext(ucontext_t *ucp);
    C_API int setcontext(const ucontext_t *ucp);
    C_API int makecontext(ucontext_t *, void (*)(), int, ...);
    C_API int swapcontext(routine_t *, const routine_t *);
#else
#   include <ucontext.h>
#endif
#endif

#ifndef INTERRUPT_MODE
#define INTERRUPT_MODE 2
#endif

#ifdef USE_CORO
#define main(...)                       \
    main(int argc, char** argv) {       \
        return raii_main(argc, argv);   \
    }                                   \
    int coro_main(__VA_ARGS__)
#else
#define main(...)                       \
    main(int argc, char** argv) {       \
        return coro_main(argc, argv);   \
    }                                   \
    int coro_main(__VA_ARGS__)
#endif

/* Cast `val` to generic union value_t storage type */
#define $$$(val) ((value_t *)(val))

#ifndef CORO_STACK_SIZE
/* Stack size when creating a coroutine. */
#   define CORO_STACK_SIZE (16 * 1024)
#endif

#ifndef SCRAPE_SIZE
#   define SCRAPE_SIZE (2 * 64)
#endif

/* Number used only to assist checking for stack overflows. */
#define CORO_MAGIC_NUMBER 0x7E3CB1A9

/* In alignas(a), 'a' should be a power of two that is at least the type's
   alignment and at most the implementation's alignment limit.  This limit is
   2**13 on MSVC. To be portable to MSVC through at least version 10.0,
   'a' should be an integer constant, as MSVC does not support expressions
   such as 1 << 3.

   The following C11 requirements are NOT supported on MSVC:

   - If 'a' is zero, alignas has no effect.
   - alignas can be used multiple times; the strictest one wins.
   - alignas (TYPE) is equivalent to alignas (alignof (TYPE)).
*/
#if !defined(alignas)
  #if defined(__STDC__) /* C Language */
    #if defined(_MSC_VER) /* Don't rely on MSVC's C11 support */
      #define alignas(bytes) __declspec(align(bytes))
    #elif __STDC_VERSION__ >= 201112L /* C11 and above */
      #include <stdalign.h>
    #elif defined(__clang__) || defined(__GNUC__) /* C90/99 on Clang/GCC */
      #define alignas(bytes) __attribute__ ((aligned (bytes)))
    #else /* Otherwise, we ignore the directive (user should provide their own) */
      #define alignas(bytes)
    #endif
  #elif defined(__cplusplus) /* C++ Language */
    #if __cplusplus < 201103L
      #if defined(_MSC_VER)
        #define alignas(bytes) __declspec(align(bytes))
      #elif defined(__clang__) || defined(__GNUC__) /* C++98/03 on Clang/GCC */
        #define alignas(bytes) __attribute__ ((aligned (bytes)))
      #else /* Otherwise, we ignore the directive (unless user provides their own) */
        #define alignas(bytes)
      #endif
    #else /* C++ >= 11 has alignas keyword */
      /* Do nothing */
    #endif
  #endif /* = !defined(__STDC_VERSION__) && !defined(__cplusplus) */
#endif

/*[amd64, arm, ppc, x86]:
   by default, coro_swap_function is marked as a text (code) section
   if not supported, uncomment the below line to use mprotect instead */
/* #define MPROTECT */

/*[amd64]:
   Win64 only: provides a substantial speed-up, but will thrash XMM regs
   do not use this unless you are certain your application won't use SSE */
/* #define NO_SSE */

/*[amd64, aarch64]:
   Win64 only: provides a small speed-up, but will break stack unwinding
   do not use this if your application uses exceptions or setjmp/longjmp */
/* #define NO_TIB */

#if defined(__clang__)
  #pragma clang diagnostic ignored "-Wparentheses"

  /* placing code in section(text) does not mark it executable with Clang. */
  #undef  MPROTECT
  #define MPROTECT
#endif

#if (defined(__clang__) || defined(__GNUC__)) && defined(__i386__)
  #define fastcall __attribute__((fastcall))
#elif defined(_MSC_VER) && defined(_M_IX86)
  #define fastcall __fastcall
#else
  #define fastcall
#endif

#if defined(_MSC_VER)
  #undef  MPROTECT
  #define MPROTECT
  #define section(name) __declspec(allocate("." #name))
#elif defined(__APPLE__)
  #define section(name) __attribute__((section("__TEXT,__" #name)))
#else
  #define section(name) __attribute__((section("." #name "#")))
#endif

#ifdef USE_VALGRIND
#include <valgrind/valgrind.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif
    /* Creates an coroutine of given function with arguments,
    and add to schedular, same behavior as Go. */
    C_API rid_t go(callable_t, u64, ...);

    /* Returns results of an completed coroutine, by `result id`, will panic,
    if called before `waitfor` returns. */
    C_API value_t result_for(rid_t);

    /* Explicitly give up the CPU for at least ms milliseconds.
    Other tasks continue to run during this time. */
    C_API u32 sleepfor(u32 ms);

    /* Creates an coroutine of given function with argument,
    and immediately execute. */
    C_API void launch(func_t, u64, ...);

    /* Yield execution to another coroutine and reschedule current. */
    C_API void yielding(void);

    /* Defer execution `LIFO` of given function with argument,
    to when current coroutine exits/returns. */
    C_API void defer(func_t, void_t);

    /* Same as `defer` but allows recover from an Error condition throw/panic,
    you must call `catching` inside function to mark Error condition handled. */
    C_API void defer_recover(func_t, void_t);

    /* Compare `err` to current error condition of coroutine,
    will mark exception handled, if `true`. */
    C_API bool catching(string_t);

    /* Get current error condition string. */
    C_API string_t catch_message(void);

    /* Creates/initialize the next series/collection of coroutine's created to be part of `wait group`,
    same behavior of Go's waitGroups.

    All coroutines here behaves like regular functions, meaning they return values,
    and indicate a terminated/finish status.

    The initialization ends when `waitfor` is called, as such current coroutine will pause,
    and execution will begin and wait for the group of coroutines to finished. */
    C_API waitgroup_t waitgroup(void);
    C_API waitgroup_t waitgroup_ex(u32 capacity);

    /* Pauses current coroutine, and begin execution of coroutines in `wait group` object,
    will wait for all to finish.

    Returns `vector/array` of `results id`, accessible using `result_for` function. */
    C_API waitresult_t waitfor(waitgroup_t);

    C_API awaitable_t async(callable_t, u64, ...);
    C_API value_t await(awaitable_t);

    /* Check ~ptr~ and `free` using it's matching `type_of` ~cleanup~ function. */
    C_API void delete(void_t ptr);

    /* Collect coroutines with references preventing immediate cleanup. */
    C_API void coro_gc(routine_t *);
    C_API routine_t *coro_ref_current(void);
    C_API void coro_ref(routine_t *);
    C_API void coro_unref(routine_t *);

    C_API memory_t *coro_scope(void);
    C_API memory_t *get_coro_scope(routine_t *);
    C_API signed int get_coro_err(routine_t *);
    C_API void coro_err_set(routine_t *, signed int code);
    C_API void_t get_coro_data(routine_t *);
    C_API void coro_data_set(routine_t *, void_t data);
    C_API void_t get_coro_timer(routine_t *co);
    C_API void coro_timer_set(routine_t *co, void_t data);
    C_API value_t *get_coro_result(routine_t *);
    C_API routine_t *get_coro_context(routine_t *);
    C_API void coro_info(routine_t *, int pos);

    /* Sets the current coroutine's name.*/
    C_API void coro_name(char *fmt, ...);

    /* Check for at least `n` bytes left on the stack. If not present, panic/abort. */
    C_API void coro_stack_check(int);

    /* Suspends the execution of current coroutine, switch to scheduler. */
    C_API void coro_suspend(void);

    /* Return handle to current coroutine. */
    C_API routine_t *coro_active(void);

    /* Add coroutine to queue. */
    C_API void coro_enqueue(routine_t *t);

    /* Multithreading checker for available coroutines in `waitgroup`, if any,
    transfer from `global` run queue, to current thread `local` run queue.

    If `main/process thread` caller, will globally signal all child threads
    to start there execution, and assign coroutines. */
    C_API void coro_stealer(void);

    C_API value_t coro_await(callable_t, size_t, ...);
    C_API value_t coro_interrupt(callable_t, size_t, ...);
    C_API void_t coro_interrupt_erred(routine_t *, int);
    C_API void coro_interrupt_switch(routine_t *);
    C_API void coro_interrupt_complete(routine_t *, void_t result, ptrdiff_t plain, bool is_plain);
    C_API void coro_interrupt_result(routine_t *co, void_t data, ptrdiff_t plain, bool is_plain);
    C_API void coro_interrupt_finisher(routine_t *co, void_t result, ptrdiff_t plain,
                                       bool use_yield, bool halted, bool use_context, bool is_plain);
    C_API void coro_interrupt_setup(call_interrupter_t, call_t, call_timer_t, func_t, func_t);
    C_API void coro_interrupt_event(func_t, void_t, func_t);
    C_API void coro_interrupt_waitgroup_destroy(routine_t *);

    /* Check for coroutine completetion. */
    C_API bool coro_terminated(routine_t *);
    C_API void coro_halt_set(routine_t *);
    C_API void coro_halt_clear(routine_t *);

    C_API void_t interrupt_handle(void);
    C_API void_t interrupt_data(void);
    C_API i32 interrupt_code(void);
    C_API bits_t interrupt_bitset(void);
    C_API arrays_t interrupt_args(void);

    C_API void set_interrupt_handle(void_t);
    C_API void set_interrupt_data(void_t);
    C_API void set_interrupt_code(i32);
    C_API void set_interrupt_bitset(bits_t);
    C_API void set_interrupt_args(arrays_t);

    /* Print `current` coroutine internal data state, only active in `debug` builds. */
    C_API void coro_info_active(void);
    C_API void coro_yield_info(void);
    C_API signed int coro_err_code(void);
    C_API string_t coro_get_name(void);

    /* Set global coroutine `runtime` stack size, default: 16Kb,
    `coro_main` preset to `128Kb`, `8x 'CORO_STACK_SIZE'`. */
    C_API void coro_stacksize_set(u32);
    C_API bool coro_is_valid(void);

    /* Return the unique `result id` for the current coroutine. */
    C_API rid_t coro_id(void);
    C_API raii_deque_t *coro_pool_init(size_t queue_size);
    C_API int coro_start(coro_sys_func, u32 argc, void_t argv, size_t queue_size);

    /* This library provides its own ~main~,
    which call this function as an coroutine! */
    C_API int coro_main(int, char **);
    C_API int raii_main(int, char **);

    C_API void preempt_init(u32 usecs);
    C_API void preempt_disable(void);
    C_API void preempt_enable(void);
    C_API void preempt_stop(void);

    C_API coro_sys_func coro_main_func;
    C_API bool coro_sys_set;
#ifdef __cplusplus
}
#endif
#endif /* _CORO_H */
