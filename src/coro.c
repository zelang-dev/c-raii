#include "raii.h"

static volatile bool thrd_queue_set = false;
static raii_deque_t *thrd_queue_global = {0};
static int coro_argc;
static char **coro_argv;

struct routine_s {
    unsigned long int uc_flags;
    struct ucontext *uc_link;
    stack_t uc_stack;
    mcontext_t uc_mcontext;
    __sigset_t uc_sigmask;
    void *stack_base;
#if defined(_WIN32) && (defined(_M_X64) || defined(_M_IX86))
    void *stack_limit;
#endif
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
    bool process_active;
    bool is_event_err;
    bool is_address;
    bool is_waiting;
    bool is_group_finish;
    bool is_group;
    bool is_channeling;
    bool is_plain;
    bool flagged;
    signed int event_err_code;
    size_t alarm_time;
    size_t cycles;
    /* unique coroutine id */
    unsigned int cid;
    /* thread id assigned in `gq_result` */
    unsigned int tid;
    coro_states status;
    run_mode run_code;
#if defined(USE_VALGRIND)
    unsigned int vg_stack_id;
#endif
    void_t args;
    /* Coroutine result of function return/exit. */
    void_t results;
    /* Coroutine plain result of function return/exit. */
    size_t plain_results;
    raii_func_t func;
    memory_t scope[1];
    routine_t *next;
    routine_t *prev;
    arrays_t wait_group;
    arrays_t event_group;
    routine_t *context;
    char name[64];
    char state[64];
    char scrape[SCRAPE_SIZE];
    /* Used to check stack overflow. */
    size_t magic_number;
};

/* scheduler queue struct */
typedef struct scheduler_s {
    raii_type type;
    routine_t *head;
    routine_t *tail;
} scheduler_t;

typedef struct {
    int is_main;

    /* has the coroutine sleep/wait system started. */
    int sleep_activated;

    /* number of coroutines waiting in sleep mode. */
    int sleeping_counted;

    /* track the number of coroutines used */
    int used_count;

    /* indicator for thread termination. */
    u32 exiting;

    /* thread id assigned by `gq_result` */
    u32 thrd_id;

    /* number of other coroutine that ran while the current coroutine was waiting.*/
    u32 num_others_ran;

    u32 sleep_id;

    /* record which coroutine is executing for scheduler */
    routine_t *running;

    /* record which coroutine sleeping in scheduler */
    scheduler_t sleep_queue[1];

    /* coroutines's FIFO scheduler queue */
    scheduler_t run_queue[1];

    /* Variable holding the current running coroutine per thread. */
    routine_t *active_handle;

    /* Variable holding the main target that gets called once an coroutine
    function fully completes and return. */
    routine_t *main_handle;

    /* Variable holding the previous running coroutine per thread. */
    routine_t *current_handle;

    /* Store/hold the registers of the default coroutine thread state,
    allows the ability to switch from any function, non coroutine context. */
    routine_t active_buffer[1];
} coro_thread_t;
thrd_static(coro_thread_t, coro, NULL)
static char error_message[ERROR_SCRAPE_SIZE] = {0};

/* These are non-NULL pointers that will result in page faults under normal
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
    int cpus;
    size_t queue_size;
    thrd_t thread;
    memory_t *scope;
    cacheline_pad_t pad;
    atomic_flag started;
    atomic_flag shutdown;
    atomic_size_t cpu_core;
    atomic_size_t available;
    /* Assume that they never overflow */
    atomic_size_t top, bottom;
    atomic_coro_array_t array;
    cacheline_pad_t _pad;
    raii_deque_t **local;
};
make_atomic(raii_deque_t *, thread_deque_t)

