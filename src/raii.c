#include "raii.h"

thrd_local(memory_t, raii, NULL)
const raii_values_t raii_values_empty[1] = {0};
future_results_t gq_result = {0};

#if defined(WIN32)
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
#endif

RAII_INLINE uint64_t get_timer(void) {
    uint64_t lapse = 0;
    struct timeval tv;
    struct timespec ts;

#ifdef HAVE_TIMESPEC_GET
    if (timespec_get(&ts, TIME_UTC))
        lapse = ts.tv_sec * 1000000000 + ts.tv_nsec * 1000;
#elif defined(__APPLE__) || defined(__MACH__)
    uint64_t t = mach_absolute_time();

    if (&gq_result.timer)
        lapse = t * ((double)gq_result.timer.numer / (double)gq_result.timer.denom);
#elif defined(_CTHREAD_POSIX_)

    /* Has 2038 issue if time_t: tv.tv_sec is 32-bit. */
    if (!clock_gettime(CLOCK_MONOTONIC, &ts))
        lapse = ts.tv_sec * 1000000000 + ts.tv_nsec * 1000;
#elif defined(_WIN32)
    LARGE_INTEGER count;

    if (QueryPerformanceCounter(&count) && &gq_result.timer)
        lapse = count.QuadPart / gq_result.timer.QuadPart;
#endif

    /* macOS , mingw.org, used on mingw-w64.
       Has 2038 issue if time_t: tv.tv_sec is 32-bit.
     */
    if (!lapse && !gettimeofday(&tv, NULL))
        lapse = tv.tv_sec * 1000000000 + tv.tv_usec * 1000;

    return lapse;
}

values_type *value_create(const_t data, raii_type op) {
    values_type *value = try_calloc(1, sizeof(values_type));
    size_t slen;
    string text;

    switch (op) {
        case RAII_UINT:
        case RAII_ULONG:
        case RAII_MAXSIZE:
            value->max_size = *(size_t *)data;
            break;
        case RAII_FLOAT:
        case RAII_DOUBLE:
            value->precision = *(double *)data;
            break;
        case RAII_BOOL:
            value->boolean = *(bool *)data;
            break;
        case RAII_UCHAR:
        case RAII_CHAR:
            value->schar = *(char *)data;
            break;
        case RAII_USHORT:
        case RAII_SHORT:
            value->s_short = *(short *)data;
            break;
        case RAII_INT:
        case RAII_LONG:
        case RAII_SLONG:
        case RAII_INTEGER:
        case RAII_LLONG:
            value->long_long = *(int64_t *)data;
            break;
        case RAII_FUNC:
            value->func = (raii_func_t)data;
            break;
        case RAII_CHAR_P:
        case RAII_CONST_CHAR:
        case RAII_STRING:
            slen = simd_strlen((string)data);
            if (slen > sizeof(values_type) - 1) {
                text = try_calloc(1, slen + sizeof(char) + 1);
                str_copy(text, (string)data, slen);
                value->char_ptr = text;
            } else {
                value->char_ptr = (string)data;
            }
            break;
        case RAII_UCHAR_P:
        case RAII_OBJ:
        case RAII_PTR:
        default:
            value->object = (void_t)data;
            break;
    }

    return value;
}

RAII_INLINE result_t raii_result_get(rid_t id) {
    return (result_t)atomic_load_explicit(&gq_result.results[id], memory_order_relaxed);
}

RAII_INLINE bool result_is_ready(rid_t id) {
    return raii_result_get(id)->is_ready;
}

result_t raii_result_create(void) {
    result_t result, *results;
    size_t id = atomic_fetch_add(&gq_result.result_id_generate, 1);
    result = (result_t)malloc_full(gq_result.scope, sizeof(struct result_data), RAII_FREE);
    results = (result_t *)atomic_load_explicit(&gq_result.results, memory_order_acquire);
    if (id % gq_result.queue_size == 0 || is_empty(results))
        results = try_realloc(results, (id + gq_result.queue_size) * sizeof(results[0]));

    result->is_ready = false;
    result->id = id;
    result->result = nullptr;
    result->type = RAII_VALUE;
    results[id] = result;
    atomic_store_explicit(&gq_result.results, results, memory_order_release);
    return result;
}

RAII_INLINE int exit_scope(void) {
    raii_deferred_free(get_scope());
    return 0;
}

RAII_INLINE memory_t *get_scope(void) {
    if (coro_is_valid())
        return coro_scope();

    memory_t *scope = raii_init();
    if (scope->status == RAII_GUARDED_STATUS)
        scope = ((memory_t *)scope->arena);

    return scope;
}

