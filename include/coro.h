
#ifndef _CORO_H
#define _CORO_H

#if ((defined(__clang__) || defined(__GNUC__)) && defined(__i386__)) || (defined(_MSC_VER) && defined(_M_IX86))
#   undef USE_UCONTEXT
#   undef USE_SJLJ
#elif ((defined(__clang__) || defined(__GNUC__)) && defined(__amd64__)) || (defined(_MSC_VER) && defined(_M_AMD64))
#   undef USE_UCONTEXT
#   undef USE_SJLJ
#elif (defined(__clang__) || defined(__GNUC__)) && (defined(__arm__) || defined(__aarch64__) || defined(__ARM_EABI__))
#   undef USE_UCONTEXT
#   undef USE_SJLJ
#elif (defined(__clang__) || defined(__GNUC__)) && defined(__riscv)
#   undef USE_UCONTEXT
#   undef USE_SJLJ
#elif (defined(__clang__) || defined(__GNUC__)) && defined(__powerpc64__)
#   define USE_UCONTEXT
#   undef USE_SJLJ
#define PPC_MIN_STACK 0x10000lu
#define PPC_ALIGN(p, x) ((void_t)((uintptr_t)(p) & ~((x)-1)))
#define PPC_MIN_STACK_FRAME 0x20lu
#define STACK_PPC_ALIGN 0x10lu
#else
#   define USE_UCONTEXT
#   undef USE_SJLJ
#endif

#if defined(USE_SJLJ)
/* for sigsetjmp(), sigjmp_buf, and stack_t */
#define _POSIX_C_SOURCE 200809L
#include <signal.h>
#include <setjmp.h>
#endif

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
    CORO_RUN_INTERRUPT,
    CORO_RUN_ASYNC,
} run_mode;

typedef struct routine_s routine_t;
typedef struct coro_s coro_t;
typedef struct hash_s hash_t;
typedef hash_t *waitgroup_t;
typedef struct generator_s _generator_t;
typedef _generator_t *generator_t;
typedef int (*coro_sys_func)(u32, void_t);
typedef struct awaitable_s {
    raii_type type;
    rid_t cid;
    waitgroup_t wg;
} _awaitable_t, *awaitable_t;

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
    void_t ss_sp;
    size_t ss_size;
    int ss_flags;
} stack_t;

typedef CONTEXT mcontext_t;
typedef unsigned long __sigset_t;
typedef coro_t ucontext_t;

C_API int getcontext(ucontext_t *ucp);
C_API int setcontext(const ucontext_t *ucp);
C_API int makecontext(ucontext_t *, void (*)(), int, ...);
C_API int swapcontext(ucontext_t *, const ucontext_t *);
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

