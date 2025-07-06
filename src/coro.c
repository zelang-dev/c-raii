#include "channel.h"

static volatile bool thrd_queue_set = false;
static volatile bool coro_interrupt_set = false;
static volatile bool coro_threading_enabled = true;
static volatile sig_atomic_t can_cleanup = true;
static call_interrupter_t coro_interrupt_loop = nullptr;
static call_timer_t coro_interrupt_timer = nullptr;
static call_t coro_interrupt_timer_system = nullptr;
static call_t coro_interrupt_init = nullptr;
static func_t coro_interrupt_shutdown = nullptr;
static int coro_argc;
static char **coro_argv;
static void(fastcall *coro_swap)(routine_t *, routine_t *) = 0;

coro_sys_func coro_main_func = nullptr;
bool coro_sys_set = false;

/* Base coroutine context. */
typedef struct {
#if defined(_WIN32) && defined(_M_IX86) && !defined(USE_UCONTEXT)
    void_t rip, *rsp, *rbp, *rbx, *r12, *r13, *r14, *r15;
    void_t xmm[20]; /* xmm6, xmm7, xmm8, xmm9, xmm10, xmm11, xmm12, xmm13, xmm14, xmm15 */
    void_t fiber_storage;
    void_t dealloc_stack;
#elif (defined(__x86_64__) || defined(_M_X64)) && !defined(USE_UCONTEXT)
#ifdef _WIN32
    void_t rip, *rsp, *rbp, *rbx, *r12, *r13, *r14, *r15, *rdi, *rsi;
    void_t xmm[20]; /* xmm6, xmm7, xmm8, xmm9, xmm10, xmm11, xmm12, xmm13, xmm14, xmm15 */
    void_t fiber_storage;
    void_t dealloc_stack;
#else
    void_t rip, *rsp, *rbp, *rbx, *r12, *r13, *r14, *r15;
#endif
#elif (defined(__i386) || defined(__i386__)) && !defined(USE_UCONTEXT)
    void_t eip, *esp, *ebp, *ebx, *esi, *edi;
#elif defined(__riscv) && !defined(USE_UCONTEXT)
    void *s[12]; /* s0-s11 */
    void *ra;
    void *pc;
    void *sp;
#ifdef __riscv_flen
#if __riscv_flen == 64
    double fs[12]; /* fs0-fs11 */
#elif __riscv_flen == 32
    float fs[12]; /* fs0-fs11 */
#endif
#endif /* __riscv_flen */
#elif defined(__ARM_EABI__) && !defined(USE_UCONTEXT)
#ifndef __SOFTFP__
    void_t f[16];
#endif
    void_t d[4]; /* d8-d15 */
    void_t r[4]; /* r4-r11 */
    void_t lr;
    void_t sp;
#elif defined(__aarch64__) && !defined(USE_UCONTEXT)
    void_t x[12]; /* x19-x30 */
    void_t sp;
    void_t lr;
    void_t d[8]; /* d8-d15 */
#elif (defined(__powerpc64__) && defined(_CALL_ELF) && _CALL_ELF == 2) && !defined(USE_UCONTEXT)
    uint64_t gprs[32];
    uint64_t lr;
    uint64_t ccr;
    /* FPRs */
    uint64_t fprs[32];
#ifdef __ALTIVEC__
    /* Altivec (VMX) */
    uint64_t vmx[12 * 2];
    uint32_t vrsave;
#endif
#else
    unsigned long int uc_flags;
    struct ucontext *uc_link;
    stack_t uc_stack;
    mcontext_t uc_mcontext;
    __sigset_t uc_sigmask;
#endif
} coro_t;

/* Coroutine extended context. */
struct routine_s {
    coro_t type[1];
    /* Stack base address, can be used to scan memory in a garbage collector. */
    void_t stack_base;
#if defined(_WIN32) && (defined(_M_X64) || defined(_M_IX86))
    void_t stack_limit;
#endif
    /* Used to check stack overflow. */
    size_t magic_number;
    /* Coroutine stack size. */
    size_t stack_size;
    bool taken;
    bool ready;
    bool system;
    bool exiting;
    bool halt;
    bool wait_active;
    bool interrupt_active;
    bool event_active;
    bool event_system;
    bool is_event_err;
    bool is_waiting;
    bool is_group_finish;
    bool is_group;
    bool is_referenced;
    bool flagged;
    bool is_generator;
    signed int event_err_code;
    size_t alarm_time;
    size_t cycles;
    u32 interrupt_timers;
    /* unique result id */
    rid_t rid;
    /* current generator id */
    rid_t gen_id;
    /* unique coroutine id */
    u32 cid;
    /* thread id assigned in `deque_init()` */
    u32 tid;
    coro_states status;
    run_mode run_code;
#if defined(USE_VALGRIND)
    u32 vg_stack_id;
#endif
    void_t user_data;
    void_t timer;
    void_t args;
    /* Coroutine result of function return/exit. */
    value_t *results;
    raii_func_t func;
    raii_values_t interrupt_result[1];
    memory_t scope[1];
    routine_t *next;
    routine_t *prev;
    generator_t yield;
    waitgroup_t wait_group;
    waitgroup_t event_group;
    routine_t *context;
    char name[64];
    char scrape[SCRAPE_SIZE];
};

struct generator_s {
    raii_type type;
    rid_t rid;
    bool is_ready;
    value_t values[1];
    routine_t *context;
};

/* scheduler queue struct */
typedef struct scheduler_s {
    raii_type type;
    routine_t *head;
    routine_t *tail;
} scheduler_t;

typedef struct {
    bool stopped;
    bool started;
    bool is_main;
    /* has the coroutine sleep/wait system started. */
    bool sleep_activated;
    /* number of coroutines waiting in sleep mode. */
    int sleeping_counted;
    /* track the number of coroutines used */
    int used_count;
    /* count coroutines participating in ~thread~ `waitfor` execution */
    u32 group_count;
    /* indicator for thread termination. */
    u32 exiting;
    /* thread id assigned by `coro_pool_init` */
    u32 thrd_id;
    /* number of other coroutine that ran while the current coroutine was waiting.*/
    u32 num_others_ran;
    /* random seed (for work stealing) */
    u32 seed;
    u32 stolen_count;
    i32 interrupter_active;
    /* record thread integration code */
    i32 interrupt_code;
    void_t interrupt_unused;
    void_t interrupt_data;
    /* record thread integration handle */
    void_t interrupt_handle;
    bits_t interrupt_bitset;
    /* array for thread integration data */
    arrays_t interrupt_array;
    routine_t *sleep_handle;
    /* record which coroutine is executing for scheduler */
    routine_t *running;
    /* Variable holding the current running coroutine per thread. */
    routine_t *active_handle;
    /* Variable holding the main target `scheduler` that gets called once an coroutine
    function fully completes and return. */
    routine_t *main_handle;
    /* Variable holding the previous running coroutine per thread. */
    routine_t *current_handle;
    /* Store/hold the registers of the default coroutine thread state,
    allows the ability to switch from any function, non coroutine context. */
    routine_t active_buffer[1];
    /* record which coroutine sleeping in scheduler */
    scheduler_t sleep_queue[1];
    /* coroutines's FIFO scheduler queue */
    scheduler_t run_queue[1];
} coro_thread_t;
thrd_static(coro_thread_t, coro, nullptr)

/* These are non-nullptr pointers that will result in page faults under normal
 * circumstances, used to verify that nobody uses non-initialized entries.
 */
static routine_t *RAII_EMPTY_T = (routine_t *)0x300, *RAII_ABORT_T = (routine_t *)0x400;
make_atomic(routine_t *, atomic_routine_t)
typedef struct {
    atomic_size_t size;
    atomic_routine_t buffer[];
} deque_array_t;

make_atomic(deque_array_t *, atomic_coro_array_t)
struct raii_deque_s {
    raii_type type;
    thrd_t thread;
    memory_t *scope;
    waitgroup_t grouped;
    cacheline_pad_t _pad;

    atomic_flag taken;
    atomic_flag started;
    atomic_flag shutdown;

    /* Used to determent which thread's `run queue`
    receive next `coroutine` task, `counter % cpu cores` */
    atomic_size_t cpu_id_count;
    atomic_size_t available;
    atomic_size_t steal_count;

    /* Assume that they never overflow */
    atomic_size_t top, bottom;
    atomic_coro_array_t array;

    cacheline_pad_t pad;
    raii_deque_t **local;
    cacheline_pad_t pad_;
};
make_atomic(raii_deque_t *, thread_deque_t)

/*
 * `deque_init` `deque_resize` `deque_take` `deque_push` `deque_steal`
 *
 * Modified from https://github.com/sysprog21/concurrent-programs/blob/master/work-steal/work-steal.c
 *
 * A work-stealing scheduler described in
 * Robert D. Blumofe, Christopher F. Joerg, Bradley C. Kuszmaul, Charles E.
 * Leiserson, Keith H. Randall, and Yuli Zhou. Cilk: An efficient multithreaded
 * runtime system. In Proceedings of the Fifth ACM SIGPLAN Symposium on
 * Principles and Practice of Parallel Programming (PPoPP), pages 207-216,
 * Santa Barbara, California, July 1995.
 * https://people.eecs.berkeley.edu/~kubitron/courses/cs262a-F21/handouts/papers/Cilk-PPoPP95.pdf
 *
 * However, that refers to an outdated model of Cilk; an update appears in
 * the essential idea of work stealing mentioned in Leiserson and Platt,
 * Programming Parallel Applications in Cilk
 */
static void deque_init(raii_deque_t *q, u32 size_hint) {
    atomic_init(&q->top, 0);
    atomic_init(&q->bottom, 0);
    deque_array_t *a = try_calloc(1, sizeof(deque_array_t) + sizeof(routine_t *) * size_hint);
    atomic_init(&a->size, size_hint);
    atomic_init(&q->array, a);
    atomic_init(&q->available, 0);
    atomic_init(&q->steal_count, 0);
    atomic_init(&q->cpu_id_count, 0);
    atomic_flag_clear(&q->shutdown);
    atomic_flag_clear(&q->started);
    atomic_flag_test_and_set(&q->taken);
    q->grouped = nullptr;
    q->local = nullptr;
    q->type = RAII_POOL;
}

static void deque_resize(raii_deque_t *q) {
    deque_array_t *a = (deque_array_t *)atomic_load_explicit(&q->array, memory_order_relaxed);
    size_t old_size = a->size;
    size_t new_size = old_size * 2;
    deque_array_t *new = try_calloc(1, sizeof(deque_array_t) + sizeof(routine_t *) * new_size);
    atomic_init(&new->size, new_size);
    size_t i, t = atomic_load_explicit(&q->top, memory_order_relaxed);
    size_t b = atomic_load_explicit(&q->bottom, memory_order_relaxed);
    for (i = t; i < b; i++)
        new->buffer[i % new_size] = a->buffer[i % old_size];

    atomic_store_explicit(&q->array, new, memory_order_relaxed);
    /* The question arises as to the appropriate timing for releasing memory
     * associated with the previous array denoted by *a. In the original Chase
     * and Lev paper, this task was undertaken by the garbage collector, which
     * presumably possessed knowledge about ongoing steal operations by other
     * threads that might attempt to access data within the array.
     *
     * In our context, the responsible deallocation of *a cannot occur at this
     * point, as another thread could potentially be in the process of reading
     * from it. Thus, we opt to abstain from freeing *a in this context,
     * resulting in memory leakage. It is worth noting that our expansion
     * strategy for these queues involves consistent doubling of their size;
     * this design choice ensures that any leaked memory remains bounded by the
     * memory actively employed by the functional queues.
     */
    RAII_FREE(a);
}

