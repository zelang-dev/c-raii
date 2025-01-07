#ifndef RAII_DEFINE_TYPES_H
#define RAII_DEFINE_TYPES_H

#include <stddef.h>
#include <stdbool.h>
#include <inttypes.h>

#if defined(_WIN32) || defined(_WIN64)
    #include "compat/sys/time.h"
    #include <excpt.h>
    #ifndef SYS_CONSOLE
        /* O.S. physical ~input/output~ console `DEVICE`. */
        #define SYS_CONSOLE "\\\\?\\CON"
        /* O.S. physical ~null~ `DEVICE`. */
        #define SYS_NULL "\\\\?\\NUL"
        /* O.S. physical ~pipe~ prefix `string name` including trailing slash. */
        #define SYS_PIPE "\\\\.\\pipe\\"
    #endif
#else
    #ifndef SYS_CONSOLE
        /* O.S. physical ~input/output~ console `DEVICE`. */
        #define SYS_CONSOLE "/dev/tty"
        /* O.S. physical ~null~ `DEVICE`. */
        #define SYS_NULL "/dev/null"
        /* O.S. physical ~pipe~ prefix `string name` including trailing slash. */
        #define SYS_PIPE "./"
    #endif
    #include <libgen.h>
#endif

/* Unsigned types. */
typedef unsigned char u8;
/* Unsigned types. */
typedef unsigned short u16;
/* Unsigned types. */
typedef unsigned int u32;
/* Unsigned types. */
typedef unsigned long long u64;

/* Signed types. */
typedef signed char s8;
/* Signed types. */
typedef signed short s16;
/* Signed types. */
typedef signed int s32;
/* Signed types. */
typedef signed long long s64;

/* Regular types. */
typedef char i8;
/* Regular types. */
typedef short i16;
/* Regular types. */
typedef int i32;
/* Regular types. */
typedef long long i64;

/* Floating point types. */
typedef float f32;
/* Double floating point types. */
typedef double f64;

/* Boolean types. */
typedef u8 b8;
/* Boolean types. */
typedef u32 b32;

/* Void pointer types. */
typedef void *void_t;
/* Const void pointer types. */
typedef const void *const_t;

/* Char pointer types. */
typedef char *string;
/* Const char pointer types. */
typedef const char *string_t;
/* Unsigned char pointer types. */
typedef unsigned char *u_string;
/* Const unsigned char pointer types. */
typedef const unsigned char *u_string_t;
/* Const unsigned char types. */
typedef const unsigned char u_char_t;

typedef void (*func_t)(void_t);
typedef void_t(*raii_func_t)(void_t);
typedef void (*func_args_t)(void_t, ...);
typedef void_t(*raii_func_args_t)(void_t, ...);

/* Generic simple union storage types. */
typedef union {
    int integer;
    unsigned int u_int;
    int *int_ptr;
    signed long s_long;
    unsigned long u_long;
    long long long_long;
    size_t max_size;
    uintptr_t ulong_long;
    float point;
    double precision;
    bool boolean;
    signed short s_short;
    unsigned short u_short;
    unsigned short *u_short_ptr;
    unsigned char *uchar_ptr;
    signed char schar;
    unsigned char uchar;
    char *char_ptr;
    void_t object;
    ptrdiff_t **array;
    char **array_char;
    intptr_t **array_int;
    uintptr_t **array_uint;
    raii_func_t func;
} value_t, *params_t, *arrays_t, *waitgroup_t, *waitresult_t;
typedef void_t(*callable_t)(params_t);

enum {
    RAII_OK = 0,
    /* invalid address indicator */
    RAII_ERR = -1,
};

typedef enum {
    RAII_INVALID = RAII_ERR,
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
    RAII_CORO,
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
    RAII_NO_INSTANCE,
    RAII_ARENA,
    RAII_THREAD,
    RAII_GUARDED_STATUS,
    RAII_VECTOR,
    RAII_QUEUE,
    RAII_SPAWN,
    RAII_POOL,
    RAII_SYNC,
    RAII_COUNT
} raii_type;

/* Smart memory pointer, the allocated memory requested in `arena` field,
all other fields private, this object binds any additional requests to it's lifetime. */
typedef struct memory_s memory_t;
typedef struct _future *future;
typedef struct future_pool *future_t;
typedef struct raii_deque_s raii_deque_t;
typedef struct raii_results_s future_results_t;
typedef memory_t unique_t;
typedef intptr_t(*raii_callable_t)(intptr_t);
typedef uintptr_t(*raii_callable_args_t)(uintptr_t, ...);
typedef uintptr_t *(*raii_callable_const_t)(const char *, ...);

/* Generic simple union storage types. */
typedef union {
    int integer;
    unsigned int u_int;
    int *int_ptr;
    signed long s_long;
    unsigned long u_long;
    long long long_long;
    size_t max_size;
    uintptr_t ulong_long;
    float point;
    double precision;
    bool boolean;
    signed short s_short;
    unsigned short u_short;
    unsigned short *u_short_ptr;
    unsigned char *uchar_ptr;
    signed char schar;
    unsigned char uchar;
    char *char_ptr;
    void_t object;
    ptrdiff_t **array;
    char **array_char;
    intptr_t **array_int;
    uintptr_t **array_uint;
    raii_func_t func;
    char buffer[64];
} values_type, *vectors_t, *args_t;

typedef struct {
    values_type value;
    int is_arrayed;
    int is_vectored;
    void_t extended;
    value_t valued;
} raii_values_t;

typedef struct {
    raii_type type;
    void_t value;
} var_t;

typedef struct {
    raii_type type;
    void_t value;
    func_t dtor;
} object_t;

typedef struct {
    raii_type type;
    void_t base;
    size_t elements;
} raii_array_t;

typedef struct {
    raii_type type;
    raii_array_t base;
} defer_t;

typedef struct {
    raii_type type;
    void (*func)(void_t);
    void_t data;
    void_t check;
} defer_func_t;

#ifndef __cplusplus
#define nullptr NULL
#endif

#define nil nullptr

#define time_Second 1000
#define time_Minute 60000
#define time_Hour   3600000

/**
 * Simple macro for making sure memory addresses are aligned
 * to the nearest power of two
 */
#ifndef align_up
#define align_up(num, align) (((num) + ((align)-1)) & ~((align)-1))
#endif

#ifndef container_of
#define container_of(ptr, type, member) \
                      ((type *) ((char *)(ptr) - offsetof(type, member)))
#endif

#if defined(__clang__)
#  define COMPILER_CLANG
#elif defined(_MSC_VER)
#  define COMPILER_CL
#elif defined(__GNUC__)
#  define COMPILER_GCC
#endif

#if defined(COMPILER_CLANG)
#  define FILE_NAME __FILE_NAME__
#else
#  define FILE_NAME __FILE__
#endif

#define Statement(s) do {\
s\
} while (0)

#define trace Statement(printf("%s:%d: Trace\n", FILE_NAME, __LINE__);)
#define unreachable Statement(printf("How did we get here? In %s on line %d\n", FILE_NAME, __LINE__);)
#define PATH_MAX 4096

#define Gb(count) (u64) (count * 1024 * 1024 * 1024)
#define Mb(count) (u64) (count * 1024 * 1024)
#define Kb(count) (u64) (count * 1024)

#endif /* RAII_DEFINE_TYPES_H */
