#include "raii.h"

bool raii_rpmalloc_set = false;
thrd_local(memory_t, raii, NULL)

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
void *realloc_array(void *optr, size_t nmemb, size_t size) {
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

void *raii_array_append(raii_array_t *a, size_t element_size) {
    if (!(a->elements % INCREMENT)) {
        void *new_base;
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
    scope->err = (void *)ex;
    scope->panic = message;
    ex_swap_set(ctx, (void *)scope);
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

        scope->arena = NULL;
        scope->protector = NULL;
        scope->panic = NULL;
        scope->err = NULL;
        scope->is_protected = false;
        scope->is_recovered = false;
        scope->mid = -1;

        ex_context_t *ctx = ex_init();
        ctx->data = (void *)scope;
        ctx->prev = (void *)scope;
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

void *try_calloc(int count, size_t size) {
    void *ptr = RAII_CALLOC(count, size);
    if (ptr == NULL) {
        errno = ENOMEM;
        raii_panic("Calloc failed!");
    }

    return ptr;
}

void *try_malloc(size_t size) {
    void *ptr = RAII_MALLOC(size);
    if (ptr == NULL) {
        errno = ENOMEM;
        raii_panic("Malloc failed!");
    }

    return ptr;
}

void *try_realloc(void *old_ptr, size_t size) {
    void *ptr = RAII_REALLOC(old_ptr, size);
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

RAII_INLINE string raii_concat(int num_args, ...) {
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
    raii->mid = -1;
    return raii;
}

unique_t *unique_init_arena(void) {
    unique_t *raii = unique_init();
    raii->arena = (void *)arena_init(0);
    raii->is_arena = true;

    return raii;
}

void *malloc_full(memory_t *scope, size_t size, func_t func) {
    void *arena = try_malloc(size);
    if (is_empty(scope->protector))
        scope->protector = try_malloc(sizeof(ex_ptr_t));

    scope->protector->is_emulated = scope->is_emulated;
    ex_protect_ptr(scope->protector, arena, func);
    scope->is_protected = true;
    scope->mid = raii_deferred(scope, func, arena);

    return arena;
}

RAII_INLINE void *malloc_default(size_t size) {
    return malloc_full(raii_init(), size, RAII_FREE);
}

void *malloc_by(memory_t *scope, size_t size) {
    if (scope->is_arena)
        return arena_alloc(scope->arena, size);

    return malloc_full(scope, size, RAII_FREE);
}

RAII_INLINE void *malloc_arena(memory_t *scope, size_t size) {
    return arena_alloc(scope->arena, size);
}

void *calloc_full(memory_t *scope, int count, size_t size, func_t func) {
    void *arena = try_calloc(count, size);
    if (is_empty(scope->protector))
        scope->protector = try_calloc(1, sizeof(ex_ptr_t));

    scope->protector->is_emulated = scope->is_emulated;
    ex_protect_ptr(scope->protector, arena, func);
    scope->is_protected = true;
    scope->mid = raii_deferred(scope, func, arena);

    return arena;
}

RAII_INLINE void *calloc_default(int count, size_t size) {
    return calloc_full(raii_init(), count, size, RAII_FREE);
}

void *calloc_by(memory_t *scope, int count, size_t size) {
    if (scope->is_arena)
        return arena_calloc(scope->arena, (long)count, (long)size);

    return calloc_full(scope, count, size, RAII_FREE);
}

RAII_INLINE void *calloc_arena(memory_t *scope, int count, size_t size) {
    return arena_calloc(scope->arena, (long)count, (long)size);
}

void raii_delete(memory_t *ptr) {
    if (ptr == NULL)
        return;

    raii_deferred_free(ptr);

#ifdef emulate_tls
    if (ptr != raii_local()) {
#else
    if (ptr != &thrd_raii_buffer) {
#endif
        memset(ptr, -1, sizeof(memory_t));
        RAII_FREE(ptr);
    }

    ptr = NULL;
}

void free_arena(memory_t *ptr) {
    if (ptr == NULL)
        return;

    arena_t arena = NULL;
    if (!is_empty(ptr->arena))
        arena = (arena_t)ptr->arena;

    raii_delete(ptr);
    if (!is_empty(arena))
        arena_free(arena);

    ptr = NULL;
    arena = NULL;
}

RAII_INLINE void raii_destroy(void) {
    raii_delete(raii_local());
}

static void raii_array_free(void *data) {
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

static void deferred_canceled(void *data) {}

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
}

static size_t raii_deferred_any(memory_t *scope, func_t func, void *data, void *check) {
    defer_func_t *deferred;

    RAII_ASSERT(func);

    deferred = raii_deferred_array_append(&scope->defer);
    if (UNLIKELY(!deferred)) {
        RAII_LOG("Could not add new deferred function.");
        return -1;
    } else {
        deferred->func = func;
        deferred->data = data;
        deferred->check = check;

        return raii_deferred_array_get_index(&scope->defer, (raii_array_t *)deferred);
    }
}

RAII_INLINE size_t raii_deferred(memory_t *scope, func_t func, void *data) {
    return raii_deferred_any(scope, func, data, NULL);
}

RAII_INLINE size_t raii_defer(func_t func, void *data) {
    return raii_deferred(raii_local(), func, data);
}

RAII_INLINE void raii_recover(func_t func, void *data) {
    raii_deferred_any(raii_local(), func, data, (void *)"err");
}

RAII_INLINE void raii_recover_by(memory_t *scope, func_t func, void *data) {
    raii_deferred_any(scope, func, data, (void *)"err");
}

bool raii_caught(const char *err) {
    memory_t *scope = raii_local();
    const char *exception = (const char *)(!is_empty((void *)scope->panic) ? scope->panic : scope->err);

    if (exception == NULL && is_str_eq(err, ex_local()->ex)) {
        ex_local()->state = ex_catch_st;
        return true;
    }

    if ((scope->is_recovered = is_str_eq(err, exception)))
        ex_local()->state = ex_catch_st;

    return scope->is_recovered;
}

bool raii_is_caught(memory_t *scope, const char *err) {
    const char *exception = (const char *)(!is_empty((void *)scope->panic) ? scope->panic : scope->err);
    if ((scope->is_recovered = is_str_eq(err, exception)))
        ex_local()->state = ex_catch_st;

    return scope->is_recovered;
}

const char *raii_message(void) {
    memory_t *scope = raii_local();
    const char *exception = (const char *)(!is_empty((void *)scope->panic) ? scope->panic : scope->err);
    return exception == NULL ? ex_local()->ex : exception;
}

RAII_INLINE const char *raii_message_by(memory_t *scope) {
    return !is_empty((void *)scope->panic) ? scope->panic : scope->err;
}

RAII_INLINE void raii_defer_cancel(size_t index) {
    raii_deferred_cancel(raii_local(), index);
}

RAII_INLINE void raii_defer_fire(size_t index) {
    raii_deferred_fire(raii_local(), index);
}

void guard_set(ex_context_t *ctx, const char *ex, const char *message) {
    memory_t *scope = raii_local()->arena;
    scope->err = (void *)ex;
    scope->panic = message;
    ctx->is_guarded = true;
    ex_swap_set(ctx, (void *)scope);
    ex_unwind_set(ctx, scope->is_protected);
}

void guard_reset(void *scope, ex_setup_func set, ex_unwind_func unwind) {
    raii_local()->arena = scope;
    ex_swap_reset(ex_local());
    ex_local()->is_guarded = false;
    exception_setup_func = set;
    exception_unwind_func = unwind;
}

void guard_delete(memory_t *ptr) {
    if (is_guard(ptr)) {
        memset(ptr, -1, sizeof(ptr));
        RAII_FREE(ptr);
        ptr = NULL;
    }
}

RAII_INLINE values_type raii_value(void *data) {
    if (data)
        return ((raii_values_t *)data)->value;

    RAII_LOG("attempt to get value on null");
    return;
}

void args_free(args_t params) {
    if (is_type(params, RAII_ARGS)) {
        RAII_FREE(params->args);
        memset(params, -1, sizeof(args_t));
        RAII_FREE(params);
    }
}

values_type raii_get_args(memory_t *scope, void_t params, int item) {
    args_t args = (args_t)params;
    memory_t *scoped = scope;
    if (!args->defer_set) {
        args->defer_set = true;
        if (is_empty(scope))
            scoped = is_empty(args->context) ? raii_init() : args->context;
/*
        if (is_empty(scope) && is_empty(args->context)) {
            if (is_empty(scoped->protector))
                scoped->protector = try_calloc(1, sizeof(ex_ptr_t));

            scoped->protector->is_emulated = scoped->is_emulated;
            ex_protect_ptr(scoped->protector, params, (func_t)args_free);
            scoped->is_protected = true;
        }
*/
        scoped->mid = raii_deferred(scoped, (func_t)args_free, params);
    }

    return args_in(args, item);
}

RAII_INLINE values_type get_arg(void_t params) {
    return raii_value(params);
}

RAII_INLINE values_type get_args(args_t params, int item) {
    return raii_get_args(nullptr, (void_t)params, item);
}

RAII_INLINE values_type args_in(args_t params, size_t index) {
    return (index >= 0 && index < params->n_args)
        ? params->args[index].value
        : ((raii_values_t *)0)->value;
}

static args_t raii_args_ex(memory_t *scope, const char *desc, va_list argp) {
    size_t i, count = simd_strlen(desc);
    args_t params = try_calloc(1, sizeof(args_t));
    params->args = try_calloc(count, sizeof(raii_values_t));

    for (i = 0; i < count; i++) {
        switch (*desc++) {
            case 'l':
                // signed `long` argument
                params->args[i].value.s_long = (long)va_arg(argp, long);
                break;
            case 'z':
                // unsigned `size_t` argument
                params->args[i].value.max_size = (size_t)va_arg(argp, size_t);
                break;
            case 'u':
                // unsigned `u` argument
                params->args[i].value.u_int = (uint32_t)va_arg(argp, uint32_t);
                break;
            case 'i':
                // signed `i` argument
                params->args[i].value.integer = (int32_t)va_arg(argp, int32_t);
                break;
            case 'd':
                // signed `int64_t` argument
                params->args[i].value.long_long = (int64_t)va_arg(argp, int64_t);
                break;
            case 'c':
                // character argument
                params->args[i].value.schar = (char)va_arg(argp, int);
                break;
            case 's':
                // string argument
                params->args[i].value.char_ptr = (char *)va_arg(argp, char *);
                break;
            case 'a':
                // array argument
                params->args[i].value.array = (uintptr_t **)va_arg(argp, uintptr_t **);
                break;
            case 'x':
                // executable argument
                params->args[i].value.func = (raii_func_t)va_arg(argp, raii_func_args_t);
                break;
            case 'f':
                // float argument
                params->args[i].value.precision = (double)va_arg(argp, double);
                break;
            case 'p':
                // void pointer (any arbitrary pointer) argument
            default:
                params->args[i].value.object = va_arg(argp, void *);
                break;
        }
    }

    params->defer_set = false;
    params->n_args = count;
    params->context = scope;
    params->type = RAII_ARGS;
    return params;
}

args_t raii_args_for(memory_t *scope, const char *desc, ...) {
    args_t params;
    va_list args;

    va_start(args, desc);
    params = raii_args_ex(scope, desc, args);
    va_end(args);

    return params;
}

args_t args_for(const char *desc, ...) {
    args_t params;
    va_list args;

    va_start(args, desc);
    params = raii_args_ex(nullptr, desc, args);
    va_end(args);

    return params;
}

RAII_INLINE raii_type type_of(void *self) {
    return ((var_t *)self)->type;
}

RAII_INLINE bool is_guard(void *self) {
    return !is_empty(self) && ((unique_t *)self)->status == RAII_GUARDED_STATUS;
}

RAII_INLINE bool is_type(void *self, raii_type check) {
    return type_of(self) == check;
}

RAII_INLINE bool is_instance_of(void *self, void *check) {
    return type_of(self) == type_of(check);
}

RAII_INLINE bool is_value(void *self) {
    return (type_of(self) > RAII_NULL) && (type_of(self) < RAII_NAN);
}

RAII_INLINE bool is_instance(void *self) {
    return (type_of(self) > RAII_NAN) && (type_of(self) < RAII_NO_INSTANCE);
}

RAII_INLINE bool is_valid(void *self) {
    return is_value(self) || is_instance(self);
}

RAII_INLINE bool is_zero(size_t self) {
    return self == 0;
}

RAII_INLINE bool is_empty(void *self) {
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
    return (str != NULL && str2 != NULL) && (simd_strcmp(str, str2) == 0);
}

RAII_INLINE bool is_str_empty(const char *str) {
    return is_str_eq(str, "");
}

void raii_rpmalloc_init(void) {
    if (!raii_rpmalloc_set && !rpmalloc_is_thread_initialized()) {
        raii_rpmalloc_set = true;
        rpmalloc_initialize();
        atexit(rpmalloc_shutdown);
    } else if (!rpmalloc_is_thread_initialized()) {
        rpmalloc_thread_initialize();
    }
}
