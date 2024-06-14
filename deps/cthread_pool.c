

#include "cthread.h"

typedef enum {
    immediate_shutdown = 1,
    graceful_shutdown = 2
} pool_shutdown_t;

/* The work struct */
typedef struct {
    /* Pointer to the function that will perform the task. */
    void (*function)(void *);
    /* Argument to be passed to the function. */
    void *argument;
} pool_task_t;

/* The pool struct */
struct pool_s {
    mtx_t lock;
    /* Condition variable to notify worker threads. */
    cnd_t notify;
    /* threads currently working*/
    int num_working;
    /* used for thread count etc */
    mtx_t count_lock;
    /* signal to thrd_wait     */
    cnd_t all_idle;
    /* Array containing worker threads ID. */
    thrd_t *threads;
    /* Array containing the task queue. */
    pool_task_t *queue;
    /* Number of threads*/
    int thread_count;
    /* Size of the task queue.*/
    int queue_size;
    /* Index of the first element. */
    int head;
    /* Index of the next element. */
    int tail;
    /* Number of pending tasks */
    int count;
    /* Flag indicating if the pool is shutting down*/
    int shutdown;
    /* Number of started threads */
    int started;
};

/**
 * @brief the worker thread
 * @param pool the pool which own the thread
 */
static void *pool_thread(void *args);

int thrd_free(thrd_pool_t *pool);

thrd_pool_t *thrd_pool(int thread_count, int queue_size) {
    thrd_pool_t *pool;
    int i;

    if ((pool = (thrd_pool_t *)C11_MALLOC(sizeof(thrd_pool_t))) == NULL) {
        goto err;
    }

    /* Initialize */
    pool->thread_count = 0;
    pool->queue_size = queue_size;
    pool->head = pool->tail = pool->count = 0;
    pool->shutdown = pool->started = 0;
    pool->num_working = 0;

    /* Allocate thread and task queue */
    pool->threads = (thrd_t *)C11_MALLOC(sizeof(thrd_t) * thread_count);
    pool->queue = (pool_task_t *)C11_MALLOC
    (sizeof(pool_task_t) * queue_size);

    /* Initialize mutex and conditional variable first */
    if ((mtx_init(&(pool->lock), mtx_plain) != thrd_success) ||
        (cnd_init(&(pool->notify)) != thrd_success) ||
        (mtx_init(&(pool->count_lock), mtx_plain) != thrd_success) ||
        (cnd_init(&(pool->all_idle)) != thrd_success) ||
        (pool->threads == NULL) ||
        (pool->queue == NULL)) {
        goto err;
    }

    /* Start worker threads */
    for (i = 0; i < thread_count; i++) {
        if (thrd_create(&(pool->threads[i]), (thrd_start_t)pool_thread, (void *)pool) != thrd_success) {
            thrd_destroy(pool, 0);
            return NULL;
        }
        pool->thread_count++;
        pool->started++;
    }

    return pool;

err:
    if (pool) {
        thrd_free(pool);
    }
    return NULL;
}

int thrd_add(thrd_pool_t *pool, void (*function)(void *),
             void *argument) {
    int err = 0;
    int next;

    if (pool == NULL || function == NULL) {
        return pool_invalid;
    }

    if (mtx_lock(&(pool->lock)) != thrd_success) {
        return pool_lock_failure;
    }

    next = (pool->tail + 1) % pool->queue_size;

    do {
        /* Are we full ? */
        if (pool->count == pool->queue_size) {
            err = pool_queue_full;
            break;
        }

        /* Are we shutting down ? */
        if (pool->shutdown) {
            err = pool_shutdown;
            break;
        }

        /* Add task to queue */
        pool->queue[pool->tail].function = function;
        pool->queue[pool->tail].argument = argument;
        pool->tail = next;
        pool->count += 1;

        /* pthread_cond_broadcast */
        if (cnd_signal(&(pool->notify)) != thrd_success) {
            err = pool_lock_failure;
            break;
        }
    } while (0);

    if (mtx_unlock(&pool->lock) != thrd_success) {
        err = pool_lock_failure;
    }

    return err;
}

int thrd_destroy(thrd_pool_t *pool, int flags) {
    int i, err = 0;

    if (pool == NULL) {
        return pool_invalid;
    }

    if (mtx_lock(&(pool->lock)) != thrd_success) {
        return pool_lock_failure;
    }

    do {
        /* Already shutting down */
        if (pool->shutdown) {
            err = pool_shutdown;
            break;
        }

        pool->shutdown = (flags & pool_graceful) ?
            graceful_shutdown : immediate_shutdown;

        /* Wake up all worker threads */
        if ((cnd_broadcast(&(pool->notify)) != thrd_success) ||
            (mtx_unlock(&(pool->lock)) != thrd_success)) {
            err = pool_lock_failure;
            break;
        }

        /* Join all worker thread */
        for (i = 0; i < pool->thread_count; i++) {
            if (thrd_join(pool->threads[i], NULL) != thrd_success) {
                err = pool_thread_failure;
            }
        }
    } while (0);

    /* Only if everything went well do we deallocate the pool */
    if (!err) {
        thrd_free(pool);
    }
    return err;
}

int thrd_free(thrd_pool_t *pool) {
    if (pool == NULL || pool->started > 0) {
        return -1;
    }

    /* Did we manage to allocate ? */
    if (pool->threads) {
        C11_FREE(pool->threads);
        C11_FREE(pool->queue);

        mtx_destroy(&(pool->lock));
        cnd_destroy(&(pool->notify));

        mtx_destroy(&(pool->count_lock));
        cnd_destroy(&(pool->all_idle));
    }
    C11_FREE(pool);
    return 0;
}

static void *pool_thread(void *args) {
    thrd_pool_t *pool = (thrd_pool_t *)args;
    pool_task_t task;

    for (;;) {
        /* Lock must be taken to wait on conditional variable */
        mtx_lock(&(pool->lock));

        /* Wait on condition variable, check for spurious wakeups.
           When returning from cnd_wait(), we own the lock. */
        while ((pool->count == 0) && (!pool->shutdown)) {
            cnd_wait(&(pool->notify), &(pool->lock));
        }

        if ((pool->shutdown == immediate_shutdown) ||
            ((pool->shutdown == graceful_shutdown) &&
             (pool->count == 0))) {
            break;
        }

        /* Grab our task */
        task.function = pool->queue[pool->head].function;
        task.argument = pool->queue[pool->head].argument;
        pool->head = (pool->head + 1) % pool->queue_size;
        pool->count -= 1;

        /* Unlock */
        mtx_unlock(&(pool->lock));

        /* Get to work */
        mtx_lock(&pool->count_lock);
        pool->num_working++;
        mtx_unlock(&pool->count_lock);

        (*(task.function))(task.argument);

        mtx_lock(&pool->count_lock);
        pool->num_working--;
        if (!pool->num_working)
            cnd_signal(&pool->all_idle);

        mtx_unlock(&pool->count_lock);
    }

    pool->started--;

    mtx_unlock(&(pool->lock));
    thrd_exit(0);
    return(NULL);
}

void thrd_wait(thrd_pool_t *pool) {
    mtx_lock(&pool->count_lock);
    while (pool->count || pool->num_working) {
        cnd_wait(&pool->all_idle, &pool->count_lock);
    }
    mtx_unlock(&pool->count_lock);
}
