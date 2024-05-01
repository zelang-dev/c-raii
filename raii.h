#ifndef RAII_H
#define RAII_H

#include "exception.h"

#ifdef __cplusplus
    extern "C" {
#endif

typedef struct memory_s memory_t;
typedef void (*func_t)(void *);

typedef enum {
    RAII_NULL,
    RAII_INT,
    RAII_ENUM,
    RAII_INTEGER,
    RAII_UINT,
    RAII_SLONG,
    RAII_ULONG,
    RAII_LLONG,
    RAII_MAXSIZE,
    RAII_FLOAT,
    RAII_DOUBLE,
    RAII_BOOL,
    RAII_SHORT,
    RAII_USHORT,
    RAII_CHAR,
    RAII_UCHAR,
    RAII_UCHAR_P,
    RAII_CHAR_P,
    RAII_CONST_CHAR,
    RAII_STRING,
    RAII_ARRAY,
    RAII_HASH,
    RAII_OBJ,
    RAII_PTR,
    RAII_FUNC,
    RAII_NAN,
    RAII_DEF_ARR,
    RAII_DEF_FUNC,
    RAII_ROUTINE,
    RAII_OA_HASH,
    RAII_REFLECT_TYPE,
    RAII_REFLECT_INFO,
    RAII_REFLECT_VALUE,
    RAII_MAP_VALUE,
    RAII_MAP_STRUCT,
    RAII_MAP_ITER,
    RAII_MAP_ARR,
    RAII_ERR_PTR,
    RAII_ERR_CONTEXT,
    RAII_PROMISE,
    RAII_FUTURE,
    RAII_FUTURE_ARG,
    RAII_EVENT_ARG,
    RAII_ARGS,
    RAII_PROCESS,
    RAII_SCHED,
    RAII_CHANNEL,
    RAII_STRUCT,
    RAII_UNION,
    RAII_VALUE,
    RAII_NO_INSTANCE
} raii_type;

typedef struct {
    raii_type type;
    void *value;
} var_t;

typedef struct {
    raii_type type;
    void *base;
    size_t elements;
} raii_array_t;

typedef struct {
    raii_type type;
    raii_array_t base;
} defer_t;

typedef struct {
    raii_type type;
    void (*func)(void *);
    void *data;
    void *check;
} defer_func_t;

struct memory_s {
    void *arena;
    int status;
    bool is_recovered;
    void *volatile err;
    const char *volatile panic;
    bool is_protected;
    ex_ptr_t *protector;
    defer_t defer;
    size_t mid;
};

C_API memory_t *raii_init(void);
C_API int raii_deferred_init(defer_t *array);

/* Defer execution `LIFO` of given function with argument,
to when current scope exits. */
C_API size_t raii_defer(func_t, void *);

C_API void raii_defer_cancel(size_t index);
C_API void raii_deferred_cancel(memory_t *scope, size_t index);

C_API void raii_defer_fire(size_t index);
C_API void raii_deferred_fire(memory_t *scope, size_t index);

/* Same as `defer` but allows recover from an Error condition throw/panic,
you must call `raii_catch` inside function to mark Error condition handled. */
C_API void raii_recover(func_t, void *);

/* Compare `err` to current error condition, will mark exception handled, if `true`. */
C_API bool raii_catch(const char *err);

/* Get current error condition string. */
C_API const char *raii_message(void);

/* Defer execution `LIFO` of given function with argument,
to when scope exits. */
C_API size_t raii_deferred(memory_t *, func_t, void *);
C_API size_t raii_deferred_count(memory_t *);

C_API memory_t *raii_malloc_full(size_t size, func_t func);
C_API void *malloc_full(memory_t *scope, size_t size, func_t func);
C_API memory_t *raii_new(size_t size);
C_API void *new_arena(memory_t *scope, size_t size);

C_API memory_t *raii_calloc_full(int count, size_t size, func_t func);
C_API void *calloc_full(memory_t *scope, int count, size_t size, func_t func);
C_API memory_t *raii_new_by(int count, size_t size);
C_API void *new_arena_by(memory_t *scope, int count, size_t size);

C_API void raii_delete(memory_t *ptr);
C_API void raii_deferred_free(memory_t *);

/* Same as `defer` but allows recover from an Error condition throw/panic,
you must call `raii_catch` inside function to mark Error condition handled. */
C_API void raii_recover_by(memory_t *, func_t, void *);


C_API raii_type type_of(void *);
C_API bool is_type(void *, raii_type);
C_API bool is_instance_of(void *, void *);
C_API bool is_value(void *);
C_API bool is_instance(void *);
C_API bool is_valid(void *);

C_API bool is_zero(size_t);
C_API bool is_empty(void *);
C_API bool is_str_in(const char *text, char *pattern);
C_API bool is_str_eq(const char *str, const char *str2);
C_API bool is_str_empty(const char *str);

C_API void *try_calloc(int, size_t);
C_API void *try_malloc(size_t);

C_API thread_local memory_t *raii_context;

#ifdef __cplusplus
    }
#endif
#endif /* RAII_H */
