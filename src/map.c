
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

static void map_add_pair(map_t hash, kv_pair_t *kv) {
    map_item_t *item;
    item = (map_item_t *)try_calloc(1, sizeof(map_item_t));
    item->type = kv->type;
    item->indic++;
    item->key = kv->key;
    item->value = *kv->extended;
    item->prev = hash->tail;
    item->next = nullptr;

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
    kv_pair_t *kv;
    void_t has;
    string k;
    int i;

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
                    kv = (kv_pair_t *)hash_put(hash->dict, k, va_arg(ap, void_t));
                }

                map_add_pair(hash, kv);
            } else {
                for (item = hash->head; item; item = item->next) {
                    if (item->value.char_ptr == ((values_type *)has)->char_ptr) {
                        kv = (kv_pair_t *)hash_replace(hash->dict, k, va_arg(ap, void_t));
                        item->value = *kv->extended;
                        item->type = kv->type;
                        break;
                    }
                }
            }
        }
        va_end(ap);
    }

    return hash;
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
        //if (hash->slice != nullptr)
        //    slice_free(hash);

        ZE_FREE(hash);
    }
}

u32 map_push(map_t hash, void_t value) {
    char hash_key[SCRAPE_SIZE] = {0};
    map_item_t *item;
    kv_pair_t *kv;

    item = (map_item_t *)try_calloc(1, sizeof(map_item_t));
    item->indic++;
    simd_itoa(item->indic, hash_key);
    kv = (kv_pair_t *)hash_put(hash->dict, hash_key, value);
    item->type = kv->type;
    item->key = kv->key;
    item->value = *kv->extended;
    item->prev = hash->tail;
    item->next = nullptr;

    hash->tail = item;
    hash->length++;

    if (!hash->head)
        hash->head = item;
    else
        item->prev->next = item;

    return item->indic;
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
    kv_pair_t *kv;

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
    kv = (kv_pair_t *)hash_put(hash->dict, hash_key, value);
    item->key = kv->key;
    item->value = *kv->extended;
    item->type = kv->type;

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
    kv_pair_t *kv;
    void_t has = hash_get(hash->dict, key);
    if (is_empty(has)) {
        kv_pair_t *kv = (kv_pair_t *)hash_put(hash->dict, key, value);
        map_add_pair(hash, kv);
    } else {
        for (item = hash->head; item; item = item->next) {
            if (item->value.char_ptr == ((values_type *)has)->char_ptr) {
                kv = (kv_pair_t *)hash_replace(hash->dict, key, value);
                item->value = *kv->extended;
                item->type = kv->type;
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
        iterator->type = RAII_MAP_ITER;
        iterator->hash = hash;
        iterator->item = forward ? hash->head : hash->tail;
        iterator->forward = forward;

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