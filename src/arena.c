#include "raii.h"

#ifndef THRESHOLD
    #define THRESHOLD 10
#endif

struct arena_s {
    raii_type type;
    arena_t prev;
    char *avail;
    char *limit;
    void *base;
    size_t total;
};

union header {
    arena_t tag;
    values_type slot;
};

static arena_t free_list;
static int num_free;

arena_t arena_new(void) {
    arena_t arena = try_malloc(sizeof(*arena));
    arena->prev = NULL;
    arena->limit = arena->avail = NULL;
    arena->base = NULL;
    arena->total = 0;
    arena->type = RAII_ARENA + RAII_STRUCT;
    return arena;
}

void arena_dispose(arena_t arena) {
    if (is_empty(arena))
        return;

    if (is_type(arena, RAII_ARENA + RAII_STRUCT)) {
        arena_free(arena);
        memset(arena, -1, sizeof(arena_t));
        RAII_FREE(arena->base);
        RAII_FREE(arena);
        arena = NULL;
    }
}

void *arena_alloc(arena_t arena, long nbytes) {
    RAII_ASSERT(arena);
    RAII_ASSERT(nbytes > 0);
    nbytes = ((nbytes + sizeof(values_type) - 1) /
              (sizeof(values_type))) * (sizeof(values_type));
    while (nbytes > arena->limit - arena->avail) {
        arena_t ptr;
        char *limit;
        if ((ptr = free_list) != NULL) {
            free_list = free_list->prev;
            num_free--;
            limit = ptr->limit;
        } else {
            long m = sizeof(union header) + nbytes + THRESHOLD * 1024;
            arena->total += m;
            ptr = try_realloc(arena->base, arena->total);
            arena->base = ptr;
            limit = (char *)arena->base + arena->total;
        }

        *ptr = *arena;
        arena->avail = (char *)((union header *)ptr + 1);
        arena->limit = limit;
        arena->prev = ptr;
        ptr->type = RAII_ARENA;
    }

    arena->avail += nbytes;
    return arena->avail - nbytes;
}

void *arena_calloc(arena_t arena, long count, long nbytes) {
    RAII_ASSERT(count > 0);
    void *ptr;
    ptr = arena_alloc(arena, count * nbytes);
    memset(ptr, '\0', count * nbytes);
    return ptr;
}

void arena_free(arena_t arena) {
    if (is_empty(arena))
        return;

    while (arena->prev && is_type(arena->prev, RAII_ARENA)) {
        arena_t tmp = arena->prev;
        if (num_free < THRESHOLD) {
            arena->prev->prev = free_list;
            free_list = arena->prev;
            num_free++;
            free_list->limit = arena->limit;
        } else {
            arena->avail = (char *)((union header *)arena->base + 1);
            arena->limit = (char *)arena->base + arena->total;
            arena->prev = NULL;
        }

        arena = tmp;
    }
}

void arena_print(const arena_t arena) {
    printf("capacity: %zu, total: %zu, free_list:: %d\n", arena_capacity(arena), arena_total(arena), num_free);
}

RAII_INLINE size_t arena_capacity(const arena_t arena) {
    return !is_empty(arena) && is_type(arena, RAII_ARENA + RAII_STRUCT) ? arena->limit - arena->avail : 0;
}

RAII_INLINE size_t arena_total(const arena_t arena) {
    return !is_empty(arena) && is_type(arena, RAII_ARENA + RAII_STRUCT) ? arena->total : 0;
}
