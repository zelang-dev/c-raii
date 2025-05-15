
#include "map.h"
#include "reflection.h"

struct map_item_s {
    raii_type type;
    values_type value;
    u32 indic;
    string_t key;
    map_item_t *prev;
    map_item_t *next;
};

struct map_s {
    raii_type type;
    raii_type item_type;
    bool started;
    bool sliced;
    u32 indices;
    u32 num_slices;
    int64_t length;
    hash_t *dict;
    slice_t *slice;
    map_item_t *head;
    map_item_t *tail;
};

struct map_iterator_s {
    raii_type type;
    bool forward;
    map_t hash;
    map_item_t *item;
};

static void map_add_pair(map_t hash, hash_pair_t *kv) {
    map_item_t *item;
    item = (map_item_t *)try_calloc(1, sizeof(map_item_t));
    if (hash->item_type == RAII_MAP_ARR)
        item->indic = hash->indices;
    else
        item->indic = hash->indices++;

    item->key = hash_pair_key(kv);
    item->value = hash_pair_value(kv);
    item->prev = hash->tail;
    item->next = nullptr;
    item->type = hash_pair_type(kv);

    hash->tail = item;
    hash->length++;

    if (!hash->head)
        hash->head = item;
    else
        item->prev->next = item;
}

static map_t map_for_ex(map_t hash, u32 num_of_pairs, va_list ap_copy) {
    va_list ap;
    raii_type n = RAII_ERR;
    map_item_t *item;
    hash_pair_t *kv;
    void_t has;
    string k;
    u32 i;

    if (is_empty(hash))
        hash = map_create();

    if (num_of_pairs > 0) {
        hash->item_type = RAII_MAP;
        va_copy(ap, ap_copy);
        for (i = 0; i < num_of_pairs; i++) {
            n = va_arg(ap, raii_type);
            k = va_arg(ap, string);
            has = hash_get(hash->dict, k);
            if (is_empty(has)) {
                if (n == RAII_DOUBLE) {
                    kv = insert_double(hash->dict, k, va_arg(ap, double));
                } else if (n == RAII_LLONG) {
                    kv = insert_signed(hash->dict, k, va_arg(ap, int64_t));
                } else if (n == RAII_MAXSIZE) {
                    kv = insert_unsigned(hash->dict, k, va_arg(ap, size_t));
                } else if (n == RAII_FUNC) {
                    kv = insert_func(hash->dict, k, va_arg(ap, raii_func_args_t));
                } else if (n == RAII_SHORT) {
                    kv = insert_short(hash->dict, k, va_arg(ap, int));
                } else if (n == RAII_BOOL) {
                    kv = insert_bool(hash->dict, k, va_arg(ap, int));
                } else if (n == RAII_CHAR) {
                    kv = insert_char(hash->dict, k, va_arg(ap, int));
                } else if (n == RAII_STRING) {
                    kv = insert_string(hash->dict, k, va_arg(ap, string));
                } else {
                    kv = (hash_pair_t *)hash_put(hash->dict, k, va_arg(ap, void_t));
                }

                map_add_pair(hash, kv);
            } else {
                for (item = hash->head; item; item = item->next) {
                    if (item->value.char_ptr == ((values_type *)has)->char_ptr) {
                        kv = (hash_pair_t *)hash_replace(hash->dict, k, va_arg(ap, void_t));
                        item->value = hash_pair_value(kv);
                        item->type = hash_pair_type(kv);
                        break;
                    }
                }
            }
        }
        va_end(ap);
    }

    return hash;
}

static void slice_free(slice_t array) {
    map_item_t *tmp, *next;
    slice_t each;
    u32 i;

    if (is_empty(array))
        return;

    for (i = 0; i <= array->num_slices; i++) {
        each = array->slice[array->num_slices - i];
        if (each) {
            while (each->head) {
                next = each->head->next;
                tmp = each->head;
                ZE_FREE(tmp);
                each->head = next;
            }
            ZE_FREE(each);
        }
    }

    ZE_FREE(array->slice);
}

