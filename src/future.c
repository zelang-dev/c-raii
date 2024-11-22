#include "raii.h"

static promise *promise_create(void) {
    promise *p = calloc_default(1, sizeof(promise));
    p->scope = raii_local();
    mtx_init(&p->mutex, mtx_plain);
    cnd_init(&p->cond);
    srand((unsigned int)time(NULL));
    p->id = rand();
    p->done = false;
    p->type = RAII_PROMISE;

    return p;
}

static void promise_set(promise *p, void_t res) {
    mtx_lock(&p->mutex);
    p->result = (raii_values_t*)calloc_full(p->scope, 1, sizeof(raii_values_t) + sizeof(res), RAII_FREE);
    if (!is_empty(res))
        p->result->value.object = res;

    p->done = true;
    cnd_signal(&p->cond);
    mtx_unlock(&p->mutex);
}

static raii_values_t *promise_get(promise *p) {
    mtx_lock(&p->mutex);
    while (!p->done) {
        cnd_wait(&p->cond, &p->mutex);
    }

    mtx_unlock(&p->mutex);
    return p->result;
}

static bool promise_done(promise *p) {
    mtx_lock(&p->mutex);
    bool done = p->done;
    mtx_unlock(&p->mutex);
    return done;
}

static void promise_close(promise *p) {
    if (is_type(p, RAII_PROMISE)) {
        p->type = -1;
        mtx_destroy(&p->mutex);
        cnd_destroy(&p->cond);
    }
}

static future *future_create(thrd_func_t start_routine) {
    future *f = try_calloc(1, sizeof(future));
    f->func = (thrd_func_t)start_routine;
    srand((unsigned int)time(NULL));
    f->id = rand();
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
    future_arg *f_arg = try_calloc(1, sizeof(future_arg));
    f_arg->func = f->func;
    f_arg->arg = arg;
    f_arg->value = value;
    f_arg->type = RAII_FUTURE_ARG;
    int r = thrd_create(&f->thread, raii_wrapper, f_arg);
}

future *thrd_for(thrd_func_t fn, void_t args) {
    promise *p = promise_create();
    future *f = future_create(fn);
    f->value = p;
    thrd_start(f, p, args);
    return f;
}

values_type thrd_get(future *f) {
    promise *p = f->value;
    raii_values_t *r = promise_get(p);
    future_close(f);
    promise_close(p);
    return r->value;
}

RAII_INLINE bool thrd_is_done(future *f) {
    return promise_done(f->value);
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