#ifndef CORO_STACK_SIZE
/* Stack size when creating a coroutine. */
#if defined(USE_UCONTEXT)
#   define CORO_STACK_SIZE (16 * 1024)
#else
#   define CORO_STACK_SIZE (16 * 1024)
#endif
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
    C_API void yield(void);

    /* Suspends the execution of current `Generator/Coroutine`, and passing ~data~.
    WILL PANIC if not an ~Generator~ function called in.
    WILL `yield` until ~data~ is retrived using `yield_for`. */
    C_API void yielding(void_t);

    /* Creates an `Generator/Coroutine` of given function with arguments,
    MUST use `yielding` to pass data, and `yield_for` to get data. */
    C_API generator_t generator(callable_t, u64, ...);

    /* Resume specified ~coroutine/generator~, returning data from `yielding`. */
    C_API value_t yield_for(generator_t);

    /* Return `generator id` in scope for last `yield_for` execution. */
    C_API rid_t yield_id(void);

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

    C_API bool is_coroutine(void_t ptr);
    C_API void coro_flag_set(routine_t *co);

    /* Collect coroutines with references preventing immediate cleanup. */
    C_API void coro_gc(routine_t *);
    C_API routine_t *coro_ref_current(void);
    C_API void coro_ref(routine_t *);
    C_API void coro_unref(routine_t *);

    C_API memory_t *coro_scope(void);
    C_API memory_t *get_coro_scope(routine_t *);

    C_API waitgroup_t coro_waitgroup(void);
    C_API waitgroup_t get_coro_waitgroup(routine_t *);

    C_API signed int get_coro_err(routine_t *);
    C_API void coro_err_set(routine_t *, signed int code);

    C_API void_t coro_data(void);
    C_API void_t get_coro_data(routine_t *);
    C_API void coro_data_set(routine_t *, void_t data);

    C_API value_t *get_coro_result(routine_t *);
    C_API routine_t *get_coro_context(routine_t *);
    C_API void coro_context_set(routine_t *, routine_t *);

    C_API generator_t coro_generator(void);
    C_API generator_t get_coro_generator(routine_t *co);
    C_API void coro_generator_set(routine_t *co, generator_t gen);

    C_API void coro_info(routine_t *, int pos);

    /* Sets the current coroutine's name.*/
    C_API void coro_name(char *fmt, ...);

    /* Check for at least `n` bytes left on the stack. If not present, panic/abort. */
    C_API void coro_stack_check(int);
    C_API void coro_enqueue(routine_t *);

    /* Suspends the execution of current coroutine, switch to scheduler. */
    C_API void coro_suspend(void);

    /* Return handle to current coroutine. */
    C_API routine_t *coro_active(void);

    /* Return handle to coroutine selected running in
    `scheduler` aka ~thread~ before `interrupter`. */
    C_API routine_t *coro_running(void);

    C_API routine_t *coro_sleeping(void);

    /* Multithreading checker for available coroutines in `waitgroup`, if any,
    transfer from `global` run queue, to current thread `local` run queue.

    If `main/process thread` caller, will globally signal all child threads
    to start there execution, and assign coroutines. */
    C_API void coro_stealer(void);

    /* Prepare/mark next `Go/coroutine` as `interrupt` event to be ~detached~. */
    C_API void coro_mark(void);

    /* Set name on `Go` result `id`, and finish an previous `coro_mark` ~interrupt~ setup. */
    C_API routine_t *coro_unmark(rid_t cid, string_t name);

    /* Detach an `interrupt` coroutine that was `coro_mark`, will not prevent system from shuting down. */
    C_API void coro_detached(routine_t *);

    /* This function forms the basics for `integrating` with an `callback/event loop` like system.
    Internally referenced as an `interrupt`.

    The function provided and arguments will be launch in separate coroutine,
    there should be an `preset` callback having either:

    - `coro_await_finish(routine_t *co, void_t result, ptrdiff_t plain, bool is_plain)`
    - `coro_await_exit`
    - `coro_await_upgrade`

    These functions are designed to break the `waitfor` loop, set `result`, and `return` to ~caller~.
    The launched coroutine should first call `coro_active()` and store the `required` context. */
    C_API value_t coro_await(callable_t, size_t, ...);

    /* Create an coroutine and immediately execute, intended to be used to launch
    another coroutine like `coro_await` to create an background `interrupt` coroutine. */
    C_API void coro_launch(callable_t fn, u64 num_of_args, ...);

    /* Same as `coro_await_finish`, but adding conditionals to either `stop` or `switch`.
    Should be used to control `waitfor` loop, can `continue` after returning some `temporay data/result`.

    Meant for `special` network connection handling. */
    C_API void coro_await_upgrade(routine_t *co, void_t result, ptrdiff_t plain, bool is_plain,
                                  bool halted, bool switching);

    /* Similar to `coro_await_upgrade`, but does not ~halt/exit~,
    should be used for `Generator` callback handling.
    WILL switch to `generator` function `called` then ~conditionally~ back to `caller`. */
    C_API void coro_await_yield(routine_t *co, void_t result, ptrdiff_t plain, bool is_plain, bool switching);

    /* Should be used inside an `preset` callback, this function:
    - signal `coroutine` in `waitfor` loop to `stop`.
    - set `result`, either `pointer` or `non-pointer` return type.
    - then `switch` to stored `coroutine context` to return to `caller`.

    Any `resource` release `routines` should be placed after this function. */
    C_API void coro_await_finish(routine_t *co, void_t result, ptrdiff_t plain, bool is_plain);

    /* Similar to `coro_await_finish`, but should be used for exiting some
     `background running coroutine` to perform cleanup. */
    C_API void coro_await_exit(routine_t *co, void_t result, ptrdiff_t plain, bool is_plain);

    /* Should be used as part of `coro_await` initialization function to
    indicate an `error` condition, where the `preset` callback WILL NOT be called.
    - This will `set` coroutine to `error state` then `switch` to stored `coroutine context`
    to return to `caller`. */
    C_API void coro_await_canceled(routine_t *, signed int code);

    /* Should be used as part of an `preset` ~interrupt~ callback
    to `record/indicate` an `error` condition. */
    C_API void_t coro_await_erred(routine_t *, int);

    /* Check for coroutine completetion. */
    C_API bool coro_terminated(routine_t *);
    C_API void coro_halt_set(routine_t *);
    C_API void coro_halt_clear(routine_t *);

    /* Setup callback handlers to integrate into coroutine `scheduler`, and other routines.

    - `loopfunc` - is executed before any coroutine `switch`, the `interrupter`.
     ** Coroutine `scheduler` will use `perthreadfunc` handle to make `interrupter` call.

    - `perthreadfunc` - will create each thread's own `loopfunc` handle.
     ** `interrupt_handle()` will return `perthreadfunc` handle, use `interrupt_handle_set()` will set.

    - `shutdownfunc` - shutdown routine to call to cleanup `loopfunc` and `perthreadfunc` processing.
     ** The `shutdownfunc` will be called at exit, before any `scheduler` cleanup routines.

    NOTE: `loopfunc`, `perthreadfunc`, and `shutdownfunc` are required for proper integration. */
    C_API void coro_interrupt_setup(call_interrupter_t loopfunc, call_t perthreadfunc, func_t shutdownfunc);

    C_API void_t interrupt_handle(void);
    C_API void_t interrupt_data(void);
    C_API i32 interrupt_code(void);
    C_API bits_t interrupt_bitset(void);
    C_API arrays_t interrupt_array(void);
    C_API i32 is_interrupting(void);

    C_API void interrupt_handle_set(void_t);
    C_API void interrupt_data_set(void_t);
    C_API void interrupt_code_set(i32);
    C_API void interrupt_bitset_set(bits_t);
    C_API void interrupt_array_set(arrays_t);

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
    /* Return the current `active` coroutine id. */
    C_API u32 coro_active_id(void);
    /* Return `thread id` ~index~ current coroutine assigned to. */
    C_API u32 coro_thrd_id(void);
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