static void slice_set(slice_t array, hash_pair_t *kv, int64_t index) {
    if (!is_empty(kv)) {
        struct map_item_s *item = (struct map_item_s *)try_calloc(1, sizeof(struct map_item_s));
        item->indic = index;
        item->key = hash_pair_key(kv);
        item->value = hash_pair_value(kv);
        item->prev = array->tail;
        item->next = nullptr;
        item->type = hash_pair_type(kv);

        array->tail = item;
        array->length++;

        if (!array->head)
            array->head = item;
        else
            item->prev->next = item;
    }
}

slice_t slice(map_array_t array, int64_t start, int64_t end) {
    char hash_key[SCRAPE_SIZE] = {0};
    if (array->item_type != RAII_MAP_ARR)
        raii_panic("slice() only accept `map_array_t` type!");

    if (array->num_slices % 64 == 0) {
        array->slice = ZE_REALLOC(array->slice, (array->num_slices + 64) * sizeof(array->slice[0]));
        if (array->slice == nullptr)
            raii_panic("realloc() failed");
    }

    slice_t slice = (slice_t)try_calloc(1, sizeof(_map_t));
    int64_t i, index = 0;
    for (i = start; i < end; i++) {
        simd_itoa(i, hash_key);
        slice_set(slice, hash_get_pair(array->dict, hash_key), index);
        index++;
    }

    slice->sliced = true;
    slice->type = array->type;
    slice->dict = array->dict;
    slice->item_type = array->item_type;
    array->slice[array->num_slices++] = slice;
    array->slice[array->num_slices] = nullptr;

    return slice;
}

static string_t slice_find(map_array_t array, int64_t index) {
    struct map_item_s *item;
    if (is_empty(array) || !array->sliced)
        return nullptr;

    for (item = array->head; item != nullptr; item = item->next) {
        if (item->indic == index)
            return item->key;
    }

    return nullptr;
}

RAII_INLINE void slice_put(slice_t hash, int64_t index, void_t value) {
    map_put(hash, slice_find(hash, index), value);
}

RAII_INLINE values_type slice_get(slice_t hash, int64_t index) {
    return map_get(hash, slice_find(hash, index));
}

RAII_INLINE void_t slice_delete(slice_t hash, int64_t index) {
    return map_delete(hash, slice_find(hash, index));
}

map_t map_create(void) {
    map_t hash = (map_t)try_malloc(sizeof(struct map_s));
    hash->started = false;
    hash->dict = hash_create_ex(256);
    hash->type = RAII_MAP_STRUCT;
    return hash;
}

RAII_INLINE map_t maps(void) {
    map_t hash = map_create();
    deferring((func_t)map_free, hash);

    return hash;
}

map_t map_for(u32 num_of_pairs, ...) {
    va_list argp;
    map_t hash;

    va_start(argp, num_of_pairs);
    hash = map_for_ex(nullptr, num_of_pairs, argp);
    va_end(argp);

    return hash;
}

static void map_append(map_array_t hash, array_type type, void_t value) {
    char k[SCRAPE_SIZE] = {0};
    hash_pair_t *kv;

    if (!hash->started) {
        hash->started = true;
        hash->indices = 0;
    } else {
        hash->indices++;
    }

    simd_itoa(hash->indices, k);
    if (type == RAII_DOUBLE || type == RAII_FLOAT) {
        kv = insert_double(hash->dict, k, *(double *)&value);
    } else if (type == RAII_LLONG || type == RAII_LONG || type == RAII_INT) {
        kv = insert_signed(hash->dict, k, *(int64_t *)&value);
    } else if (type == RAII_MAXSIZE) {
        kv = insert_unsigned(hash->dict, k, *(size_t *)&value);
    } else if (type == RAII_FUNC) {
        kv = insert_func(hash->dict, k, (raii_func_args_t)value);
    } else if (type == RAII_SHORT) {
        kv = insert_short(hash->dict, k, *(short *)&value);
    } else if (type == RAII_BOOL) {
        kv = insert_bool(hash->dict, k, *(bool *)&value);
    } else if (type == RAII_CHAR) {
        kv = insert_char(hash->dict, k, *(char *)&value);
    } else if (type == RAII_STRING) {
        kv = insert_string(hash->dict, k, (string)value);
    } else {
        kv = (hash_pair_t *)hash_put(hash->dict, k, value);
    }

    map_add_pair(hash, kv);
}