static void deque_init(raii_deque_t *q, u32 size_hint) {
    atomic_init(&q->top, 0);
    atomic_init(&q->bottom, 0);
    deque_array_t *a = try_calloc(1, sizeof(deque_array_t) + sizeof(routine_t *) * size_hint);
    atomic_init(&a->size, size_hint);
    atomic_init(&q->array, a);
    atomic_init(&q->available, 0);
    atomic_init(&q->cpu_core, 0);
    atomic_flag_clear(&q->shutdown);
    atomic_flag_clear(&q->started);
    q->cpus = -1;
    q->queue_size = size_hint;
    q->local = nullptr;
    q->type = RAII_POOL;
}

static void deque_reset(raii_deque_t *q, u32 shrink) {
    atomic_init(&q->top, 0);
    atomic_init(&q->bottom, 0);
    deque_array_t *a = (deque_array_t *)atomic_load_explicit(&q->array, memory_order_consume);
    a = try_realloc(a, sizeof(deque_array_t) + sizeof(routine_t *) * shrink);
    atomic_init(&a->size, shrink);
    atomic_init(&q->array, a);
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
            if (!atomic_cas(&q->top, &t, t + 1))
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
        if (!atomic_cas(&q->top, &t, t + 1))
            /* Failed race */
            return RAII_ABORT_T;
    }

    return x;
}

static void deque_free(raii_deque_t *q) {
    deque_array_t *a = nullptr;
    result_t *r = nullptr;
    if (!is_empty(q)) {
        a = atomic_get(deque_array_t *, &q->array);
        r = atomic_get(result_t *, &gq_result.results);
        if (!is_empty(a)) {
            atomic_store(&q->array, NULL);
            RAII_FREE((void_t)a);
        }

        if (!is_empty(r)) {
            atomic_store(&gq_result.results, nullptr);
            RAII_FREE((void_t)r);
        }

        memset(q, 0, sizeof(q));
        RAII_FREE(q);
    }
}

static void deque_destroy(void) {
    if (is_type(thrd_queue_global, RAII_POOL)) {
        int i;
        raii_deque_t *queue = thrd_queue_global;
        memory_t *scope = queue->scope;
        queue->type = -1;
        if (!is_empty(queue->local)) {
            for (i = 0; i < queue->cpus; i++) {
                atomic_flag_test_and_set(&queue->local[i]->shutdown);
                thrd_join(queue->local[i]->thread, NULL);
            }
        }

        atomic_flag_test_and_set(&queue->shutdown);
        raii_delete(scope);
    }
}

static void deque_clear(raii_deque_t *q) {
    deque_array_t *a = NULL;
    if (!is_empty(q)) {
        a = atomic_get(deque_array_t *, &q->array);
        if (!is_empty(a)) {
            atomic_store(&q->array, NULL);
            RAII_FREE((void_t)a);
        }

        memset(q, 0, sizeof(q));
    }
}

static routine_t *deque_peek(raii_deque_t *q, u32 index) {
    deque_array_t *a = (deque_array_t *)atomic_load(&q->array);
    if (!is_empty(a) && (index <= a->size))
        return (routine_t *)atomic_load_explicit(&a->buffer[index % a->size], memory_order_relaxed);

    return NULL;
}

static RAII_INLINE void coro_scheduler(void);
static RAII_INLINE bool is_status_invalid(routine_t *t) {
    return (t->status > CORO_EVENT || t->status < 0);
}

void coro_result_set(routine_t *co, void_t data) {
    if (!is_empty(data)) {
        co->results = data;
    }
}

/* called only if routine_t func returns */
static void coro_done(void) {
    routine_t *co = coro_active();
    if (!co->interrupt_active) {
        co->halt = true;
        co->status = CORO_DEAD;
    }

    coro_scheduler();
}

static void coro_awaitable(void) {
    routine_t *co = coro_active();

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
    } end_try;
}

static void coro_func(void) {
    coro_awaitable();
    coro_done(); /* called only if coroutine function returns */
}