static routine_t *deque_take(raii_deque_t *q) {
    size_t b = atomic_load_explicit(&q->bottom, memory_order_relaxed) - 1;
    size_t t = atomic_load_explicit(&q->top, memory_order_relaxed);
    deque_array_t *a = (deque_array_t *)atomic_load_explicit(&q->array, memory_order_relaxed);
    atomic_store_explicit(&q->bottom, b, memory_order_relaxed);
    atomic_thread_fence(memory_order_seq_cst);
    routine_t *x;
    if (t <= b) {
        /* Non-empty queue */
        x = (routine_t *)atomic_load_explicit(&a->buffer[b % a->size], memory_order_relaxed);
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

static void deque_push(raii_deque_t *q, routine_t *w) {
    size_t b = atomic_load_explicit(&q->bottom, memory_order_relaxed);
    size_t t = atomic_load_explicit(&q->top, memory_order_acquire);
    deque_array_t *a = (deque_array_t *)atomic_load_explicit(&q->array, memory_order_relaxed);
    if (b - t > a->size - 1) { /* Full queue */
        deque_resize(q);
        a = (deque_array_t *)atomic_load_explicit(&q->array, memory_order_relaxed);
    }

    atomic_store_explicit(&a->buffer[b % a->size], w, memory_order_relaxed);
    atomic_thread_fence(memory_order_release);
    atomic_store_explicit(&q->bottom, b + 1, memory_order_relaxed);
}

static routine_t *deque_steal(raii_deque_t *q) {
    size_t t = atomic_load_explicit(&q->top, memory_order_acquire);
    atomic_thread_fence(memory_order_seq_cst);
    size_t b = atomic_load_explicit(&q->bottom, memory_order_acquire);
    routine_t *x = RAII_EMPTY_T;
    if (t < b) {
        /* Non-empty queue */
        deque_array_t *a = (deque_array_t *)atomic_load_explicit(&q->array, memory_order_consume);
        x = (routine_t *)atomic_load_explicit(&a->buffer[t % a->size], memory_order_relaxed);
        if (!atomic_compare_exchange_strong_explicit(
            &q->top, &t, t + 1, memory_order_seq_cst, memory_order_relaxed))
            /* Failed race */
            return RAII_ABORT_T;
    }

    return x;
}

static void deque_free(raii_deque_t *q) {
    deque_array_t *a = nullptr;
    if (!is_empty(q)) {
        a = atomic_get(deque_array_t *, &q->array);
        if (!is_empty(a)) {
            atomic_store(&q->array, nullptr);
            RAII_FREE((void_t)a);
        }

        memset(q, 0, sizeof(*q));
        RAII_FREE(q);
    }
}

static void deque_destroy(void) {
    if (is_type(gq_result.queue, RAII_POOL)) {
        raii_deque_t *queue = gq_result.queue;
        memory_t *scope = queue->scope;
        int i;
        queue->type = RAII_ERR;
        if (!is_empty(queue->local)) {
            for (i = 1; i < gq_result.thread_count; i++) {
                atomic_flag_test_and_set(&queue->local[i]->shutdown);
                if (atomic_flag_load(&gq_result.is_errorless))
                    thrd_join(queue->local[i]->thread, nullptr);
                else
                    pthread_cancel(queue->local[i]->thread);
            }
        }

        atomic_flag_test_and_set(&queue->shutdown);
        raii_delete(scope);

        result_t *r = atomic_get(result_t *, &gq_result.results);
        if (!is_empty(r)) {
            atomic_store(&gq_result.results, nullptr);
            RAII_FREE((void_t)r);
        }
    }
}

static routine_t *deque_peek(raii_deque_t *q, u32 index) {
    deque_array_t *a = (deque_array_t *)atomic_load(&q->array);
    if (!is_empty(a) && (index <= a->size))
        return (routine_t *)atomic_load_explicit(&a->buffer[index % a->size], memory_order_relaxed);

    return nullptr;
}

static void coro_transfer(raii_deque_t *queue);
static void coro_destroy(void);
static void coro_scheduler(void);

static RAII_INLINE void coro_interrupter(void) {
    if (coro_interrupt_set) {
        coro()->interrupter_active++;
        coro_interrupt_loop(coro()->interrupt_handle, INTERRUPT_MODE);
        coro()->interrupter_active--;
    }
}

static RAII_INLINE string __itoa(int64_t number) {
    return simd_itoa(number, coro_active()->scrape);
}

/* Utility for aligning addresses. */
static RAII_INLINE size_t _coro_align_forward(size_t addr, size_t align) {
    return (addr + (align - 1)) & ~(align - 1);
}

static RAII_INLINE bool is_status_invalid(routine_t *t) {
    return (is_empty(t) || t->status > CORO_EVENT || t->status < 0);
}

static RAII_INLINE bool coro_is_running(void) {
    return coro_sys_set && gq_result.queue;
}

static RAII_INLINE bool coro_is_threading(void) {
    return coro_is_running() && gq_result.queue->local;
}

static RAII_INLINE int coro_queue_active_count(void) {
    return (int)atomic_load_explicit(&gq_result.active_count, memory_order_relaxed);
}

static RAII_INLINE bool coro_sched_is_assignable(size_t active) {
    return coro_is_threading() && (active / gq_result.thread_count) >= 1;
}

static RAII_INLINE void coro_result_set(routine_t *co, void_t data) {
    if (!is_empty(data) && co->rid != RAII_ERR) {
        rid_t id = co->rid;
        raii_values_t *result = (raii_values_t *)calloc_full(gq_result.scope, 1, sizeof(raii_values_t), RAII_FREE);
        result_t *results = (result_t *)atomic_load_explicit(&gq_result.results, memory_order_acquire);
        atomic_thread_fence(memory_order_seq_cst);
        results[id]->result = result;
        results[id]->result->valued.object = data;
        co->results = &results[id]->result->valued;
        results[id]->is_ready = true;
        atomic_store_explicit(&gq_result.results, results, memory_order_release);
    }
}

static RAII_INLINE void coro_deferred_free(routine_t *coro) {
    raii_deferred_free(coro->scope);
    raii_deferred_init(&coro->scope->defer);
}

/* called only if routine_t func returns */
static void coro_done(void) {
    if (!coro_active()->interrupt_active) {
        coro_active()->halt = true;
        coro_active()->status = CORO_DEAD;
    }

    coro_scheduler();
}

static void coro_awaitable(void) {
    routine_t *co = coro_active();
    array_deferred_set((arrays_t)co->args, co->scope);

    try {
        if (co->interrupt_active) {
            co->func(co->args);
        } else {
            coro_result_set(co, co->func(co->args));
            coro_deferred_free(co);
        }
    } catch_if {
        coro_deferred_free(co);
        if (co->scope->is_recovered || raii_is_caught(co->scope, "sig_winch"))
            ex_flags_reset();
        else
            atomic_flag_clear(&gq_result.is_errorless);
    } finally {
        if (co->is_event_err) {
            co->halt = true;
            co->status = CORO_ERRED;
            co->interrupt_active = false;
        } else if (co->interrupt_active) {
            co->status = CORO_EVENT;
        } else if (co->event_active) {
            co->is_waiting = false;
        } else {
            co->status = CORO_NORMAL;
        }
    }
}

static void coro_func(void) {
    coro_awaitable();
    coro_done(); /* called only if coroutine function returns */
}

/* Add coroutine to scheduler queue, try prepending. */
static void coro_insert(scheduler_t *l, routine_t *t) {
    routine_t *other = nullptr, *head = l->head, *tail = l->tail;
    if (head) {
        t->next = head;
        if (head->prev) {
            other = head->prev;
            t->prev = other;
            other->next = t;
        } else {
            t->prev = other;
            l->head = t;
        }
        head->prev = t;
    } else {
        t->prev = tail;
        if (tail->next) {
            other = tail->next;
            t->next = other;
            other->prev = t;
        } else {
            t->next = other;
            l->tail = t;
        }
        tail->next = t;
    }
}

/* Add coroutine to scheduler queue, appending. */
static void coro_add(scheduler_t *l, routine_t *t) {
    if (l->tail) {
        l->tail->next = t;
        t->prev = l->tail;
    } else {
        l->head = t;
        t->prev = nullptr;
    }

    l->tail = t;
    t->next = nullptr;
}

/* Remove coroutine from scheduler queue. */
static void coro_remove(scheduler_t *l, routine_t *t) {
    if (t->prev)
        t->prev->next = t->next;
    else
        l->head = t->next;

    if (t->next)
        t->next->prev = t->prev;
    else
        l->tail = t->prev;
}

/* Transfer tasks from `global` run queue to current thread's `local` run queue. */
static void coro_atomic_enqueue(routine_t *t) {
    atomic_thread_fence(memory_order_acquire);
    raii_deque_t *queue = gq_result.queue->local[t->tid];
    deque_push(queue, t);
    atomic_fetch_add(&queue->available, 1);
    atomic_thread_fence(memory_order_release);
}

RAII_INLINE void coro_enqueue(routine_t *t) {
    t->ready = true;
    /* Don't add initial coroutine representing main/child thread to local `deque` run queue.
    Also when flagged, a coroutine being added while system in interruption state. */
    if (t->flagged || !coro_is_threading() || coro_is_threading()
        && ((coro()->is_main
             && t->run_code == CORO_RUN_MAIN && !atomic_flag_load(&gq_result.is_started))
            || (!coro()->is_main
                && t->run_code == CORO_RUN_THRD && !coro()->started))) {
        coro_add(coro()->run_queue, t);
    } else {
        coro_atomic_enqueue(t);
    }
}

static RAII_INLINE routine_t *coro_dequeue(scheduler_t *l) {
    routine_t *t = nullptr;
    if (l->head != nullptr) {
        t = l->head;
        coro_remove(l, t);
    }

    return t;
}

#ifdef MPROTECT
alignas(4096)
#else
section(text)
#endif

#if defined(WIN32) && defined(USE_UCONTEXT)
int getcontext(ucontext_t *ucp) {
    int ret;

    /* Retrieve the full machine context */
    ucp->uc_mcontext.ContextFlags = CONTEXT_FULL;
    ret = GetThreadContext(GetCurrentThread(), &ucp->uc_mcontext);

    return (ret == 0) ? -1 : 0;
}

int setcontext(const ucontext_t *ucp) {
    int ret;

    /* Restore the full machine context (already set) */
    ret = SetThreadContext(GetCurrentThread(), &ucp->uc_mcontext);
    return (ret == 0) ? -1 : 0;
}

int makecontext(ucontext_t *ucp, void (*func)(), int argc, ...) {
    int i;
    va_list ap;
    char *sp;

    /* Stack grows down */
    sp = (char *)(size_t)ucp->uc_stack.ss_sp + ucp->uc_stack.ss_size;

    if (sp < (char *)ucp->uc_stack.ss_sp) {
        /* errno = ENOMEM;*/
        return RAII_ERR;
    }

    /* Set the instruction and the stack pointer */
#if defined(_X86_)
    ucp->uc_mcontext.Eip = (unsigned long long)coro_func;
    ucp->uc_mcontext.Esp = (unsigned long long)(sp - 4);
#else
    ucp->uc_mcontext.Rip = (unsigned long long)coro_func;
    ucp->uc_mcontext.Rsp = (unsigned long long)(sp - 40);
#endif
    /* Save/Restore the full machine context */
    ucp->uc_mcontext.ContextFlags = CONTEXT_FULL;

    /* Copy the arguments */
    va_start(ap, argc);
    for (i = 0; i < argc; i++) {
        memcpy(sp, ap, 8);
        ap += 8;
        sp += 8;
    }
    va_end(ap);

    return 0;
}

int swapcontext(routine_t *oucp, const routine_t *ucp) {
    int ret;

    if (is_empty(oucp) || is_empty(ucp)) {
        /*errno = EINVAL;*/
        return RAII_ERR;
    }

    ret = getcontext((ucontext_t *)oucp);
    if (ret == 0) {
        ret = setcontext((ucontext_t *)ucp);
    }
    return ret;
}
#endif

#if defined(USE_UCONTEXT)
static routine_t *coro_derive(void_t memory, size_t size) {
    if (!coro()->active_handle)
        coro()->active_handle = coro()->active_buffer;

    ucontext_t *contxt = (ucontext_t *)memory;
    memory = (unsigned char *)memory + sizeof(routine_t);
    size -= sizeof(routine_t);
    if ((!getcontext(contxt) && !(contxt->uc_stack.ss_sp = 0)) && (contxt->uc_stack.ss_sp = memory)) {
        contxt->uc_link = (ucontext_t *)coro()->active_handle;
        contxt->uc_stack.ss_size = size;
        makecontext(contxt, coro_func, 0);
    } else {
        raii_panic("getcontext failed!");
    }

    return (routine_t *)contxt;
}
#else

#if ((defined(__clang__) || defined(__GNUC__)) && defined(__i386__)) || (defined(_MSC_VER) && defined(_M_IX86))
/* ABI: fastcall */
static const unsigned char coro_swap_function[4096] = {
    0x89, 0x22,       /* mov [edx],esp    */
    0x8b, 0x21,       /* mov esp,[ecx]    */
    0x58,             /* pop eax          */
    0x89, 0x6a, 0x04, /* mov [edx+ 4],ebp */
    0x89, 0x72, 0x08, /* mov [edx+ 8],esi */
    0x89, 0x7a, 0x0c, /* mov [edx+12],edi */
    0x89, 0x5a, 0x10, /* mov [edx+16],ebx */
    0x8b, 0x69, 0x04, /* mov ebp,[ecx+ 4] */
    0x8b, 0x71, 0x08, /* mov esi,[ecx+ 8] */
    0x8b, 0x79, 0x0c, /* mov edi,[ecx+12] */
    0x8b, 0x59, 0x10, /* mov ebx,[ecx+16] */
    0xff, 0xe0,       /* jmp eax          */
};

#ifdef _WIN32
#include <windows.h>
static void coro_init(void) {
#ifdef MPROTECT
    DWORD old_privileges;
    VirtualProtect((void_t)coro_swap_function, sizeof coro_swap_function, PAGE_EXECUTE_READ, &old_privileges);
#endif
}
#else
#ifdef MPROTECT
#include <unistd.h>
#include <sys/mman.h>
#endif

static void coro_init(void) {
#ifdef MPROTECT
    unsigned long addr = (unsigned long)coro_swap_function;
    unsigned long base = addr - (addr % sysconf(_SC_PAGESIZE));
    unsigned long size = (addr - base) + sizeof coro_swap_function;
    mprotect((void_t)base, size, PROT_READ | PROT_EXEC);
#endif
}
#endif

routine_t *coro_derive(void_t memory, size_t size) {
    routine_t *handle;
    if (!coro_swap) {
        coro_init();
        coro_swap = (void(fastcall *)(routine_t *, routine_t *))coro_swap_function;
    }

    if ((handle = (routine_t *)memory)) {
        unsigned long stack_top = (unsigned long)handle + size;
        stack_top -= 32;
        stack_top &= ~((unsigned long)15);
        long *p = (long *)(stack_top); /* seek to top of stack */
        *--p = (long)coro_done;          /* if func returns */
        *--p = (long)coro_awaitable;     /* start of function */
        *(long *)handle = (long)p;     /* stack pointer */

#ifdef USE_VALGRIND
        size_t stack_addr = _coro_align_forward((size_t)handle + sizeof(routine_t), 16);
        handle->vg_stack_id = VALGRIND_STACK_REGISTER(stack_addr, stack_addr + size);
#endif
    }

    return handle;
}
#elif ((defined(__clang__) || defined(__GNUC__)) && defined(__amd64__)) || (defined(_MSC_VER) && defined(_M_AMD64))
#ifdef _WIN32
/* ABI: Win64 */
static const unsigned char coro_swap_function[4096] = {
    0x48, 0x89, 0x22,             /* mov [rdx],rsp           */
    0x48, 0x8b, 0x21,             /* mov rsp,[rcx]           */
    0x58,                         /* pop rax                 */
    0x48, 0x83, 0xe9, 0x80,       /* sub rcx,-0x80           */
    0x48, 0x83, 0xea, 0x80,       /* sub rdx,-0x80           */
    0x48, 0x89, 0x6a, 0x88,       /* mov [rdx-0x78],rbp      */
    0x48, 0x89, 0x72, 0x90,       /* mov [rdx-0x70],rsi      */
    0x48, 0x89, 0x7a, 0x98,       /* mov [rdx-0x68],rdi      */
    0x48, 0x89, 0x5a, 0xa0,       /* mov [rdx-0x60],rbx      */
    0x4c, 0x89, 0x62, 0xa8,       /* mov [rdx-0x58],r12      */
    0x4c, 0x89, 0x6a, 0xb0,       /* mov [rdx-0x50],r13      */
    0x4c, 0x89, 0x72, 0xb8,       /* mov [rdx-0x48],r14      */
    0x4c, 0x89, 0x7a, 0xc0,       /* mov [rdx-0x40],r15      */
#if !defined(NO_SSE)
        0x0f, 0x29, 0x72, 0xd0,       /* movaps [rdx-0x30],xmm6  */
        0x0f, 0x29, 0x7a, 0xe0,       /* movaps [rdx-0x20],xmm7  */
        0x44, 0x0f, 0x29, 0x42, 0xf0, /* movaps [rdx-0x10],xmm8  */
        0x44, 0x0f, 0x29, 0x0a,       /* movaps [rdx],     xmm9  */
        0x44, 0x0f, 0x29, 0x52, 0x10, /* movaps [rdx+0x10],xmm10 */
        0x44, 0x0f, 0x29, 0x5a, 0x20, /* movaps [rdx+0x20],xmm11 */
        0x44, 0x0f, 0x29, 0x62, 0x30, /* movaps [rdx+0x30],xmm12 */
        0x44, 0x0f, 0x29, 0x6a, 0x40, /* movaps [rdx+0x40],xmm13 */
        0x44, 0x0f, 0x29, 0x72, 0x50, /* movaps [rdx+0x50],xmm14 */
        0x44, 0x0f, 0x29, 0x7a, 0x60, /* movaps [rdx+0x60],xmm15 */
#endif
        0x48, 0x8b, 0x69, 0x88,       /* mov rbp,[rcx-0x78]      */
        0x48, 0x8b, 0x71, 0x90,       /* mov rsi,[rcx-0x70]      */
        0x48, 0x8b, 0x79, 0x98,       /* mov rdi,[rcx-0x68]      */
        0x48, 0x8b, 0x59, 0xa0,       /* mov rbx,[rcx-0x60]      */
        0x4c, 0x8b, 0x61, 0xa8,       /* mov r12,[rcx-0x58]      */
        0x4c, 0x8b, 0x69, 0xb0,       /* mov r13,[rcx-0x50]      */
        0x4c, 0x8b, 0x71, 0xb8,       /* mov r14,[rcx-0x48]      */
        0x4c, 0x8b, 0x79, 0xc0,       /* mov r15,[rcx-0x40]      */
#if !defined(NO_SSE)
        0x0f, 0x28, 0x71, 0xd0,       /* movaps xmm6, [rcx-0x30] */
        0x0f, 0x28, 0x79, 0xe0,       /* movaps xmm7, [rcx-0x20] */
        0x44, 0x0f, 0x28, 0x41, 0xf0, /* movaps xmm8, [rcx-0x10] */
        0x44, 0x0f, 0x28, 0x09,       /* movaps xmm9, [rcx]      */
        0x44, 0x0f, 0x28, 0x51, 0x10, /* movaps xmm10,[rcx+0x10] */
        0x44, 0x0f, 0x28, 0x59, 0x20, /* movaps xmm11,[rcx+0x20] */
        0x44, 0x0f, 0x28, 0x61, 0x30, /* movaps xmm12,[rcx+0x30] */
        0x44, 0x0f, 0x28, 0x69, 0x40, /* movaps xmm13,[rcx+0x40] */
        0x44, 0x0f, 0x28, 0x71, 0x50, /* movaps xmm14,[rcx+0x50] */
        0x44, 0x0f, 0x28, 0x79, 0x60, /* movaps xmm15,[rcx+0x60] */
#endif
#if !defined(NO_TIB)
        0x65, 0x4c, 0x8b, 0x04, 0x25, /* mov r8,gs:0x30          */
        0x30, 0x00, 0x00, 0x00,
        0x41, 0x0f, 0x10, 0x40, 0x08, /* movups xmm0,[r8+0x8]    */
        0x0f, 0x29, 0x42, 0x70,       /* movaps [rdx+0x70],xmm0  */
        0x0f, 0x28, 0x41, 0x70,       /* movaps xmm0,[rcx+0x70]  */
        0x41, 0x0f, 0x11, 0x40, 0x08, /* movups [r8+0x8],xmm0    */
#endif
        0xff, 0xe0,                   /* jmp rax                 */
};

#include <windows.h>

static void coro_init(void) {
#ifdef MPROTECT
    DWORD old_privileges;
    VirtualProtect((void_t)coro_swap_function, sizeof coro_swap_function, PAGE_EXECUTE_READ, &old_privileges);
#endif
}
#else
/* ABI: SystemV */
static const unsigned char coro_swap_function[4096] = {
    0x48, 0x89, 0x26,       /* mov [rsi],rsp    */
    0x48, 0x8b, 0x27,       /* mov rsp,[rdi]    */
    0x58,                   /* pop rax          */
    0x48, 0x89, 0x6e, 0x08, /* mov [rsi+ 8],rbp */
    0x48, 0x89, 0x5e, 0x10, /* mov [rsi+16],rbx */
    0x4c, 0x89, 0x66, 0x18, /* mov [rsi+24],r12 */
    0x4c, 0x89, 0x6e, 0x20, /* mov [rsi+32],r13 */
    0x4c, 0x89, 0x76, 0x28, /* mov [rsi+40],r14 */
    0x4c, 0x89, 0x7e, 0x30, /* mov [rsi+48],r15 */
    0x48, 0x8b, 0x6f, 0x08, /* mov rbp,[rdi+ 8] */
    0x48, 0x8b, 0x5f, 0x10, /* mov rbx,[rdi+16] */
    0x4c, 0x8b, 0x67, 0x18, /* mov r12,[rdi+24] */
    0x4c, 0x8b, 0x6f, 0x20, /* mov r13,[rdi+32] */
    0x4c, 0x8b, 0x77, 0x28, /* mov r14,[rdi+40] */
    0x4c, 0x8b, 0x7f, 0x30, /* mov r15,[rdi+48] */
    0xff, 0xe0,             /* jmp rax          */
};

#ifdef MPROTECT
#include <unistd.h>
#include <sys/mman.h>
#endif

static void coro_init(void) {
#ifdef MPROTECT
    unsigned long long addr = (unsigned long long)coro_swap_function;
    unsigned long long base = addr - (addr % sysconf(_SC_PAGESIZE));
    unsigned long long size = (addr - base) + sizeof coro_swap_function;
    mprotect((void_t)base, size, PROT_READ | PROT_EXEC);
#endif
}
#endif
routine_t *coro_derive(void_t memory, size_t size) {
    routine_t *handle;
    if (!coro_swap) {
        coro_init();
        coro_swap = (void (*)(routine_t *, routine_t *))coro_swap_function;
    }

    if ((handle = (routine_t *)memory)) {
        size_t stack_top = (size_t)handle + size;
        stack_top -= 32;
        stack_top &= ~((size_t)15);
        int64_t *p = (int64_t *)(stack_top); /* seek to top of stack */
        *--p = (int64_t)coro_done;               /* if coroutine returns */
        *--p = (int64_t)coro_awaitable;
        *(int64_t *)handle = (int64_t)p;                  /* stack pointer */
#if defined(_WIN32) && !defined(NO_TIB)
        ((int64_t *)handle)[30] = (int64_t)handle + size; /* stack base */
        ((int64_t *)handle)[31] = (int64_t)handle;        /* stack limit */
#endif

#ifdef USE_VALGRIND
        size_t stack_addr = _coro_align_forward((size_t)handle + sizeof(routine_t), 16);
        handle->vg_stack_id = VALGRIND_STACK_REGISTER(stack_addr, stack_addr + size);
#endif
    }

    return handle;
}
#elif defined(__clang__) || defined(__GNUC__)
#if defined(__arm__)
#ifdef MPROTECT
#include <unistd.h>
#include <sys/mman.h>
#endif

static const size_t coro_swap_function[1024] = {
    0xe8a16ff0, /* stmia r1!, {r4-r11,sp,lr} */
    0xe8b0aff0, /* ldmia r0!, {r4-r11,sp,pc} */
    0xe12fff1e, /* bx lr                     */
};

static void coro_init(void) {
#ifdef MPROTECT
    size_t addr = (size_t)coro_swap_function;
    size_t base = addr - (addr % sysconf(_SC_PAGESIZE));
    size_t size = (addr - base) + sizeof coro_swap_function;
    mprotect((void_t)base, size, PROT_READ | PROT_EXEC);
#endif
}

routine_t *coro_derive(void_t memory, size_t size) {
    size_t *handle;
    routine_t *co;
    if (!coro_swap) {
        coro_init();
        coro_swap = (void (*)(routine_t *, routine_t *))coro_swap_function;
    }

    if ((handle = (size_t *)memory)) {
        size_t stack_top = (size_t)handle + size;
        stack_top &= ~((size_t)15);
        size_t *p = (size_t *)(stack_top);
        handle[8] = (size_t)p;
        handle[9] = (size_t)coro_func;

        co = (routine_t *)handle;
#ifdef USE_VALGRIND
        size_t stack_addr = _coro_align_forward((size_t)co + sizeof(routine_t), 16);
        co->vg_stack_id = VALGRIND_STACK_REGISTER(stack_addr, stack_addr + size);
#endif
    }

    return co;
}
#elif defined(__aarch64__)
static const uint32_t coro_swap_function[1024] = {
    0x910003f0, /* mov x16,sp           */
    0xa9007830, /* stp x16,x30,[x1]     */
    0xa9407810, /* ldp x16,x30,[x0]     */
    0x9100021f, /* mov sp,x16           */
    0xa9015033, /* stp x19,x20,[x1, 16] */
    0xa9415013, /* ldp x19,x20,[x0, 16] */
    0xa9025835, /* stp x21,x22,[x1, 32] */
    0xa9425815, /* ldp x21,x22,[x0, 32] */
    0xa9036037, /* stp x23,x24,[x1, 48] */
    0xa9436017, /* ldp x23,x24,[x0, 48] */
    0xa9046839, /* stp x25,x26,[x1, 64] */
    0xa9446819, /* ldp x25,x26,[x0, 64] */
    0xa905703b, /* stp x27,x28,[x1, 80] */
    0xa945701b, /* ldp x27,x28,[x0, 80] */
    0xf900303d, /* str x29,    [x1, 96] */
    0xf940301d, /* ldr x29,    [x0, 96] */
    0x6d072428, /* stp d8, d9, [x1,112] */
    0x6d472408, /* ldp d8, d9, [x0,112] */
    0x6d082c2a, /* stp d10,d11,[x1,128] */
    0x6d482c0a, /* ldp d10,d11,[x0,128] */
    0x6d09342c, /* stp d12,d13,[x1,144] */
    0x6d49340c, /* ldp d12,d13,[x0,144] */
    0x6d0a3c2e, /* stp d14,d15,[x1,160] */
    0x6d4a3c0e, /* ldp d14,d15,[x0,160] */
#if defined(_WIN32) && !defined(NO_TIB)
    0xa940c650, /* ldp x16,x17,[x18, 8] */
    0xa90b4430, /* stp x16,x17,[x1,176] */
    0xa94b4410, /* ldp x16,x17,[x0,176] */
    0xa900c650, /* stp x16,x17,[x18, 8] */
#endif
    0xd61f03c0, /* br x30               */
};

#ifdef _WIN32
#include <windows.h>

static void coro_init(void) {
#ifdef MPROTECT
    DWORD old_privileges;
    VirtualProtect((void_t)coro_swap_function, sizeof coro_swap_function, PAGE_EXECUTE_READ, &old_privileges);
#endif
}
#else
#ifdef MPROTECT
#include <unistd.h>
#include <sys/mman.h>
#endif

static void coro_init(void) {
#ifdef MPROTECT
    size_t addr = (size_t)coro_swap_function;
    size_t base = addr - (addr % sysconf(_SC_PAGESIZE));
    size_t size = (addr - base) + sizeof coro_swap_function;
    mprotect((void_t)base, size, PROT_READ | PROT_EXEC);
#endif
}
#endif

routine_t *coro_derive(void_t memory, size_t size) {
    size_t *handle;
    routine_t *co;
    if (!coro_swap) {
        coro_init();
        coro_swap = (void (*)(routine_t *, routine_t *))coro_swap_function;
    }

    if ((handle = (size_t *)memory)) {
        size_t stack_top = (size_t)handle + size;
        stack_top &= ~((size_t)15);
        size_t *p = (size_t *)(stack_top);
        handle[0] = (size_t)p;              /* x16 (stack pointer) */
        handle[1] = (size_t)coro_func;        /* x30 (link register) */
        handle[12] = (size_t)p;             /* x29 (frame pointer) */

#if defined(_WIN32) && !defined(NO_TIB)
        handle[22] = (size_t)handle + size; /* stack base */
        handle[23] = (size_t)handle;        /* stack limit */
#endif

        co = (routine_t *)handle;
#ifdef USE_VALGRIND
        size_t stack_addr = _coro_align_forward((size_t)co + sizeof(routine_t), 16);
        co->vg_stack_id = VALGRIND_STACK_REGISTER(stack_addr, stack_addr + size);
#endif
    }

    return co;
}
#elif defined(__powerpc64__) && defined(_CALL_ELF) && _CALL_ELF == 2
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define ALIGN(p, x) ((void_t)((uintptr_t)(p) & ~((x)-1)))

#define MIN_STACK 0x10000lu
#define MIN_STACK_FRAME 0x20lu
#define STACK_ALIGN 0x10lu

static void coro_init(void) {}

void swap_context(routine_t *read, routine_t *write);
__asm__(
    ".text\n"
    ".align 4\n"
    ".type swap_context @function\n"
    "swap_context:\n"
    ".cfi_startproc\n"

    /* save GPRs */
    "std 1, 8(4)\n"
    "std 2, 16(4)\n"
    "std 12, 96(4)\n"
    "std 13, 104(4)\n"
    "std 14, 112(4)\n"
    "std 15, 120(4)\n"
    "std 16, 128(4)\n"
    "std 17, 136(4)\n"
    "std 18, 144(4)\n"
    "std 19, 152(4)\n"
    "std 20, 160(4)\n"
    "std 21, 168(4)\n"
    "std 22, 176(4)\n"
    "std 23, 184(4)\n"
    "std 24, 192(4)\n"
    "std 25, 200(4)\n"
    "std 26, 208(4)\n"
    "std 27, 216(4)\n"
    "std 28, 224(4)\n"
    "std 29, 232(4)\n"
    "std 30, 240(4)\n"
    "std 31, 248(4)\n"

    /* save LR */
    "mflr 5\n"
    "std 5, 256(4)\n"

    /* save CCR */
    "mfcr 5\n"
    "std 5, 264(4)\n"

    /* save FPRs */
    "stfd 14, 384(4)\n"
    "stfd 15, 392(4)\n"
    "stfd 16, 400(4)\n"
    "stfd 17, 408(4)\n"
    "stfd 18, 416(4)\n"
    "stfd 19, 424(4)\n"
    "stfd 20, 432(4)\n"
    "stfd 21, 440(4)\n"
    "stfd 22, 448(4)\n"
    "stfd 23, 456(4)\n"
    "stfd 24, 464(4)\n"
    "stfd 25, 472(4)\n"
    "stfd 26, 480(4)\n"
    "stfd 27, 488(4)\n"
    "stfd 28, 496(4)\n"
    "stfd 29, 504(4)\n"
    "stfd 30, 512(4)\n"
    "stfd 31, 520(4)\n"

#ifdef __ALTIVEC__
    /* save VMX */
    "li 5, 528\n"
    "stvxl 20, 4, 5\n"
    "addi 5, 5, 16\n"
    "stvxl 21, 4, 5\n"
    "addi 5, 5, 16\n"
    "stvxl 22, 4, 5\n"
    "addi 5, 5, 16\n"
    "stvxl 23, 4, 5\n"
    "addi 5, 5, 16\n"
    "stvxl 24, 4, 5\n"
    "addi 5, 5, 16\n"
    "stvxl 25, 4, 5\n"
    "addi 5, 5, 16\n"
    "stvxl 26, 4, 5\n"
    "addi 5, 5, 16\n"
    "stvxl 27, 4, 5\n"
    "addi 5, 5, 16\n"
    "stvxl 28, 4, 5\n"
    "addi 5, 5, 16\n"
    "stvxl 29, 4, 5\n"
    "addi 5, 5, 16\n"
    "stvxl 30, 4, 5\n"
    "addi 5, 5, 16\n"
    "stvxl 31, 4, 5\n"
    "addi 5, 5, 16\n"

    /* save VRSAVE */
    "mfvrsave 5\n"
    "stw 5, 736(4)\n"
#endif

    /* restore GPRs */
    "ld 1, 8(3)\n"
    "ld 2, 16(3)\n"
    "ld 12, 96(3)\n"
    "ld 13, 104(3)\n"
    "ld 14, 112(3)\n"
    "ld 15, 120(3)\n"
    "ld 16, 128(3)\n"
    "ld 17, 136(3)\n"
    "ld 18, 144(3)\n"
    "ld 19, 152(3)\n"
    "ld 20, 160(3)\n"
    "ld 21, 168(3)\n"
    "ld 22, 176(3)\n"
    "ld 23, 184(3)\n"
    "ld 24, 192(3)\n"
    "ld 25, 200(3)\n"
    "ld 26, 208(3)\n"
    "ld 27, 216(3)\n"
    "ld 28, 224(3)\n"
    "ld 29, 232(3)\n"
    "ld 30, 240(3)\n"
    "ld 31, 248(3)\n"

    /* restore LR */
    "ld 5, 256(3)\n"
    "mtlr 5\n"

    /* restore CCR */
    "ld 5, 264(3)\n"
    "mtcr 5\n"

    /* restore FPRs */
    "lfd 14, 384(3)\n"
    "lfd 15, 392(3)\n"
    "lfd 16, 400(3)\n"
    "lfd 17, 408(3)\n"
    "lfd 18, 416(3)\n"
    "lfd 19, 424(3)\n"
    "lfd 20, 432(3)\n"
    "lfd 21, 440(3)\n"
    "lfd 22, 448(3)\n"
    "lfd 23, 456(3)\n"
    "lfd 24, 464(3)\n"
    "lfd 25, 472(3)\n"
    "lfd 26, 480(3)\n"
    "lfd 27, 488(3)\n"
    "lfd 28, 496(3)\n"
    "lfd 29, 504(3)\n"
    "lfd 30, 512(3)\n"
    "lfd 31, 520(3)\n"

#ifdef __ALTIVEC__
    /* restore VMX */
    "li 5, 528\n"
    "lvxl 20, 3, 5\n"
    "addi 5, 5, 16\n"
    "lvxl 21, 3, 5\n"
    "addi 5, 5, 16\n"
    "lvxl 22, 3, 5\n"
    "addi 5, 5, 16\n"
    "lvxl 23, 3, 5\n"
    "addi 5, 5, 16\n"
    "lvxl 24, 3, 5\n"
    "addi 5, 5, 16\n"
    "lvxl 25, 3, 5\n"
    "addi 5, 5, 16\n"
    "lvxl 26, 3, 5\n"
    "addi 5, 5, 16\n"
    "lvxl 27, 3, 5\n"
    "addi 5, 5, 16\n"
    "lvxl 28, 3, 5\n"
    "addi 5, 5, 16\n"
    "lvxl 29, 3, 5\n"
    "addi 5, 5, 16\n"
    "lvxl 30, 3, 5\n"
    "addi 5, 5, 16\n"
    "lvxl 31, 3, 5\n"
    "addi 5, 5, 16\n"

    /* restore VRSAVE */
    "lwz 5, 720(3)\n"
    "mtvrsave 5\n"
#endif

    /* branch to LR */
    "blr\n"

    ".cfi_endproc\n"
    ".size swap_context, .-swap_context\n");

routine_t *coro_derive(void_t memory, size_t size) {
    uint8_t *sp;
    routine_t *context = (routine_t *)memory;
    if (!coro_swap) {
        coro_swap = (void (*)(routine_t *, routine_t *))swap_context;
    }

    /* save current context into new context to initialize it */
    swap_context(context, context);

    /* align stack */
    sp = (uint8_t *)memory + size - STACK_ALIGN;
    sp = (uint8_t *)ALIGN(sp, STACK_ALIGN);

    /* write 0 for initial backchain */
    *(uint64_t *)sp = 0;

    /* create new frame with backchain */
    sp -= MIN_STACK_FRAME;
    *(uint64_t *)sp = (uint64_t)(sp + MIN_STACK_FRAME);

    /* update context with new stack (r1) and func (r12, lr) */
    context->gprs[1] = (uint64_t)sp;
    context->gprs[12] = (uint64_t)coro_func;
    context->lr = (uint64_t)coro_func;

#ifdef USE_VALGRIND
    size_t stack_addr = _coro_align_forward((size_t)context + sizeof(routine_t), 16);
    context->vg_stack_id = VALGRIND_STACK_REGISTER(stack_addr, stack_addr + size);
#endif
    return context;
}

#elif defined(__ARM_EABI__)
void swap_context(routine_t *from, routine_t *to);
__asm__(
    ".text\n"
#ifdef __APPLE__
    ".globl _swap_context\n"
    "_swap_context:\n"
#else
    ".globl swap_context\n"
    ".type swap_context #function\n"
    ".hidden swap_context\n"
    "swap_context:\n"
#endif

#ifndef __SOFTFP__
    "vstmia r0!, {d8-d15}\n"
#endif
    "stmia r0, {r4-r11, lr}\n"
    ".byte 0xE5, 0x80,  0xD0, 0x24\n" /* should be "str sp, [r0, #9*4]\n", it's causing vscode display issue */
#ifndef __SOFTFP__
    "vldmia r1!, {d8-d15}\n"
#endif
    ".byte 0xE5, 0x91, 0xD0, 0x24\n" /* should be "ldr sp, [r1, #9*4]\n", it's causing vscode display issue */
    "ldmia r1, {r4-r11, pc}\n"
#ifndef __APPLE__
    ".size swap_context, .-swap_context\n"
#endif
);

static void coro_init(void) {}

routine_t *coro_derive(void_t memory, size_t size) {
    routine_t *ctx = (routine_t *)memory;
    if (!coro_swap) {
        coro_swap = (void (*)(routine_t *, routine_t *))swap_context;
    }

    ctx->d[0] = memory;
    ctx->d[1] = (void *)(coro_awaitable);
    ctx->d[2] = (void *)(coro_done);
    ctx->lr = (void *)(coro_awaitable);
    ctx->sp = (void *)((size_t)memory + size);
#ifdef USE_VALGRIND
    size_t stack_addr = _coro_align_forward((size_t)memory + sizeof(routine_t), 16);
    ctx->vg_stack_id = VALGRIND_STACK_REGISTER(stack_addr, stack_addr + size);
#endif

    return ctx;
}

#elif defined(__riscv)
void swap_context(routine_t *from, routine_t *to);
__asm__(
    ".text\n"
    ".globl swap_context\n"
    ".type swap_context @function\n"
    ".hidden swap_context\n"
    "swap_context:\n"
#if __riscv_xlen == 64
    "  sd s0, 0x00(a0)\n"
    "  sd s1, 0x08(a0)\n"
    "  sd s2, 0x10(a0)\n"
    "  sd s3, 0x18(a0)\n"
    "  sd s4, 0x20(a0)\n"
    "  sd s5, 0x28(a0)\n"
    "  sd s6, 0x30(a0)\n"
    "  sd s7, 0x38(a0)\n"
    "  sd s8, 0x40(a0)\n"
    "  sd s9, 0x48(a0)\n"
    "  sd s10, 0x50(a0)\n"
    "  sd s11, 0x58(a0)\n"
    "  sd ra, 0x60(a0)\n"
    "  sd ra, 0x68(a0)\n" // pc
    "  sd sp, 0x70(a0)\n"
#ifdef __riscv_flen
#if __riscv_flen == 64
    "  fsd fs0, 0x78(a0)\n"
    "  fsd fs1, 0x80(a0)\n"
    "  fsd fs2, 0x88(a0)\n"
    "  fsd fs3, 0x90(a0)\n"
    "  fsd fs4, 0x98(a0)\n"
    "  fsd fs5, 0xa0(a0)\n"
    "  fsd fs6, 0xa8(a0)\n"
    "  fsd fs7, 0xb0(a0)\n"
    "  fsd fs8, 0xb8(a0)\n"
    "  fsd fs9, 0xc0(a0)\n"
    "  fsd fs10, 0xc8(a0)\n"
    "  fsd fs11, 0xd0(a0)\n"
    "  fld fs0, 0x78(a1)\n"
    "  fld fs1, 0x80(a1)\n"
    "  fld fs2, 0x88(a1)\n"
    "  fld fs3, 0x90(a1)\n"
    "  fld fs4, 0x98(a1)\n"
    "  fld fs5, 0xa0(a1)\n"
    "  fld fs6, 0xa8(a1)\n"
    "  fld fs7, 0xb0(a1)\n"
    "  fld fs8, 0xb8(a1)\n"
    "  fld fs9, 0xc0(a1)\n"
    "  fld fs10, 0xc8(a1)\n"
    "  fld fs11, 0xd0(a1)\n"
#else
#error "Unsupported RISC-V FLEN"
#endif
#endif //  __riscv_flen
    "  ld s0, 0x00(a1)\n"
    "  ld s1, 0x08(a1)\n"
    "  ld s2, 0x10(a1)\n"
    "  ld s3, 0x18(a1)\n"
    "  ld s4, 0x20(a1)\n"
    "  ld s5, 0x28(a1)\n"
    "  ld s6, 0x30(a1)\n"
    "  ld s7, 0x38(a1)\n"
    "  ld s8, 0x40(a1)\n"
    "  ld s9, 0x48(a1)\n"
    "  ld s10, 0x50(a1)\n"
    "  ld s11, 0x58(a1)\n"
    "  ld ra, 0x60(a1)\n"
    "  ld a2, 0x68(a1)\n" // pc
    "  ld sp, 0x70(a1)\n"
    "  jr a2\n"
#elif __riscv_xlen == 32
    "  sw s0, 0x00(a0)\n"
    "  sw s1, 0x04(a0)\n"
    "  sw s2, 0x08(a0)\n"
    "  sw s3, 0x0c(a0)\n"
    "  sw s4, 0x10(a0)\n"
    "  sw s5, 0x14(a0)\n"
    "  sw s6, 0x18(a0)\n"
    "  sw s7, 0x1c(a0)\n"
    "  sw s8, 0x20(a0)\n"
    "  sw s9, 0x24(a0)\n"
    "  sw s10, 0x28(a0)\n"
    "  sw s11, 0x2c(a0)\n"
    "  sw ra, 0x30(a0)\n"
    "  sw ra, 0x34(a0)\n" // pc
    "  sw sp, 0x38(a0)\n"
#ifdef __riscv_flen
#if __riscv_flen == 64
    "  fsd fs0, 0x3c(a0)\n"
    "  fsd fs1, 0x44(a0)\n"
    "  fsd fs2, 0x4c(a0)\n"
    "  fsd fs3, 0x54(a0)\n"
    "  fsd fs4, 0x5c(a0)\n"
    "  fsd fs5, 0x64(a0)\n"
    "  fsd fs6, 0x6c(a0)\n"
    "  fsd fs7, 0x74(a0)\n"
    "  fsd fs8, 0x7c(a0)\n"
    "  fsd fs9, 0x84(a0)\n"
    "  fsd fs10, 0x8c(a0)\n"
    "  fsd fs11, 0x94(a0)\n"
    "  fld fs0, 0x3c(a1)\n"
    "  fld fs1, 0x44(a1)\n"
    "  fld fs2, 0x4c(a1)\n"
    "  fld fs3, 0x54(a1)\n"
    "  fld fs4, 0x5c(a1)\n"
    "  fld fs5, 0x64(a1)\n"
    "  fld fs6, 0x6c(a1)\n"
    "  fld fs7, 0x74(a1)\n"
    "  fld fs8, 0x7c(a1)\n"
    "  fld fs9, 0x84(a1)\n"
    "  fld fs10, 0x8c(a1)\n"
    "  fld fs11, 0x94(a1)\n"
#elif __riscv_flen == 32
    "  fsw fs0, 0x3c(a0)\n"
    "  fsw fs1, 0x40(a0)\n"
    "  fsw fs2, 0x44(a0)\n"
    "  fsw fs3, 0x48(a0)\n"
    "  fsw fs4, 0x4c(a0)\n"
    "  fsw fs5, 0x50(a0)\n"
    "  fsw fs6, 0x54(a0)\n"
    "  fsw fs7, 0x58(a0)\n"
    "  fsw fs8, 0x5c(a0)\n"
    "  fsw fs9, 0x60(a0)\n"
    "  fsw fs10, 0x64(a0)\n"
    "  fsw fs11, 0x68(a0)\n"
    "  flw fs0, 0x3c(a1)\n"
    "  flw fs1, 0x40(a1)\n"
    "  flw fs2, 0x44(a1)\n"
    "  flw fs3, 0x48(a1)\n"
    "  flw fs4, 0x4c(a1)\n"
    "  flw fs5, 0x50(a1)\n"
    "  flw fs6, 0x54(a1)\n"
    "  flw fs7, 0x58(a1)\n"
    "  flw fs8, 0x5c(a1)\n"
    "  flw fs9, 0x60(a1)\n"
    "  flw fs10, 0x64(a1)\n"
    "  flw fs11, 0x68(a1)\n"
#else
#error "Unsupported RISC-V FLEN"
#endif
#endif // __riscv_flen
    "  lw s0, 0x00(a1)\n"
    "  lw s1, 0x04(a1)\n"
    "  lw s2, 0x08(a1)\n"
    "  lw s3, 0x0c(a1)\n"
    "  lw s4, 0x10(a1)\n"
    "  lw s5, 0x14(a1)\n"
    "  lw s6, 0x18(a1)\n"
    "  lw s7, 0x1c(a1)\n"
    "  lw s8, 0x20(a1)\n"
    "  lw s9, 0x24(a1)\n"
    "  lw s10, 0x28(a1)\n"
    "  lw s11, 0x2c(a1)\n"
    "  lw ra, 0x30(a1)\n"
    "  lw a2, 0x34(a1)\n" // pc
    "  lw sp, 0x38(a1)\n"
    "  jr a2\n"
#else
#error "Unsupported RISC-V XLEN"
#endif // __riscv_xlen
    ".size swap_context, .-swap_context\n"
);

static void coro_init(void) {}

routine_t *coro_derive(void_t memory, size_t size) {
    routine_t *ctx = (routine_t *)memory;
    if (!coro_swap) {
        coro_swap = (void (*)(routine_t *, routine_t *))swap_context;
    }

    ctx->s[0] = memory;
    ctx->s[1] = (void *)(coro_awaitable);
    ctx->pc = (void *)(coro_awaitable);
    ctx->ra = (void *)(coro_done);
    ctx->sp = (void *)((size_t)memory + size);
#ifdef USE_VALGRIND
    size_t stack_addr = _coro_align_forward((size_t)memory + sizeof(routine_t), 16);
    ctx->vg_stack_id = VALGRIND_STACK_REGISTER(stack_addr, stack_addr + size);
#endif

    return ctx;
}
/*
#if __riscv_xlen == 32
#define I_STORE "sw"
#define I_LOAD  "lw"
#elif __riscv_xlen == 64
#define I_STORE "sd"
#define I_LOAD  "ld"
#else
#error Unsupported RISC-V XLEN
#endif

#if !defined(__riscv_flen)
#define F_STORE "#"
#define F_LOAD  "#"
#elif __riscv_flen == 32
#define F_STORE "fsw"
#define F_LOAD  "flw"
#elif __riscv_flen == 64
#define F_STORE "fsd"
#define F_LOAD  "fld"
#else
#error Unsupported RISC-V FLEN
#endif

__attribute__((naked))
static void swap_context(routine_t *active, routine_t *previous) {
__asm__(
  I_STORE " ra,   0 *8(a1)\n"
  I_STORE " sp,   1 *8(a1)\n"
  I_STORE " s0,   2 *8(a1)\n"
  I_STORE " s1,   3 *8(a1)\n"
  I_STORE " s2,   4 *8(a1)\n"
  I_STORE " s3,   5 *8(a1)\n"
  I_STORE " s4,   6 *8(a1)\n"
  I_STORE " s5,   7 *8(a1)\n"
  I_STORE " s6,   8 *8(a1)\n"
  I_STORE " s7,   9 *8(a1)\n"
  I_STORE " s8,   10*8(a1)\n"
  I_STORE " s9,   11*8(a1)\n"
  I_STORE " s10,  12*8(a1)\n"
  I_STORE " s11,  13*8(a1)\n"

  F_STORE " fs0,  14*8(a1)\n"
  F_STORE " fs1,  15*8(a1)\n"
  F_STORE " fs2,  16*8(a1)\n"
  F_STORE " fs3,  17*8(a1)\n"
  F_STORE " fs4,  18*8(a1)\n"
  F_STORE " fs5,  19*8(a1)\n"
  F_STORE " fs6,  20*8(a1)\n"
  F_STORE " fs7,  21*8(a1)\n"
  F_STORE " fs8,  22*8(a1)\n"
  F_STORE " fs9,  23*8(a1)\n"
  F_STORE " fs10, 24*8(a1)\n"
  F_STORE " fs11, 25*8(a1)\n"

  I_LOAD  " ra,   0 *8(a0)\n"
  I_LOAD  " sp,   1 *8(a0)\n"
  I_LOAD  " s0,   2 *8(a0)\n"
  I_LOAD  " s1,   3 *8(a0)\n"
  I_LOAD  " s2,   4 *8(a0)\n"
  I_LOAD  " s3,   5 *8(a0)\n"
  I_LOAD  " s4,   6 *8(a0)\n"
  I_LOAD  " s5,   7 *8(a0)\n"
  I_LOAD  " s6,   8 *8(a0)\n"
  I_LOAD  " s7,   9 *8(a0)\n"
  I_LOAD  " s8,   10*8(a0)\n"
  I_LOAD  " s9,   11*8(a0)\n"
  I_LOAD  " s10,  12*8(a0)\n"
  I_LOAD  " s11,  13*8(a0)\n"

  F_LOAD  " fs0,  14*8(a0)\n"
  F_LOAD  " fs1,  15*8(a0)\n"
  F_LOAD  " fs2,  16*8(a0)\n"
  F_LOAD  " fs3,  17*8(a0)\n"
  F_LOAD  " fs4,  18*8(a0)\n"
  F_LOAD  " fs5,  19*8(a0)\n"
  F_LOAD  " fs6,  20*8(a0)\n"
  F_LOAD  " fs7,  21*8(a0)\n"
  F_LOAD  " fs8,  22*8(a0)\n"
  F_LOAD  " fs9,  23*8(a0)\n"
  F_LOAD  " fs10, 24*8(a0)\n"
  F_LOAD  " fs11, 25*8(a0)\n"

  "ret\n"
);
}

routine_t *coro_derive(void_t memory, size_t size) {
    uint64_t *handle;
    if (!coro_swap) {
        coro_swap = (void (*)(routine_t *, routine_t *))swap_context;
    }

    if (handle = (uint64_t *)memory) {
        unsigned int offset = (size & ~15);
        uint64_t *p = (uint64_t *)((uint8_t *)handle + offset);
        *(uintptr_t *)&handle[0] = (uintptr_t)coro_func;    // ra (return address)
        *(uintptr_t *)&handle[1] = (uintptr_t)p;            // sp (stack pointer)
        *(uintptr_t *)&handle[2] = (uintptr_t)p;            // s0 (frame pointer)
        *(uintptr_t *)&handle[3] = (uintptr_t)coro_func;    // s1 (entry point)
    }
#ifdef USE_VALGRIND
    size_t stack_addr = _coro_align_forward((size_t)handle + sizeof(routine_t), 16);
    ctx->vg_stack_id = VALGRIND_STACK_REGISTER(stack_addr, stack_addr + size);
#endif

    return (routine_t *)handle;
}
*/
#endif
#endif
#endif

/* Switch to specified coroutine. */
static RAII_INLINE void coro_switch(routine_t *handle) {
#if defined(_M_X64) || defined(_M_IX86)
    register routine_t *coro_previous_handle = coro()->active_handle;
#else
    routine_t *coro_previous_handle = coro()->active_handle;
#endif
    coro()->active_handle = handle;
    coro()->active_handle->status = CORO_RUNNING;
    coro()->current_handle = coro_previous_handle;
    coro()->current_handle->status = CORO_NORMAL;
#if defined(USE_UCONTEXT)
    swapcontext((ucontext_t *)coro_previous_handle, (ucontext_t *)coro()->active_handle);
#else
    coro_swap(coro()->active_handle, coro_previous_handle);
#endif
}

RAII_INLINE bool is_coroutine(void_t ptr) {
    return !is_status_invalid(ptr) && ((routine_t *)ptr)->magic_number == CORO_MAGIC_NUMBER;
}

RAII_INLINE void coro_flag_set(routine_t *co) {
    co->flagged = true;
}

RAII_INLINE routine_t *coro_active(void) {
    if (!coro()->active_handle) {
        coro()->active_handle = coro()->active_buffer;
    }

    return coro()->active_handle;
}

RAII_INLINE routine_t *coro_running(void) {
    return coro()->running;
}

RAII_INLINE routine_t *coro_sleeping(void) {
    return coro()->sleep_handle;
}

RAII_INLINE bool coro_is_valid(void) {
    return !is_coro_empty();
}

RAII_INLINE memory_t *coro_scope(void) {
    return coro_active()->scope;
}

RAII_INLINE memory_t *get_coro_scope(routine_t *co) {
    return co->scope;
}

RAII_INLINE waitgroup_t coro_waitgroup(void) {
    return coro_active()->wait_group;
}

RAII_INLINE waitgroup_t get_coro_waitgroup(routine_t *co) {
    return co->wait_group;
}

RAII_INLINE signed int get_coro_err(routine_t *co) {
    return co->event_err_code;
}

RAII_INLINE void coro_err_set(routine_t *co, signed int code) {
    co->event_err_code = code;
}

RAII_INLINE void_t get_coro_data(routine_t *co) {
    return co->user_data;
}

RAII_INLINE void coro_data_set(routine_t *co, void_t data) {
    co->user_data = data;
}

RAII_INLINE void_t get_coro_timer(routine_t *co) {
    return co->timer;
}

RAII_INLINE void coro_timer_set(routine_t *co, void_t data) {
    co->timer = data;
}

RAII_INLINE value_t *get_coro_result(routine_t *co) {
    return co->results;
}

RAII_INLINE routine_t *get_coro_context(routine_t *co) {
    return co->context;
}

RAII_INLINE void coro_context_set(routine_t *co, routine_t *t) {
    co->context = t;
}

/* Return handle to previous coroutine. */
static RAII_INLINE routine_t *coro_current(void) {
    return coro()->current_handle;
}

static RAII_INLINE void coro_scheduler(void) {
    coro_switch(coro()->main_handle);
}

/* Delete specified coroutine. */
static void coro_delete(routine_t *co) {
    if (!co) {
        RAII_LOG("attempt to delete an invalid coroutine");
    } else if (!(co->status == CORO_NORMAL
                 || co->status == CORO_DEAD
                 || co->status == CORO_ERRED
                 || co->status == CORO_EVENT_DEAD)
               && !co->exiting
               ) {
        co->event_err_code = -CORO_ERRED;
    } else {
#ifdef USE_VALGRIND
        if (co->vg_stack_id != 0) {
            VALGRIND_STACK_DEREGISTER(co->vg_stack_id);
            co->vg_stack_id = 0;
        }
#endif
        if (co->interrupt_active) {
            co->status = CORO_EVENT_DEAD;
            co->interrupt_active = false;
            co->is_waiting = false;
        } else if (co->magic_number == CORO_MAGIC_NUMBER) {
            co->magic_number = RAII_ERR;
            RAII_FREE(co);
        }
    }
}

RAII_INLINE void coro_stack_check(int n) {
    routine_t *t = coro()->running;

    if ((char *)&t <= (char *)t->stack_base || (char *)&t - (char *)t->stack_base < 256 + n || t->magic_number != CORO_MAGIC_NUMBER) {
        char error_message[256] = {0};
        snprintf(error_message, 256, "coroutine stack overflow: &t=%p stack=%p n=%d\n", &t, t->stack_base, 256 + n);
        raii_panic(error_message);
    }
}

static RAII_INLINE void coro_yielding(routine_t *co) {
    if (!coro()->interrupter_active)
        coro_stack_check(0);

    coro_switch(co);
}

RAII_INLINE void coro_suspend(void) {
    coro_yielding(coro_current());
}

/* Check for coroutine completetion and return. */
RAII_INLINE bool coro_terminated(routine_t *co) {
    return co->halt;
}

/* Create new coroutine. */
static routine_t *coro_create(size_t size, raii_func_t func, void_t args) {
    /* Stack size should be at least `CORO_STACK_SIZE`. */
    if ((size != 0 && size < CORO_STACK_SIZE) || size == 0)
        size = CORO_STACK_SIZE;

    size = _coro_align_forward(size + sizeof(routine_t), 16); /* Stack size should be aligned to 16 bytes. */
    void_t memory = try_calloc(1, size + sizeof(raii_values_t));
    routine_t *co = coro_derive(memory, size);
    if (!coro()->current_handle)
        coro()->current_handle = coro_active();

    if (!coro()->main_handle)
        coro()->main_handle = coro()->active_handle;

    if (UNLIKELY(raii_deferred_init(&co->scope->defer) < 0)) {
        RAII_FREE(co);
        return (routine_t *)RAII_ERR;
    }

    co->func = func;
    co->args = args;
    co->status = CORO_SUSPENDED;
    co->stack_size = size + sizeof(raii_values_t);
    co->is_referenced = false;
    co->halt = false;
    co->ready = false;
    co->flagged = false;
    co->run_code = CORO_RUN_NORMAL;
    co->taken = false;
    co->wait_active = false;
    co->wait_group = nullptr;
    co->event_group = nullptr;
    co->interrupt_active = false;
    co->event_active = false;
    co->event_system = false;
    co->is_event_err = false;
    co->is_waiting = false;
    co->is_group = false;
    co->is_generator = false;
    co->gen_id = RAII_ERR;
    co->is_group_finish = true;
    co->interrupt_timers = 0;
    co->event_err_code = 0;
    co->cycles = 0;
    co->results = nullptr;
    co->user_data = nullptr;
    co->timer = nullptr;
    co->yield = nullptr;
    co->scope->is_protected = false;
    co->stack_base = (unsigned char *)(co + 1);
    co->magic_number = CORO_MAGIC_NUMBER;
    if (coro_interrupt_set && is_empty(coro()->interrupt_handle))
        coro_interrupt_init();

    return co;
}

/* Create a new coroutine running func(arg) with stack size
and startup type: `CORO_RUN_NORMAL`, `CORO_RUN_MAIN`, `CORO_RUN_SYSTEM`, `CORO_RUN_EVENT`. */
static rid_t create_coro(raii_func_t fn, void_t arg, u32 stack, run_mode code) {
    rid_t id;
    bool is_assigning, is_thread, is_group, not_resultable;
    routine_t *t = coro_create(stack, fn, arg);
    routine_t *c = coro_active();

    t->run_code = code;
    if (c->interrupt_active || c->event_active)
        t->run_code = CORO_RUN_EVENT;

    not_resultable = t->run_code == CORO_RUN_THRD || t->run_code == CORO_RUN_SYSTEM
        || t->run_code == CORO_RUN_INTERRUPT;
    if (not_resultable)
        t->rid = RAII_ERR;
    else
        t->rid = (u32)raii_result_create()->id;

    t->cid = (u32)atomic_fetch_add(&gq_result.id_generate, 1) + 1;
    t->tid = coro()->thrd_id;
    is_group = c->wait_active && !is_empty(c->wait_group) && !c->is_group_finish
        && is_empty(c->event_group);
    if (coro_is_threading()) {
        is_assigning = coro_sched_is_assignable(coro_queue_active_count());
        is_thread = not_resultable || t->run_code == CORO_RUN_MAIN
            || atomic_flag_load_explicit(&gq_result.is_disabled, memory_order_relaxed);
        if ((is_group || is_assigning) && !is_thread)
            t->tid = atomic_fetch_add(&gq_result.queue->cpu_id_count, 1) % gq_result.thread_count;

        atomic_fetch_add(&gq_result.active_count, 1);
    } else if (t->run_code == CORO_RUN_NORMAL) {
        coro()->used_count++;
    }

    if (t->run_code != CORO_RUN_NORMAL) {
        t->taken = true;
        coro()->used_count++;
    }

    id = t->rid;
    if (c->event_active && !is_empty(c->event_group) && id != RAII_ERR) {
        t->event_active = true;
        t->is_waiting = true;
        hash_put(c->event_group, __itoa(id), t);
        c->event_active = false;
    } else if (is_group && id != RAII_ERR) {
        t->is_waiting = true;
        t->is_group = true;
        hash_put(c->wait_group, __itoa(id), t);
    }

    if (c->interrupt_active) {
        c->interrupt_active = false;
        t->interrupt_active = true;
        t->timer = c->timer;
        t->context = c;
        if (t->timer) {
            t->context = coro_running();
            t->context->context = t;
        }
    }

    if (!is_group || !coro_is_threading())
        coro_enqueue(t);
    else
        t->ready = true;

    return id;
}

void coro_name(char *fmt, ...) {
    va_list args;
    routine_t *t = coro()->running;
    va_start(args, fmt);
    vsnprintf(t->name, sizeof t->name, fmt, args);
    va_end(args);
}

/* The current coroutine will be scheduled again once all the
other currently-ready coroutines have a chance to run. Returns
the number of other tasks that ran while the current task was waiting. */
static int coro_yielding_active(void) {
    int n = coro()->num_others_ran;
    yield();
    return coro()->num_others_ran - n - 1;
}

/* Returns coroutine status state string. */
static string_t coro_state(int status) {
    switch (status) {
        case CORO_DEAD:
            return "Dead/Not initialized";
        case CORO_NORMAL:
            return "Active/Not running";
        case CORO_RUNNING:
            return "Active/Running";
        case CORO_SUSPENDED:
            return "Suspended/Not started";
        case CORO_EVENT:
            return "External/Event callback";
        case CORO_EVENT_DEAD:
            return "External/Event deleted/uninitialized";
        case CORO_ERRED:
            return "Erred/Exception generated";
        default:
            return "Unknown";
    }
}

RAII_INLINE void coro_info(routine_t *t, int pos) {
#ifdef USE_DEBUG
    bool line_end = false;
    if (is_empty(t)) {
        line_end = true;
        t = coro_active();
    }

    char line[SCRAPE_SIZE];
    snprintf(line, SCRAPE_SIZE, "\033[0K\n\r\033[%dA", pos);
    fprintf(stderr, "\t\t - Thrd #%zx, id: %u cid: %u (%s) %s cycles: %zu%s",
            thrd_self(),
            t->tid,
            t->cid,
            (!is_empty(t->name) && t->cid > 0 ? t->name : !t->is_referenced ? "" : "referenced"),
            coro_state(t->status),
            t->cycles,
            (line_end ? "\033[0K\n" : line)
    );
#endif
}

/* Mark the current coroutine as a ``system`` coroutine. These are ignored for the
purposes of deciding the program is done running */
static void coro_system(void) {
    if (!coro()->running->system) {
        coro()->running->system = true;
        if (coro_is_threading())
            atomic_fetch_sub(&gq_result.active_count, 1);
        --coro()->used_count;
        coro()->running->tid = coro()->thrd_id;
        coro()->sleep_handle = coro()->running;
    }
}

static void_t coro_wait_system(void_t v) {
    routine_t *t;
    size_t now;
    (void)v;

    coro_system();
    if (coro()->is_main)
        coro_name("coro_wait_system");
    else
        coro_name("coro_wait_system #%d", (int)coro()->thrd_id);

    if (coro_interrupt_set && coro_interrupt_timer_system) {
        coro_interrupt_timer_system();
    } else {
        while (raii_is_running()) {
            /* let everyone else run */
            while (coro_yielding_active() > 0)
                ;
            now = get_timer();
            coro_info(coro_active(), 1);
            while ((t = coro()->sleep_queue->head) && now >= t->alarm_time || (t && coro_interrupt_set && t->halt)) {
                coro_remove(coro()->sleep_queue, t);
                if (!t->system && --coro()->sleeping_counted == 0)
                    coro()->used_count--;

                if (coro_interrupt_set && t->timer && t->context && t->context->timer) {
                    coro_timer_set(t->context, nullptr);
                } else if (!t->halt && !t->timer) {
                    coro_enqueue(t);
                }
            }
        }
    }

    return 0;
}

static void add_timeout(routine_t *running, routine_t *context, u32 ms, size_t now) {
    size_t when = now + (size_t)ms * 1000000;
    routine_t *t = nullptr;

    for (t = coro()->sleep_queue->head; !is_empty(t) && t->alarm_time < when; t = t->next)
        ;

    if (t) {
        context->prev = t->prev;
        context->next = t;
    } else {
        context->prev = coro()->sleep_queue->tail;
        context->next = nullptr;
    }

    t = context;
    t->alarm_time = when;
    if (t->prev)
        t->prev->next = t;
    else
        coro()->sleep_queue->head = t;

    if (t->next)
        t->next->prev = t;
    else
        coro()->sleep_queue->tail = t;

    if (!running->system && coro()->sleeping_counted++ == 0)
        coro()->used_count++;
}

u32 sleepfor(u32 ms) {
    size_t now;

    if (!coro()->sleep_activated) {
        coro()->sleep_activated = true;
        create_coro(coro_wait_system, nullptr, Kb(18), CORO_RUN_SYSTEM);
        coro_stealer();
        if (coro_interrupt_set && coro_interrupt_timer)
            yield();
    }

    now = get_timer();
    if (!coro_interrupt_timer_system) {
        add_timeout(coro()->running, coro()->running, ms, now);
    }

    if (coro_interrupt_set)
        coro_active()->interrupt_timers++;

    if (coro_interrupt_set && coro_interrupt_timer) {
        coro_interrupt_timer(ms);
    } else {
        coro_switch(coro_current());
    }

    if (coro_interrupt_set)
        coro_active()->interrupt_timers--;

    return (u32)(get_timer() - now) / 1000000;
}

static void coro_sched_init(bool is_main, u32 thread_id) {
    coro()->stopped = false;
    coro()->started = false;
    coro()->is_main = is_main;
    coro()->exiting = 0;
    coro()->thrd_id = thread_id;
    coro()->sleep_activated = false;
    coro()->sleeping_counted = 0;
    coro()->used_count = 0;
    coro()->group_count = 0;
    coro()->seed = 0;
    coro()->stolen_count = 0;
    coro()->sleep_handle = nullptr;
    coro()->active_handle = nullptr;
    coro()->main_handle = nullptr;
    coro()->current_handle = nullptr;
    coro()->interrupt_code = 0;
    coro()->interrupter_active = 0;
    coro()->interrupt_handle = nullptr;
    coro()->interrupt_array = nullptr;
    coro()->interrupt_data = nullptr;
    coro()->interrupt_bitset = nullptr;
    coro()->run_queue->type = RAII_SCHED;
    coro()->sleep_queue->type = RAII_SCHED;
}

/* Check `thread` local coroutine use count for zero. */
static RAII_INLINE bool coro_sched_empty(void) {
    return coro()->used_count == 0;
}

static RAII_INLINE bool coro_sched_is_stealable(void) {
    int active_count = coro_queue_active_count();
    return active_count > 1 && coro_sched_is_assignable(active_count);
}

/* Check `local` run queue `head` for not `nullptr`. */
static RAII_INLINE bool coro_sched_active(void) {
    return coro()->run_queue->head != nullptr;
}

static RAII_INLINE bool coro_sched_is_sleeping(void) {
    return (coro()->sleeping_counted > 0 && coro_sched_active());
}

/* Check `global` coroutine active count status is `zero` or less. */
static RAII_INLINE bool coro_queue_is_empty(void) {
    return coro_queue_active_count() == 0;
}

static RAII_INLINE bool coro_queue_is_available(void) {
    return (atomic_load_explicit(&gq_result.queue->local[coro()->thrd_id]->available, memory_order_relaxed) > 0);
}

static void coro_unwind_setup(ex_context_t *ctx, const char *ex, const char *message) {
    routine_t *co = coro_active();
    co->scope->err = (void_t)ex;
    co->scope->panic = message;
    ex_data_set(ctx, (void_t)co);
    ex_unwind_set(ctx, co->scope->is_protected);
}

/* Check available coroutines in thread `deque` ~temp~ run queue.

If `coroutine` in `waitgroup`, take all. Otherwise,
take `1`, and transfer to `local` run queue.

Note: only coroutines in `deque` ~temp~ run queue, can have work stealing applied. */
static bool coro_take(raii_deque_t *queue, bool take_all) {
    size_t i, available, active;
    bool work_taken = false;
    atomic_thread_fence(memory_order_seq_cst);
    if ((available = atomic_load_explicit(&queue->available, memory_order_relaxed)) > 0) {
        work_taken = true;
        active = take_all || (available > (int)(atomic_load(&gq_result.active_count) - 1))
            ? available : 1;
        for (i = 0; i < active; i++) {
            routine_t *t = deque_steal(queue);
            if (t == RAII_ABORT_T) {
                --i;
                continue;
            } else if (t == RAII_EMPTY_T)
                break;

            atomic_fetch_sub(&queue->available, 1);
            /* Mark/Add only once, each coroutine to `local` scheduler count,
            additional count for `waitgroup`, if needed. */
            if (!t->taken) {
                t->taken = true;
                coro()->used_count++;
                if (t->is_group)
                    coro()->group_count++;
            }

            coro_add(coro()->run_queue, t);
        }

        if (take_all) {
            atomic_flag_test_and_set(&queue->taken);
            atomic_fetch_add(&gq_result.take_count, 1);
            if (atomic_load_explicit(&gq_result.take_count, memory_order_relaxed) == gq_result.thread_count) {
                gq_result.is_takeable--;
                atomic_store(&gq_result.take_count, 0);
                atomic_flag_clear(&gq_result.is_waitable);
            }
        }
    }

    return work_taken;
}

/* Check available coroutines in all `deque` thread ~temp~ run queues,
and transfer all to main/process thread.

This is done at initial startup, only if not enough useful work available for threading. */
static void coro_transfer(raii_deque_t *queue) {
    routine_t *t = nullptr;
    hash_pair_t *pair = nullptr;
    raii_deque_t *q = nullptr;
    waitgroup_t wg = coro_active()->wait_group;
    size_t available, i, cap, count = 0;
    if (coro_is_threading()) {
        if (!is_empty(wg)) {
            cap = hash_capacity(wg);
            for (i = 0; i < cap; i++) {
                if (t = ((routine_t *)hash_pair_value(pair = hash_buckets(wg, i)).object)) {
                    if (t->system) {
                        t->taken = true;
                        coro()->sleep_handle = t;
                    }

                    t->tid = 0;
                    coro_enqueue(t);
                    if (++count == hash_count(wg))
                        break;
                }
            }
        } else {
            for (i = 1; i < gq_result.thread_count; i++) {
                q = gq_result.queue->local[i];
                if ((available = atomic_load_explicit(&q->available, memory_order_relaxed)) > 0) {
                    for (i = 0; i < available; i++) {
                        t = deque_take(q);
                        if (t == RAII_EMPTY_T)
                            continue;

                        if (t->system) {
                            t->taken = true;
                            coro()->sleep_handle = t;
                        } else {
                            if (t->run_code == CORO_RUN_THRD) {
                                deque_push(queue, t);
                                continue;
                            }

                            atomic_fetch_sub(&q->available, 1);
                            t->taken = false;
                        }

                        t->tid = 0;
                        coro_enqueue(t);
                    }
                }
            }
        }

        atomic_flag_test_and_set(&gq_result.is_disabled);
    }
}

static RAII_INLINE void coro_group_result_set(routine_t *co) {
    atomic_lock(&gq_result.group_lock);
    if (!is_empty(gq_result.group_result))
        $append_unsigned(gq_result.group_result, co->rid);

    atomic_unlock(&gq_result.group_lock);
}

static void coro_thread_waitfor(waitgroup_t wg) {
    routine_t *co, *c = coro_active();
    hash_pair_t *pair = nullptr;
    string_t key = nullptr;
    u32 cap, i, group_capacity = coro()->group_count;
    coro()->group_count = 0;
    bool has_completed = false;

    while (hash_count(wg) && !has_completed) {
        cap = (u32)hash_capacity(wg);
        for (i = 0; i < cap; i++) {
            if (group_capacity == 0) {
                has_completed = true;
                break;
            }

            if (co = ((routine_t *)hash_pair_value(pair = hash_buckets(wg, i)).object)) {
                key = hash_pair_key(pair);
                if (co->tid != coro()->thrd_id) {
                    continue;
                } else if (!coro_terminated(co)) {
                    if (!co->interrupt_active && co->status == CORO_NORMAL) {
                        coro_enqueue(co);
                    } else if (co->interrupt_active && co->status == CORO_SUSPENDED) {
                        coro_flag_set(co);
                        coro_enqueue(co);
                    }

                    coro_info(c, 1);
                    coro_yielding_active();
                } else {
                    group_capacity--;
                    if (!is_empty(co->results) && co->rid != RAII_ERR)
                        coro_group_result_set(co);

                    if (co->interrupt_active) {
                        if (coro_queue_active_count() > 0)
                            atomic_fetch_sub(&gq_result.active_count, 1);
                        coro_deferred_free(co);
                        co->magic_number = RAII_ERR;
                        RAII_FREE(co);
                    } else {
                        coro_delete(co);
                    }

                    hash_delete(wg, key);
                }
            }
        }
    }
    --coro()->used_count;
}

static void_t coro_thread_main(void_t args) {
    raii_deque_t *queue = (raii_deque_t *)args;
    waitgroup_t grouped = nullptr;

    coro_name("coro_thread_main #%d", (int)coro()->thrd_id);
    coro_info(coro_active(), -1);

    coro()->started = true;
    if (atomic_flag_load(&gq_result.is_waitable) && !atomic_flag_load(&queue->taken))
        coro_take(queue, true);

    while (!coro_sched_empty() && raii_is_running()) {
        if (!is_empty(queue->grouped) && coro()->group_count > 0) {
            grouped = queue->grouped;
            queue->grouped = nullptr;
            coro_thread_waitfor(grouped);
        } else if (coro()->used_count > 1) {
            yield();
        } else {
            break;
        }
    }

    return 0;
}

static void coro_cleanup(void) {
    if (coro()->is_main) {
        atomic_flag_test_and_set(&gq_result.is_finish);
        if (!can_cleanup)
            return;

        atomic_thread_fence(memory_order_seq_cst);
        can_cleanup = false;
        if (!coro()->exiting)
            while (!coro_queue_is_empty() && atomic_flag_load(&gq_result.is_errorless))
                thrd_yield();

        if (!is_empty(coro()->sleep_handle) && coro()->sleep_handle->magic_number == CORO_MAGIC_NUMBER) {
            RAII_FREE(coro()->sleep_handle);
            coro()->sleep_handle = nullptr;
        }

        if (coro_interrupt_set)
            coro_interrupt_shutdown(nullptr);

        channel_destroy();
        coro_destroy();
        deque_destroy();
    }
}

/* Simple random number generated (like rand) using the given seed. */
static RAII_INLINE u32 rng(u32 *seed, int max) {
    u32 next = *seed;

    next *= 1103515245;
    next += 12345;

    *seed = next;

    return next % max;
}

/**
 * (Try to) steal and execute a task from a random worker.
 */
static routine_t *deque_random_steal(void) {
    routine_t *t = RAII_EMPTY_T;
    if (coro_sched_is_stealable()) {
        u32 i, victim = (coro()->thrd_id + 1 + rng(&coro()->seed, gq_result.thread_count - 1)) % gq_result.thread_count;
        for (i = victim; i < gq_result.thread_count; i++) {
            if (i == coro()->thrd_id) {
                continue;
            }

            raii_deque_t *queue = gq_result.queue->local[i];
            if (atomic_load(&queue->available) > 1) {
                t = deque_take(queue);
                if (t == RAII_EMPTY_T)
                    continue;

                atomic_fetch_sub(&queue->available, 1);
                if (t->system || t->is_group || t->run_code == CORO_RUN_THRD) {
                    coro_enqueue(t);
                    t = RAII_EMPTY_T;
                    continue;
                }

                atomic_fetch_add(&queue->steal_count, 1);
                coro()->stolen_count++;
                t->tid = coro()->thrd_id;
                break;
            }
        }
    }

    return t;
}

static int scheduler(void) {
    routine_t *t = nullptr;
    bool stole, have_work = false;

    for (;;) {
        stole = false;
        /* Don't take on thread first launch/startup, only afterwards. */
        if (((coro()->is_main && atomic_flag_load_explicit(&gq_result.is_started, memory_order_relaxed))
             || (!coro()->is_main && coro()->started)) && coro_is_threading()) {
            if (have_work = coro_take(gq_result.queue->local[coro()->thrd_id], false))
                t = nullptr;
        }

        if (coro_sched_empty() || !coro_sched_active() || t == RAII_EMPTY_T || coro()->exiting) {
            if (coro()->is_main && (raii_is_exiting() || coro()->exiting || t == RAII_EMPTY_T
                                    || (coro_queue_is_empty() && coro_sched_empty()))) {
                coro_cleanup();
                if (coro()->used_count > 0) {
                    RAII_INFO("\nNo runnable coroutines! %d stalled\n", coro()->used_count);
                    exit(1);
                } else {
                    RAII_LOG("\nCoro scheduler exited");
                    exit(0);
                }
            } else if (!coro()->is_main && !coro_sched_active() && t != RAII_EMPTY_T
                       && !atomic_flag_load(&gq_result.is_finish)
                       && !coro_sched_is_sleeping()) {
        /* TODO: bug fix stealing logic and account for any stolen coroutines back to thread taken from. */
                if ((t = deque_random_steal()) == RAII_EMPTY_T)
                    continue;

                stole = true;
            } else if (!coro()->is_main && (coro_sched_empty() || raii_is_exiting())) {
                if (coro_is_threading() && coro_queue_active_count() > 0)
                    atomic_fetch_sub(&gq_result.active_count, 1);
                RAII_INFO("Thrd #%zx waiting to exit.\033[0K\n", thrd_self());
                /* Wait for global exit signal */
                while (raii_is_running()
                       && !atomic_flag_load(&gq_result.queue->local[coro()->thrd_id]->shutdown))
                    thrd_yield();

                RAII_INFO("Thrd #%zx exiting, %d runnable coroutines.\033[0K\n", thrd_self(), coro()->used_count);
                return coro()->exiting;
            }
        }

        if (!stole) {
            t = coro_dequeue(coro()->run_queue);
            if (t == nullptr)
                continue;
        }

        t->ready = false;
        coro()->running = t;
        coro()->num_others_ran++;
        t->cycles++;

        coro_interrupter();
        if (!is_status_invalid(t) && !t->halt)
            coro_switch(t);

        coro()->running = nullptr;
        if (t->halt || t->exiting) {
            if (!t->system && !t->event_system) {
                --coro()->used_count;
                if (coro_is_threading() && coro_queue_active_count() > 0)
                    atomic_fetch_sub(&gq_result.active_count, 1);
            }

            if (coro_interrupt_set) {
                if (t->run_code == CORO_RUN_MAIN) {
                    if (coro_queue_active_count() > 0)
                        atomic_fetch_sub(&gq_result.active_count, 1);

                    if (coro_queue_is_empty()) {
                        if (coro()->used_count && interrupt_code())
                            coro()->used_count -= coro()->used_count;
                        else if (coro_sched_is_sleeping() && coro()->used_count > 0)
                            coro()->used_count--;
                    }
                }

                if (coro()->used_count < 0)
                    coro()->used_count++;
            }

            if (!t->is_waiting && !t->is_referenced) {
                if (coro_interrupt_set && t->is_event_err)
                    coro_interrupt_shutdown(t);
                coro_delete(t);
            } else if (t->is_referenced) {
                coro_gc(t);
            }
        }
    }
}

static int thrd_coro_wrapper(void_t arg) {
    worker_t *pool = (worker_t *)arg;
    raii_deque_t *queue = (raii_deque_t *)pool->arg;
    int res = 0, tid = pool->id;
    RAII_FREE(arg);
    rpmalloc_init();

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    coro_sched_init(false, tid);
    /* Wait for global start signal */
    while (!atomic_flag_load_explicit(&gq_result.is_started, memory_order_relaxed))
        ;

    create_coro(coro_thread_main, queue, gq_result.stacksize * 6, CORO_RUN_THRD);
    res = scheduler();

    if (!is_empty(coro()->sleep_handle) && coro()->sleep_handle->magic_number == CORO_MAGIC_NUMBER)
        RAII_FREE(coro()->sleep_handle);

    if (coro_interrupt_set)
        coro_interrupt_shutdown(nullptr);

    rpmalloc_thread_finalize(1);
    thrd_exit(res);
    return res;
}

static void_t main_main(void_t v) {
    coro_name("coro_main");
    coro()->exiting = coro_sys_set && coro_main_func
        ? coro_main_func(coro_argc, coro_argv) : coro_main(coro_argc, coro_argv);

    return 0;
}

static void coro_initialize(void) {
    atomic_thread_fence(memory_order_seq_cst);
    if (!coro_sys_set) {
        coro_sys_set = true;
        exception_setup_func = coro_unwind_setup;
        exception_unwind_func = (ex_unwind_func)coro_deferred_free;
        exception_ctrl_c_func = (ex_terminate_func)coro_cleanup;
        exception_terminate_func = (ex_terminate_func)coro_cleanup;
        gq_result.stacksize = CORO_STACK_SIZE;
        gq_result.is_takeable = 0;
        gq_result.cpu_count = thrd_cpu_count();
        gq_result.thread_count = gq_result.cpu_count + 1;
        if (gq_result.queue_size == 0)
            gq_result.queue_size = 1 << (gq_result.cpu_count > 7
                                         ? 13
                                         : gq_result.cpu_count * 2);
        gq_result.scope = unique_init();
        gq_result.queue = nullptr;
        gq_result.gc = nullptr;
        gq_result.group_result = nullptr;
        atomic_init(&gq_result.results, nullptr);
        atomic_init(&gq_result.result_id_generate, 0);
        atomic_init(&gq_result.id_generate, 0);
        atomic_init(&gq_result.active_count, 0);
        atomic_init(&gq_result.take_count, 0);
        atomic_flag_clear(&gq_result.group_lock);
        atomic_flag_clear(&gq_result.is_finish);
        atomic_flag_clear(&gq_result.is_started);
        atomic_flag_clear(&gq_result.is_waitable);
        atomic_flag_clear(&gq_result.is_disabled);
        atomic_flag_test_and_set(&gq_result.is_errorless);
#if defined(_WIN32)
        QueryPerformanceFrequency(&gq_result.timer);
#elif defined(__APPLE__) || defined(__MACH__)
        mach_timebase_info(&gq_result.timer);
#endif
        ex_signal_setup();
    }
}

int raii_main(int argc, char **argv) {
    coro_argc = argc;
    coro_argv = argv;

    rpmalloc_init();
    coro_initialize();
    coro_pool_init(gq_result.queue_size);
    coro_sched_init(true, 0);
    create_coro(main_main, nullptr, gq_result.stacksize * 8, CORO_RUN_MAIN);
    scheduler();
    unreachable;

    return coro()->exiting;
}

/* Transfer tasks from `global` run queue to thread's `local` run queue. */
static RAII_INLINE void coro_post_available(void) {
    routine_t *t = nullptr;
    hash_pair_t *pair = nullptr;
    hash_t *wg = coro_active()->wait_group;
    size_t count = 0, i, cap = hash_capacity(wg);
    atomic_thread_fence(memory_order_seq_cst);
    gq_result.is_takeable++;
    for (i = 0; i < cap; i++) {
        if (t = ((routine_t *)hash_pair_value(pair = hash_buckets(wg, i)).object)) {
            coro_atomic_enqueue(t);
            if (++count == hash_count(wg))
                break;
        }
    }

    for (i = 0; i < gq_result.thread_count; i++) {
        atomic_flag_clear(&gq_result.queue->local[i]->taken);
        gq_result.queue->local[i]->grouped = wg;
    }
}

/* Multithreading checker for available coroutines in `waitgroup`, if any,
transfer from `global` ~array~ run queue, to current thread `local` run queue.

If `main/process thread` caller, will globally signal all child threads
to start there execution, and assign coroutines. */
void coro_stealer(void) {
    atomic_thread_fence(memory_order_seq_cst);
    if (coro()->is_main) {
        if (coro_is_threading()) {
            if (!atomic_flag_load(&gq_result.is_started)
                && !atomic_flag_load(&gq_result.is_waitable))
                coro_transfer(gq_result.queue);

            if (!gq_result.is_takeable
                && atomic_flag_load(&gq_result.is_waitable)
                && coro_active()->is_group_finish && coro_active()->wait_group)
                coro_post_available();
        }

        if (!atomic_flag_load(&gq_result.is_started)) {
            atomic_flag_test_and_set(&gq_result.is_started);
        }
    }

    if (coro_is_threading() && gq_result.is_takeable
        && atomic_flag_load(&gq_result.is_waitable)
        && !atomic_flag_load(&gq_result.queue->local[coro()->thrd_id]->taken)) {
        coro_take(gq_result.queue->local[coro()->thrd_id], true);
    }
}

RAII_INLINE void yield(void) {
    coro_stealer();
    coro_enqueue(coro()->running);
    coro_suspend();
}

RAII_INLINE void defer(func_t func, void_t data) {
    raii_deferred(coro_scope(), func, data);
}

RAII_INLINE void defer_recover(func_t func, void_t data) {
    raii_recover_by(coro_scope(), func, data);
}

RAII_INLINE bool catching(string_t err) {
    return raii_is_caught(coro_scope(), err);
}

RAII_INLINE string_t catch_message(void) {
    return raii_message_by(coro_scope());
}

RAII_INLINE value_t result_for(rid_t id) {
    result_t value = raii_result_get(id);
    if (value->is_ready)
        return value->result->valued;

    throw(logic_error);
}

waitgroup_t waitgroup_ex(u32 capacity) {
    routine_t *c = coro_active();
    size_t i, resized = 0, cap = capacity;
    if (!is_zero(capacity) && (capacity > gq_result.thread_count * 2)) {
        cap = capacity + (capacity * 0.0025);
        hash_set_capacity(cap);
        resized = cap / gq_result.thread_count;
    }

    waitgroup_t wg = hash_create_ex(cap);
    c->wait_active = true;
    c->wait_group = wg;
    c->is_group_finish = false;

    if (coro_is_threading() && resized) {
        atomic_thread_fence(memory_order_seq_cst);
        for (i = 0; i < gq_result.thread_count; i++) {
            raii_deque_t *q = gq_result.queue->local[i];
            if (atomic_load_explicit(&q->array->size, memory_order_relaxed) < resized) {
                deque_array_t *a = (deque_array_t *)atomic_load_explicit(&q->array, memory_order_acquire);
                a = try_realloc(a, sizeof(deque_array_t) + sizeof(routine_t *) * resized);
                atomic_store_explicit(&q->array, a, memory_order_release);
                atomic_store(&a->size, resized);
            }
        }
    }

    return wg;
}

RAII_INLINE waitgroup_t waitgroup(void) {
    return waitgroup_ex(gq_result.queue_size / 2);
}

waitresult_t waitfor(waitgroup_t wg) {
    routine_t *co, *c = coro_active();
    string_t key = nullptr;
    waitresult_t wgr = nullptr;
    hash_pair_t *pair = nullptr;
    u32 group_capacity, cap, i;
    bool is_wait = false, has_completed = false;

    if (c->wait_active && is_equal_ex(c->wait_group, wg)) {
        c->is_group_finish = true;
        if (coro_sched_is_assignable(coro_queue_active_count())
            && !atomic_flag_load_explicit(&gq_result.is_disabled, memory_order_relaxed))
            atomic_flag_test_and_set(&gq_result.is_waitable);

        atomic_lock(&gq_result.group_lock);
        if (is_empty(gq_result.group_result)) {
            wgr = array_of(c->scope, 0);
            array_deferred_set(wgr, c->scope);
            gq_result.group_result = wgr;
        }
        atomic_unlock(&gq_result.group_lock);

        yield();
        if (is_wait = atomic_flag_load(&gq_result.is_waitable)) {
            group_capacity = coro()->group_count;
            coro()->group_count = 0;
            gq_result.queue->grouped = nullptr;
        }

        while (hash_count(wg) && !has_completed) {
            cap = (u32)hash_capacity(wg);
            for (i = 0; i < cap; i++) {
                if (co = ((routine_t *)hash_pair_value(pair = hash_buckets(wg, i)).object)) {
                    key = hash_pair_key(pair);
                    if (is_wait && group_capacity == 0) {
                        has_completed = true;
                        break;
                    } else if (is_wait && co->tid != coro()->thrd_id) {
                        continue;
                    } else if (!coro_terminated(co)) {
                        if (!co->interrupt_active && co->status == CORO_NORMAL) {
                            coro_enqueue(co);
                        } else if (co->interrupt_active && co->status == CORO_SUSPENDED) {
                            coro_flag_set(co);
                            coro_enqueue(co);
                        }

                        coro_info(c, 1);
                        coro_yielding_active();
                    } else {
                        if (is_wait)
                            group_capacity--;

                        if (!is_empty(co->results) && co->rid != RAII_ERR)
                            coro_group_result_set(co);

                        if (co->interrupt_active) {
                            if (coro_queue_active_count() > 0)
                                atomic_fetch_sub(&gq_result.active_count, 1);
                            coro_deferred_free(co);
                            co->magic_number = RAII_ERR;
                            RAII_FREE(co);
                        } else {
                            coro_delete(co);
                        }

                        hash_delete(wg, key);
                    }
                }
            }
        }

        while (is_wait && hash_count(wg)) {
            yield();
        }

        c->wait_active = false;
        c->wait_group = nullptr;
        atomic_lock(&gq_result.group_lock);
        wgr = gq_result.group_result;
        if (is_zero(c->interrupt_timers))
            gq_result.group_result = nullptr;

        atomic_unlock(&gq_result.group_lock);
        --coro()->used_count;
        hash_free(wg);

        return wgr;
    }

    return nullptr;
}

rid_t go(callable_t fn, u64 num_of_args, ...) {
    va_list ap;

    va_start(ap, num_of_args);
    params_t params = array_ex(coro_scope(), num_of_args, ap);
    va_end(ap);

    return create_coro((raii_func_t)fn, params, gq_result.stacksize, CORO_RUN_NORMAL);
}

void launch(func_t fn, u64 num_of_args, ...) {
    va_list ap;

    va_start(ap, num_of_args);
    params_t params = array_ex(coro_scope(), num_of_args, ap);
    va_end(ap);

    create_coro((raii_func_t)fn, params, gq_result.stacksize, CORO_RUN_NORMAL);
    yield();
}

awaitable_t async(callable_t fn, u64 num_of_args, ...) {
    va_list ap;
    awaitable_t awaitable = try_calloc(1, sizeof(struct awaitable_s));
    routine_t *c = coro_active();
    waitgroup_t wg = waitgroup_ex(2);

    va_start(ap, num_of_args);
    params_t params = array_ex(c->scope, num_of_args, ap);
    va_end(ap);

    rid_t rid = create_coro((raii_func_t)fn, params, gq_result.stacksize, CORO_RUN_ASYNC);
    c->wait_group = nullptr;
    awaitable->wg = wg;
    awaitable->cid = rid;
    awaitable->type = RAII_CORO;

    return awaitable;
}

value_t await(awaitable_t task) {
    if (!is_empty(task) && is_type(task, RAII_CORO)) {
        task->type = RAII_ERR;
        rid_t rid = task->cid;
        waitgroup_t wg = task->wg;
        RAII_FREE(task);
        coro_active()->wait_group = wg;
        waitresult_t wgr = waitfor(wg);
        if ($size(wgr) > 0)
            return result_for(rid);
    }

    return raii_values_empty->valued;
}

RAII_INLINE rid_t yield_id(void) {
    return coro_active()->gen_id;
}

generator_t generator(callable_t fn, u64 num_of, ...) {
    generator_t gen = nullptr;
    routine_t *t, *c = coro_active();
    waitgroup_t wg = waitgroup_ex(2);
    va_list ap;

    va_start(ap, num_of);
    params_t params = array_ex(c->scope, num_of, ap);
    va_end(ap);

    rid_t rid = create_coro((raii_func_t)fn, params, gq_result.stacksize, CORO_RUN_NORMAL);
    t = (routine_t *)((values_type *)hash_get(wg, __itoa(rid)))->object;
    if (!snprintf(t->name, sizeof(t->name), "Generator #%d", (int)rid))
        RAII_LOG("Invalid generator");

    gen = calloc_full(t->scope, 1, sizeof(_generator_t), RAII_FREE);
    gen->rid = rid;
    gen->is_ready = false;
    gen->context = t;
    gen->type = RAII_YIELD;

    t->yield = gen;
    t->is_generator = true;
    t->is_waiting = false;

    c->wait_group = nullptr;
    c->wait_active = false;
    c->is_group_finish = true;
    hash_free(wg);
    return gen;
}

RAII_INLINE void yielding(void_t data) {
    routine_t *co = coro_active();
    if (!co->is_generator)
        throw(logic_error);

    while (co->yield->is_ready) {
        yield();
    }

    co->yield->values->object = data;
    co->yield->is_ready = true;
    coro_yielding_active();
}

RAII_INLINE value_t yield_for(generator_t gen) {
    if (!is_type(gen, RAII_YIELD))
        return raii_values_empty->valued;

    while (!gen->is_ready && !gen->context->halt) {
        if (gen->context->status == CORO_SUSPENDED)
            coro_enqueue(gen->context);

        coro_yield_info();
    }

    if (gen->context->halt && !gen->is_ready)
        return raii_values_empty->valued;

    coro_active()->gen_id = gen->rid;
    gen->is_ready = false;
    return *gen->values;
}

void delete(void_t ptr) {
    match(ptr) {
        and (RAII_CHANNEL)
            channel_free(ptr);
        or (RAII_HASH)
            hash_free(ptr);
        or (RAII_OBJECT)
            ((object_t *)ptr)->dtor(ptr);
        otherwise {
            if (is_valid(ptr)) {
                memset(ptr, 0, sizeof(ptr));
                RAII_FREE(ptr);
            } else {
                RAII_LOG("Pointer not freed, possible double free attempt!");
            }
        }
    }
}

value_t coro_await(callable_t fn, size_t num_of_args, ...) {
    va_list ap;
    waitgroup_t wg = waitgroup_ex(2);
    coro_active()->interrupt_active = true;

    va_start(ap, num_of_args);
    params_t params = array_ex(coro_scope(), num_of_args, ap);
    va_end(ap);

    rid_t rid = create_coro((raii_func_t)fn, params, gq_result.stacksize, CORO_RUN_NORMAL);
    waitresult_t wgr = waitfor(wg);
    if ($size(wgr) > 0)
        return result_for(rid);

    return raii_values_empty->valued;
}

void coro_launch(callable_t fn, u64 num_of_args, ...) {
    va_list ap;

    va_start(ap, num_of_args);
    params_t params = array_ex(coro_scope(), num_of_args, ap);
    va_end(ap);

    create_coro((raii_func_t)fn, params, Kb(16), CORO_RUN_INTERRUPT);
    yield();
}

void coro_mark(void) {
    routine_t *co = coro_active();
    waitgroup_t eg = waitgroup_ex(2);
    co->event_group = eg;
    co->event_active = true;
}

routine_t *coro_unmark(rid_t id, string_t name) {
    routine_t *c = nullptr, *co = coro_active();
    if (!is_empty(co->event_group)) {
        waitgroup_t eg = co->event_group;
        c = (routine_t *)((values_type *)hash_get(eg, __itoa(id)))->object;
        c->is_waiting = false;
        if (!snprintf(c->name, sizeof(c->name), "%s #%d", name, (int)c->cid))
            RAII_LOG("Invalid unmarking");

        co->event_group = nullptr;
        co->is_group_finish = true;
        hash_free(eg);
    }

    return c;
}

void coro_detached(routine_t *co) {
    if (!is_empty(co) && coro_interrupt_set && !co->event_system && co->event_active) {
        co->event_system = true;
        --coro()->used_count;
        atomic_fetch_sub(&gq_result.active_count, 1);
        if (!is_empty(co->context->event_group)){
            waitgroup_t eg = co->context->event_group = nullptr;
            co->context->event_group = nullptr;
            hash_free(eg);
        }
    }
}

static void coro_await_result(routine_t *co, void_t data, ptrdiff_t plain, bool is_plain) {
    if (is_plain || (!is_empty(data) && co->rid != RAII_ERR)) {
        rid_t id = co->rid;
        result_t *results = (result_t *)atomic_load_explicit(&gq_result.results, memory_order_acquire);
        atomic_thread_fence(memory_order_seq_cst);
        if (is_plain)
            co->interrupt_result->valued.long_long = plain;
        else
            co->interrupt_result->valued.object = data;

        results[id]->result = co->interrupt_result;
        co->results = &results[id]->result->valued;
        results[id]->is_ready = true;
        atomic_store_explicit(&gq_result.results, results, memory_order_release);
    }
}

static RAII_INLINE void coro_await_switch(routine_t *co) {
    if (!coro_terminated(co))
        coro_switch(co);
}

RAII_INLINE void coro_await_upgrade(routine_t *co, void_t result, ptrdiff_t plain, bool is_plain,
                                    bool halted, bool switching) {
    co->halt = halted;
    coro_await_result(co, result, plain, is_plain);
    if (switching)
        coro_await_switch(co->context);
}

RAII_INLINE void coro_await_finish(routine_t *co, void_t result, ptrdiff_t plain, bool is_plain) {
    coro_await_upgrade(co, result, plain, is_plain, true, true);
}

RAII_INLINE void coro_await_exit(routine_t *co, void_t result, ptrdiff_t plain, bool is_plain) {
    if (!is_empty(co)) {
        co->halt = true;
        coro_await_result(co, result, plain, is_plain);
        coro_switch(co);
        if (!is_empty(co->context))
            coro_await_switch(co->context);
    }

    coro_scheduler();
}

RAII_INLINE void coro_await_canceled(routine_t *co, signed int code) {
    coro_await_erred(co, code);
    coro_err_set(co->context, code);
    coro_await_switch(co->context);
}

RAII_INLINE void coro_interrupt_setup(call_interrupter_t loopfunc, call_t perthreadfunc,
                                      func_t shutdownfunc, call_timer_t timerfunc,
                                      call_t systemfunc) {
    if (!coro_interrupt_set && loopfunc && perthreadfunc && shutdownfunc) {
        coro_interrupt_set = true;
        coro_interrupt_loop = loopfunc;
        coro_interrupt_init = perthreadfunc;
        coro_interrupt_shutdown = shutdownfunc;
        if (timerfunc)
            coro_interrupt_timer = timerfunc;

        if (systemfunc)
            coro_interrupt_timer_system = systemfunc;
    }
}

RAII_INLINE void coro_halt_set(routine_t *co) {
    co->halt = true;
}

RAII_INLINE void coro_halt_clear(routine_t *co) {
    co->halt = false;
}

RAII_INLINE void_t interrupt_handle(void) {
    return coro()->interrupt_handle;
}

RAII_INLINE void_t interrupt_data(void) {
    return coro()->interrupt_data;
}

RAII_INLINE i32 interrupt_code(void) {
    return coro()->interrupt_code;
}

RAII_INLINE bits_t interrupt_bitset(void) {
    return coro()->interrupt_bitset;
}

RAII_INLINE arrays_t interrupt_array(void) {
    return coro()->interrupt_array;
}

RAII_INLINE i32 is_interrupting(void) {
    return coro()->interrupter_active;
}

RAII_INLINE void interrupt_handle_set(void_t handle) {
    coro()->interrupt_handle = handle;
}

RAII_INLINE void interrupt_data_set(void_t data) {
    coro()->interrupt_data = data;
}

RAII_INLINE void interrupt_code_set(i32 code) {
    coro()->interrupt_code = code;
}

RAII_INLINE void interrupt_bitset_set(bits_t mask) {
    coro()->interrupt_bitset = mask;
}

RAII_INLINE void interrupt_array_set(arrays_t args) {
    coro()->interrupt_array = args;
}

static RAII_INLINE void coro_destroy(void) {
    if (!is_empty(gq_result.gc)) {
        atomic_lock(&gq_result.group_lock);
        foreach(t in gq_result.gc) {
            if (((routine_t *)t.object)->magic_number == CORO_MAGIC_NUMBER)
                RAII_FREE(t.object);
        }

        array_delete(gq_result.gc);
        gq_result.gc = nullptr;
        atomic_unlock(&gq_result.group_lock);
    }
}

RAII_INLINE void coro_gc(routine_t *co) {
    atomic_lock(&gq_result.group_lock);
    if (is_empty(gq_result.gc))
        gq_result.gc = array_of(gq_result.scope, 0);

    if (co->magic_number == CORO_MAGIC_NUMBER)
        $append(gq_result.gc, co);

    atomic_unlock(&gq_result.group_lock);
}

RAII_INLINE void coro_ref(routine_t *t) {
    t->is_referenced = true;
}

RAII_INLINE void coro_unref(routine_t *t) {
    t->is_referenced = false;
}

RAII_INLINE routine_t *coro_ref_current(void) {
    routine_t *t = coro_running();
    coro_ref(t);

    return t;
}

RAII_INLINE void_t coro_await_erred(routine_t *co, int code) {
    co->is_event_err = true;
    co->event_err_code = code;
    co->status = CORO_ERRED;
    return nullptr;
}

RAII_INLINE signed int coro_err_code(void) {
    return coro_active()->event_err_code;
}

RAII_INLINE string_t coro_get_name(void) {
    return coro_active()->name;
}

RAII_INLINE rid_t coro_id(void) {
    return coro_active()->rid;
}

RAII_INLINE u32 coro_active_id(void) {
    return coro_active()->cid;
}

RAII_INLINE void coro_info_active(void) {
    coro_info(nullptr, 0);
}

RAII_INLINE void coro_yield_info(void) {
    coro_info(coro_active(), 1);
    yield();
}

RAII_INLINE void coro_stacksize_set(u32 size) {
    atomic_thread_fence(memory_order_seq_cst);
    gq_result.stacksize = size;
}

raii_deque_t *coro_pool_init(size_t queue_size) {
    raii_deque_t **local, *queue = nullptr;
    if (!thrd_queue_set) {
        thrd_queue_set = true;
        coro_initialize();

        size_t i;
        unique_t *scope = gq_result.scope, *global = coro_sys_set ? coro_scope() : raii_init();
        if (queue_size > 0 && coro_threading_enabled) {
            local = (raii_deque_t **)calloc_full(scope, gq_result.thread_count, sizeof(local[0]), RAII_FREE);
            local[0] = (raii_deque_t *)malloc_full(scope, sizeof(raii_deque_t), (func_t)deque_free);
            deque_init(local[0], queue_size);
            for (i = 1; i < gq_result.thread_count; i++) {
                local[i] = (raii_deque_t *)malloc_full(scope, sizeof(raii_deque_t), (func_t)deque_free);
                deque_init(local[i], queue_size);
                local[i]->scope = nullptr;
                worker_t *f_work = try_malloc(sizeof(worker_t));
                f_work->func = nullptr;
                f_work->value = nullptr;
                f_work->id = (int)i;
                f_work->arg = (void_t)local[i];
                f_work->type = RAII_FUTURE_ARG;

                if (thrd_create(&local[i]->thread, thrd_coro_wrapper, f_work) != thrd_success)
                    throw(future_error);
            }
            queue = local[0];
            queue->local = local;
        } else {
            queue = (raii_deque_t *)malloc_full(scope, sizeof(raii_deque_t), (func_t)deque_free);
            deque_init(queue, gq_result.queue_size);
        }

        queue->scope = scope;
        global->queued = queue;
        gq_result.queue = queue;
        if (queue_size > 0)
            gq_result.queue_size = queue_size;

        atexit(deque_destroy);
    } else if (raii_local()->threading) {
        throw(logic_error);
    }

    return queue;
}

int coro_start(coro_sys_func main, u32 argc, void_t argv, size_t queue_size) {
    if (is_empty(coro_main_func)) {
        coro_main_func = main;
        gq_result.queue_size = queue_size;
        if (queue_size == RAII_ERR) {
            coro_threading_enabled = false;
            gq_result.queue_size = 0;
        }

        return raii_main((int)argc, (char **)argv);
    }

    return RAII_ERR;
}
