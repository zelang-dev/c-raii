#include "raii.h"

static unique_t *thrd_arena_tls = NULL;
static mtx_t thrd_arena_mtx[1] = {0};
tss_t thrd_arena_tss = 0;

static void thrd_arena_delete(void) {
    if (is_empty(thrd_arena_tls))
        return;

    raii_delete_arena(thrd_arena_tls);
    mtx_destroy(thrd_arena_mtx);
    thrd_arena_tls = NULL;
    thrd_arena_tss = 0;
}

void thrd_defer(func_t func, void *arg) {
    raii_deferred(raii_local(), func, arg);
}

void thrd_init(void) {
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

        ex_protect_ptr(thrd_arena_tls->protector, thrd_arena_tls->arena, (func_t)arena_clear);
        atexit(thrd_arena_delete);
    }
}

void *thrd_unique(size_t size) {
    if (is_empty(tss_get(thrd_arena_tss))) {
        if (is_empty(thrd_arena_tls))
            raii_panic("Failed! Thrd not `thrd_init`");

        raii_init()->arena = thrd_alloc(size);
        if (tss_set(thrd_arena_tss, raii_local()) != thrd_success)
            raii_panic("Thrd `tss_set` failed!");
    }

    return raii_local()->arena;
}

void *thrd_alloc(size_t size) {
    if (mtx_lock(thrd_arena_mtx) != thrd_success)
        raii_panic("Thrd `mtx_lock` failed!");

    void *block = malloc_arena(thrd_arena_tls, size);
    if (mtx_unlock(thrd_arena_mtx) != thrd_success)
        raii_panic("Thrd `mtx_unlock` failed!");

    return block;
}

void *thrd_get(void) {
    void *ptr;
    if (is_empty(ptr = tss_get(thrd_arena_tss)))
        return ptr;

    return ((memory_t *)ptr)->arena;
}
