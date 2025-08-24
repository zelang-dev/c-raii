#ifndef _WIN32
#   define _GNU_SOURCE
#endif

/*
Small future and promise library in C using emulated C11 threads

Modified from https://gist.github.com/Geal/8f85e02561d101decf9a
*/

#include "raii.h"

struct future_pool {
    raii_type type;
    arrays_t jobs;
    memory_t *scope;
    future *futures;
    raii_deque_t *queue;
};

promise *promise_create(memory_t *scope) {
    promise *p = malloc_full(scope, sizeof(promise), RAII_FREE);
    p->scope = scope;
    atomic_flag_clear(&p->mutex);
    atomic_flag_clear(&p->done);
    p->type = RAII_PROMISE;

    return p;
}

void promise_set(promise *p, void_t res) {
    atomic_lock(&p->mutex);
    p->result = (raii_values_t *)calloc_full(p->scope, 1, sizeof(raii_values_t), RAII_FREE);
    if (!is_empty(res)) {
        if (is_args(res) || is_vector(res)) {
            p->result->is_vectored = is_vector(res);
            p->result->is_arrayed = !p->result->is_vectored;
            p->result->value.object = vector_copy(p->scope, (vectors_t)p->result->extended, res);
        } else {
            p->result->value.object = ((raii_values_t *)res)->value.object;
        }
    }
    atomic_unlock(&p->mutex);
    atomic_flag_test_and_set(&p->done);
}

void promise_erred(promise *p, ex_context_t err) {
	atomic_lock(&p->mutex);
	p->scope->err = (void_t)err.ex;
	p->scope->panic = err.panic;
	p->scope->backtrace = err.backtrace;
	atomic_unlock(&p->mutex);
	atomic_flag_test_and_set(&p->done);
}

static RAII_INLINE raii_values_t *promise_get(promise *p) {
    while (!atomic_flag_load(&p->done))
        thrd_yield();

    return p->result;
}

void promise_close(promise *p) {
    if (is_type(p, RAII_PROMISE)) {
        p->type = RAII_ERR;
        atomic_flag_clear(&p->mutex);
        atomic_flag_clear(&p->done);
        if (!is_empty(p->scope->err))
            ex_throw(p->scope->err, "unknown", 0, "future_wrapper", p->scope->panic, p->scope->backtrace);
    }
}

future future_create(thrd_func_t start_routine) {
	future f = try_malloc(sizeof(struct _future));
	atomic_flag_clear(&f->started);
    f->is_pool = 0;
    f->func = (thrd_func_t)start_routine;
    f->type = RAII_FUTURE;

    return f;
}

static void future_close(future f) {
    if (is_type(f, RAII_FUTURE)) {
        if (!f->is_pool)
            thrd_join(f->thread, NULL);

        memset(f, 0, sizeof(*f));
        RAII_FREE(f);
    }
}

static int thrd_raii_wrapper(void_t arg) {
    worker_t *f = (worker_t *)arg;
    int err = 0;
    template_t res[1] = {0};
    f->value->scope->err = nullptr;
    raii_init()->threading++;
	raii_local()->queued = f->queue;
	raii_local()->local = f->arg;
    rpmalloc_init();

    guard {
        args_destructor_set(f->arg);
        res->object = f->func((args_t)f->arg);
    	promise_set(f->value, res->object);
    } guarded_exception(f->value);

    if (is_type(f, RAII_FUTURE_ARG)) {
        memset(f, 0, sizeof(*f));
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

future thrd_async(thrd_func_t fn, size_t num_of_args, ...) {
    va_list ap;

    va_start(ap, num_of_args);
    args_t args = args_ex(num_of_args, ap);
    va_end(ap);

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

RAII_INLINE future thrd_launch(thrd_func_t fn, void_t args) {
    args_t params = args_for(1, args);
    return thrd_async_ex(get_scope(), fn, params);
}

template_t thrd_get(future f) {
    if (is_type(f, RAII_FUTURE) && !is_empty(f->value) && is_type(f->value, RAII_PROMISE)) {
        raii_values_t *r = promise_get(f->value);
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

RAII_INLINE void thrd_until(future fut) {
    thrd_wait(fut, coro_yield_info);
}

vectors_t thrd_data(size_t numof, ...) {
    va_list ap;
    vectors_t args = nullptr;
    size_t i;
    bool is_arguments = is_args(raii_init()->local);
	if (!is_arguments) {
		va_start(ap, numof);
        args = args_ex(numof, ap);
        va_end(ap);
	} else {
        args = raii_local()->local;
        $clear(args);
        va_start(ap, numof);
        for (i = 0; i < numof; i++)
            vector_push_back(args, va_arg(ap, void_t));
        va_end(ap);
    }

    return args;
}

RAII_INLINE template_t thrd_result(rid_t id) {
    result_t value = raii_result_get(id);
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

void thrd_set_result(raii_values_t *r, int id) {
    result_t *results = (result_t *)atomic_load_explicit(&gq_result.results, memory_order_acquire);
    results[id]->result = (raii_values_t *)calloc_full(gq_result.scope, 1, sizeof(raii_values_t), RAII_FREE);
    if (r->is_arrayed || r->is_vectored) {
        results[id]->result = r;
    } else if (!is_zero(r->value.integer)) {
        results[id]->result->value.object = r->value.object;
    }

    results[id]->is_ready = true;
    atomic_store_explicit(&gq_result.results, results, memory_order_release);
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

        queue = coro_pool_init(0);
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

rid_t thrd_spawn(thrd_func_t fn, size_t num_of_args, ...) {
    va_list ap;
    if (is_raii_empty() || !is_type(raii_local()->threaded, RAII_SPAWN))
        raii_panic("No initialization, No `thrd_scope`!");

    va_start(ap, num_of_args);
    args_t args = args_ex(num_of_args, ap);
    va_end(ap);

    unique_t *scope = raii_local();
    future_t pool = scope->threaded;
    result_t result = raii_result_create();
    rid_t result_id = result->id;

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

    $append_unsigned(pool->jobs, result_id);
    pool->futures[result_id] = f;
   // if (!is_empty(queue->local))
       // thrd_add(f, queue, f_work, nil, nil);
    //else
    if (thrd_create(&f->thread, thrd_raii_wrapper, f_work) != thrd_success)
        throw(future_error);

    return result_id;
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
            result = callback(result, i, results[i]->result->value.object);
    }
}

void thrd_destroy(future_t f) {
    if (is_type(f, RAII_SPAWN)) {
        memory_t *scope = f->scope;
        f->type = RAII_ERR;
        raii_delete(scope);
        RAII_FREE(f->futures);
        RAII_FREE(f);
    }
}

void thrd_delete(future f) {
    if (is_type(f, RAII_FUTURE)) {
        f->type = RAII_ERR;
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
