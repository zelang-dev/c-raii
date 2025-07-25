
/*
A dynamic array implementation in C similar to the one found in standard C++, with auto destructor memory cleanup.

Modified version of https://github.com/eteran/c-vector
 */

#include "raii.h"

typedef struct vector_metadata_s {
    raii_type type;
    int defer_set;
    size_t size;
    size_t capacity;
    func_t destructor;
    memory_t *context;
} vector_metadata_t;

#define vector_address(vec) (&((vector_metadata_t *)(vec))[-1])
#define vector_base(ptr) ((void_t)&((vector_metadata_t *)(ptr))[1])
#define vector_set_context(vec, ptr) vector_address(vec)->context = (ptr)
#define vector_set_capacity(vec, size) vector_address(vec)->capacity = (size)
#define vector_set_size(vec, _size) vector_address(vec)->size = (_size)
#define vector_set_type(vec, _type) vector_address(vec)->type = (_type)
#define vector_defer_set(vec) vector_address(vec)->defer_set = true
#define vector_defer_unset(vec) vector_address(vec)->defer_set = false
#define vector_set_destructor(vec, elem_destructor_fn) vector_address(vec)->destructor = (elem_destructor_fn)
#define vector_destructor(vec) vector_address(vec)->destructor
#define vector_context(vec) vector_address(vec)->context
#define vector_length(vec) ((vec) ? vector_address(vec)->size : (size_t)0)
#define vector_type(vec) ((vec) ? vector_address(vec)->type : RAII_ERR)
#define vector_deferred(vec) vector_address(vec)->defer_set
#define vector_cap(vec) ((vec) ? vector_address(vec)->capacity : (size_t)0)
#define vector_grow(vec, count, scope)                     \
    do {                                            \
        const size_t cv_sz__ = (count) * sizeof(*(vec)) + sizeof(vector_metadata_t);    \
        if (vec) {                                  \
            void *cv_p1__ = vector_address(vec);    \
            void *cv_p2__ = try_realloc(cv_p1__, cv_sz__);  \
            (vec) = vector_base(cv_p2__);           \
        } else {                                    \
            void *cv_p__ = try_malloc(cv_sz__);     \
            (vec) = vector_base(cv_p__);            \
            vector_set_size((vec), 0);              \
            vector_set_destructor((vec), NULL);     \
            vector_set_context((vec), (scope));     \
            vector_defer_unset(vec);                \
            vector_set_type((vec), RAII_VECTOR);    \
        }                                           \
        vector_set_capacity((vec), (count));        \
    } while (0)

static RAII_INLINE size_t vec_num(void) {
    size_t count = thrd_cpu_count();
    return 1 << ((count > 5 ) ? 6 : count * 2);
}

static RAII_INLINE void vector_delete(vectors_t vec) {
    if (vec) {
        void_t p1__ = vector_address(vec);
        func_t destructor__ = vector_destructor(vec);
        if (destructor__) {
            size_t i__;
            for (i__ = 0; i__ < vector_length(vec); ++i__) {
                destructor__(&(vec)[i__]);
            }
        }

        RAII_FREE(p1__);
    }
}

RAII_INLINE void vector_insert(vectors_t vec, size_t pos, void_t val) {
    size_t size, cv_cap__ = vector_cap(vec);
    memory_t *scope = vector_context(vec);
    if (cv_cap__ <= vector_length(vec)) {
        size = cv_cap__ << 1;
        vector_grow((vec), size, scope);
    }

    if ((pos) < vector_length(vec)) {
        memmove((vec)+(pos)+1, (vec)+(pos), sizeof(*(vec)) * ((vector_length(vec)) - (pos)));
    }

    (vec)[(pos)].object = (val);
    vector_set_size((vec), vector_length(vec) + 1);
}

void vector_erase(vectors_t vec, size_t i) {
    if (vec) {
        const size_t cv_sz__ = vector_length(vec);
        if ((i) < cv_sz__) {
            func_t destructor__ = vector_destructor(vec);
            if (destructor__) {
                destructor__(&(vec)[i]);
            }

            vector_set_size((vec), cv_sz__ - 1);
            memmove((vec)+(i), (vec)+(i)+1, sizeof(*(vec)) * (cv_sz__ - 1 - (i)));
        }
    }
}

RAII_INLINE void vector_clear(vectors_t vec) {
    if (vec) {
        func_t destructor__ = vector_destructor(vec);
        if (destructor__) {
            size_t i__;
            for (i__ = 0; i__ < vector_length(vec); ++i__) {
                destructor__(&(vec)[i__]);
            }
        }

        vector_set_size(vec, 0);
    }
}

RAII_INLINE vectors_t vector_copy(memory_t *scope, vectors_t des, vectors_t src) {
    if (src) {
        des = args_for(0);
        foreach(x in src)
            vector_push_back(des, x.object);
        vector_defer_set(des);
        raii_deferred((gq_result.scope ? gq_result.scope : scope), RAII_FREE, vector_context(des));
        raii_deferred(scope, (func_t)vector_delete, des);
    }

    return des;
}

