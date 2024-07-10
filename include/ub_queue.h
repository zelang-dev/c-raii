/* Lock-free Unbounded Single-Producer Single-consumer (SPSC) queue.
 *
 * Based on Dmitry Vyukov's Unbounded SPSC queue:
 *   http://www.1024cores.net/home/lock-free-algorithms/queues/unbounded-spsc-queue
 */
#ifndef _UB_QUEUE_H_
#define _UB_QUEUE_H_

#include "raii.h"

#ifndef CACHELINE_SIZE
#define CACHELINE_SIZE 64
#endif

typedef char cacheline_pad_t[CACHELINE_SIZE];

typedef struct ub_queue ub_node_t;
make_atomic(ub_node_t, atomic_queue_t)
struct ub_queue {
    atomic_queue_t *_next;		/**> Single linked list of nodes */
    void *_value;
};

typedef struct ub_queue_cache {
    raii_type type;
    atomic_size_t count;

    /** Delimiter between consumer part and producer part,
     * so that they situated on different cache lines */
    cacheline_pad_t _pad0;

    /* Consumer part
     * accessed mainly by consumer, infrequently be producer */
    atomic_queue_t *_tail; 		/**> Tail of the queue. */
    cacheline_pad_t _pad1;

    /* Producer part
     * accessed only by producer */
    ub_node_t *_head;		/**> Head of the queue. */
    cacheline_pad_t _pad2;

    ub_node_t *_first;		/**> Last unused node (tail of node cache). */
    cacheline_pad_t _pad3;

    ub_node_t *_between;	/**> Helper which points somewhere between _first and _tail */
    cacheline_pad_t _pad4;
} ub_queue_t;

/** Initialize queue */
C_API int ub_init(ub_queue_t *, size_t size);

/** Return unbounded queue `scoped` to current `thread` */
C_API ub_queue_t *ub_raii(size_t size);

/** Destroy queue and release memory */
C_API int ub_destroy(ub_queue_t *q);

/** Push a value to unbounded queue
 *  return : 1 always as its an unbounded queue
 */
C_API bool ub_enqueue(ub_queue_t *q, void *v);

/** Pull a value from unbounded queue
 *  return : 1 if success else 0
 */
C_API bool ub_dequeue(ub_queue_t *q, void **v);

/** Is unbounded queue currently empty? */
C_API bool ub_is_empty(ub_queue_t *q);

/** Unbounded queue current items count */
C_API size_t ub_count(ub_queue_t *q);

#endif /* _UB_QUEUE_H_ */
