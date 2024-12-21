
/*
A dynamic array implementation in C similar to the one found in standard C++, with auto destructor memory cleanup.

Modified version of https://github.com/eteran/c-vector
 */

#include "raii.h"

typedef struct vector_metadata_s {
    raii_type type;
    int defer_set;
    int is_returning;
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
#define vector_return_set(vec) vector_address(vec)->is_returning = true
#define vector_return_unset(vec) vector_address(vec)->is_returning = false
#define vector_set_destructor(vec, elem_destructor_fn) vector_address(vec)->destructor = (elem_destructor_fn)
#define vector_destructor(vec) vector_address(vec)->destructor
#define vector_context(vec) vector_address(vec)->context
#define vector_length(vec) ((vec) ? vector_address(vec)->size : (size_t)0)
#define vector_type(vec) ((vec) ? vector_address(vec)->type : -1)
#define vector_deferred(vec) vector_address(vec)->defer_set
#define vector_returning(vec) vector_address(vec)->is_returning
#define vector_cap(vec) ((vec) ? vector_address(vec)->capacity : (size_t)0)
#define vector_at(vec, n) ((vec) ? (((int)(n) < 0 || (size_t)(n) >= vector_length(vec)) ? NULL : &(vec)[n]) : NULL)
#define vector_front(vec) ((vec) ? ((vector_length(vec) > 0) ? vector_at(vec, 0) : NULL) : NULL)
#define vector_back(vec) ((vec) ? ((vector_length(vec) > 0) ? vector_at(vec, vector_length(vec) - 1) : NULL) : NULL)
#define vector_grow(vec, count)                     \
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
            vector_set_context((vec), get_scope()); \
            vector_defer_unset(vec);                \
            vector_return_unset(vec);               \
            vector_set_type((vec), RAII_VECTOR);    \
        }                                           \
        vector_set_capacity((vec), (count));        \
    } while (0)

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

RAII_INLINE void vector_insert(vectors_t vec, int pos, void_t val) {
    size_t cv_cap__ = vector_cap(vec);
    if (cv_cap__ <= vector_length(vec)) {
        vector_grow((vec), cv_cap__);
    }

    if ((pos) < vector_length(vec)) {
        memmove((vec)+(pos)+1, (vec)+(pos), sizeof(*(vec)) * ((vector_length(vec)) - (pos)));
    }

    (vec)[(pos)].object = (val);
    vector_set_size((vec), vector_length(vec) + 1);
}

void vector_erase(vectors_t vec, int i) {
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

RAII_INLINE void vector_push_back(vectors_t vec, void_t value) {
    size_t cv_cap__ = vector_cap(vec);
    if (cv_cap__ <= vector_length(vec)) {
        vector_grow(vec, 1);
    }

    vec[vector_length(vec)].object = value;
    vector_set_size(vec, vector_length(vec) + 1);
}

vectors_t vector_for(vectors_t v, size_t item_count, ...) {
    va_list ap;
    size_t i;

    if (is_empty(v))
        v = vector_any();

    if (item_count > 0) {
        va_start(ap, item_count);
        for (i = 0; i < item_count; i++)
            vector_push_back(v, va_arg(ap, void_t));
        va_end(ap);
    }

    return v;
}

RAII_INLINE vectors_t vector_any(void) {
    vectors_t vec = nullptr;
    size_t cores = thrd_cpu_count();
    vector_grow(vec, cores);
    deferring((func_t)vector_delete, vec);

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
        deferring((func_t)vector_delete, params);
    }
}

RAII_INLINE void args_returning_set(args_t params) {
    if (vector_type(params) == RAII_ARGS) {
        vector_return_set(params);
    }
}

args_t args_for(size_t count, ...) {
    va_list ap;
    size_t i;
    args_t params = nullptr;
    size_t size = count ? count : thrd_cpu_count();
    vector_grow(params, size);

    if (count > 0) {
        va_start(ap, count);
        for (i = 0; i < count; i++)
            vector_push_back(params, va_arg(ap, void_t));
        va_end(ap);
    }

    vector_set_type(params, RAII_ARGS);
    return params;
}

RAII_INLINE bool is_args(args_t params) {
    return vector_type(params) == RAII_ARGS;
}

RAII_INLINE bool is_args_returning(args_t params) {
    return is_args(params) && vector_returning(params);
}

RAII_INLINE values_type get_arg(void_t params) {
    return raii_value(params);
}
