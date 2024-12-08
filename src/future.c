#ifndef _WIN32
#   define _GNU_SOURCE
#endif

/*
Small future and promise library in C using emulated C11 threads

Modified from https://gist.github.com/Geal/8f85e02561d101decf9a
*/

#include "raii.h"

static promise *promise_create(void) {
    promise *p = calloc_default(1, sizeof(promise));
    p->scope = raii_local();
    atomic_flag_clear(&p->mutex);
    atomic_flag_clear(&p->done);
    p->type = RAII_PROMISE;

    return p;
}

static void promise_set(promise *p, void_t res, bool is_args) {
    atomic_lock(&p->mutex);
    if (!is_empty(res)) {
        if (is_args) {
            p->result = (raii_values_t *)res;
        } else {
            p->result = (raii_values_t *)calloc_full(p->scope, 1, sizeof(raii_values_t), RAII_FREE);
            p->result->value.object = res;
        }
    }

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
        if (!is_empty(p->scope->err))
            ex_throw(p->scope->err, "unknown", 0, "thrd_raii_wrapper", p->scope->panic, p->scope->backtrace);
    }
}

static future future_create(thrd_func_t start_routine) {
    future f = try_malloc(sizeof(struct _future));
    f->func = (thrd_func_t)start_routine;
    f->type = RAII_FUTURE;

    return f;
}

static void future_close(future f) {
    if (is_type(f, RAII_FUTURE) && thrd_join(f->thread, NULL) == thrd_success) {
        memset(f, 0, sizeof(f));
        RAII_FREE(f);
    }
}

static int thrd_raii_wrapper(void_t arg) {
    future_arg *f = (future_arg *)arg;
    int err = 0;
    void_t res = nullptr;
    f->value->scope->err = nullptr;
    rpmalloc_init();

    guard {
        res = f->func((args_t)f->arg);
    } guarded_exception(f->value->scope);

    promise_set(f->value, res, (is_type(f->arg, RAII_ARGS) && ((args_t)f->arg)->is_returning));
    if (is_type(f, RAII_FUTURE_ARG)) {
        memset(f, 0, sizeof(f));
        RAII_FREE(f);
    }

    raii_destroy();
    rpmalloc_thread_finalize(1);
    thrd_exit(err);
    return err;
}

static void thrd_start(future f, promise *value, void_t arg) {
    future_arg *f_arg = try_calloc(1, sizeof(future_arg));
    f_arg->func = f->func;
    f_arg->arg = arg;
    f_arg->value = value;
    f_arg->type = RAII_FUTURE_ARG;
    if (thrd_create(&f->thread, thrd_raii_wrapper, f_arg) != thrd_success)
        throw(future_error);
}

future thrd_async(thrd_func_t fn, void_t args) {
    promise *p = promise_create();
    future f = future_create(fn);
    f->value = p;
    thrd_start(f, p, args);
    return f;
}

values_type thrd_get(future f) {
    if (is_type(f, RAII_FUTURE) && !is_empty(f->value) && is_type(f->value, RAII_PROMISE)) {
        raii_values_t *r = promise_get(f->value);
        future_close(f);
        promise_close(f->value);
        if (is_empty(r))
            return thrd_value(0)->value;

        return r->value;
    }

    throw(logic_error);
}

RAII_INLINE bool thrd_is_done(future f) {
    return atomic_flag_load_explicit(&f->value->done, memory_order_relaxed);
}

RAII_INLINE void thrd_wait(future f, wait_func yield) {
    while (!thrd_is_done(f))
        yield();
}

RAII_INLINE raii_values_t *thrd_returning(args_t args, void_t value, size_t size) {
    raii_deferred(args->context, RAII_FREE, value);
    raii_values_t *result = (raii_values_t *)calloc_full(args->context, 1, sizeof(raii_values_t), RAII_FREE);
    result->value.object = value;
    result->size = size;
    result->extended = value;
    args->is_returning = true;
    return result;
}

RAII_INLINE values_type thrd_result(result_t value) {
    if (value->is_ready)
        return value->result->value;

    throw(logic_error);
}

RAII_INLINE uintptr_t thrd_self(void) {
#ifdef _WIN32
    return (uintptr_t)thrd_current().p;
#else
    return (uintptr_t)thrd_current();
#endif
}

#ifdef _WIN32
size_t thrd_cpu_count(void) {
    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);
    return (size_t)system_info.dwNumberOfProcessors;
}
#elif (defined(__linux__) || defined(__linux))
#include <sched.h>
size_t thrd_cpu_count(void) {
    cpu_set_t cpuset;
    sched_getaffinity(0, sizeof(cpuset), &cpuset);
    return CPU_COUNT(&cpuset);
}
#else
size_t thrd_cpu_count(void) {
    return sysconf(_SC_NPROCESSORS_ONLN);
}
#endif

