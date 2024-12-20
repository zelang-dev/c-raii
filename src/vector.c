#include "raii.h"

static void vector_free(vector_t v) {
    if (is_type(v, RAII_VECTOR)) {
        memory_t *scope = v->scope;
        RAII_FREE(v->items);
        v->type = -1;
        raii_delete(scope);
    }
}

static void vector_resize(vector_t v, int capacity) {
    RAII_INFO("vector_resize: %d to %d\n", v->capacity, capacity);

    raii_values_t *items = try_realloc(v->items, sizeof(raii_values_t) * capacity);
    if (items) {
        v->items = items;
        v->capacity = capacity;
    }
}

RAII_INLINE void vector_init(vector_t v) {
    v->capacity = thrd_cpu_count();
    v->total = 0;
    v->items = try_malloc(sizeof(raii_values_t) * v->capacity);
    v->type = RAII_VECTOR;
}

vector_t vector_local(int count, ...) {
    va_list ap;
    int i;
    memory_t *scope = unique_init();
    vector_t v = (vector_t)malloc_full(scope, sizeof(struct vector_s), RAII_FREE);
    v->scope = scope;
    vector_init(v);
    deferring((func_t)vector_free, v);
    if (count > 0) {
        va_start(ap, count);
        for (i = 0; i < count; i++)
            vector_add(v, va_arg(ap, void_t));
        va_end(ap);
    }

    return v;
}

void vector_of(vector_t v, int item_count, ...) {
    va_list ap;
    int i;

    va_start(ap, item_count);
    for (i = 0; i < item_count; i++)
        vector_add(v, va_arg(ap, void_t));
    va_end(ap);
}

RAII_INLINE int vector_size(vector_t v) {
    return v->total;
}

RAII_INLINE int vector_capacity(vector_t v) {
    return v->capacity;
}

RAII_INLINE void vector_add(vector_t v, void_t item) {
    if (v->capacity == v->total)
        vector_resize(v, v->capacity * 2);

    v->items[v->total++].value.object = item;
}

RAII_INLINE void vector_set(vector_t v, int index, void_t item) {
    if (index >= 0 && index < v->total)
        v->items[index].value.object = item;
}

RAII_INLINE values_type vector_get(vector_t v, int index) {
    if (index >= 0 && index < v->total)
        return v->items[index].value;

    return raii_values_empty->value;
}

void vector_erase(vector_t v, int index) {
    int i;
    if (index < 0 || index >= v->total)
        return;

    v->items[index].value.object = nullptr;

    for (i = index; i < v->total - 1; i++) {
        v->items[i].value.object = v->items[i + 1].value.object;
        v->items[i + 1].value.object = nullptr;
    }

    v->total--;

    if (v->total > 0 && v->total == v->capacity / 4)
        vector_resize(v, v->capacity / 2);
}

RAII_INLINE void vector_clear(vector_t v) {
    int i, end = vector_size(v) - 1;
    for (i = end; i >= 0 ; i--)
        vector_erase(v, i);
}

RAII_INLINE void vector_delete(values_type *vec) {
    if (vec) {
        void *p1__ = vector_address(vec);
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

RAII_INLINE void vector_push_back(values_type *vec, void_t value) {
    size_t cv_cap__ = vector_cap(vec);
    if (cv_cap__ <= vector_length(vec)) {
        vector_grow(vec, 1);
    }

    vec[vector_length(vec)].object = value;
    vector_set_size(vec, vector_length(vec) + 1);
}
