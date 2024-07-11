#include "raii.h"

union header {
    arena_t tag;
    values_type slot;
};

static arena_t free_list = NULL;
static int num_free = 0;

/**
 * Will allow to translate back and forth between the pointer
 * and the container we use for managing the data.
 */
#define ARENA_HEADER_SZ offsetof(union header, tag->base)

arena_t arena_init(size_t threshold) {
    arena_t arena = try_malloc(sizeof(*arena));
    arena->next = NULL;
    arena->limit = arena->avail = NULL;
    arena->base = NULL;
    arena->total = 0;
    arena->bytes = 0;
    arena->is_global = false;
    arena->threshold = (threshold <= 0) ? 10 : threshold;
    arena->type = RAII_ARENA + RAII_STRUCT;
    return arena;
}

void arena_free(arena_t arena) {
    if (is_empty(arena))
        return;

    if (is_type(arena, RAII_ARENA + RAII_STRUCT)) {
        num_free += (int)arena->threshold + 1;
        if (arena->is_global)
            RAII_FREE(arena->base);
        else
            arena_clear(arena);

        memset(arena, -1, sizeof(arena_t));
        RAII_FREE(arena);
        arena = NULL;
    }
}

void *arena_alloc(arena_t arena, long nbytes) {
    if (is_empty(arena))
        raii_panic("Bad block, `NULL` detected!");

    RAII_ASSERT(nbytes > 0);
    nbytes = align_up(nbytes, sizeof(u16));
    while (nbytes > arena->limit - arena->avail) {
        arena_t ptr;
        char *limit;
        if ((ptr = free_list) != NULL) {
            free_list = free_list->next;
            num_free--;
            limit = ptr->limit;
        } else {
            long m = align_up(sizeof(union header) + nbytes + arena->threshold * 1024, sizeof(u16));
            arena->total += m;
            if (arena->is_global)
                ptr = RAII_REALLOC(arena->base, arena->total);
            else
                ptr = RAII_MALLOC(arena->total);

            if (ptr == NULL) {
                errno = ENOMEM;
                return NULL;
            }

            if (arena->is_global) {
                arena->base = ptr;
                limit = (char *)arena->base + arena->total;
            } else {
                if (is_empty(arena->base))
                    arena->base = ptr;

                limit = (char *)ptr + arena->total;
            }
        }

        *ptr = *arena;
        arena->avail = (char *)((union header *)ptr + 1);
        arena->limit = limit;
        arena->next = ptr;
        ptr->type = RAII_ARENA;
    }

    if (arena->next)
        arena->next->bytes = arena->bytes;

    arena->bytes = nbytes;
    arena->avail += nbytes;

    return arena->avail - nbytes;
}

void *arena_calloc(arena_t arena, long count, long nbytes) {
    RAII_ASSERT(count > 0);
    void *ptr = arena_alloc(arena, count * nbytes);
    memset(ptr, 0, count * nbytes);
    return ptr;
}

void arena_clear(arena_t arena) {
    if (is_empty(arena))
        return;

    while (!arena->is_global && arena->next && is_type(arena->next, RAII_ARENA)) {
        arena_t tmp = arena->next;
        if (num_free < (int)arena->threshold) {
            arena->next->next = free_list;
            free_list = arena->next;
            num_free++;
            free_list->limit = arena->limit;
            if (is_zero(free_list->bytes))
                arena->avail -= arena->bytes;
            else
                arena->avail -= free_list->bytes;
        } else {
            memset(arena->next, -1, sizeof(arena_t));
            RAII_FREE(arena->next);
            arena->next = NULL;
        }

        arena = tmp;
    }
}

void arena_print(const arena_t arena) {
    printf("capacity: %zu, total: %zu, free_list:: %d\n", arena_capacity(arena), arena_total(arena), num_free);
}

RAII_INLINE size_t arena_capacity(const arena_t arena) {
    return !is_empty(arena) && is_type(arena, RAII_ARENA + RAII_STRUCT)
        ? arena->limit - arena->avail
        : 0;
}

RAII_INLINE size_t arena_total(const arena_t arena) {
    return !is_empty(arena) && is_type(arena, RAII_ARENA + RAII_STRUCT)
        ? MAX(arena->total, sizeof(union header)) - sizeof(union header)
        : 0;
}
