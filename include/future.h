
#ifndef _FUTURE_H
#define _FUTURE_H

#include "rtypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Calls ~fn~ (with ~number of args~ then ~actaul arguments~) in separate thread, returning without waiting
for the execution of ~fn~ to complete. The value returned by ~fn~ can be accessed
by calling `thrd_get()`. */
C_API future thrd_async(thrd_func_t fn, size_t, ...);

/* Same as `thrd_async`, allows passing custom `context` scope for internal `promise`
for auto cleanup within caller's `scope`. */
C_API future thrd_async_ex(memory_t *scope, thrd_func_t fn, void_t args);

/* Calls ~fn~ (with ~args~ as argument) in separate thread, returning without waiting
for the execution of ~fn~ to complete. The value returned by ~fn~ can be accessed
by calling `thrd_get()`. */
C_API future thrd_launch(thrd_func_t fn, void_t args);

/* Returns the value of `future` ~promise~, a thread's shared object, If not ready, this
function blocks the calling thread and waits until it is ready. */
C_API template_t thrd_get(future);

/* This function blocks the calling thread and waits until `future` is ready,
will execute provided `yield` callback function continuously. */
C_API void thrd_wait(future, wait_func yield);

/* Same as `thrd_wait`, but `yield` execution to another coroutine and reschedule current,
until `thread` ~future~ is ready, completed execution. */
C_API void thrd_until(future);

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
C_API template_t thrd_result(rid_t id);
C_API void thrd_set_result(raii_values_t *, int);

C_API future_t thrd_for(for_func_t loop, intptr_t initial, intptr_t times);

C_API void thrd_then(result_func_t callback, future_t iter, void_t result);
C_API void thrd_destroy(future_t);
C_API bool thrd_is_finish(future_t);

#ifdef __cplusplus
}
#endif
#endif /* _FUTURE_H */
