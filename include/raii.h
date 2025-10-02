#ifndef RAII_H
#define RAII_H

#define RAII_VERSION_MAJOR 2
#define RAII_VERSION_MINOR 2
#define RAII_VERSION_PATCH 1

#include "vector.h"
#include "cthread.h"
#include "catomic.h"
#include <stdio.h>
#include <time.h>
#include "coro.h"

#if defined(__APPLE__) || defined(__MACH__)
#include<mach/mach_time.h>
#endif

#ifdef __cplusplus
    extern "C" {
#endif

#ifndef O_BINARY
#   define O_BINARY 0
#endif

struct memory_s {
    void_t arena;
    raii_type status;
    int threading;
    bool is_recovered;
    bool is_protected;
    defer_t defer;
    future_t threaded;
    vectors_t local;
    ex_ptr_t *protector;
    ex_backtrace_t *backtrace;
    void_t volatile err;
    string_t volatile panic;
    raii_deque_t *queued;
};

struct _promise {
	raii_type type;
	int id;
	atomic_flag done;
	atomic_spinlock mutex;
	memory_t *scope;
	raii_values_t *result;
};

typedef struct _future_arg worker_t;
struct _future_arg {
    raii_type type;
    int id;
    unique_t *local;
    void_t arg;
    thrd_func_t func;
    promise *value;
    raii_deque_t *queue;
};
make_atomic(worker_t, atomic_worker_t)

typedef struct {
    atomic_size_t size;
    atomic_worker_t buffer[];
} future_array_t;
make_atomic(future_array_t *, atomic_future_t)

typedef struct result_data {
    raii_type type;
    bool is_ready;
    rid_t id;
    raii_values_t *result;
} _result_t, *result_t;
make_atomic(result_t, atomic_result_t)

struct raii_results_s {
    raii_type type;
    volatile sig_atomic_t is_takeable;
#if defined(_WIN32)
    LARGE_INTEGER timer;
#elif defined(__APPLE__) || defined(__MACH__)
    mach_timebase_info_data_t timer;
#endif

    /* Stack size when creating a coroutine. */
    u32 stacksize;

    /* Number of CPU cores this machine has,
    it determents the number of threads to use. */
    size_t cpu_count;
    size_t thread_count;

    size_t capacity;
    size_t queue_size;
    arrays_t gc;
    memory_t *scope;

    /* Array of `coroutine` results in an thread `waitgroup`. */
    waitresult_t group_result;
    raii_deque_t *queue;
    cacheline_pad_t _pad;

    /* Lock to collect thread `coroutine` results in waitgroup. */
    atomic_spinlock group_lock;

    /* Exception/error indicator, only private `co_awaitable()`
    will clear to break any possible infinite wait/loop condition,
    normal cleanup code will not be executed. */
    atomic_flag is_errorless;
    atomic_flag is_finish;
    atomic_flag is_started;
    atomic_flag is_waitable;
    atomic_flag is_disabled;

    /* track the number of global coroutines active/available */
    atomic_size_t active_count;

    /* coroutine unique id */
    atomic_size_t id_generate;

    /* result generator also used to determent which thread's `run queue`
    receive next `coroutine` task, `counter % cpu cores` */
    atomic_size_t result_id_generate;
    atomic_size_t take_count;
    atomic_result_t *results;
};

struct _future {
	raii_type type;
	int id;
	int is_pool;
	atomic_flag started;
	thrd_t thread;
	thrd_func_t func;
	memory_t *scope;
	promise *value;
};

C_API raii_results_t gq_result;

C_API memory_t *raii_local(void);
/* Return current `thread` smart memory pointer. */
C_API memory_t *raii_init(void);

/* Return current `context` ~scope~ smart pointer, in use! */
C_API memory_t *get_scope(void);

/* Begin `unwinding/executing`, current ~scope~ `raii_defer` statements. */
C_API int exit_scope(void);

C_API void raii_unwind_set(ex_context_t *ctx, const char *ex, const char *message);
C_API int raii_deferred_init(defer_t *array);

C_API result_t raii_result_create(void);
C_API result_t raii_result_get(rid_t);

/* Check status of an `result id` */
C_API bool result_is_ready(rid_t);

/* Defer execution `LIFO` of given function with argument,
to current `thread` scope lifetime/destruction. */
C_API size_t raii_defer(func_t, void_t);

/* Defer execution `LIFO` of given function with argument,
execution begins when current `context` scope exits or panic/throw. */
C_API size_t deferring(func_t func, void_t data);

/* Same as `raii_defer` but allows recover from an Error condition throw/panic,
you must call `raii_caught` inside function to mark Error condition handled. */
C_API void raii_recover(func_t, void_t);

/* Same as `defer` but allows recover from an Error condition throw/panic,
you must call `raii_is_caught` inside function to mark Error condition handled. */
C_API void raii_recover_by(memory_t *, func_t, void_t);

/* Compare `err` to current error condition, will mark exception handled, if `true`. */
C_API bool raii_caught(const char *err);

/* Compare `err` to scoped error condition, will mark exception handled, if `true`. */
C_API bool raii_is_caught(memory_t *scope, const char *err);

/* Compare `err` to scoped error condition, will mark exception handled, if `true`.
Only valid between `guard` blocks or inside `thread/future` call. */
C_API bool is_recovered(const char *err);

/* Get current error condition string. */
C_API const char *raii_message(void);

/* Get scoped error condition string. */
C_API const char *raii_message_by(memory_t *scope);

/* Get scoped error condition string.
Only valid between `guard` blocks or inside `thread/future` call. */
C_API const char *err_message(void);

/* Defer execution `LIFO` of given function with argument,
to the given `scoped smart pointer` lifetime/destruction. */
C_API size_t raii_deferred(memory_t *, func_t, void_t);
C_API size_t raii_deferred_count(memory_t *);

/* Request/return raw memory of given `size`, using smart memory pointer's lifetime scope handle.
DO NOT `free`, will be freed with given `func`, when scope smart pointer panics/returns/exits. */
C_API void_t malloc_full(memory_t *scope, size_t size, func_t func);

/* Request/return raw memory of given `size`, using smart memory pointer's lifetime scope handle.
DO NOT `free`, will be freed with given `func`, when scope smart pointer panics/returns/exits. */
C_API void_t calloc_full(memory_t *scope, int count, size_t size, func_t func);

/* Same as `raii_deferred_free`, but also destroy smart pointer. */
C_API void raii_delete(memory_t *ptr);

/* Same as `raii_deferred_clean`, but also
reset/clear current `thread` smart pointer. */
C_API void raii_destroy(void);

/* Begin `unwinding`, executing given scope smart pointer `raii_deferred` statements. */
C_API void raii_deferred_free(memory_t *);

/* Begin `unwinding`, executing current `thread` scope `raii_defer` statements. */
C_API void raii_deferred_clean(void);

C_API void_t raii_memdup(const_t src, size_t len);
C_API string *raii_split(string_t s, string_t delim, int *count);
C_API string raii_concat(int num_args, ...);
C_API string raii_replace(string_t haystack, string_t needle, string_t replace);
C_API u_string raii_encode64(u_string_t src);
C_API u_string raii_decode64(u_string_t src);

/* Creates smart memory pointer, this object binds any additional requests to it's lifetime.
for use with `malloc_*` `calloc_*` wrapper functions to request/return raw memory. */
C_API unique_t *unique_init(void);

/* Returns protected raw memory pointer of given `size`,
DO NOT FREE, will `throw/panic` if memory request fails.
This uses current `context` smart pointer, being in `guard` blocks,
inside `thread/future`, or active `coroutine` call. */
C_API void_t malloc_local(size_t size);

/* Returns protected raw memory pointer of given `size`,
DO NOT FREE, will `throw/panic` if memory request fails.
This uses current `context` smart pointer, being in `guard` blocks,
inside `thread/future`, or active `coroutine` call. */
C_API void_t calloc_local(int count, size_t size);

C_API template_t *value_create(const_t, raii_type);
C_API template_t raii_value(void_t);
C_API raii_type type_of(void_t);
C_API bool is_type(void_t, raii_type);
C_API bool is_inaccessible(void_t);
C_API bool is_instance_of(void_t, void_t);
C_API bool is_value(void_t);
C_API bool is_instance(void_t);
C_API bool is_valid(void_t);
C_API bool is_invalid(void_t);
C_API bool is_union(void_t);
C_API bool is_null(void_t);
C_API bool is_zero(size_t);
C_API bool is_empty(void_t);
C_API bool is_true(bool);
C_API bool is_false(bool);
C_API bool is_str_in(const char *text, char *pattern);
C_API bool is_str_eq(const char *str, const char *str2);
C_API bool is_str_empty(const char *str);
C_API bool is_guard(void_t self);
C_API bool is_equal(void_t, void_t);
C_API bool is_equal_ex(void_t, void_t);
C_API bool is_addressable(void_t);

C_API bool is_promise(void_t);
C_API bool is_future(void_t);

C_API bool raii_is_exiting(void);
C_API bool raii_is_running(void);

/* Tries to query the system for current time using `MONOTONIC` clock,
 or whatever method ~system/platform~ provides for `REALTIME`. */
C_API uint64_t get_timer(void);

C_API void_t try_calloc(int, size_t);
C_API void_t try_malloc(size_t);
C_API void_t try_realloc(void_t, size_t);

C_API void guarding(future f, args_t args);
C_API void guard_set(ex_context_t *ctx, const char *ex, const char *message);
C_API void guard_reset(void_t scope, ex_setup_func set, ex_unwind_func unwind);
C_API void guard_delete(memory_t *ptr);

#ifndef MAX
# define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef MIN
# define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#define NONE

/* Returns protected raw memory pointer,
DO NOT FREE, will `throw/panic` if memory request fails. */
#define _malloc(size)           malloc_local(size)

/* Returns protected raw memory pointer,
DO NOT FREE, will `throw/panic` if memory request fails. */
#define _calloc(count, size)    calloc_local(count, size)

/* Defer execution `LIFO` of given function with argument,
execution begins when current `guard` scope exits or panic/throw. */
#define _defer(func, ptr)       raii_recover_by(_$##__FUNCTION__, (func_t)func, ptr)

/* Compare `err` to scoped error condition, will mark exception handled, if `true`. */
#define _recovered(err)           raii_is_caught(raii_local()->arena, err)

/* Compare `err` to scoped error condition,
will mark exception handled, if `true`.
DO NOT PUT `err` in quote's like "err". */
#define _is_caught(err)         raii_is_caught(raii_local()->arena, EX_STR(err))

/* Get scoped error condition string. */
#define _get_message()          raii_message_by(raii_local()->arena)

/* Stops the ordinary flow of control and begins panicking,
throws an exception of given message. */
#define _panic          raii_panic

/* Makes a reference assignment of current scoped smart pointer. */
#define _assign_ptr(scope)      unique_t *scope = _$##__FUNCTION__

/* Exit `guarded` section, begin executing deferred functions,
return given `value` when done, use `NONE` for no return. */
#define _return(value)              \
    raii_delete(_$##__FUNCTION__);  \
    guard_reset(s##__FUNCTION__, sf##__FUNCTION__, uf##__FUNCTION__);   \
    ex_update(ex_err.next);         \
    if (ex_local() == &ex_err)      \
        ex_update(ex_err.next);     \
    return value;

/* Creates an scoped guard section, it replaces `{`.
Usage of: `_defer`, `_malloc`, `_calloc`, `_assign_ptr` macros
are only valid between these sections.
    - Use `_return(x);` macro, or `break;` to exit early.
    - Use `_assign_ptr(var)` macro, to make assignment of block scoped smart pointer. */
#define guard                                           \
{                                                       \
    if (!exception_signal_set)                          \
        ex_signal_setup();                              \
    void_t s##__FUNCTION__ = raii_init()->arena;         \
    ex_setup_func gsf_##__FUNCTION__ = guard_setup_func;      \
    ex_unwind_func guf_##__FUNCTION__ = guard_unwind_func;    \
    guard_unwind_func = (ex_unwind_func)raii_deferred_free; \
    guard_setup_func = guard_set;                   \
    unique_t *_$##__FUNCTION__ = unique_init();         \
    (_$##__FUNCTION__)->status = RAII_GUARDED_STATUS;   \
    raii_local()->status = (_$##__FUNCTION__)->status;  \
    raii_local()->arena = (void_t)_$##__FUNCTION__;     \
    try {                                               \
        do {

    /* This ends an scoped guard section, it replaces `}`.
    On exit will begin executing deferred functions,
    return given `result` when done, use `NONE` for no return. */
#define unguarded(result)                                           \
        } while (false);                                            \
            raii_deferred_free(_$##__FUNCTION__);                   \
    } catch_if {                                                    \
        if ((is_type(&(_$##__FUNCTION__)->defer, RAII_DEF_ARR)))    \
            raii_deferred_free(_$##__FUNCTION__);                   \
    } finally {                                                     \
        guard_reset(s##__FUNCTION__, gsf_##__FUNCTION__, guf_##__FUNCTION__);   \
        guard_delete(_$##__FUNCTION__);                             \
    }                                                             \
    return result;                                                  \
}

    /* This ends an scoped guard section, it replaces `}`.
    On exit will begin executing deferred functions. */
#define guarded                                 \
        } while (false);                        \
        raii_deferred_free(_$##__FUNCTION__);   \
    } catch_if {                                \
        raii_deferred_free(_$##__FUNCTION__);   \
    } finally {                                 \
        guard_reset(s##__FUNCTION__, gsf_##__FUNCTION__, guf_##__FUNCTION__);   \
        guard_delete(_$##__FUNCTION__);         \
    }                                     \
}

    /* This ends an scoped guard section, it replaces `}`.
    On exit will begin executing deferred functions. Will catch and set `error`
    this is an internal macro for `thrd_spawn` and `thrd_async` usage. */
#define guarded_exception(error)                \
        } while (false);                        \
        raii_deferred_free(_$##__FUNCTION__);   \
    } catch_if {                             \
        raii_deferred_free(_$##__FUNCTION__);   \
        if (!_$##__FUNCTION__->is_recovered && raii_is_caught(_$##__FUNCTION__, raii_message_by(_$##__FUNCTION__))) { \
            promise_erred(error, ex_err);  \
        }                                       \
    } finally {                                 \
        guard_reset(s##__FUNCTION__, gsf_##__FUNCTION__, guf_##__FUNCTION__);   \
        guard_delete(_$##__FUNCTION__);         \
    }                                     \
}

#define block(type, name, ...)          \
    type name(__VA_ARGS__)              \
    guard

#define unblocked(result)               \
    unguarded(result)

#define blocked                         \
    guarded

#define try         ex_trying
#define catch_any   ex_catching_any
#define catch_if    ex_catching_if
#define catch(e)    ex_catching(e)
#define finally     ex_finallying
#define finality    ex_finalitying

#define caught(E) raii_caught(EX_STR(E))
#define rethrow try_rethrow(&ex_err)

#define time_spec(sec, nsec) &(struct timespec){ .tv_sec = sec ,.tv_nsec = nsec }
thrd_local_extern(memory_t, raii)
thrd_local_extern(ex_context_t, except)
C_API const raii_values_t raii_values_empty[1];
C_API ex_setup_func guard_setup_func;
C_API ex_unwind_func guard_unwind_func;
C_API ex_terminate_func guard_ctrl_c_func;
C_API ex_terminate_func guard_terminate_func;

#define match(variable_type) switch (type_of(variable_type))
#define and(ENUM) case ENUM:
#define or(ENUM) break; case ENUM:
#define otherwise break; default:

/* `Cast` ~val~, a `non-pointer` to `pointer` like value,
makes reference if variable. */
#define casting(val) (void_t)((ptrdiff_t)(val))

#ifndef va_copy
# ifdef __va_copy
#  define va_copy(a,b) __va_copy(a,b)
# else /* !__va_copy */
#  define va_copy(a,b) (*(a) = *(b))
# endif /* __va_copy */
#endif /* va_copy */

C_API int cerr(string_t msg, ...);
C_API int cout(string_t msg, ...);

/**
* Set usage `message` to display to `user`
* documenting all defined ~command-line~ options/flags.
*
* @param message usage/help menu.
* @param is_ordered command-line arguments in specificied order, allows duplicates.
*
* - If ~is_ordered~ `true` will retain each `argv[]` index in `is_cli_getopt` calls.
*/
C_API void cli_message_set(string_t message, bool is_ordered);

/**
* Parse and check command-line options, aka `getopt`.
*
* If `flag` match, MUST call `cli_getopt()` to ~retrieve~ next `argv[]`.
*
* @param flag argument/options to match against, if `nullptr`, `cli_getopt()` returns first `argv[1]`.
* @param is_single or `is_boolean` argument, if `true`, only `flag` is returned by `cli_getopt()`.
*
* - NOTE: `is_single` WILL also parse `-flag=XXXX`, where `cli_getopt()` returns `XXXX`.
*/
C_API bool is_cli_getopt(string_t flag, bool is_single);

/*
* Returns `argv[1]` or next `argv[]`, from matching `is_cli_getopt()`, aka `get_opt`.
*/
C_API string cli_getopt(void);

C_API void cli_arguments_set(int argc, char **argv);

#ifdef __cplusplus
    }
#endif
#endif /* RAII_H */