future_t thrd_pool(size_t count, size_t queue_count) {
    unique_t *scope = unique_init();
    future_t pool = (future_t)calloc_full(scope, 1, sizeof(struct future_pool), RAII_FREE);
    future *futures = (future *)calloc_default(count, sizeof(futures[0]) * 2);
    size_t i;

    pool->thread_count = count;
    pool->cpu_present = thrd_cpu_count();
    for (i = 0; i < count; i++) {
        future f = future_create(nullptr);
        future_arg *f_arg = try_calloc(1, sizeof(future_arg));
        f_arg->queue = pool->queue;
        f_arg->type = RAII_FUTURE_ARG;
        if (thrd_create(&f->thread, thrd_raii_wrapper, f_arg) != thrd_success)
            throw(future_error);

        futures[i] = f;
    }

    pool->futures = futures;
    pool->type = RAII_POOL;

    return pool;
}

future_t thrd_scope(void) {
    unique_t *scope = unique_init(), *spawn = raii_init();
    future_t pool = (future_t)calloc_full(scope, 1, sizeof(struct future_pool), RAII_FREE);

    pool->scope = scope;
    pool->thread_count = 0;
    pool->cpu_present = thrd_cpu_count();
    pool->futures = NULL;
    pool->results = NULL;
    ex_protect_ptr(scope->protector, &pool->futures, RAII_FREE);
    ex_protect_ptr(scope->protector, &pool->results, RAII_FREE);
    pool->type = RAII_SPAWN;
    spawn->arena = pool;

    return pool;
}

result_t thrd_spawn(thrd_func_t fn, void_t args) {
    if (is_raii_empty() || !is_type(raii_local()->arena, RAII_SPAWN))
        raii_panic("No initialization, No `thrd_scope`!");

    future_t pool = (future_t)raii_local()->arena;
    if (pool->thread_count % pool->cpu_present == 0) {
        pool->futures = try_realloc(pool->futures, (pool->thread_count + pool->cpu_present) * sizeof(pool->futures[0]));
        pool->results = try_realloc(pool->results, (pool->thread_count + pool->cpu_present) * sizeof(pool->results[0]));
    }

    promise *p = promise_create();
    future f = future_create(fn);
    ex_protect_ptr(raii_local()->protector, &f, RAII_FREE);
    future_arg *f_arg = try_calloc(1, sizeof(future_arg));
    f_arg->func = f->func;
    f_arg->arg = args;
    f_arg->value = p;
    f_arg->type = RAII_FUTURE_ARG;
    f->value = p;
    if (thrd_create(&f->thread, thrd_raii_wrapper, f_arg) != thrd_success)
        throw(future_error);

    pool->futures[pool->thread_count] = f;
    pool->results[pool->thread_count] = (result_t)calloc_full(pool->scope, 1, sizeof(struct result_data), RAII_FREE);
    pool->results[pool->thread_count]->is_ready = false;
    pool->results[pool->thread_count]->result = NULL;
    pool->results[pool->thread_count]->type = RAII_VALUE;
    pool->thread_count++;

    return pool->results[pool->thread_count - 1];
}

result_t thrd_spawn_ex(thrd_func_t fn, const char *desc, ...) {
    va_list args;
    va_start(args, desc);
    args_t params = raii_args_ex(raii_local(), desc, args);
    va_end(args);

    return thrd_spawn(fn, (void_t)params);
}

future_t thrd_sync(future_t v) {
    if (!is_type(v, RAII_SPAWN))
        throw(logic_error);

    int i, x;
    memory_t *scope = v->scope;
    for (i = 0; i < v->thread_count; i++) {
        raii_values_t *result = promise_get(v->futures[i]->value);
        size_t size = result->size;
        v->results[i]->result = (raii_values_t *)calloc_full(scope, 1, sizeof(raii_values_t), RAII_FREE);
        if (size > 0) {
            v->results[i]->result->extended = calloc_full(scope, 1, size, RAII_FREE);
            memcpy(v->results[i]->result->extended, result->extended, size);
            v->results[i]->result->value.object = v->results[i]->result->extended;
        } else {
            v->results[i]->result->value.array = result->value.array;
        }

        v->results[i]->is_ready = true;
        future_close(v->futures[i]);
        promise_close(v->futures[i]->value);
    }

    raii_deferred_clean();
    return v;
}

void thrd_then(result_func_t callback, future_t iter, void_t result) {
    int i, max = iter->thread_count;
    for (i = 0; i < max; i++)
        if (iter->results[i]->is_ready)
            result = callback(result, i, iter->results[i]->result->value);
}

void thrd_destroy(future_t f) {
    if (is_type(f, RAII_SPAWN)) {
        memory_t *scope = f->scope;
        ex_ptr_t *err = scope->protector;
        f->type = -1;
        RAII_FREE(f->futures);
        RAII_FREE(f->results);
        ex_unprotected_ptr(err);
        raii_delete(scope);
    }
}

RAII_INLINE bool thrd_is_finish(future_t f) {
    size_t i;
    for (i = 0; i < f->thread_count; i++) {
        if (!thrd_is_done(f->futures[i]))
            return false;
    }

    return true;
}