int raii_array_init(raii_array_t *a) {
    if (UNLIKELY(!a))
        return -EINVAL;

    a->base = NULL;
    a->elements = 0;
    a->type = RAII_DEF_ARR;

    return 0;
}

int raii_array_reset(raii_array_t *a) {
    if (UNLIKELY(!a))
        return -EINVAL;

    RAII_FREE(a->base);
    a->base = NULL;
    a->elements = 0;
    memset(a, 0, sizeof(raii_array_t));

    return 0;
}

#if !defined(HAS_REALLOC_ARRAY)
#if !defined(HAVE_BUILTIN_MUL_OVERFLOW)
/*
* This is sqrt(SIZE_MAX+1), as s1*s2 <= SIZE_MAX
* if both s1 < MUL_NO_OVERFLOW and s2 < MUL_NO_OVERFLOW
*/
#define MUL_NO_OVERFLOW ((size_t)1 << (sizeof(size_t) * 4))
static inline bool umull_overflow(size_t a, size_t b, size_t *out) {
    if ((a >= MUL_NO_OVERFLOW || b >= MUL_NO_OVERFLOW) && a > 0 && SIZE_MAX / a < b)
        return true;
    *out = a * b;
    return false;
}
#else
#define umull_overflow __builtin_mul_overflow
#endif
void_t realloc_array(void_t optr, size_t nmemb, size_t size) {
    size_t total_size;
    if (UNLIKELY(umull_overflow(nmemb, size, &total_size))) {
        errno = ENOMEM;
        return NULL;
    }

    return RAII_REALLOC(optr, total_size);
}
#endif /* HAS_REALLOC_ARRAY */

#if !defined(HAVE_BUILTIN_ADD_OVERFLOW)
static inline bool add_overflow(size_t a, size_t b, size_t *out) {
    if (UNLIKELY(a > 0 && b > SIZE_MAX - a))
        return true;

    *out = a + INCREMENT;
    return false;
}
#else
#define add_overflow __builtin_add_overflow
#endif

void_t raii_array_append(raii_array_t *a, size_t element_size) {
    if (!(a->elements % INCREMENT)) {
        void_t new_base;
        size_t new_cap;

        if (UNLIKELY(add_overflow(a->elements, INCREMENT, &new_cap))) {
            errno = EOVERFLOW;
            return NULL;
        }

        new_base = realloc_array(a->base, new_cap, element_size);
        if (UNLIKELY(!new_base))
            return NULL;

        a->base = new_base;
    }

    return ((unsigned char *)a->base) + a->elements++ * element_size;
}

RAII_INLINE int raii_deferred_init(defer_t *array) {
    return raii_array_init((raii_array_t *)array);
}

void raii_unwind_set(ex_context_t *ctx, const char *ex, const char *message) {
    memory_t *scope = raii_init();
    scope->err = (void_t)ex;
    scope->panic = message;
    ex_swap_set(ctx, (void_t)scope);
    ex_unwind_set(ctx, scope->is_protected);
}

RAII_INLINE memory_t *raii_local(void) {
    thrd_local_return(memory_t *, raii)
}

memory_t *raii_init(void) {
    memory_t *scope;
    if (is_empty(scope = raii_local())) {
        scope = raii();
        if (UNLIKELY(raii_deferred_init(&scope->defer) < 0))
            raii_panic("Deferred initialization failed!");

        scope->threading = 0;
        scope->arena = NULL;
        scope->protector = NULL;
        scope->panic = NULL;
        scope->err = NULL;
        scope->threaded = NULL;
        scope->local = NULL;
        scope->queued = NULL;
        scope->is_protected = false;
        scope->is_recovered = false;
        scope->mid = RAII_ERR;

        ex_context_t *ctx = ex_init();
        ctx->data = (void_t)scope;
        ctx->prev = (void_t)scope;
        ctx->is_raii = true;
        if (!exception_signal_set)
            ex_signal_setup();

        atexit(raii_destroy);
    }

    return scope;
}

RAII_INLINE size_t raii_mid(void) {
    return raii_local()->mid;
}

RAII_INLINE size_t raii_last_mid(memory_t *scope) {
    return scope->mid;
}

void_t try_calloc(int count, size_t size) {
    void_t ptr = RAII_CALLOC(count, size);
    if (ptr == NULL) {
        errno = ENOMEM;
        raii_panic("Calloc failed!");
    }

    return ptr;
}

void_t try_malloc(size_t size) {
    void_t ptr = RAII_MALLOC(size);
    if (ptr == NULL) {
        errno = ENOMEM;
        raii_panic("Malloc failed!");
    }

    return ptr;
}