/* Add coroutine to scheduler queue, appending. */
static void coro_add(scheduler_t *l, routine_t *t) {
    if (l->tail) {
        l->tail->next = t;
        t->prev = l->tail;
    } else {
        l->head = t;
        t->prev = NULL;
    }

    l->tail = t;
    t->next = NULL;
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

RAII_INLINE void coro_enqueue(routine_t *t) {
    t->ready = true;
    if (is_false(t->taken)) {
        atomic_thread_fence(memory_order_acquire);
        deque_push(thrd_queue_global, t);
        atomic_thread_fence(memory_order_release);
    } else {
        coro_add(coro()->run_queue, t);
    }
}

RAII_INLINE routine_t *coro_dequeue(scheduler_t *l) {
    routine_t *t = NULL;
    if (l->head != NULL) {
        t = l->head;
        coro_remove(l, t);
    }

    return t;
}

#ifdef _WIN32
int gettimeofday(struct timeval *tp, struct timezone *tzp) {
    /*
     * Note: some broken versions only have 8 trailing zero's, the correct
     * epoch has 9 trailing zero's
     */
    static const uint64_t EPOCH = ((uint64_t)116444736000000000ULL);

    SYSTEMTIME  system_time;
    FILETIME    file_time;
    uint64_t    time;

    GetSystemTime(&system_time);
    SystemTimeToFileTime(&system_time, &file_time);
    time = ((uint64_t)file_time.dwLowDateTime);
    time += ((uint64_t)file_time.dwHighDateTime) << 32;

    tp->tv_sec = (long)((time - EPOCH) / 10000000L);
    tp->tv_usec = (long)(system_time.wMilliseconds * 1000);
    return 0;
}

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
        return -1;
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

    return 0;
}

int swapcontext(routine_t *oucp, const routine_t *ucp) {
    int ret;

    if (is_empty(oucp) || is_empty(ucp)) {
        /*errno = EINVAL;*/
        return -1;
    }

    ret = getcontext((ucontext_t *)oucp);
    if (ret == 0) {
        ret = setcontext((ucontext_t *)ucp);
    }
    return ret;
}
#endif

static size_t nsec(void) {
    struct timeval tv;

    if (gettimeofday(&tv, 0) < 0)
        return -1;

    return (size_t)tv.tv_sec * 1000 * 1000 * 1000 + tv.tv_usec * 1000;
}

routine_t *coro_derive(void_t memory, size_t size) {
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
    swapcontext((ucontext_t *)coro_previous_handle, (ucontext_t *)coro()->active_handle);
}

RAII_INLINE routine_t *coro_active(void) {
    if (!coro()->active_handle) {
        coro()->active_handle = coro()->active_buffer;
    }

    return coro()->active_handle;
}

RAII_INLINE memory_t *coro_scope(void) {
    return coro_active()->scope;
}

static RAII_INLINE routine_t *coro_current(void) {
    return coro()->current_handle;
}

static RAII_INLINE routine_t *coro_coroutine(void) {
    return coro()->running;
}

static RAII_INLINE void coro_scheduler(void) {
    coro_switch(coro()->main_handle);
}

void coro_delete(routine_t *co) {
    if (!co) {
        RAII_LOG("attempt to delete an invalid coroutine");
    } else if (!(co->status == CORO_NORMAL
                 || co->status == CORO_DEAD
                 || co->status == CORO_ERRED
                 || co->status == CORO_EVENT_DEAD)
               && !co->exiting
               ) {
        co->flagged = true;
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
            co->magic_number = -1;
            RAII_FREE(co);
        }
    }
}

void coro_stack_check(int n) {
    routine_t *t = coro()->running;
    if ((char *)&t <= (char *)t->stack_base || (char *)&t - (char *)t->stack_base < 256 + n || t->magic_number != CORO_MAGIC_NUMBER) {
        snprintf(error_message, ERROR_SCRAPE_SIZE, "coroutine stack overflow: &t=%p stack=%p n=%d\n", &t, t->stack_base, 256 + n);
        raii_panic(error_message);
    }
}

RAII_INLINE void coro_yielding(routine_t *co) {
    coro_stack_check(0);
    coro_switch(co);
}