RAII_INLINE void vector_push_back(vectors_t vec, void_t value) {
    size_t size, cv_cap__ = vector_cap(vec);
    memory_t *scope = vector_context(vec);
    if (cv_cap__ <= vector_length(vec)) {
        size = cv_cap__ << 1;
        vector_grow(vec, size, scope);
    }

    vec[vector_length(vec)].object = value;
    vector_set_size(vec, vector_length(vec) + 1);
}

vectors_t vector_for(vectors_t v, size_t item_count, ...) {
    va_list ap;
    size_t i;

    if (is_empty(v))
        v = vector_variant();

    if (item_count > 0) {
        va_start(ap, item_count);
        for (i = 0; i < item_count; i++)
            vector_push_back(v, va_arg(ap, void_t));
        va_end(ap);
    }

    return v;
}

RAII_INLINE vectors_t vector_variant(void) {
    vectors_t vec = nullptr;
    size_t n = vec_num();
    memory_t *scope = unique_init();
    vector_grow(vec, n, scope);
    raii_deferred(scope, (func_t)vector_delete, vec);
    deferring((func_t)raii_delete, scope);

    return vec;
}

RAII_INLINE size_t vector_size(vectors_t v) {
    return vector_length(v);
}

RAII_INLINE size_t vector_capacity(vectors_t v) {
    return vector_cap(v);
}

RAII_INLINE memory_t *vector_scope(vectors_t v) {
    return vector_context(v);
}

RAII_INLINE void args_destructor_set(args_t params) {
    if (vector_type(params) == RAII_ARGS && !vector_deferred(params)) {
        vector_defer_set(params);
        memory_t *scope = vector_context(params);
        deferring((func_t)raii_delete, scope);
        raii_deferred(scope, (func_t)vector_delete, params);
        raii_local()->local = params;
    }
}

args_t args_ex(size_t num_of, va_list ap_copy) {
    va_list ap;
    size_t i;

    args_t params = nullptr;
    if (num_of > 0) {
        va_copy(ap, ap_copy);
        params = args_for(0);
        for (i = 0; i < num_of; i++)
            $push_back(params, va_arg(ap, void_t));
        va_end(ap);
    }

    return params;
}

arrays_t array_ex(memory_t *scope, size_t num_of, va_list ap_copy) {
    va_list ap;
    size_t i;

    arrays_t params = nullptr;
    if (num_of > 0) {
        va_copy(ap, ap_copy);
        params = array_of(scope, 0);
        for (i = 0; i < num_of; i++)
            $append(params, va_arg(ap, void_t));
        va_end(ap);
    }

    return params;
}

RAII_INLINE arrays_t array_copy(arrays_t des, arrays_t src) {
    memory_t *scope;
    size_t cv_sz___;
    if (src) {
        des = arrays();
        scope = vector_context(des);
        foreach(x in src)
            $append(des, x.object);
        cv_sz___ = vector_length(des);
        vector_grow((des), (cv_sz___), scope);
        vector_set_type(des, vector_type(src));
    }

    return des;
}

ranges_t rangeing(int start, int stop, int steps) {
    ranges_t array = arrays();
    memory_t *scope = vector_context(array);
    int i, n = stop - start;
    size_t cv_sz___;
    for (i = start; i < stop; i = i + steps)
        $append(array, (intptr_t)i);

    cv_sz___ = vector_length(array);
    vector_grow((array), (cv_sz___), scope);
    vector_set_type(array, RAII_RANGE);
    return array;
}

ranges_t range(int start, int stop) {
    ranges_t array = arrays();
    memory_t *scope = vector_context(array);
    int i, n = stop - start;
    vector_grow((array), (n + 1), scope);
    for (i = start; i < stop; i++)
        $append(array, (intptr_t)i);

    vector_set_type(array, RAII_RANGE);
    return array;
}

ranges_t range_char(string_t text) {
    ranges_t array = arrays();
    memory_t *scope = vector_context(array);
    size_t len = simd_strlen(text);
    size_t i;
    vector_grow((array), (len + 1), scope);
    for (i = 0; i < len; i++)
        $append(array, (uintptr_t)*text++);

    vector_set_type(array, RAII_RANGE_CHAR);
    return array;
}

RAII_INLINE void array_remove(arrays_t arr, size_t i) {
    if (arr) {
        const size_t cv_sz__ = vector_length(arr);
        if ((i) < cv_sz__) {
            func_t destructor__ = vector_destructor(arr);
            if (destructor__) {
                destructor__(&(arr)[i]);
            }

            vector_set_size((arr), cv_sz__ - 1);
            memmove((arr)+(i), (arr)+(i)+1, sizeof(*(arr)) * (cv_sz__ - 1 - (i)));
        }
    }
}

RAII_INLINE void array_delete(arrays_t arr) {
    if (arr) {
        void_t p1__ = vector_address(arr);
        func_t destructor__ = vector_destructor(arr);
        if (destructor__) {
            size_t i__;
            for (i__ = 0; i__ < vector_length(arr); ++i__) {
                destructor__(&(arr)[i__]);
            }
        }

        RAII_FREE(p1__);
    }
}

