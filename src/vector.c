#include "raii.h"

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

void vector_delete(vector_t v, int index) {
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

RAII_INLINE void vector_free(vector_t v) {
    if (is_type(v, RAII_VECTOR)) {
        memory_t *scope = v->scope;
        RAII_FREE(v->items);
        v->type = -1;
        raii_delete(scope);
    }
}