RAII_INLINE void coro_suspend(void) {
    coro_yielding(coro_current());
}


RAII_INLINE bool coro_terminated(routine_t *co) {
    return co->halt;
}

RAII_INLINE void coro_yield(void) {
    coro_enqueue(coro()->running);
    coro_suspend();
}

/* Utility for aligning addresses. */
static RAII_INLINE size_t _coro_align_forward(size_t addr, size_t align) {
    return (addr + (align - 1)) & ~(align - 1);
}

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
        return (routine_t *)-1;
    }

    co->func = func;
    co->status = CORO_SUSPENDED;
    co->stack_size = size + sizeof(raii_values_t);
    co->is_channeling = false;
    co->halt = false;
    co->ready = false;
    co->flagged = false;
    co->taken = false;
    co->wait_active = false;
    co->wait_group = NULL;
    co->event_group = NULL;
    co->interrupt_active = false;
    co->event_active = false;
    co->process_active = false;
    co->is_event_err = false;
    co->is_plain = false;
    co->is_address = false;
    co->is_waiting = false;
    co->is_group = false;
    co->is_group_finish = true;
    co->event_err_code = 0;
    co->args = args;
    co->cycles = 0;
    co->results = NULL;
    co->plain_results = -1;
    co->scope->is_protected = false;
    co->stack_base = (unsigned char *)(co + 1);
    co->magic_number = CORO_MAGIC_NUMBER;

    return co;
}

static u32 create_routine(raii_func_t fn, void_t arg, u32 stack, run_mode code) {
    u32 id;
    routine_t *t = coro_create(stack, fn, arg);
    routine_t *c = coro_active();

    if (c->interrupt_active || c->event_active)
        t->run_code = CORO_RUN_EVENT;
    else
        t->run_code = code;

    t->cid = (u32)atomic_fetch_add(&gq_result.id_generate, 1) + 1;
    if (t->run_code == CORO_RUN_NORMAL) {
        atomic_fetch_add(&gq_result.active_count, 1);
    } else {
        t->taken = true;
        coro()->used_count++;
    }

    id = t->cid;
    if (c->event_active && !is_empty(c->event_group)) {
        t->event_active = true;
        t->is_waiting = true;
        $push_back(c->event_group, t);
        c->event_active = false;
    } else if (c->wait_active && !is_empty(c->wait_group) && !c->is_group_finish) {
        t->is_waiting = true;
        t->is_address = true;
        $push_back(c->wait_group, t);
    }

    if (c->interrupt_active) {
        c->interrupt_active = false;
        t->interrupt_active = true;
        t->context = c;
    }

    coro_enqueue(t);
    return id;
}

void coro_system(void) {
    if (!coro()->running->system) {
        coro()->running->system = true;
        coro()->running->tid = coro()->thrd_id;
        --coro()->used_count;
    }
}

void coro_name(char *fmt, ...) {
    va_list args;
    routine_t *t = coro()->running;
    va_start(args, fmt);
    vsnprintf(t->name, sizeof t->name, fmt, args);
    va_end(args);
}

RAII_INLINE bool coro_is_main(void) {
    return coro()->is_main;
}

void coro_set_state(char *fmt, ...) {
    va_list args;
    routine_t *t = coro()->running;
    va_start(args, fmt);
    vsnprintf(t->state, sizeof t->name, fmt, args);
    va_end(args);
}

int coro_yielding_active(void) {
    int n = coro()->num_others_ran;
    coro_set_state("yielded");
    coro_yield();
    coro_set_state("running");
    return coro()->num_others_ran - n - 1;
}

