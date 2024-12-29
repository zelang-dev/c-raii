#ifndef _WIN32
#   define _GNU_SOURCE
#endif

/*
Small future and promise library in C using emulated C11 threads

Modified from https://gist.github.com/Geal/8f85e02561d101decf9a
*/

#include "raii.h"
future_results_t gq_result = {0};

struct future_pool {
    raii_type type;
    arrays_t jobs;
    memory_t *scope;
    future *futures;
    raii_deque_t *queue;
};

struct _future {
    raii_type type;
    int id;
    int is_pool;
    thrd_t thread;
    thrd_func_t func;
    memory_t *scope;
    promise *value;
};

static promise *promise_create(memory_t *scope) {
    promise *p = malloc_full(scope, sizeof(promise), RAII_FREE);
    p->scope = scope;
    atomic_flag_clear(&p->mutex);
    atomic_flag_clear(&p->done);
    p->type = RAII_PROMISE;

    return p;
}

static void promise_set(promise *p, void_t res, bool is_args) {
    atomic_lock(&p->mutex);
    if (is_args) {
        p->result = (raii_values_t *)res;
    } else {
        p->result = (raii_values_t *)malloc_full(p->scope, sizeof(raii_values_t), RAII_FREE);
        if (!is_empty(res))
            p->result->value.object = ((raii_values_t *)res)->value.object;

        p->result->size = 0;
        p->result->extended = nullptr;
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
    f->is_pool = 0;
    f->func = (thrd_func_t)start_routine;
    f->type = RAII_FUTURE;

    return f;
}

static void future_close(future f) {
    if (is_type(f, RAII_FUTURE)) {
        if (!f->is_pool)
            thrd_join(f->thread, NULL);

        memset(f, 0, sizeof(f));
        RAII_FREE(f);
    }
}

static int thrd_raii_wrapper(void_t arg) {
    worker_t *f = (worker_t *)arg;
    int i, tid, err = 0;
    void_t res = nullptr;
    tid = f->id;
    f->value->scope->err = nullptr;
    raii_local()->threading++;
    raii_local()->queued = f->queue;
    rpmalloc_init();

    guard {
        res = f->func((args_t)f->arg);
    } guarded_exception(f->value->scope);

    promise_set(f->value, res, is_args_returning((args_t)f->arg));
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
    worker_t *f_work = try_malloc(sizeof(worker_t));
    f_work->func = f->func;
    f_work->arg = arg;
    f_work->value = value;
    f_work->type = RAII_FUTURE_ARG;
    if (thrd_create(&f->thread, thrd_raii_wrapper, f_work) != thrd_success)
        throw(future_error);
}

future thrd_async(thrd_func_t fn, void_t args) {
    promise *p = promise_create(get_scope());
    future f = future_create(fn);
    f->value = p;
    thrd_start(f, p, args);
    return f;
}

future thrd_async_ex(memory_t *scope, thrd_func_t fn, void_t args) {
    promise *p = promise_create(scope);
    future f = future_create(fn);
    f->value = p;
    thrd_start(f, p, args);
    return f;
}

values_type thrd_get(future f) {
    if (is_type(f, RAII_FUTURE) && !is_empty(f->value) && is_type(f->value, RAII_PROMISE)) {
        raii_values_t *r = promise_get(f->value);
        f->scope = unique_init();
        size_t size = r->size;
        raii_values_t *result = (raii_values_t *)malloc_full(f->scope, sizeof(raii_values_t), RAII_FREE);
        if (size > 0) {
            result->extended = malloc_full(f->scope, size, RAII_FREE);
            memcpy(result->extended, r->extended, size);
            result->value.object = result->extended;
        } else {
            result->value.array = r->value.array;
        }

        if (thrd_join(f->thread, NULL) == thrd_success) {
            if (!is_empty(f->value->scope->err))
                raii_defer((func_t)thrd_delete, f);
            promise_close(f->value);
        }

        thrd_delete(f);

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
    raii_deferred(vector_scope(args), RAII_FREE, value);
    raii_values_t *result = (raii_values_t *)malloc_full(vector_scope(args), sizeof(raii_values_t), RAII_FREE);
    result->value.object = value;
    result->size = size;
    result->extended = value;
    args_returning_set(args);
    return result;
}

RAII_INLINE raii_values_t *thrd_data(void_t value) {
    if (is_empty(value))
        return (raii_values_t *)raii_values_empty;

    raii_values_t *result = (raii_values_t *)malloc_local(sizeof(raii_values_t));
    result->value.object = value;
    result->size = 0;
    return result;
}

RAII_INLINE raii_values_t *thrd_value(uintptr_t value) {
    if (is_zero(value))
        return (raii_values_t *)raii_values_empty;

    raii_values_t *result = (raii_values_t *)malloc_local(sizeof(raii_values_t));
    result->value.ulong_long = value;
    result->size = 0;
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

void thrd_set_result(raii_values_t *result, int id) {
    result_t *results = (result_t *)atomic_load_explicit(&gq_result.results, memory_order_acquire);
    results[id]->result = (raii_values_t *)calloc_full(gq_result.scope, 1, sizeof(raii_values_t), RAII_FREE);
    size_t size = result->size;
    if (size > 0) {
        results[id]->result->extended = calloc_full(gq_result.scope, 1, size, RAII_FREE);
        memcpy(results[id]->result->extended, result->extended, size);
        results[id]->result->value.object = results[id]->result->extended;
    } else if (!is_zero(result->value.integer)) {
        results[id]->result->value.object = result->value.object;
    }

    results[id]->is_ready = true;
    atomic_store_explicit(&gq_result.results, results, memory_order_release);
}

static result_t thrd_get_result(int id) {
    return (result_t)atomic_load_explicit(&gq_result.results[id], memory_order_relaxed);
}

static result_t thrd_result_create(int id) {
    result_t result, *results;
    result = (result_t)malloc_full(gq_result.scope, sizeof(struct result_data), RAII_FREE);
    results = (result_t *)atomic_load_explicit(&gq_result.results, memory_order_acquire);
    if (id % gq_result.queue_size == 0)
        results = try_realloc(results, (id + gq_result.queue_size) * sizeof(results[0]));

    result->is_ready = false;
    result->id = id;
    result->result = nullptr;
    result->type = RAII_VALUE;
    results[id] = result;
    atomic_store_explicit(&gq_result.results, results, memory_order_release);
    return result;
}

future_t thrd_scope(void) {
    raii_deque_t *queue = nullptr;
    future_t pool = nullptr;
    unique_t *scope, *spawn = raii_init();

    if (!is_empty(spawn->queued) && is_type(spawn->queued, RAII_POOL)) {
        queue = spawn->queued;
    } else {
        if (raii_local()->threading)
            throw(logic_error);

        thrd_init(0);
        queue = raii_local()->queued;
    }

    scope = unique_init();
    pool = (future_t)try_calloc(1, sizeof(struct future_pool));
    pool->futures = nullptr;
    pool->scope = scope;
    pool->queue = queue;
    pool->jobs = array_of(scope, 0);
    array_deferred_set(pool->jobs, scope);
    pool->type = RAII_SPAWN;
    spawn->threaded = pool;

    return pool;
}

result_t thrd_spawn(thrd_func_t fn, void_t args) {
    if (is_raii_empty() || !is_type(raii_local()->threaded, RAII_SPAWN))
        raii_panic("No initialization, No `thrd_scope`!");

    unique_t *scope = raii_local();
    future_t pool = scope->threaded;
    size_t tid, result_id = atomic_fetch_add(&gq_result.result_count, 1);
    result_t result = thrd_result_create(result_id);

    if (result_id % gq_result.queue_size == 0 || is_empty(pool->futures))
        pool->futures = try_realloc(pool->futures, (result_id + gq_result.queue_size) * sizeof(pool->futures[0]));

    promise *p = promise_create(pool->scope);
    future f = future_create(fn);
    p->id = result_id;
    f->value = p;
    f->id = result_id;

    worker_t *f_work = try_malloc(sizeof(worker_t));
    f_work->func = f->func;
    f_work->arg = args;
    f_work->value = p;
    f_work->queue = scope->queued;
    f_work->type = RAII_FUTURE_ARG;

    $append(pool->jobs, result_id);
    pool->futures[result_id] = f;
   // if (!is_empty(queue->local))
       // thrd_add(f, queue, f_work, nil, nil);
    //else
    if (thrd_create(&f->thread, thrd_raii_wrapper, f_work) != thrd_success)
        throw(future_error);

    return result;
}

future_t thrd_sync(future_t pool) {
    if (!is_type(pool, RAII_SPAWN))
        throw(logic_error);

    int i;
    foreach(num in pool->jobs) {
        i = num.integer;
        raii_values_t *result = promise_get(pool->futures[i]->value);
        thrd_set_result(result, i);
        future_close(pool->futures[i]);
        if (!is_empty(pool->futures[i]->value->scope->err))
            raii_defer((func_t)thrd_delete, pool->futures[i]);

        promise_close(pool->futures[i]->value);
    }

    raii_deferred_clean();
    deferring((func_t)thrd_destroy, pool);
    return pool;
}

void thrd_then(result_func_t callback, future_t iter, void_t result) {
    int i;
    result_t *results = (result_t *)atomic_load_explicit(&gq_result.results, memory_order_relaxed);
    foreach(num in iter->jobs) {
        i = num.integer;
        if (results[i]->is_ready)
            result = callback(result, i, results[i]->result->value);
    }
}

void thrd_destroy(future_t f) {
    if (is_type(f, RAII_SPAWN)) {
        memory_t *scope = f->scope;
        f->type = -1;
        raii_delete(scope);
        RAII_FREE(f->futures);
        RAII_FREE(f);
    }
}

void thrd_delete(future f) {
    if (is_type(f, RAII_FUTURE)) {
        f->type = -1;
        if (!is_empty(f->scope))
            raii_delete(f->scope);

        RAII_FREE(f);
    }
}

RAII_INLINE bool thrd_is_finish(future_t f) {
    size_t i;
    foreach(num in f->jobs) {
        i = num.max_size;
        if (!thrd_is_done(f->futures[i]))
            return false;
    }

    return true;
}
