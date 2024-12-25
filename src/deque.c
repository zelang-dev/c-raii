#include "raii.h"

/* These are non-NULL pointers that will result in page faults under normal
 * circumstances, used to verify that nobody uses non-initialized entries.
 */
static values_type *RAII_EMPTY_T = (values_type *)0x300, *RAII_ABORT_T = (values_type *)0x400;

void deque_resize(deque_values_type *q) {
    values_type_array *a = (values_type_array *)atomic_load_explicit(&q->array, memory_order_relaxed);
    size_t old_size = a->size;
    size_t new_size = old_size * 2;
    values_type_array *new = try_malloc(sizeof(values_type_array) + sizeof(values_type *) * new_size);
    atomic_init(&new->size, new_size);
    size_t i, t = atomic_load_explicit(&q->top, memory_order_relaxed);
    size_t b = atomic_load_explicit(&q->bottom, memory_order_relaxed);
    for (i = t; i < b; i++)
        new->buffer[i % new_size] = a->buffer[i % old_size];

    atomic_store_explicit(&q->array, new, memory_order_relaxed);
    RAII_FREE(a);
}

void deque_push(deque_values_type *q, values_type *w) {
    size_t b = atomic_load_explicit(&q->bottom, memory_order_relaxed);
    size_t t = atomic_load_explicit(&q->top, memory_order_acquire);
    values_type_array *a = (values_type_array *)atomic_load_explicit(&q->array, memory_order_relaxed);
    if (b - t > a->size - 1) { /* Full queue */
        deque_resize(q);
        a = (values_type_array *)atomic_load_explicit(&q->array, memory_order_relaxed);
    }

    atomic_store_explicit(&a->buffer[b % a->size], w, memory_order_relaxed);
    atomic_thread_fence(memory_order_release);
    atomic_store_explicit(&q->bottom, b + 1, memory_order_relaxed);
}

values_type *deque_take(deque_values_type *q) {
    size_t b = atomic_load_explicit(&q->bottom, memory_order_relaxed) - 1;
    size_t t = atomic_load_explicit(&q->top, memory_order_relaxed);
    values_type_array *a = (values_type_array *)atomic_load_explicit(&q->array, memory_order_relaxed);
    atomic_store_explicit(&q->bottom, b, memory_order_relaxed);
    atomic_thread_fence(memory_order_seq_cst);
    values_type *x;
    if (t <= b) {
        /* Non-empty queue */
        x = (values_type *)atomic_load_explicit(&a->buffer[b % a->size], memory_order_relaxed);
        if (t == b) {
            /* Single last element in queue */
            if (!atomic_compare_exchange_strong_explicit(&q->top, &t, t + 1,
                                                         memory_order_seq_cst,
                                                         memory_order_relaxed))
                /* Failed race */
                x = RAII_EMPTY_T;
            atomic_store_explicit(&q->bottom, b + 1, memory_order_relaxed);
        }
    } else { /* Empty queue */
        x = RAII_EMPTY_T;
        atomic_store_explicit(&q->bottom, b + 1, memory_order_relaxed);
    }
    return x;
}

values_type *deque_steal(deque_values_type *q) {
    size_t t = atomic_load_explicit(&q->top, memory_order_acquire);
    atomic_thread_fence(memory_order_seq_cst);
    size_t b = atomic_load_explicit(&q->bottom, memory_order_acquire);
    values_type *x = RAII_EMPTY_T;
    if (t < b) {
        /* Non-empty queue */
        values_type_array *a = (values_type_array *)atomic_load_explicit(&q->array, memory_order_consume);
        x = (values_type *)atomic_load_explicit(&a->buffer[t % a->size], memory_order_relaxed);
        if (!atomic_compare_exchange_strong_explicit(
            &q->top, &t, t + 1, memory_order_seq_cst, memory_order_relaxed))
            /* Failed race */
            return RAII_ABORT_T;
    }
    return x;
}

void deque_init(deque_values_type *q, int size_hint) {
    atomic_init(&q->top, 0);
    atomic_init(&q->bottom, 0);
    values_type_array *a = try_malloc(sizeof(values_type_array) + sizeof(values_type *) * size_hint);
    atomic_init(&a->size, size_hint);
    atomic_init(&q->array, a);
}

static void deque_free(deque_values_type *q) {
    values_type_array *a = nullptr;
    if (!is_empty(q)) {
        a = atomic_get(values_type_array *, &q->array);
        if (!is_empty(a)) {
            atomic_store(&q->array, nullptr);
            RAII_FREE((void_t)a);
        }

        memset(q, 0, sizeof(q));
        RAII_FREE(q);
    }
}