string_t coro_state(int status) {
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
    fprintf(stderr, "\t\t - Thrd #%zx, cid: %u (%s) %s cycles: %zu%s",
            thrd_self(),
            t->cid,
            (!is_empty(t->name) && t->cid > 0 ? t->name : !t->is_channeling ? "" : "channel"),
            coro_state(t->status),
            t->cycles,
            (line_end ? "\033[0K\n" : line)
    );
#endif
}
static void_t coro_wait_system(void_t v) {
    routine_t *t;
    size_t now;

    coro_system();
    if (coro_is_main())
        coro_name("coro_wait_system");
    else
        coro_name("coro_wait_system #%d", (int)coro()->thrd_id);

    for (;;) {
        /* let everyone else run */
        while (coro_yielding_active() > 0)
            ;
        now = nsec();
        coro_info(coro_active(), 1);
        while ((t = coro()->sleep_queue->head) && now >= t->alarm_time) {
            coro_remove(coro()->sleep_queue, t);
            if (!t->system && --coro()->sleeping_counted == 0)
                coro()->used_count--;

            coro_enqueue(t);
        }
    }
}

u32 coro_sleep_for(u32 ms) {
    size_t when, now;
    routine_t *t;

    if (!coro()->sleep_activated) {
        coro()->sleep_activated = true;
        coro()->sleep_id = create_routine(coro_wait_system, NULL, CORO_STACK_SIZE, CORO_RUN_SYSTEM);
    }

    now = nsec();
    when = now + (size_t)ms * 1000000;
    for (t = coro()->sleep_queue->head; !is_empty(t) && t->alarm_time < when; t = t->next)
        ;

    if (t) {
        coro()->running->prev = t->prev;
        coro()->running->next = t;
    } else {
        coro()->running->prev = coro()->sleep_queue->tail;
        coro()->running->next = NULL;
    }

    t = coro()->running;
    t->alarm_time = when;
    if (t->prev)
        t->prev->next = t;
    else
        coro()->sleep_queue->head = t;

    if (t->next)
        t->next->prev = t;
    else
        coro()->sleep_queue->tail = t;

    if (!t->system && coro()->sleeping_counted++ == 0)
        coro()->used_count++;

    coro_switch(coro_current());

    return (u32)(nsec() - now) / 1000000;
}

static void coro_thrd_init(bool is_main, u32 thread_id) {
    coro()->is_main = is_main;
    coro()->exiting = 0;
    coro()->thrd_id = thread_id;
    coro()->sleep_activated = false;
    coro()->sleeping_counted = 0;
    coro()->used_count = 0;
    coro()->sleep_id = 0;
    coro()->active_handle = NULL;
    coro()->main_handle = NULL;
    coro()->current_handle = NULL;
}

/* Check `thread` local coroutine use count for zero. */
RAII_INLINE bool coro_sched_empty(void) {
    return coro()->used_count == 0;
}

/* Check `local` run queue `head` for not `NULL`. */
RAII_INLINE bool coro_sched_active(void) {
    return coro()->run_queue->head != NULL;
}

RAII_INLINE void coro_sched_dec(void) {
    --coro()->used_count;
}

/* Check `global` run queue count for zero. */
RAII_INLINE bool coro_sched_is_empty(void) {
    return atomic_load_explicit(&gq_result.active_count, memory_order_relaxed) == 0;
}

static void_t coro_thread_main(void_t v) {
    (void)v;
    coro_name("coro_thread_main #%d", (int)coro()->thrd_id);
    coro_info(coro_active(), -1);
    while (!coro_sched_empty() && atomic_flag_load(&gq_result.is_errorless) && !atomic_flag_load(&gq_result.is_finish)) {
        if (coro()->used_count > 1) {
            coro_yield();
        } else {
            break;
        }
    }

    return 0;
}