map_array_t map_array(array_type type, u32 num_of_items, ...) {
    map_array_t array = maps();
    va_list argp;
    u32 i;

    array->num_slices = 0;
    array->item_type = RAII_MAP_ARR;
    va_start(argp, num_of_items);
    for (i = 0; i < num_of_items; i++)
        map_append(array, type, va_arg(argp, void_t));
    va_end(argp);

    return array;
}

void map_free(map_t hash) {
    map_item_t *next;

    if (!hash)
        return;

    if (is_type(hash, RAII_MAP_STRUCT)) {
        hash->type = RAII_ERR;
        while (hash->head) {
            next = hash->head->next;
            ZE_FREE(hash->head);
            hash->head = next;
        }

        hash_free(hash->dict);
        if (!is_empty(hash->slice))
            slice_free(hash);

        ZE_FREE(hash);
    }
}

void map_push(map_t hash, void_t value) {
    char hash_key[SCRAPE_SIZE] = {0};
    hash_pair_t *kv;

    if (!hash->started) {
        hash->started = true;
        hash->indices = 0;
    } else {
        hash->indices++;
    }

    simd_itoa(hash->indices, hash_key);
    kv = (hash_pair_t *)hash_put(hash->dict, hash_key, value);
    map_add_pair(hash, kv);
}

values_type map_pop(map_t hash) {
    values_type value;
    map_item_t *item;

    if (!hash || !hash->tail)
        return raii_values_empty->value;

    item = hash->tail;
    hash->tail = hash->tail->prev;
    hash->length--;

    if (hash->length == 0)
        hash->head = nullptr;

    value = item->value;
    ZE_FREE(item);

    return value;
}

u32 map_shift(map_t hash, void_t value) {
    char hash_key[SCRAPE_SIZE] = {0};
    map_item_t *item;
    hash_pair_t *kv;

    if (!hash)
        return;

    item = (map_item_t *)try_calloc(1, sizeof(map_item_t));
    item->type = RAII_MAP_VALUE;
    item->prev = nullptr;
    item->next = hash->head;
    if (hash->head == nullptr)
        item->indic = 0;
    else
        item->indic = --item->next->indic;

    hash->head = item;
    hash->length++;

    if (!hash->tail)
        hash->tail = item;
    else
        item->next->prev = item;

    simd_itoa(item->indic, hash_key);
    kv = (hash_pair_t *)hash_put(hash->dict, hash_key, value);
    item->key = hash_pair_key(kv);
    item->value = hash_pair_value(kv);
    item->type = hash_pair_type(kv);

    return item->indic;
}

values_type map_unshift(map_t hash) {
    values_type value;
    map_item_t *item;

    if (!hash || !hash->head)
        return raii_values_empty->value;

    item = hash->head;
    hash->head = hash->head->next;
    hash->length--;

    if (hash->length == 0)
        hash->tail = nullptr;

    value = item->value;
    ZE_FREE(item);

    return value;
}

RAII_INLINE size_t map_count(map_t hash) {
    if (hash)
        return hash->length;

    return 0;
}

void_t map_remove(map_t hash, void_t value) {
    map_item_t *item;

    if (!hash)
        return nullptr;

    for (item = hash->head; item != nullptr; item = item->next) {
        if (is_equal(item->value.object, value)) {
            hash_delete(hash->dict, item->key);
            if (item->prev)
                item->prev->next = item->next;
            else
                hash->head = item->next;

            if (item->next)
                item->next->prev = item->prev;
            else
                hash->tail = item->prev;

            ZE_FREE(item);
            hash->length--;

            return value;
        }
    }

    return nullptr;
}

RAII_INLINE void_t map_delete(map_t hash, string_t key) {
    if (!hash)
        return nullptr;

    return map_remove(hash, hash_get(hash->dict, key));
}