void_t try_realloc(void_t old_ptr, size_t size) {
    void_t ptr = RAII_REALLOC(old_ptr, size);
    if (ptr == NULL) {
        errno = ENOMEM;
        raii_panic("Realloc failed!");
    }

    return ptr;
}

RAII_INLINE void_t raii_memdup(const_t src, size_t len) {
    return (void_t)str_memdup_ex(raii_init(), src, len);
}

RAII_INLINE string *raii_split(string_t s, string_t delim, int *count) {
    return str_split_ex(raii_init(), s, delim, count);
}

string raii_concat(int num_args, ...) {
    va_list ap;
    string data;

    va_start(ap, num_args);
    data = str_concat_ex(raii_init(), num_args, ap);
    va_end(ap);

    return data;
}

RAII_INLINE string raii_replace(string_t haystack, string_t needle, string_t replace) {
    return str_replace_ex(raii_init(), haystack, needle, replace);
}

RAII_INLINE u_string raii_encode64(u_string_t src) {
    return str_encode64_ex(raii_init(), src);
}

RAII_INLINE u_string raii_decode64(u_string_t src) {
    return str_decode64_ex(raii_local(), src);
}

unique_t *unique_init(void) {
    unique_t *raii = try_calloc(1, sizeof(unique_t));
    if (UNLIKELY(raii_deferred_init(&raii->defer) < 0))
        raii_panic("Deferred initialization failed!");

    raii->arena = NULL;
    raii->protector = NULL;
    raii->is_protected = false;
    raii->mid = RAII_ERR;
    return raii;
}

void_t malloc_full(memory_t *scope, size_t size, func_t func) {
    void_t arena = try_malloc(size);
    if (is_empty(scope->protector))
        scope->protector = try_malloc(sizeof(ex_ptr_t));

    scope->protector->is_emulated = scope->is_emulated;
    ex_protect_ptr(scope->protector, arena, func);
    scope->is_protected = true;
    scope->mid = raii_deferred(scope, func, arena);

    return arena;
}

RAII_INLINE void_t malloc_local(size_t size) {
    return malloc_full(get_scope(), size, RAII_FREE);
}

void_t calloc_full(memory_t *scope, int count, size_t size, func_t func) {
    void_t arena = try_calloc(count, size);
    if (is_empty(scope->protector))
        scope->protector = try_calloc(1, sizeof(ex_ptr_t));

    scope->protector->is_emulated = scope->is_emulated;
    ex_protect_ptr(scope->protector, arena, func);
    scope->is_protected = true;
    scope->mid = raii_deferred(scope, func, arena);

    return arena;
}

RAII_INLINE void_t calloc_local(int count, size_t size) {
    return calloc_full(get_scope(), count, size, RAII_FREE);
}

void raii_delete(memory_t *ptr) {
    if (ptr == NULL)
        return;

    raii_deferred_free(ptr);

#ifdef emulate_tls
    if (ptr != raii_local())
#else
    if (ptr != &thrd_raii_buffer)
#endif
    {
        memset(ptr, RAII_ERR, sizeof(memory_t));
        RAII_FREE(ptr);
    }

    ptr = NULL;
}

RAII_INLINE void raii_destroy(void) {
    raii_delete(raii_local());
}

static void raii_array_free(void_t data) {
    raii_array_t *array = data;

    raii_array_reset(array);
    memset(array, 0, sizeof(raii_array_t));
    data = NULL;
}

raii_array_t *raii_array_new(memory_t *scope) {
    raii_array_t *array = calloc_full(scope, 1, sizeof(raii_array_t), raii_array_free);
    raii_array_init(array);

    return array;
}

static RAII_INLINE defer_t *raii_deferred_array_new(memory_t *scope) {
    return (defer_t *)raii_array_new(scope);
}

static RAII_INLINE defer_func_t *raii_deferred_array_append(defer_t *array) {
    return (defer_func_t *)raii_array_append((raii_array_t *)array, sizeof(defer_func_t));
}

static RAII_INLINE int raii_deferred_array_reset(defer_t *array) {
    return raii_array_reset((raii_array_t *)array);
}

static RAII_INLINE void raii_deferred_array_free(defer_t *array) {
    raii_array_free((raii_array_t *)array);
}

static RAII_INLINE size_t raii_deferred_array_get_index(const defer_t *array, raii_array_t *elem) {
    RAII_ASSERT(elem >= (raii_array_t *)array->base.base);
    return (size_t)(elem - (raii_array_t *)array->base.base);
}