static int scheduler(void) {
    routine_t *t = NULL;

    for (;;) {
        if (coro_sched_empty() || !coro_sched_active() || t == RAII_EMPTY_T) {
            if (coro_is_main() && (t == RAII_EMPTY_T || atomic_flag_load(&gq_result.is_finish)
                                   || !atomic_flag_load(&gq_result.is_errorless)
                                   || coro_sched_is_empty())) {
                //coro_cleanup();
                if (coro()->used_count > 0) {
                    RAII_INFO("\nNo runnable coroutines! %d stalled\n", coro()->used_count);

                    exit(1);
                } else {
                    RAII_LOG("\nCoro scheduler exited");

                    exit(0);
                }
            } else {
                RAII_INFO("Thrd #%zx waiting to exit.\033[0K\n", thrd_self());
                /* Wait for global exit signal */
                while (!atomic_flag_load(&gq_result.is_finish))
                    thrd_yield();

                RAII_INFO("Thrd #%zx exiting, %d runnable coroutines.\033[0K\n", thrd_self(), coro()->used_count);
                return coro()->exiting;
            }
        }

        t = coro_dequeue(coro()->run_queue);
        if (t == NULL || t == RAII_EMPTY_T)
            continue;

        t->ready = false;
        coro()->running = t;
        coro()->num_others_ran++;
        t->cycles++;
        if (!is_status_invalid(t) && !t->halt)
            coro_switch(t);

        coro()->running = NULL;
        if (t->halt || t->exiting) {
            coro_info(t, -1);
            if (!t->system)
                --coro()->used_count;

            if (!t->is_waiting)
                coro_delete(t);
        }
    }
}

static int coro_wrapper(void_t arg) {
    worker_t *pool = (worker_t *)arg;
    raii_deque_t *queue = (raii_deque_t *)pool->arg;
    int res = 0, tid = pool->id;
    RAII_FREE(arg);

    raii_init()->threading++;
    rpmalloc_init();

    coro_thrd_init(false, tid);
    create_routine(coro_thread_main, NULL, gq_result.stacksize * 3, CORO_RUN_THRD);
    res = scheduler();

    raii_destroy();
    rpmalloc_thread_finalize(1);
    thrd_exit(res);
    return res;
}

static int thrd_main(void_t args) {
    int res = 0, id = *(int *)args;
    RAII_FREE(args);

    create_routine(coro_thread_main, NULL, gq_result.stacksize * 3, CORO_RUN_THRD);
    scheduler();
    return 0;
}


static void coro_unwind_setup(ex_context_t *ctx, const char *ex, const char *message) {
    routine_t *co = coro_active();
    co->scope->err = (void_t)ex;
    co->scope->panic = message;
    ex_data_set(ctx, (void_t)co);
    ex_unwind_set(ctx, co->scope->is_protected);
}

RAII_INLINE void coro_deferred_free(routine_t *coro) {
    raii_deferred_free(coro->scope);
    raii_deferred_init(&coro->scope->defer);
}

static void_t main_main(void_t v) {
    coro_name("coro_main");
    coro()->exiting = coro_main(coro_argc, coro_argv);
    coro_sched_dec();
    return 0;
}

int raii_main(int argc, char **argv) {
    coro_argc = argc;
    coro_argv = argv;

    rpmalloc_init();
    atomic_thread_fence(memory_order_seq_cst);
    exception_setup_func = coro_unwind_setup;
    exception_unwind_func = (ex_unwind_func)coro_deferred_free;

    gq_result.is_takeable = 0;
    gq_result.stacksize = CORO_STACK_SIZE;
    gq_result.cpu_count = thrd_cpu_count();
    gq_result.thread_count = gq_result.cpu_count + 1;
    gq_result.thread_invalid = gq_result.thread_count + 61;

    atomic_init(&gq_result.id_generate, 0);
    atomic_init(&gq_result.active_count, 0);
    atomic_init(&gq_result.take_count, 0);
    atomic_flag_clear(&gq_result.is_finish);
    atomic_flag_test_and_set(&gq_result.is_errorless);
    atomic_flag_test_and_set(&gq_result.is_interruptable);
    ex_signal_setup();

    coro_thrd_init(true, gq_result.cpu_count);
    create_routine(main_main, NULL, gq_result.stacksize * 4, CORO_RUN_MAIN);
    scheduler();
    unreachable;

    return coro()->exiting;
}