RAII_INLINE void array_append(arrays_t arr, void_t value) {
    size_t size, cv_cap__ = vector_cap(arr);
    memory_t *scope = vector_context(arr);
    if (cv_cap__ <= vector_length(arr)) {
        size = cv_cap__ << 1;
        vector_grow(arr, size, scope);
    }

    arr[vector_length(arr)].object = value;
    vector_set_size(arr, vector_length(arr) + 1);
}

RAII_INLINE value_t array_pop(arrays_t arr) {
    size_t sz = vector_length(arr);
    if (sz > 0) {
        value_t val = arr[sz - 1];
        array_remove(arr, sz - 1);

        return val;
    }

    return raii_values_empty->valued;
}

RAII_INLINE value_t array_shift(arrays_t arr) {
    size_t sz = vector_length(arr);
    if (sz > 0) {
        value_t val = arr[0];
        array_remove(arr, 0);

        return val;
    }

    return raii_values_empty->valued;
}

void array_append_item(arrays_t arr, ...) {
    va_list ap;
    raii_type n = RAII_ERR;
    size_t size, cv_cap__ = vector_cap(arr), index = vector_length(arr);
    memory_t *scope = vector_context(arr);
    if (cv_cap__ <= index) {
        size = cv_cap__ << 1;
        vector_grow(arr, size, scope);
    }

    va_start(ap, arr);
    n = va_arg(ap, raii_type);
    if (n == RAII_DOUBLE) {
        arr[index].precision = va_arg(ap, double);
    } else if (n == RAII_LLONG) {
        arr[index].long_long = va_arg(ap, int64_t);
    } else if (n == RAII_MAXSIZE) {
        arr[index].max_size = va_arg(ap, size_t);
    } else if (n == RAII_FUNC) {
        arr[index].func = (raii_func_t)va_arg(ap, raii_func_args_t);
    } else if (n == RAII_SHORT) {
        arr[index].s_short = (short)va_arg(ap, int);
    } else if (n == RAII_BOOL) {
        arr[index].boolean = (bool)va_arg(ap, int);
    } else if (n == RAII_CHAR) {
        arr[index].schar = (char)va_arg(ap, int);
    } else if (n == RAII_STRING) {
        arr[index].char_ptr = (string)va_arg(ap, string);
    } else {
        arr[index].object = va_arg(ap, void_t);
    }
    va_end(ap);
    vector_set_size(arr, index + 1);
}

RAII_INLINE void array_reset(arrays_t arr) {
    vector_clear((vectors_t)arr);
}

arrays_t array_of(memory_t *scope, size_t count, ...) {
    va_list ap;
    size_t i;
    arrays_t params = nullptr;
    size_t size = count ? count + 1 : vec_num();
    vector_grow(params, size, scope);

    if (count > 0) {
        va_start(ap, count);
        for (i = 0; i < count; i++)
            array_append(params, va_arg(ap, void_t));
        va_end(ap);
    }

    vector_set_type(params, RAII_ARRAY);
    return params;
}

RAII_INLINE arrays_t arrays(void) {
    memory_t *scope;
    if (!coro_sys_set)
        scope = unique_init();
    else
        scope = get_scope();

    arrays_t array = array_of(scope, 0);
    array_deferred_set(array, scope);
    if (!coro_sys_set)
        deferring((func_t)raii_delete, scope);

    return array;
}

RAII_INLINE void array_deferred_set(arrays_t params, memory_t *scope) {
    if (vector_type(params) == RAII_ARRAY && !vector_deferred(params)) {
        vector_defer_set(params);
        raii_deferred(scope, (func_t)array_delete, params);
    }
}

args_t args_for(size_t count, ...) {
    va_list ap;
    size_t i;
    args_t params = nullptr;
    size_t size = count ? count : vec_num();
    memory_t *scope = unique_init();
    vector_grow(params, size, scope);

    if (count > 0) {
        va_start(ap, count);
        for (i = 0; i < count; i++)
            vector_push_back(params, va_arg(ap, void_t));
        va_end(ap);
    }

    vector_set_type(params, RAII_ARGS);
    return params;
}

RAII_INLINE bool is_vector(void_t params) {
    return is_empty(params) || is_null(params)
        ? false
        : vector_type((vectors_t)params) == RAII_VECTOR;
}

RAII_INLINE bool is_array(void_t params) {
    if (is_empty(params) || is_null(params))
        return false;

    int param = vector_type((arrays_t)params);
    return param == RAII_ARRAY || param == RAII_RANGE || param == RAII_RANGE_CHAR;
}

RAII_INLINE bool is_args(void_t params) {
    return is_empty(params) || is_null(params)
        ? false
        : vector_type((args_t)params) == RAII_ARGS;
}

RAII_INLINE values_type get_arg(void_t params) {
    return raii_value(params);
}
