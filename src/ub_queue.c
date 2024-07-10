#include "ub_queue.h"

/** Allocate memory for new node. Each node stores a pointer
 * value pushed to unbounded queue
 */
static ub_node_t *ub_alloc(ub_queue_t *q) {
    /* First tries to allocate node from internal node cache,
     * if attempt fails, allocates node via rpmalloc() */
    if (q->_first != q->_between) {
        ub_node_t *n = q->_first;
        q->_first = (ub_node_t *)q->_first->_next;

        return n;
    }

    q->_between = (ub_node_t *)atomic_load_explicit(&q->_tail, memory_order_acquire);
    if (q->_first != q->_between) {
        ub_node_t *n = q->_first;
        q->_first = (ub_node_t *)q->_first->_next;

        return n;
    }

    return (ub_node_t *)malloc(sizeof(ub_node_t));
}

static FORCEINLINE ub_node_t *ub_get(ub_queue_t *q) {
    return atomic_get(ub_node_t *, &(q->_tail));
}

ub_queue_t *ub_raii(size_t size) {
    ub_queue_t *q = malloc_default(sizeof(ub_queue_t));
    raii_defer(ub_destroy, q);
    atexit(raii_deferred_clean);
    ub_init(q, size);
    return q;
}

int ub_init(ub_queue_t *q, size_t size) {
    unsigned long i;
    ub_node_t *n = malloc(sizeof(ub_node_t) * size);
    n->_next = NULL;
    q->_head = q->_first = q->_between = n;
    atomic_init(&q->_tail, n);
    atomic_store(&q->count, 0);

    /** Alloc memory at start for total size for efficiency */
    void *v = NULL;
    for (i = 0; i < size; i++)
        ub_enqueue(q, v);

    for (i = 0; i < size; i++)
        ub_dequeue(q, &v);

    q->type = RAII_QUEUE;

    return 0;
}

int ub_destroy(ub_queue_t *q) {
    if (q && q->type == RAII_QUEUE) {
        q->type = -1;
        ub_node_t *n = q->_first;
        do {
            ub_node_t *next = (ub_node_t *)n->_next;
            free((void *)n);
            n = next;
        } while (n);
    }

    return 0;
}

bool ub_enqueue(ub_queue_t *q, void *v) {
    ub_node_t *n = ub_alloc(q);
    atomic_store_explicit(&(n->_next), NULL, memory_order_release);
    n->_value = v;
    atomic_store_explicit(&(q->_head->_next), n, memory_order_release);
    q->_head = n;
    atomic_fetch_add(&q->count, 1);

    return true;
}

bool ub_dequeue(ub_queue_t *q, void **v) {
    ub_node_t *tmp = ub_get(q);
    if (atomic_load_explicit(&(tmp->_next), memory_order_acquire)) {
        *v = tmp->_next->_value;
        atomic_store_explicit(&q->_tail, tmp->_next, memory_order_release);
        atomic_fetch_sub(&q->count, 1);

        return true;
    }

    return false;
}

FORCEINLINE bool ub_is_empty(ub_queue_t *q) {
    return ub_get(q)->_next == NULL;
}

FORCEINLINE size_t ub_count(ub_queue_t *q) {
    return atomic_load(&q->count);
}