wait_group_t wait_group(void) {
    routine_t *c = coro_active();
    wait_group_t wg = array_of(c->scope, 0);
    c->wait_active = true;
    c->wait_group = wg;
    c->is_group_finish = false;

    return wg;
}

wait_result_t wait_for(wait_group_t wg) {
    routine_t *co, *c = coro_active();
    wait_result_t wgr = NULL;
    u32 id = 0;
    bool has_erred = false, has_completed = false;

    if (c->wait_active && (memcmp(c->wait_group, wg, sizeof(wg)) == 0)) {
        c->is_group_finish = true;
        coro_yield();
        wgr = array_of(c->scope, 0);
        array_deferred_set(wgr, c->scope);
        while ($size(wg) != 0) {
            foreach(task in wg) {
                co = (routine_t *)task.object;
                if (!is_empty(co)) {
                    id = i_task;
                    if (!coro_terminated(co)) {
                        if (!co->interrupt_active && co->status == CORO_NORMAL)
                            coro_enqueue(co);

                        coro_info(c, 1);
                        coro_yielding_active();
                    } else {
                        if (!is_empty(co->results))
                            $append(wgr, co->results);

                        if (co->is_event_err) {
                            has_erred = true;
                            $remove(wg, id);
                            continue;
                        }

                        if (co->interrupt_active)
                            coro_deferred_free(co);

                        $remove(wg, id);
                    }
                }
            }
        }

        c->wait_active = false;
        c->wait_group = NULL;
        coro_sched_dec();
        array_delete(wg);
        return has_erred ? NULL : wgr;
    }

    return NULL;
}

RAII_INLINE value_t wait_result(wait_result_t wgr, u32 cid) {
    if (is_empty(wgr))
        return;

    return wgr[cid];
}

void thrd_init(size_t queue_size) {
    if (!thrd_queue_set) {
        thrd_queue_set = true;
        size_t i, cpu = thrd_cpu_count() * 2;
        unique_t *scope = unique_init(), *global = raii_init();
        raii_deque_t **local, *queue = (raii_deque_t *)malloc_full(scope, sizeof(raii_deque_t), (func_t)deque_free);
        deque_init(queue, cpu);
        if (queue_size > 0) {
            local = (raii_deque_t **)calloc_full(scope, cpu, sizeof(local[0]), RAII_FREE);
            for (i = 0; i < cpu; i++) {
                local[i] = (raii_deque_t *)malloc_full(scope, sizeof(raii_deque_t), (func_t)deque_free);
                deque_init(local[i], queue_size);
                local[i]->cpus = cpu;
                local[i]->scope = nullptr;
                worker_t *f_work = try_malloc(sizeof(worker_t));
                f_work->func = nullptr;
                f_work->value = nullptr;
                f_work->id = (int)i;
                f_work->arg = (void_t)local[i];
                f_work->type = RAII_FUTURE_ARG;

                if (thrd_create(&local[i]->thread, coro_wrapper, f_work) != thrd_success)
                    throw(future_error);
            }
            queue->local = local;
        }

        queue->scope = scope;
        queue->cpus = cpu;
        queue->queue_size = !queue_size ? cpu * cpu : queue_size;
        global->queued = queue;
        thrd_queue_global = queue;
        atomic_init(&gq_result.result_count, 0);
        atomic_init(&gq_result.results, nullptr);
        gq_result.queue_size = queue->queue_size;
        gq_result.scope = queue->scope;
        atexit(deque_destroy);
    } else if (raii_local()->threading) {
        throw(logic_error);
    }
}
/*
void thrd_add(raii_deque_t *queue,
              worker_t *f_work, thrd_func_t fn, void_t args) {
    int tid = atomic_fetch_add(&queue->cpu_core, 1) % queue->cpus;
    deque_push(queue->local[tid], f_work);
    if (!atomic_flag_load(&queue->local[tid]->started))
        atomic_flag_test_and_set(&queue->local[tid]->started);

    atomic_fetch_add(&queue->local[tid]->available, 1);
}*/