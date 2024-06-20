#include "cthread.h"
#ifndef HAS_C11_THREADS
#if defined(_CTHREAD_WIN32_)
static DWORD time2msec(const struct timespec *ms) {
    return (DWORD)((ms->tv_sec * 1000u) + (ms->tv_nsec / 1000000));
}
#endif

int mtx_init(mtx_t *mtx, int type) {
    int ret;
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    if (type & mtx_recursive) {
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    }
    ret = pthread_mutex_init(mtx, &attr);
    pthread_mutexattr_destroy(&attr);

    return ret == 0 ? thrd_success : thrd_error;
}

void mtx_destroy(mtx_t *mtx) {
    pthread_mutex_destroy(mtx);
}

int mtx_lock(mtx_t *mtx) {
    return pthread_mutex_lock(mtx) == 0 ? thrd_success : thrd_error;
}

int mtx_timedlock(mtx_t *mtx, const struct timespec *ts) {
    if (!mtx || !ts)
        return thrd_error;

    struct timespec tts;
    int rt;
    tts.tv_sec = ts->tv_sec;
    tts.tv_nsec = ts->tv_nsec;

#if defined(_WIN32) || (defined(_POSIX_TIMEOUTS) && (_POSIX_TIMEOUTS >= 200112L) && defined(_POSIX_THREADS) && (_POSIX_THREADS >= 200112L))
    rt = pthread_mutex_timedlock(mtx, &tts);
#else
    struct timespec cur, dur;

    /* Try to acquire the lock and, if we fail, sleep for 5ms. */
    while ((rt = pthread_mutex_trylock(mtx)) == EBUSY) {
        timespec_get(&cur, TIME_UTC);

        if ((cur.tv_sec > ts->tv_sec) || ((cur.tv_sec == ts->tv_sec) && (cur.tv_nsec >= ts->tv_nsec))) {
            break;
        }

        dur.tv_sec = ts->tv_sec - cur.tv_sec;
        dur.tv_nsec = ts->tv_nsec - cur.tv_nsec;
        if (dur.tv_nsec < 0) {
            dur.tv_sec--;
            dur.tv_nsec += 1000000000;
        }

        if ((dur.tv_sec != 0) || (dur.tv_nsec > 5000000)) {
            dur.tv_sec = 0;
            dur.tv_nsec = 5000000;
        }

        nanosleep(&dur, NULL);
    }
#endif

    if (rt == 0)
        return thrd_success;

    return (rt == ETIMEDOUT || rt == EBUSY) ? thrd_timedout : thrd_error;
}

int mtx_trylock(mtx_t *mtx) {
    return (pthread_mutex_trylock(mtx) == 0) ? thrd_success : thrd_busy;
}

int mtx_unlock(mtx_t *mtx) {
    return pthread_mutex_unlock(mtx) == 0 ? thrd_success : thrd_error;;
}

int cnd_init(cnd_t *cond) {
    return pthread_cond_init(cond, NULL) == 0 ? thrd_success : thrd_error;
}

void cnd_destroy(cnd_t *cond) {
    pthread_cond_destroy(cond);
}

int cnd_signal(cnd_t *cond) {
    return pthread_cond_signal(cond) == 0 ? thrd_success : thrd_error;
}

int cnd_broadcast(cnd_t *cond) {
    return pthread_cond_broadcast(cond) == 0 ? thrd_success : thrd_error;
}

int cnd_wait(cnd_t *cond, mtx_t *mtx) {
    return pthread_cond_wait(cond, mtx) == 0 ? thrd_success : thrd_error;
}

int cnd_timedwait(cnd_t *cond, mtx_t *mtx, const struct timespec *ts) {
    int ret;
    ret = pthread_cond_timedwait(cond, mtx, ts);
    if (ret == ETIMEDOUT) {
        return thrd_timedout;
    }
    return ret == 0 ? thrd_success : thrd_error;
}

/** Information to pass to the new thread (what to run). */
typedef struct {
    thrd_start_t mFunction; /**< Pointer to the function to be executed. */
    void *mArg;            /**< Function argument for the thread function. */
} _thread_start_info;

/* Thread wrapper function. */
static void *_thrd_wrapper_function(void *aArg) {
    thrd_start_t fun;
    void *arg;
    int  res;

    /* Get thread startup information */
    _thread_start_info *ti = (_thread_start_info *)aArg;
    fun = ti->mFunction;
    arg = ti->mArg;

    /* The thread is responsible for freeing the startup information */
    C11_FREE((void *)ti);

    /* Call the actual client thread function */
    res = fun(arg);

    return (void *)(intptr_t)res;
}

int thrd_create(thrd_t *thr, thrd_start_t func, void *arg) {
    /* Fill out the thread startup information (passed to the thread wrapper,
       which will eventually free it) */
    _thread_start_info *ti = (_thread_start_info *)C11_MALLOC(sizeof(_thread_start_info));
    if (ti == NULL) {
        return thrd_nomem;
    }
    ti->mFunction = func;
    ti->mArg = arg;

    /* Create the thread */
    if (pthread_create(thr, NULL, _thrd_wrapper_function, (void *)ti) != 0) {
        C11_FREE(ti);
        return thrd_error;
    }

    return thrd_success;
}

thrd_t thrd_current(void) {
    return pthread_self();
}

int thrd_detach(thrd_t thr) {
    return pthread_detach(thr) == 0 ? thrd_success : thrd_error;
}

int thrd_equal(thrd_t thr0, thrd_t thr1) {
    return pthread_equal(thr0, thr1);
}

void thrd_exit(int res) {
    pthread_exit((void *)(intptr_t)res);
}

int thrd_join(thrd_t thr, int *res) {
    void *pres;
    if (pthread_join(thr, &pres) != 0) {
        return thrd_error;
    }
    if (res != NULL) {
        *res = (int)(intptr_t)pres;
    }

    return thrd_success;
}

int thrd_sleep(const struct timespec *duration, struct timespec *remaining) {
    (void)remaining;
#if !defined(_CTHREAD_WIN32_)
    nanosleep(duration, NULL);
#else
    Sleep(time2msec(duration));
#endif

    return thrd_success;
}

void thrd_yield(void) {
    sched_yield();
}

int tss_create(tss_t *key, tss_dtor_t dtor) {
    if (pthread_key_create(key, dtor) != 0) {
        return thrd_error;
    }

    return thrd_success;
}

void tss_delete(tss_t key) {
    pthread_key_delete(key);
}

void *tss_get(tss_t key) {
    return pthread_getspecific(key);
}

int tss_set(tss_t key, void *val) {
    if (pthread_setspecific(key, val) != 0) {
        return thrd_error;
    }

    return thrd_success;
}
#endif /* HAS_C11_THREADS */

#include "cthread_pool.c"