static RAII_INLINE raii_array_t *raii_deferred_array_get_element(const defer_t *array, size_t index) {
    RAII_ASSERT(index <= array->base.elements);
    return &((raii_array_t *)array->base.base)[index];
}

static RAII_INLINE size_t raii_deferred_array_len(const defer_t *array) {
    return array->base.elements;
}

static void deferred_canceled(void_t data) {}

static void raii_deferred_internal(memory_t *scope, defer_func_t *deferred) {
    const size_t num_defers = raii_deferred_array_len(&scope->defer);

    RAII_ASSERT(num_defers != 0 && deferred != NULL);

    if (deferred == (defer_func_t *)raii_deferred_array_get_element(&scope->defer, num_defers - 1)) {
        /* If we're cancelling the last defer we armed, there's no need to waste
         * space of a deferred callback to an empty function like
         * disarmed_defer(). */
        raii_array_t *defer_base = (raii_array_t *)&scope->defer;
        defer_base->elements--;
    } else {
        deferred->func = deferred_canceled;
        deferred->check = NULL;
    }
}

void raii_deferred_cancel(memory_t *scope, size_t index) {
    RAII_ASSERT(index >= 0);

    raii_deferred_internal(scope, (defer_func_t *)raii_deferred_array_get_element(&scope->defer, index));
}

void raii_deferred_fire(memory_t *scope, size_t index) {
    RAII_ASSERT(index >= 0);

    defer_func_t *deferred = (defer_func_t *)raii_deferred_array_get_element(&scope->defer, index);
    RAII_ASSERT(scope);

    deferred->func(deferred->data);

    raii_deferred_internal(scope, deferred);
}

static void raii_deferred_run(memory_t *scope, size_t generation) {
    raii_array_t *array = (raii_array_t *)&scope->defer;
    defer_func_t *defers = array->base;
    bool defer_ran = false;
    size_t i;

    scope->is_recovered = is_empty(scope->err);

    for (i = array->elements; i != generation; i--) {
        defer_func_t *defer = &defers[i - 1];
        defer_ran = true;

        if (!is_empty(scope->err) && !is_empty(defer->check))
            scope->is_recovered = false;

        defer->func(defer->data);
        defer->data = NULL;
    }

    if (scope->is_protected && !is_empty(scope->protector) && !is_empty(scope->err)) {
        scope->is_protected = false;
        if (!defer_ran) {
            scope->protector->func(*scope->protector->ptr);
            *scope->protector->ptr = NULL;
        }
    }

    array->elements = generation;
    if (!is_empty(scope->protector)) {
        ex_unprotected_ptr(scope->protector);
        RAII_FREE(scope->protector);
        scope->protector = NULL;
    }
}

size_t raii_deferred_count(memory_t *scope) {
    const raii_array_t *array = (raii_array_t *)&scope->defer;

    return array->elements;
}

void raii_deferred_free(memory_t *scope) {
    RAII_ASSERT(scope);

    if (is_type(&scope->defer, RAII_DEF_ARR)) {
        raii_deferred_run(scope, 0);
        raii_deferred_array_reset(&scope->defer);
    }
}

RAII_INLINE void raii_deferred_clean(void) {
    raii_deferred_free(raii_local());
    raii_deferred_init(&raii_local()->defer);
}

static size_t raii_deferred_any(memory_t *scope, func_t func, void_t data, void_t check) {
    defer_func_t *deferred;

    RAII_ASSERT(func);

    deferred = raii_deferred_array_append(&scope->defer);
    if (UNLIKELY(!deferred)) {
        RAII_LOG("Could not add new deferred function.");
        return RAII_ERR;
    } else {
        deferred->func = func;
        deferred->data = data;
        deferred->check = check;

        return raii_deferred_array_get_index(&scope->defer, (raii_array_t *)deferred);
    }
}

RAII_INLINE size_t raii_deferred(memory_t *scope, func_t func, void_t data) {
    return raii_deferred_any(scope, func, data, NULL);
}

RAII_INLINE size_t raii_defer(func_t func, void_t data) {
    return raii_deferred(raii_local(), func, data);
}

RAII_INLINE size_t deferring(func_t func, void_t data) {
    return raii_deferred(get_scope(), func, data);
}

RAII_INLINE void raii_recover(func_t func, void_t data) {
    raii_deferred_any(raii_local(), func, data, (void_t)"err");
}

RAII_INLINE void raii_recover_by(memory_t *scope, func_t func, void_t data) {
    raii_deferred_any(scope, func, data, (void_t)"err");
}