RAII_INLINE values_type map_get(map_t hash, string_t key) {
    return raii_value(hash_get(hash->dict, key));
}

RAII_INLINE void map_put(map_t hash, string_t key, void_t value) {
    map_item_t *item;
    hash_pair_t *kv;
    void_t has = hash_get(hash->dict, key);
    if (is_empty(has)) {
        kv = (hash_pair_t *)hash_put(hash->dict, key, value);
        map_add_pair(hash, kv);
    } else {
        for (item = hash->head; item; item = item->next) {
            if (item->value.char_ptr == ((values_type *)has)->char_ptr) {
                kv = (hash_pair_t *)hash_replace(hash->dict, key, value);
                item->value = hash_pair_value(kv);
                item->type = hash_pair_type(kv);
                break;
            }
        }
    }
}

map_t map_insert(map_t hash, ...) {
    va_list ap;
    va_start(ap, hash);
    hash = map_for_ex(hash, 1, ap);
    va_end(ap);

    return hash;
}

map_iter_t *iter_create(map_t hash, bool forward) {
    if (hash && hash->head) {
        map_iter_t *iterator;

        iterator = (map_iter_t *)try_calloc(1, sizeof(map_iter_t));
        iterator->hash = hash;
        iterator->item = forward ? hash->head : hash->tail;
        iterator->forward = forward;
        iterator->type = RAII_MAP_ITER;

        return iterator;
    }

    return nullptr;
}

map_iter_t *iter_next(map_iter_t *iterator) {
    if (iterator) {
        map_item_t *item;

        item = iterator->forward ? iterator->item->next : iterator->item->prev;
        if (item) {
            iterator->item = item;
            return iterator;
        } else {
            ZE_FREE(iterator);
            return nullptr;
        }
    }

    return nullptr;
}

RAII_INLINE values_type iter_value(map_iter_t *iterator) {
    if (iterator)
        return iterator->item->value;

    return raii_values_empty->value;
}

RAII_INLINE raii_type iter_type(map_iter_t *iterator) {
    if (iterator)
        return iterator->item->type;

    return RAII_INVALID;
}

RAII_INLINE string_t iter_key(map_iter_t *iterator) {
    if (iterator)
        return iterator->item->key;

    return nullptr;
}

map_iter_t *iter_remove(map_iter_t *iterator) {
    map_item_t *item;

    if (!iterator)
        return nullptr;

    item = iterator->forward ? iterator->hash->head : iterator->hash->tail;
    while (item) {
        if (iterator->item == item) {
            if (iterator->hash->head == item)
                iterator->hash->head = item->next;
            else
                item->prev->next = item->next;

            if (iterator->hash->tail == item)
                iterator->hash->tail = item->prev;
            else
                item->next->prev = item->prev;

            iterator->hash->length--;

            iterator->item = iterator->forward ? item->next : item->prev;
            ZE_FREE(item);
            if (iterator->item) {
                return iterator;
            } else {
                ZE_FREE(iterator);
                return nullptr;
            }
        }

        item = iterator->forward ? item->next : item->prev;
    }

    return iterator;
}

reflect_func(map_item_t,
             (UNION, values_type, value),
             (UINT, u32, indic),
             (CONST_CHAR, string_t, key),
             (STRUCT, map_item_t *, prev),
             (STRUCT, map_item_t *, next)
)

reflect_func(_map_t,
             (ENUM, raii_type, item_type),
             (BOOL, bool, started),
             (BOOL, bool, sliced),
             (UINT, u32, indices),
             (UINT, u32, num_slices),
             (LLONG, int64_t, length),
             (STRUCT, hash_t *, dict),
             (STRUCT, slice_t *, slice),
             (STRUCT, map_item_t *, head),
             (STRUCT, map_item_t *, tail)
)
reflect_alias(_map_t)

reflect_func(map_iter_t,
             (BOOL, bool, forward),
             (STRUCT, map_t, hash),
             (STRUCT, map_item_t *, item)
)