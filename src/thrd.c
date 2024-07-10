#include "raii.h"

// make_atomic(unique_t *, atomic_arena_t)
// static atomic_arena_t thrd_arena_tls = NULL;
static unique_t *thrd_arena_tls = NULL;
static mtx_t thrd_arena_mtx[1] = {0};
tss_t thrd_arena_tss = 0;

static void thrd_arena_delete(void) {
    if (is_empty(thrd_arena_tls))
        return;

    if (!is_empty(thrd_arena_tls->arena))
        arena_free((arena_t)thrd_arena_tls->arena);

    mtx_destroy(thrd_arena_mtx);

    thrd_arena_tls->arena = NULL;
    memset(thrd_arena_tls, -1, sizeof(thrd_arena_tls));
    RAII_FREE(thrd_arena_tls);
    thrd_arena_tls = NULL;
    thrd_arena_tss = 0;
    local_except_delete();
}

RAII_INLINE unique_t *thrd_scope(void) {
    return (unique_t *)tss_get(thrd_arena_tss);
}

RAII_INLINE void thrd_defer(func_t func, void *arg) {
    raii_deferred(thrd_scope(), func, arg);
}

void thrd_init(void) {
    if (rpmalloc_local_except_tls == 0) {
            rpmalloc_local_except_tls = sizeof(ex_context_t);
            rpmalloc_initialize();
            if (rpmalloc_tls_create(&rpmalloc_local_except_tss, rp_free) != 0)
                raii_panic("Thrd `rpmalloc_tls_create` failed!");
    }

    if (is_empty(thrd_arena_tls)) {
        if (mtx_init(thrd_arena_mtx, mtx_plain) != thrd_success)
            raii_panic("Thrd `mtx_init` failed!");

        thrd_arena_tls = unique_init_arena();
        ((arena_t)thrd_arena_tls->arena)->is_global = true;

        if (tss_create(&thrd_arena_tss, (func_t)raii_deferred_free) != thrd_success)
            raii_panic("Thrd `tss_create` failed!");

        raii_deferred(thrd_arena_tls, (func_t)tss_delete, &thrd_arena_tss);
        if (is_empty(thrd_arena_tls->protector))
            thrd_arena_tls->protector = try_calloc(1, sizeof(ex_ptr_t));

        thrd_arena_tls->protector->is_emulated = true;
        ex_protect_ptr(thrd_arena_tls->protector, thrd_arena_tls->arena, (func_t)arena_clear);
        atexit(thrd_arena_delete);
    }
}

void *thrd_unique(size_t size) {
    unique_t *scope;
    if (is_empty(scope = tss_get(thrd_arena_tss))) {
        if (is_empty(thrd_arena_tls))
            raii_panic("Failed! Thrd not `thrd_init`");

        scope = unique_init();
        scope->is_emulated = true;
        if (tss_set(thrd_arena_tss, (void *)scope) != thrd_success)
            raii_panic("Thrd `tss_set` failed!");

        scope->arena = thrd_alloc(size);
    }

    return scope->arena;
}

void *thrd_alloc(size_t size) {
    if (mtx_lock(thrd_arena_mtx) != thrd_success)
        raii_panic("Thrd `mtx_lock` failed!");

    void *block = malloc_arena(thrd_arena_tls, size);
    if (mtx_unlock(thrd_arena_mtx) != thrd_success)
        raii_panic("Thrd `mtx_unlock` failed!");

    return block;
}

RAII_INLINE void *thrd_malloc(size_t size) {
    return malloc_full(thrd_scope(), size, RAII_FREE);
}

void *thrd_get(void) {
    void *ptr;
    if (is_empty(ptr = tss_get(thrd_arena_tss)))
        return ptr;

    return ((memory_t *)ptr)->arena;
}
