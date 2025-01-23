#ifndef RAII_H
#define RAII_H

#include "swar.h"
#include "cthread.h"
#include "catomic.h"
#include <stdio.h>
#include <time.h>
#include "coro.h"

#ifndef O_BINARY
#   define O_BINARY 0
#endif

#ifndef CACHELINE_SIZE
/* The estimated size of the CPU's cache line when atomically updating memory.
 Add this much padding or align to this boundary to avoid atomically-updated
 memory from forcing cache invalidations on near, but non-atomic, memory.

 https://en.wikipedia.org/wiki/False_sharing
 https://github.com/golang/go/search?q=CacheLinePadSize
 https://github.com/ziglang/zig/blob/a69d403cb2c82ce6257bfa1ee7eba52f895c14e7/lib/std/atomic.zig#L445
*/
#   define CACHELINE_SIZE _CACHE_LINE
#endif

typedef char cacheline_pad_t[CACHELINE_SIZE];

#ifdef __cplusplus
    extern "C" {
#endif

#define make_deque(func_type)                       \
    make_atomic(func_type *, atomic_##func_type)    \
    typedef struct {                    \
        atomic_size_t size;             \
        atomic_##func_type buffer[];    \
    } func_type##_array;                \
    make_atomic(func_type##_array *, atomic_array_##func_type)  \
    typedef struct {                    \
        size_t capacity;                \
        /* Assume that they never overflow */   \
        atomic_size_t top, bottom;      \
        atomic_array_##func_type array; \
    } deque_##func_type;                \
    make_atomic(deque_##func_type *, atomic_deque_##func_type)

struct memory_s {
    void_t arena;
    bool is_recovered;
    bool is_protected;
    bool is_emulated;
    int threading;
    int status;
    size_t mid;
    defer_t defer;
    future_t threaded;
    vectors_t local;
    ex_ptr_t *protector;
    ex_backtrace_t *backtrace;
    void_t volatile err;
    string_t volatile panic;
    raii_deque_t *queued;
};

typedef void (*for_func_t)(i64, i64);
typedef void_t (*thrd_func_t)(args_t);
typedef void_t (*result_func_t)(void_t result, size_t id, vectors_t iter);
typedef void (*wait_func)(void);
typedef struct _promise {
    raii_type type;
    int id;
    atomic_flag done;
    atomic_spinlock mutex;
    memory_t *scope;
    raii_values_t *result;
} promise;

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

    /* Stack size when creating a coroutine. */
    u32 stacksize;

    /* Number of CPU cores this machine has,
    it determents the number of threads to use. */
    size_t cpu_count;
    size_t thread_count;

    size_t queue_size;
    arrays_t gc;
    memory_t *scope;

    /* Array of thread `coroutine` results in waitgroup. */
    waitresult_t group_result;
    cacheline_pad_t _pad;

    raii_deque_t *queue;
    cacheline_pad_t _pad1;

    /* Lock to collect thread `coroutine` results in waitgroup. */
    atomic_spinlock group_lock;
    cacheline_pad_t _pad2;

    /* Exception/error indicator, only private `co_awaitable()`
    will clear to break any possible infinite wait/loop condition,
    normal cleanup code will not be executed. */
    atomic_flag is_errorless;
    cacheline_pad_t _pad3;

    atomic_flag is_interruptable;
    cacheline_pad_t _pad4;

    atomic_flag is_finish;
    cacheline_pad_t _pad5;

    atomic_flag is_started;
    cacheline_pad_t _pad6;

    atomic_flag is_waitable;
    cacheline_pad_t _pad7;

    /* Used as flag and counter for an thread waitgroup results collected. */
    atomic_size_t group_count;
    cacheline_pad_t _pad8;

    /* track the number of global coroutines active/available */
    atomic_size_t active_count;
    cacheline_pad_t _pad9;

    /* coroutine unique id */
    atomic_size_t id_generate;
    cacheline_pad_t _pad10;

    /* result generator also used to determent which thread's `run queue`
    receive next `coroutine` task, `counter % cpu cores` */
    atomic_size_t result_id_generate;
    cacheline_pad_t _pad11;

    atomic_result_t *results;
    cacheline_pad_t _pad12;
};
C_API future_results_t gq_result;

/* Calls fn (with args as arguments) in separate thread, returning without waiting
for the execution of fn to complete. The value returned by fn can be accessed
by calling `thrd_get()`. */
C_API future thrd_async(thrd_func_t fn, size_t, ...);

/* Same as `thrd_async`, allows passing custom `context` scope for internal `promise`
for auto cleanup within caller's `scope`. */
C_API future thrd_async_ex(memory_t *scope, thrd_func_t fn, void_t args);

/* Returns the value of `future` ~promise~, a thread's shared object, If not ready, this
function blocks the calling thread and waits until it is ready. */
C_API values_type thrd_get(future);

/* This function blocks the calling thread and waits until `future` is ready,
will execute provided `yield` callback function continuously. */
C_API void thrd_wait(future, wait_func yield);

/* Check status of `future` object state, if `true` indicates thread execution has ended,
any call thereafter to `thrd_get` is guaranteed non-blocking. */
C_API bool thrd_is_done(future);
C_API void thrd_delete(future);
C_API uintptr_t thrd_self(void);
C_API size_t thrd_cpu_count(void);

/* Return/create an arbitrary `vector/array` set of `values`, only available within `thread/future` */
C_API vectors_t thrd_data(size_t, ...);

/* Return/create an single `vector/array` ~value~, only available within `thread/future` */
#define $(val) thrd_data(1, (val))

/* Return/create an pair `vector/array` ~values~, only available within `thread/future` */
#define $$(val1, val2) thrd_data(2, (val1), (val2))

C_API future_t thrd_scope(void);
C_API future_t thrd_sync(future_t);
C_API rid_t thrd_spawn(thrd_func_t fn, size_t, ...);
C_API values_type thrd_result(rid_t id);
C_API void thrd_set_result(raii_values_t *, int);

C_API future_t thrd_for(for_func_t loop, intptr_t initial, intptr_t times);

C_API void thrd_then(result_func_t callback, future_t iter, void_t result);
C_API void thrd_destroy(future_t);
C_API bool thrd_is_finish(future_t);

C_API memory_t *raii_local(void);
/* Return current `thread` smart memory pointer. */
C_API memory_t *raii_init(void);

/* Return current `context` ~scope~ smart pointer, in use! */
C_API memory_t *get_scope(void);

C_API void raii_unwind_set(ex_context_t *ctx, const char *ex, const char *message);
C_API int raii_deferred_init(defer_t *array);

C_API size_t raii_mid(void);
C_API size_t raii_last_mid(memory_t *scope);
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

C_API void raii_defer_cancel(size_t index);
C_API void raii_deferred_cancel(memory_t *scope, size_t index);

C_API void raii_defer_fire(size_t index);
C_API void raii_deferred_fire(memory_t *scope, size_t index);

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
Only valid between `guard` blocks or inside ~c++11~ like `thread/future` call. */
C_API bool is_recovered(const char *err);

/* Get current error condition string. */
C_API const char *raii_message(void);

/* Get scoped error condition string. */
C_API const char *raii_message_by(memory_t *scope);

/* Get scoped error condition string.
Only valid between `guard` blocks or inside ~c++11~ like `thread/future` call. */
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
This uses current `thread` smart pointer, unless called
between `guard` blocks, or inside ~c++11~ like `thread/future` call. */
C_API void_t malloc_local(size_t size);

/* Returns protected raw memory pointer of given `size`,
DO NOT FREE, will `throw/panic` if memory request fails.
This uses current `thread` smart pointer, unless called
between `guard` blocks, or inside ~c++11~ like `thread/future` call. */
C_API void_t calloc_local(int count, size_t size);

C_API values_type raii_value(void_t);
C_API raii_type type_of(void_t);
C_API bool is_type(void_t, raii_type);
C_API bool is_void(void_t);
C_API bool is_instance_of(void_t, void_t);
C_API bool is_value(void_t);
C_API bool is_instance(void_t);
C_API bool is_valid(void_t);
C_API bool is_zero(size_t);
C_API bool is_empty(void_t);
C_API bool is_true(bool);
C_API bool is_false(bool);
C_API bool is_str_in(const char *text, char *pattern);
C_API bool is_str_eq(const char *str, const char *str2);
C_API bool is_str_empty(const char *str);
C_API bool is_guard(void_t self);

C_API void_t try_calloc(int, size_t);
C_API void_t try_malloc(size_t);
C_API void_t try_realloc(void_t, size_t);

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
execution begins when current `context` scope exits or panic/throw. */
#define _defer(func, ptr)       deferring((func_t)func, ptr)

/* Compare `err` to scoped error condition, will mark exception handled, if `true`. */
#define _recover(err)           is_recovered(err)

/* Compare `err` to scoped error condition,
will mark exception handled, if `true`.
DO NOT PUT `err` in quote's like "err". */
#define _is_caught(err)         is_recovered(EX_STR(err))

/* Get scoped error condition string. */
#define _get_message()  err_message()

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
    ex_setup_func sf##__FUNCTION__ = exception_setup_func;      \
    ex_unwind_func uf##__FUNCTION__ = exception_unwind_func;    \
    exception_unwind_func = (ex_unwind_func)raii_deferred_free; \
    exception_setup_func = guard_set;                   \
    unique_t *_$##__FUNCTION__ = unique_init();         \
    (_$##__FUNCTION__)->status = RAII_GUARDED_STATUS;   \
    raii_local()->arena = (void_t)_$##__FUNCTION__;     \
    ex_try {                                            \
        do {

    /* This ends an scoped guard section, it replaces `}`.
    On exit will begin executing deferred functions,
    return given `result` when done, use `NONE` for no return. */
#define unguarded(result)                                           \
            } while (false);                                        \
            raii_deferred_free(_$##__FUNCTION__);                   \
    } ex_catch_if {                                                 \
        if ((is_type(&(_$##__FUNCTION__)->defer, RAII_DEF_ARR)))    \
            raii_deferred_free(_$##__FUNCTION__);                   \
    } ex_finally {                                                  \
        guard_reset(s##__FUNCTION__, sf##__FUNCTION__, uf##__FUNCTION__);   \
        guard_delete(_$##__FUNCTION__);                             \
    } ex_end_try;                                                   \
    return result;                                                  \
}

    /* This ends an scoped guard section, it replaces `}`.
    On exit will begin executing deferred functions. */
#define guarded                                                     \
        } while (false);                                            \
        raii_deferred_free(_$##__FUNCTION__);                       \
    } ex_catch_if {                                                 \
        raii_deferred_free(_$##__FUNCTION__);                       \
    } ex_finally {                                                  \
        guard_reset(s##__FUNCTION__, sf##__FUNCTION__, uf##__FUNCTION__);   \
        guard_delete(_$##__FUNCTION__);                             \
    } ex_end_try;                                                   \
}


    /* This ends an scoped guard section, it replaces `}`.
    On exit will begin executing deferred functions. Will catch and set `error`
    this is an internal macro for `thrd_spawn` and `thrd_async` usage. */
#define guarded_exception(error)                                    \
        } while (false);                                            \
        raii_deferred_free(_$##__FUNCTION__);                       \
    } ex_catch_if {                                                 \
        raii_deferred_free(_$##__FUNCTION__);                       \
        if (!_$##__FUNCTION__->is_recovered && raii_is_caught(_$##__FUNCTION__, raii_message_by(_$##__FUNCTION__))) { \
            ((memory_t *)error)->err = (void_t)ex_err.ex;           \
            ((memory_t *)error)->panic = ex_err.panic;              \
            ((memory_t *)error)->backtrace = ex_err.backtrace;      \
        }                                                           \
    } ex_finally{\
        guard_reset(s##__FUNCTION__, sf##__FUNCTION__, uf##__FUNCTION__);   \
        guard_delete(_$##__FUNCTION__);                             \
    } ex_end_try;                                                   \
}


#define block(type, name, ...)          \
    type name(__VA_ARGS__)              \
    guard

#define unblocked(result)               \
    unguarded(result)

#define blocked                         \
    guarded

#define try ex_try
#define catch_any ex_catch_any
#define catch_if ex_catch_if
#define catch(e) ex_catch(e)
#define end_try ex_end_try
#define finally ex_finally
#define caught(err) raii_caught(EX_STR(err))

#ifdef _WIN32
    #define finality ex_finality
    #define end_trying ex_end_trying
#else
    #define finality catch_any ex_finally
    #define end_trying ex_end_try
#endif

#define time_spec(sec, nsec) &(struct timespec){ .tv_sec = sec ,.tv_nsec = nsec }

thrd_local_extern(memory_t, raii)
thrd_local_extern(ex_context_t, except)
C_API const raii_values_t raii_values_empty[1];

C_API string str_memdup_ex(memory_t *defer, const_t src, size_t len);
C_API string str_copy(string dest, string_t src, size_t len);
C_API string *str_split_ex(memory_t *defer, string_t s, string_t delim, int *count);
C_API string str_concat_ex(memory_t *defer, int num_args, va_list ap_copy);
C_API string str_replace_ex(memory_t *defer, string_t haystack, string_t needle, string_t replace);
C_API u_string str_encode64_ex(memory_t *defer, u_string_t src);
C_API u_string str_decode64_ex(memory_t *defer, u_string_t src);
C_API bool is_base64(u_string_t src);
C_API int strpos(const char *text, char *pattern);

C_API vectors_t vector_variant(void);
C_API vectors_t vector_for(vectors_t, size_t, ...);
C_API void vector_insert(vectors_t, int, void_t);
C_API void vector_clear(vectors_t);
C_API void vector_erase(vectors_t, int);
C_API size_t vector_size(vectors_t);
C_API size_t vector_capacity(vectors_t);
C_API memory_t *vector_scope(vectors_t);
C_API void vector_push_back(vectors_t, void_t);
C_API vectors_t vector_copy(memory_t *, vectors_t, vectors_t);
C_API bool is_vector(void_t);

/**
* Creates an scoped `vector/array/container` for arbitrary arguments passing
* into an single `paramater` function.
* - Use standard `array access` for retrieval of an `union` storage type.
*
* - MUST CALL `args_destructor_set()` to have memory auto released
*   within ~callers~ scoped `context`, will happen either at return/exist or panics.
*
* - OTHERWISE `memory leak` will be shown in DEBUG build.
*
* NOTE: `vector_for` does auto memory cleanup.
*
* @param count numbers of parameters, `0` will create empty `vector/array`.
* @param arguments indexed in given order.
*/
C_API args_t args_for(size_t, ...);
C_API args_t args_ex(size_t, va_list);
C_API void args_destructor_set(args_t);
C_API bool is_args(void_t);

/**
* Creates an scoped `vector/array/container` for arbitrary item types.
*
* - Use standard `array access` for retrieval of an `union` storage type.
*
* - MUST CALL `array_deferred_set` to have memory auto released
*   when given `scope` context return/exist or panics.
*
* - OTHERWISE `memory leak` will be shown in DEBUG build.
*
* @param count numbers of parameters, `0` will create empty `vector/array`.
* @param arguments indexed in given order.
*/
C_API arrays_t array_of(memory_t *, size_t, ...);
C_API arrays_t array_ex(memory_t *, size_t, va_list);
C_API void array_deferred_set(arrays_t, memory_t *);
C_API void array_append(arrays_t, void_t);
C_API void array_delete(arrays_t);
C_API void array_remove(arrays_t, int);
C_API bool is_array(void_t);

/* Returns a sequence of numbers, in a given range, this is same as `arrays_of`,
but `scoped` to either ~current~ call scope, `coroutine` or `thread` termination. */
C_API arrays_t range(int start, int stop);

/* Same as `range`, but increasing by given ~steps~. */
C_API arrays_t rangeing(int start, int stop, int steps);

/* Same as `range`, but return given ~string/text~, split as array of `char`. */
C_API arrays_t range_char(string_t text);

/* Returns `empty` array `scoped` to either ~current~ call scope,
`coroutine` or `thread` termination. */
C_API arrays_t arrays(void);

#define $append(arr, value) array_append((arrays_t)arr, (void_t)value)
#define $remove(arr, index) array_remove((arrays_t)arr, index)

C_API values_type get_arg(void_t);

#define vectorize(vec) vectors_t vec = vector_variant()
#define vector(vec, count, ...) vectors_t vec = vector_for(nil, count, __VA_ARGS__)

#define $push_back(vec, value) vector_push_back((vectors_t)vec, (void_t)value)
#define $insert(vec, index, value) vector_insert((vectors_t)vec, index, (void_t)value)
#define $clear(vec) vector_clear((vectors_t)vec)
#define $size(vec) vector_size((vectors_t)vec)
#define $capacity(vec) vector_capacity((vectors_t)vec)
#define $erase(vec, index) vector_erase((vectors_t)vec, index)

#define in ,
#define foreach_xp(X, A) X A
#define foreach_in(X, S) values_type X; int i##X;  \
    for (i##X = 0; i##X < $size(S); i##X++)      \
        if ((X.object = S[i##X].object) || X.object == nullptr)
#define foreach_in_back(X, S) values_type X; int i##X; \
    for (i##X = $size(S) - 1; i##X >= 0; i##X--)     \
        if ((X.object = S[i##X].object) || X.object == nullptr)

/* The `foreach(`item `in` vector/array`)` macro, similar to `C#`,
executes a statement or a block of statements for each element in
an instance of `vectors_t/args_t/arrays_t` */
#define foreach(...) foreach_xp(foreach_in, (__VA_ARGS__))
#define foreach_back(...) foreach_xp(foreach_in_back, (__VA_ARGS__))

#define match(variable_type) switch (type_of(variable_type))
#define and(ENUM) case ENUM:
#define or(ENUM) break; case ENUM:
#define otherwise break; default:

#ifdef __cplusplus
    }
#endif
#endif /* RAII_H */
