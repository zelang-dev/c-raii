
#ifndef _CORO_H
#define _CORO_H

#define _BSD_SOURCE

/* Public API qualifier. */
#ifndef C_API
#   define C_API extern
#endif

/* Coroutine states. */
typedef enum {
    CORO_EVENT_DEAD = -1, /* The coroutine has ended it's Event Loop routine, is uninitialized or deleted. */
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
typedef struct raii_deque_s raii_deque_t;
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
#   define CORO_STACK_SIZE (8 * 1024)
#endif

#ifndef ERROR_SCRAPE_SIZE
#   define ERROR_SCRAPE_SIZE 256
#endif

#ifndef SCRAPE_SIZE
#   define SCRAPE_SIZE (2 * 64)
#endif

/* Number used only to assist checking for stack overflows. */
#define CORO_MAGIC_NUMBER 0x7E3CB1A9

#ifdef __cplusplus
extern "C" {
#endif
    C_API void coro_yield(void);
    C_API routine_t *coro_active(void);
    C_API unsigned int coro_sleep_for(unsigned int ms);
    C_API void coro_deferred_free(routine_t *);

    /* This library provides its own ~main~,
    which call this function as an coroutine! */
    C_API int coro_main(int, char **);
    C_API int raii_main(int, char **);


#ifdef __cplusplus
}
#endif


#endif /* _CORO_H */
