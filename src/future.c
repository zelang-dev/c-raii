#ifndef _WIN32
#   define _GNU_SOURCE
#endif

/*
Small future and promise library in C using emulated C11 threads

Modified from https://gist.github.com/Geal/8f85e02561d101decf9a
*/

#include "raii.h"

static volatile bool thrd_queue_set = false;
static future_deque_t *thrd_queue_global = {0};
struct future_deque {
    raii_type type;
    int cpus;
    size_t queue_size;
    thrd_t thread;
    memory_t *scope;
    future_deque_t **local;
    atomic_flag started;
    atomic_flag shutdown;
    atomic_size_t cpu_core;
    atomic_size_t available;
    atomic_size_t top, bottom;
    atomic_size_t result_count;
    atomic_result_t *results;
    atomic_future_t array;
};

struct future_pool {
    raii_type type;
    int start;
    int end;
    memory_t *scope;
    future *futures;
    future_deque_t *queue;
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

/* These are non-NULL pointers that will result in page faults under normal
 * circumstances, used to verify that nobody uses non-initialized entries.
 */
static worker_t *RAII_EMPTY = (worker_t *)0x300, *RAII_ABORT = (worker_t *)0x400;

static void deque_resize(future_deque_t *q) {
    future_array_t *a = (future_array_t *)atomic_load_explicit(&q->array, memory_order_relaxed);
    size_t old_size = a->size;
    size_t new_size = old_size * 2;
    future_array_t *new = try_malloc(sizeof(future_array_t) + sizeof(worker_t *) * new_size);
    atomic_init(&new->size, new_size);
    size_t i, t = atomic_load_explicit(&q->top, memory_order_relaxed);
    size_t b = atomic_load_explicit(&q->bottom, memory_order_relaxed);
    for (i = t; i < b; i++)
        new->buffer[i % new_size] = a->buffer[i % old_size];

    atomic_store_explicit(&q->array, new, memory_order_relaxed);
    RAII_FREE(a);
}

static void deque_push(future_deque_t *q, worker_t *w) {
    size_t b = atomic_load_explicit(&q->bottom, memory_order_relaxed);
    size_t t = atomic_load_explicit(&q->top, memory_order_acquire);
    future_array_t *a = (future_array_t *)atomic_load_explicit(&q->array, memory_order_relaxed);
    if (b - t > a->size - 1) { /* Full queue */
        deque_resize(q);
        a = (future_array_t *)atomic_load_explicit(&q->array, memory_order_relaxed);
    }
    atomic_store_explicit(&a->buffer[b % a->size], w, memory_order_relaxed);
    atomic_thread_fence(memory_order_release);
    atomic_store_explicit(&q->bottom, b + 1, memory_order_relaxed);
}

static worker_t *deque_take(future_deque_t *q) {
    size_t b = atomic_load_explicit(&q->bottom, memory_order_relaxed) - 1;
    size_t t = atomic_load_explicit(&q->top, memory_order_relaxed);
    future_array_t *a = (future_array_t *)atomic_load_explicit(&q->array, memory_order_relaxed);
    atomic_store_explicit(&q->bottom, b, memory_order_relaxed);
    atomic_thread_fence(memory_order_seq_cst);
    worker_t *x;
    if (t <= b) {
        /* Non-empty queue */
        x = (worker_t *)atomic_load_explicit(&a->buffer[b % a->size], memory_order_relaxed);
        if (t == b) {
            /* Single last element in queue */
            if (!atomic_compare_exchange_strong_explicit(&q->top, &t, t + 1,
                                                         memory_order_seq_cst,
                                                         memory_order_relaxed))
                /* Failed race */
                x = RAII_EMPTY;
            atomic_store_explicit(&q->bottom, b + 1, memory_order_relaxed);
        }
    } else { /* Empty queue */
        x = RAII_EMPTY;
        atomic_store_explicit(&q->bottom, b + 1, memory_order_relaxed);
    }
    return x;
}

static worker_t *deque_steal(future_deque_t *q) {
    size_t t = atomic_load_explicit(&q->top, memory_order_acquire);
    atomic_thread_fence(memory_order_seq_cst);
    size_t b = atomic_load_explicit(&q->bottom, memory_order_acquire);
    worker_t *x = RAII_EMPTY;
    if (t < b) {
        /* Non-empty queue */
        future_array_t *a = (future_array_t *)atomic_load_explicit(&q->array, memory_order_consume);
        x = (worker_t *)atomic_load_explicit(&a->buffer[t % a->size], memory_order_relaxed);
        if (!atomic_compare_exchange_strong_explicit(
            &q->top, &t, t + 1, memory_order_seq_cst, memory_order_relaxed))
            /* Failed race */
            return RAII_ABORT;
    }
    return x;
}

static void deque_init(future_deque_t *q, int size_hint) {
    atomic_init(&q->top, 0);
    atomic_init(&q->bottom, 0);
    future_array_t *a = try_malloc(sizeof(future_array_t) + sizeof(worker_t *) * size_hint);
    atomic_init(&a->size, size_hint);
    atomic_init(&q->array, a);
    atomic_init(&q->result_count, 0);
    atomic_init(&q->available, 0);
    atomic_init(&q->cpu_core, 0);
    atomic_init(&q->results, nullptr);
    atomic_flag_clear(&q->shutdown);
    atomic_flag_clear(&q->started);
    q->cpus = -1;
    q->queue_size = size_hint;
    q->local = nullptr;
    q->type = RAII_POOL;
}

static void deque_free(future_deque_t *q) {
    future_array_t *a = nullptr;
    result_t *r = nullptr;
    if (!is_empty(q)) {
        a = atomic_get(future_array_t *, &q->array);
        r = atomic_get(result_t *, &q->results);
        if (!is_empty(a)) {
            atomic_store(&q->array, nullptr);
            RAII_FREE((void_t)a);
        }

        if (!is_empty(r)) {
            atomic_store(&q->results, nullptr);
            RAII_FREE((void_t)r);
        }

        memset(q, 0, sizeof(q));
        RAII_FREE(q);
    }
}

static void deque_destroy(void) {
    if (is_type(thrd_queue_global, RAII_POOL)) {
        int i;
        future_deque_t *queue = thrd_queue_global;
        memory_t *scope = queue->scope;
        queue->type = -1;
        if (!is_empty(queue->local)) {
            for (i = 0; i < queue->cpus; i++)
                atomic_flag_test_and_set(&queue->local[i]->shutdown);
        }

        atomic_flag_test_and_set(&queue->shutdown);
        raii_delete(scope);
    }
}

static promise *promise_create(void) {
    promise *p = malloc_local(sizeof(promise));
    p->scope = get_scope();
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
    worker_t *f, *pool = (worker_t *)arg;
    volatile bool try_stealing = false, is_pool = is_empty(pool->func) && is_empty(pool->value)
        && !is_empty(pool->queue) && is_type(pool->queue, RAII_POOL);
    int i, tid, err = 0;
    void_t res = nullptr;
    memory_t *local = nullptr;
    f = (worker_t *)arg;
    tid = f->id;
    if (!is_pool) {
        f->value->scope->err = nullptr;
        raii_init()->queued = f->queue;
    }

    raii_init()->threading++;
    rpmalloc_init();

    if (is_pool)
        while (!atomic_flag_load(&pool->queue->started))
            ;

    do {
        if (is_pool) {
            if (!try_stealing) {
                while (atomic_load_explicit(&pool->queue->available, memory_order_relaxed) == 0
                       && !atomic_flag_load_explicit(&pool->queue->shutdown, memory_order_relaxed))
                    thrd_yield();

                if (atomic_flag_load(&pool->queue->shutdown) || atomic_flag_load(&thrd_queue_global->shutdown))
                    break;

                atomic_fetch_sub(&pool->queue->available, 1);
            }

            f = deque_take(pool->queue);
            /* Currently, there is no work present in my own queue */
            if (f == RAII_EMPTY) {
                for (i = 0; i < thrd_queue_global->cpus; i++) {
                    if (i == tid)
                        continue;
                    f = deque_steal(thrd_queue_global->local[i]);
                    if (f == RAII_ABORT) {
                        --i;
                        continue; /* Try again at the same i */
                    } else if (f == RAII_EMPTY) {
                        continue;
                    }

                    /* Found some work to do */
                    break;
                }

                if (f == RAII_EMPTY) {
                    /* Despite the previous observation of all queues being devoid
                     * of tasks during the last examination, there exists
                     * a possibility that additional work items have been introduced
                     * subsequently. To account for this scenario, a state of active
                     * waiting is adopted, wherein the program continues to loop
                     * until the current queue "available" flag becomes set, indicative of
                     * potential new work additions.
                     */
                    try_stealing = false;
                    continue;
                }
            }

            pool->queue->scope = f->local;
            raii_local()->local = f->local;
            raii_local()->queued = f->queue;
            f->value->scope->err = nullptr;
            res = nullptr;
        }

        guard {
            res = f->func((args_t)f->arg);
        } guarded_exception(f->value->scope);

        promise_set(f->value, res, is_args_returning((args_t)f->arg));
        if (is_type(f, RAII_FUTURE_ARG)) {
            if (is_pool) {
                if (!is_empty(f->local))
                    raii_delete(f->local);

                if (atomic_load(&pool->queue->available) == 0)
                    try_stealing = true;
            }

            memset(f, 0, sizeof(f));
            RAII_FREE(f);
        }
    } while (is_pool);

    if (is_pool)
        RAII_FREE(arg);

    raii_destroy();
    rpmalloc_thread_finalize(1);
    thrd_exit(err);
    return err;
}

static void thrd_start(future f, promise *value, void_t arg) {
    worker_t *f_arg = try_malloc(sizeof(worker_t));
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

void thrd_init(size_t queue_size) {
    if (!thrd_queue_set) {
        thrd_queue_set = true;
        size_t i, cpu = thrd_cpu_count();
        unique_t *scope = unique_init(), *global = raii_init();
        future_deque_t **local, *queue = (future_deque_t *)malloc_full(scope, sizeof(future_deque_t), (func_t)deque_free);
        deque_init(queue, cpu);
        if (queue_size > 0) {
            local = (future_deque_t **)calloc_full(scope, cpu, sizeof(local[0]), RAII_FREE);
            for (i = 0; i < cpu; i++) {
                local[i] = (future_deque_t *)malloc_full(scope, sizeof(future_deque_t), (func_t)deque_free);
                deque_init(local[i], queue_size);
                local[i]->cpus = cpu;
                local[i]->scope = nullptr;
                worker_t *f_arg = try_malloc(sizeof(worker_t));
                f_arg->func = nullptr;
                f_arg->id = (int)i;
                f_arg->value = nullptr;
                f_arg->queue = local[i];
                f_arg->type = RAII_FUTURE_ARG;

                if (thrd_create(&local[i]->thread, thrd_raii_wrapper, f_arg) != thrd_success)
                    throw(future_error);
            }
            queue->local = local;
        }

        queue->scope = scope;
        queue->cpus = cpu;
        queue->queue_size = !queue_size ? cpu * cpu : queue_size;
        global->queued = queue;
        thrd_queue_global = queue;
        atexit(deque_destroy);
    } else if (raii_local()->threading) {
        throw(logic_error);
    }
}

future_t thrd_scope(void) {
    future_deque_t *queue = nullptr;
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
    pool->start = atomic_load(&queue->result_count);
    pool->end = pool->start;
    pool->type = RAII_SPAWN;
    spawn->threaded = pool;

    return pool;
}

result_t thrd_spawn(thrd_func_t fn, void_t args) {
    if (is_raii_empty() || !is_type(raii_local()->threaded, RAII_SPAWN))
        raii_panic("No initialization, No `thrd_scope`!");

    unique_t *scope = raii_local();
    future_t pool = scope->threaded;
    future_deque_t *queue = scope->queued;
    size_t tid, result_id, result_count;
    result_t result, *results;

    result = (result_t)malloc_full(queue->scope, sizeof(struct result_data), RAII_FREE);
    result_count = atomic_fetch_add(&queue->result_count, 1) + 1;
    result_id = result_count - 1;
    results = (result_t *)atomic_load_explicit(&queue->results, memory_order_acquire);
    if (result_id % queue->queue_size == 0)
        results = try_realloc(results, (result_id + queue->queue_size) * sizeof(results[0]));

    result->is_ready = false;
    result->id = result_id;
    result->result = NULL;
    result->type = RAII_VALUE;
    results[result_id] = result;
    atomic_store_explicit(&queue->results, results, memory_order_release);
    if (result_id % queue->queue_size == 0 || is_empty(pool->futures))
        pool->futures = try_realloc(pool->futures, (result_id + queue->queue_size) * sizeof(pool->futures[0]));

    promise *p = malloc_full(pool->scope, sizeof(promise), RAII_FREE);
    atomic_flag_clear(&p->mutex);
    atomic_flag_clear(&p->done);
    p->scope = pool->scope;
    p->type = RAII_PROMISE;

    future f = future_create(fn);
    worker_t *f_arg = try_malloc(sizeof(worker_t));
    f_arg->func = f->func;
    f_arg->arg = args;
    f_arg->value = p;
    f_arg->queue = queue;
    f_arg->type = RAII_FUTURE_ARG;
    f->value = p;
    f->id = result_id;
    if (!is_empty(queue->local)) {
        f->is_pool++;
        f_arg->id = result_id;
        f_arg->local = unique_init();
        tid = atomic_fetch_add(&queue->cpu_core, 1) % queue->cpus;
        deque_push(queue->local[tid], f_arg);
        if (!atomic_flag_load(&queue->local[tid]->started))
            atomic_flag_test_and_set(&queue->local[tid]->started);

        atomic_fetch_add(&queue->local[tid]->available, 1);
    } else {
        if (thrd_create(&f->thread, thrd_raii_wrapper, f_arg) != thrd_success)
            throw(future_error);
    }

    pool->futures[pool->end] = f;
    pool->end++;
    return result;
}

future_t thrd_sync(future_t pool) {
    if (!is_type(pool, RAII_SPAWN))
        throw(logic_error);

    int i, id;
    memory_t *scope = pool->queue->scope;
    for (i = pool->start; i < pool->end; i++) {
        id = pool->futures[i]->id;
        raii_values_t *result = promise_get(pool->futures[i]->value);
        result_t *results = (result_t *)atomic_load_explicit(&pool->queue->results, memory_order_acquire);
        results[id]->result = (raii_values_t *)calloc_full(scope, 1, sizeof(raii_values_t), RAII_FREE);
        size_t size = result->size;
        if (size > 0) {
            results[id]->result->extended = calloc_full(scope, 1, size, RAII_FREE);
            memcpy(results[id]->result->extended, result->extended, size);
            results[id]->result->value.object = results[id]->result->extended;
        } else if (!is_zero(result->value.integer)) {
            results[id]->result->value.object = result->value.object;
        }

        results[id]->is_ready = true;
        atomic_store_explicit(&pool->queue->results, results, memory_order_release);
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
    int i, max = iter->end;
    result_t *results = (result_t *)atomic_load_explicit(&iter->queue->results, memory_order_relaxed);
    for (i = iter->start; i < max; i++)
        if (results[i]->is_ready)
            result = callback(result, i, results[i]->result->value);
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
    for (i = f->start; i < f->end; i++) {
        if (!thrd_is_done(f->futures[i]))
            return false;
    }

    return true;
}
