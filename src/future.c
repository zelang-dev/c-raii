#include "raii.h"

static promise *promise_create(void) {
    promise *p = calloc_default(1, sizeof(promise));
    atomic_flag_clear(&p->mutex);
    atomic_flag_clear(&p->done);
    p->type = RAII_PROMISE;

    return p;
}

static void promise_set(promise *p, void_t res) {
    atomic_lock(&p->mutex);
    if (!is_empty(res))
        p->result->value.object = res;

    atomic_unlock(&p->mutex);
    atomic_flag_test_and_set(&p->done);
}

static RAII_INLINE raii_values_t *promise_get(promise *p) {
    while (!atomic_flag_load(&p->done))
        thrd_yield();

    return p->result;
}

static void promise_close(promise *p) {
    if (is_type(p, RAII_PROMISE)) {
        p->type = -1;
        atomic_flag_clear(&p->mutex);
        atomic_flag_clear(&p->done);
    }
}

static future *future_create(thrd_func_t start_routine) {
    future *f = try_malloc(sizeof(future));
    f->func = (thrd_func_t)start_routine;
    f->type = RAII_FUTURE;

    return f;
}

static void future_close(future *f) {
    if (is_type(f, RAII_FUTURE) && thrd_join(f->thread, NULL) == thrd_success) {
        memset(f, 0, sizeof(f));
        RAII_FREE(f);
    }
}

static int raii_wrapper(void_t arg) {
    future_arg *f = (future_arg *)arg;
    int err = 0;
    void_t res = nullptr;
    rpmalloc_thread_initialize();

    guard {
        res = f->func((args_t)f->arg);
    } guarded;

    promise_set(f->value, res);
    if (is_type(f, RAII_FUTURE_ARG)) {
        memset(f, 0, sizeof(f));
        RAII_FREE(f);
    }

    raii_destroy();
    rpmalloc_thread_finalize(1);
    thrd_exit(err);
    return err;
}

static void thrd_start(future *f, promise *value, void_t arg) {
    future_arg *f_arg = try_malloc(sizeof(future_arg));
    f_arg->func = f->func;
    f_arg->arg = arg;
    f_arg->value = value;
    f_arg->type = RAII_FUTURE_ARG;
    int r = thrd_create(&f->thread, raii_wrapper, f_arg);
}

future *thrd_async(thrd_func_t fn, void_t args) {
    promise *p = promise_create();
    future *f = future_create(fn);
    f->value = p;
    thrd_start(f, p, args);
    return f;
}

values_type thrd_get(future *f) {
    if (is_type(f, RAII_FUTURE) && !is_empty(f->value) && is_type(f->value, RAII_PROMISE)) {
        raii_values_t *r = promise_get(f->value);
        future_close(f);
        promise_close(f->value);
        return r->value;
    }

    throw(logic_error);
}

RAII_INLINE bool thrd_is_done(future *f) {
    return atomic_flag_load_explicit(&f->value->done, memory_order_relaxed);
}

RAII_INLINE void thrd_wait(future *f, wait_func yield) {
    while (!thrd_is_done(f))
        yield();
}

RAII_INLINE raii_values_t *thrd_value(uintptr_t value) {
    return (raii_values_t *)value;
}

RAII_INLINE uintptr_t thrd_self(void) {
#ifdef _WIN32
    return (uintptr_t)thrd_current().p;
#else
    return (uintptr_t)thrd_current();
#endif
}

future_t *thrd_for(thrd_func_t fn, size_t times, const char *desc, ...) {
    future_t *pool = (future_t *)calloc_default(1, sizeof(future_t));
    pool->futures = (future **)calloc_default(times, sizeof(pool->futures[0]) * 2);
    args_t params;
    va_list args;
    size_t i;

    for (i = 0; i < times; i++) {
        va_start(args, desc);
        params = raii_args_ex(nullptr, desc, args);
        pool->futures[i] = thrd_async(fn, (void_t)params);
    }
    va_end(args);

    pool->thread_count = times;
    pool->type = RAII_POOL;
    return pool;
}

thrd_values *thrd_sync(future_t *v) {
    if (is_type(v, RAII_POOL)) {
        thrd_values *summary = (thrd_values *)calloc_default(v->thread_count, sizeof(thrd_values));
        summary->values = (raii_values_t **)calloc_default(v->thread_count, sizeof(summary->values[0]) * 2);
        size_t i;

        for (i = 0; i < v->thread_count; i++) {
            summary->values[i] = promise_get(v->futures[i]->value);
            future_close(v->futures[i]);
            promise_close(v->futures[i]->value);
        }

        summary->value_count = v->thread_count;
        summary->type = RAII_SYNC;
        return summary;
    }

    throw(logic_error);
}

values_type thrd_then(result_func_t callback, thrd_values *iter, void_t result) {
    size_t i, max = iter->value_count;

    for (i = 0; i < max; i++)
        result = callback(result, i, iter->values[i]->value);

    iter->values[max]->value.object = result;
    return iter->values[max]->value;
}

RAII_INLINE bool thrd_is_finish(future_t *f) {
    size_t i;
    for (i = 0; i < f->thread_count; i++) {
        if (!thrd_is_done(f->futures[i]))
            return false;
    }

    return true;
}