bool raii_caught(const char *err) {
    memory_t *scope = raii_local();
    const char *exception = (const char *)(!is_empty((void_t)scope->panic) ? scope->panic : scope->err);

    if (exception == NULL && is_str_eq(err, ex_local()->ex)) {
        ex_local()->state = ex_catch_st;
        return true;
    }

    if ((scope->is_recovered = is_str_eq(err, exception)))
        ex_local()->state = ex_catch_st;

    return scope->is_recovered;
}

RAII_INLINE bool raii_is_caught(memory_t *scope, const char *err) {
    const char *exception = (const char *)(!is_empty((void_t)scope->panic) ? scope->panic : scope->err);
    if ((scope->is_recovered = is_str_eq(err, exception)))
        ex_local()->state = ex_catch_st;

    return scope->is_recovered;
}

RAII_INLINE bool is_recovered(const char *err) {
    return raii_is_caught(get_scope(), err);
}

RAII_INLINE const char *raii_message(void) {
    memory_t *scope = raii_local();
    const char *exception = (const char *)(!is_empty((void_t)scope->panic) ? scope->panic : scope->err);
    return exception == NULL ? ex_local()->ex : exception;
}

RAII_INLINE const char *raii_message_by(memory_t *scope) {
    return !is_empty((void_t)scope->panic) ? scope->panic : scope->err;
}

RAII_INLINE const char *err_message(void) {
    return raii_message_by(get_scope());
}

RAII_INLINE void raii_defer_cancel(size_t index) {
    raii_deferred_cancel(raii_local(), index);
}

RAII_INLINE void raii_defer_fire(size_t index) {
    raii_deferred_fire(raii_local(), index);
}

void guard_set(ex_context_t *ctx, const char *ex, const char *message) {
    memory_t *scope = raii_local()->arena;
    scope->err = (void_t)ex;
    scope->panic = message;
    ctx->is_guarded = true;
    ex_swap_set(ctx, (void_t)scope);
    ex_unwind_set(ctx, scope->is_protected);
}

void guard_reset(void_t scope, ex_setup_func set, ex_unwind_func unwind) {
    raii_local()->arena = scope;
    ex_swap_reset(ex_local());
    ex_local()->is_guarded = false;
    exception_setup_func = set;
    exception_unwind_func = unwind;
}

void guard_delete(memory_t *ptr) {
    if (is_guard(ptr)) {
        memset(ptr, RAII_ERR, sizeof(ptr));
        RAII_FREE(ptr);
        ptr = NULL;
    }
}

RAII_INLINE values_type raii_value(void_t data) {
    if (data)
        return ((raii_values_t *)data)->value;

    return raii_values_empty->value;
}

RAII_INLINE raii_type type_of(void_t self) {
    return ((var_t *)self)->type;
}

RAII_INLINE bool is_guard(void_t self) {
    return !is_empty(self) && ((unique_t *)self)->status == RAII_GUARDED_STATUS;
}

RAII_INLINE bool is_type(void_t self, raii_type check) {
    return type_of(self) == check;
}

RAII_INLINE bool is_void(void_t self) {
    return ((void_t)self == (void_t)0xffffffffffffffff);
}

RAII_INLINE bool is_instance_of(void_t self, void_t check) {
    return type_of(self) == type_of(check);
}

RAII_INLINE bool is_value(void_t self) {
    return (type_of(self) > RAII_NULL) && (type_of(self) < RAII_NAN);
}

RAII_INLINE bool is_instance(void_t self) {
    return (type_of(self) > RAII_NAN) && (type_of(self) < RAII_NO_INSTANCE);
}

RAII_INLINE bool is_valid(void_t self) {
    return is_value(self) || is_instance(self);
}

RAII_INLINE bool is_invalid(void_t self) {
    return type_of(self) < RAII_NULL;
}

RAII_INLINE bool is_null(void_t self) {
    return type_of(self) == RAII_NULL;
}

RAII_INLINE bool is_zero(size_t self) {
    return self == 0;
}

RAII_INLINE bool is_empty(void_t self) {
    return self == NULL;
}

RAII_INLINE bool is_true(bool self) {
    return self == true;
}

RAII_INLINE bool is_false(bool self) {
    return self == false;
}

RAII_INLINE bool is_str_in(const char *text, char *pattern) {
    return strpos(text, pattern) >= 0;
}

RAII_INLINE bool is_str_eq(const char *str, const char *str2) {
    return (str != nullptr && str2 != nullptr) && (strcmp(str, str2) == 0);
}

RAII_INLINE bool is_str_empty(const char *str) {
    return is_str_eq(str, "");
}

RAII_INLINE bool is_equal(void_t mem, void_t mem2) {
    return memcmp(mem, mem2, sizeof(mem2)) == 0;
}
