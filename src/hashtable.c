/*
A `atomic` wait free open addressing hash table implemented in C.

Code associated with the following article:
https://www.andreinc.net/2021/10/02/implementing-hash-tables-in-c-part-1

Modified from https://github.com/nomemory/open-adressing-hash-table-c
*/
#include "hashtable.h"

struct hash_pair_s {
    raii_type type;
    uint32_t hash;
    void_t key;
    void_t value;
    template_t *extended;
};

make_atomic(hash_pair_t, atomic_hash_pair_t)
struct hash_s {
    raii_type type;
    bool overriden;
    bool has_erred;
    key_ops_t key_ops;
    val_ops_t val_ops;
    probe_func probing;
    cacheline_pad_t pad;
    atomic_size_t capacity;
    atomic_size_t size;
    atomic_hash_pair_t **buckets;
};

// Pair related
static hash_pair_t *pair_create(uint32_t hash, const_t key, const_t value, raii_type op);
static hash_pair_t *hash_operation(hash_t *, const_t key, const_t value, raii_type op);
static void pair_free(hash_pair_t *pair);

enum ret_ops {
    DEL,
    PUT,
    GET
};

static size_t hash_getidx(hash_t *htable, size_t idx, uint32_t hash_val, const_t key, enum ret_ops op);
static RAII_INLINE void hash_grow(hash_t *htable);
static RAII_INLINE bool hash_should_grow(hash_t *htable);
static RAII_INLINE bool hash_is_tombstone(hash_t *htable, size_t idx);
static RAII_INLINE void hash_put_tombstone(hash_t *htable, size_t idx);

static u32 hash_initial_capacity = HASH_INIT_CAPACITY;
static bool hash_initial_override = false;
key_ops_t key_ops_string = {djb2_hash, hash_string_eq, hash_string_cp, RAII_FREE, nullptr};
val_ops_t val_ops_string = {hash_string_eq, hash_string_cp, RAII_FREE, nullptr};

hash_t *hashtable_init(key_ops_t key_ops, val_ops_t val_ops, probe_func probing, u32 cap) {
    hash_t *htable = try_calloc(1, sizeof(*htable));
    u32 capacity = is_zero(cap) ? hash_initial_capacity : cap;
    atomic_init(&htable->size, 0);
    atomic_init(&htable->capacity, capacity);
    htable->overriden = !is_zero(cap);
    htable->has_erred = false;
    htable->val_ops = val_ops;
    htable->key_ops = key_ops;
    htable->probing = probing;
    atomic_init(&htable->buckets, try_calloc(1, sizeof(hash_pair_t *) * capacity));
    htable->type = RAII_HASH;

    return htable;
}

void pair_free(hash_pair_t *pair) {
    if (!is_empty(pair) && is_valid(pair)) {
        if (pair->type == RAII_CONST_CHAR) {
            void_t data = pair->extended->char_ptr;
            RAII_FREE(data);
        }

        pair->type = RAII_ERR;
        RAII_FREE(pair->extended);
        RAII_FREE(pair);
    } else if (!is_empty(pair) && is_type(pair, RAII_NULL)) {
        RAII_FREE(pair);
    }
}

void hash_free(hash_t *htable) {
    if (is_type(htable, RAII_HASH)) {
        u32 i, capacity = atomic_load(&htable->capacity);
        hash_pair_t **buckets = (hash_pair_t **)atomic_load_explicit(&htable->buckets, memory_order_consume);
        for (i = 0; i < capacity; i++) {
            if (buckets[i] && buckets[i]->key)
                if (buckets[i]) {
                    if (buckets[i]->key)
                        htable->key_ops.free(buckets[i]->key);

                    if (!is_empty(buckets[i]->value))
                        htable->val_ops.free(buckets[i]->value);
                }

            pair_free(buckets[i]);
        }

        if (buckets)
            RAII_FREE(buckets);

        memset(htable, RAII_ERR, sizeof(raii_type));
        RAII_FREE(htable);
    }
}

static RAII_INLINE void hash_grow(hash_t *htable) {
    u32 i, old_capacity;
    hash_pair_t **old_buckets;
    hash_pair_t *crt_pair;

    atomic_thread_fence(memory_order_acquire);
    old_capacity = atomic_load_explicit(&htable->capacity, memory_order_consume);
    uint64_t new_capacity_64 = old_capacity * HASH_GROWTH_FACTOR;
    if (new_capacity_64 > SIZE_MAX)
        raii_panic("re-size overflow");

    old_buckets = (hash_pair_t **)atomic_load_explicit(&htable->buckets, memory_order_consume);
    atomic_init(&htable->capacity, (size_t)new_capacity_64);
    atomic_init(&htable->size, 0);
    atomic_init(&htable->buckets, try_calloc(1, new_capacity_64 * sizeof(*(old_buckets))));
    for (i = 0; i < old_capacity; i++) {
        crt_pair = old_buckets[i];
        if (!is_empty(crt_pair) && !hash_is_tombstone(htable, i)) {
            hash_operation(htable, crt_pair->key, crt_pair->value, crt_pair->type);
            htable->val_ops.free(crt_pair->value);
            htable->key_ops.free(crt_pair->key);
            pair_free(crt_pair);
        }
    }

    RAII_FREE(old_buckets);
    atomic_thread_fence(memory_order_release);
}

static RAII_INLINE bool hash_should_grow(hash_t *htable) {
    return (atomic_load(&htable->size) / atomic_load(&htable->capacity)) > (htable->overriden ? .95 : HASH_LOAD_FACTOR);
}

hash_pair_t *hash_operation(hash_t *hash, const_t key, const_t value, raii_type op) {
    if (hash_should_grow(hash))
        hash_grow(hash);

    uint32_t hash_val = hash->key_ops.hash(key);
    size_t idx = hash_val % (u32)atomic_load_explicit(&hash->capacity, memory_order_relaxed);

    hash_pair_t **buckets = (hash_pair_t **)atomic_load_explicit(&hash->buckets, memory_order_acquire);
    atomic_thread_fence(memory_order_seq_cst);
    if (is_empty(buckets[idx])) {
        // Key doesn't exist & we add it anew
        buckets[idx] = pair_create(
            hash_val,
            hash->key_ops.cp(key, hash->key_ops.arg),
            hash->val_ops.cp(value, hash->val_ops.arg),
            op);
    } else {
        // // Probing for the next good index
        idx = hash_getidx(hash, idx, hash_val, key, PUT);

        if (is_empty(buckets[idx])) {
            buckets[idx] = pair_create(
                hash_val,
                hash->key_ops.cp(key, hash->key_ops.arg),
                hash->val_ops.cp(value, hash->val_ops.arg),
                op);
        } else {
            // Update the existing value
            // Free the old values
            RAII_FREE(buckets[idx]->extended);
            if (buckets[idx]->type == RAII_PTR)
                hash->key_ops.free(buckets[idx]->value);

            // Update the new values
            buckets[idx]->type = op;
            if (op == RAII_STRING && simd_strlen((string)value) > (sizeof(template_t) - 1))
                buckets[idx]->type = RAII_CONST_CHAR;

            buckets[idx]->extended = value_create(hash->val_ops.cp(value, hash->val_ops.arg), op);
            buckets[idx]->value = buckets[idx]->extended;
            if (op == RAII_PTR)
                buckets[idx]->value = buckets[idx]->extended->object;

            buckets[idx]->hash = hash_val;
            atomic_fetch_sub(&hash->size, 1);
        }
    }

    atomic_store_explicit(&hash->buckets, buckets, memory_order_release);
    atomic_fetch_add(&hash->size, 1);
    return buckets[idx];
}

RAII_INLINE void_t hash_put(hash_t *htable, const_t key, const_t value) {
    return (void_t)hash_operation(htable, key, value, RAII_OBJ);
}

RAII_INLINE void_t hash_put_str(hash_t *htable, const_t key, string value) {
    return hash_operation(htable, key, value, RAII_PTR);
}

RAII_INLINE void_t hash_put_obj(hash_t *htable, const_t key, const_t value) {
    return hash_operation(htable, key, value, RAII_PTR);
}

RAII_INLINE hash_pair_t *insert_func(hash_t *htable, const_t key, raii_func_args_t value) {
    return hash_operation(htable, key, value, RAII_FUNC);
}

RAII_INLINE hash_pair_t *insert_unsigned(hash_t *htable, const_t key, size_t value) {
    return hash_operation(htable, key, &value, RAII_MAXSIZE);
}

RAII_INLINE hash_pair_t *insert_signed(hash_t *htable, const_t key, int64_t value) {
    return hash_operation(htable, key, &value, RAII_LLONG);
}

RAII_INLINE hash_pair_t *insert_double(hash_t *htable, const_t key, double value) {
    return hash_operation(htable, key, &value, RAII_DOUBLE);
}

RAII_INLINE hash_pair_t *insert_string(hash_t *htable, const_t key, string value) {
    return hash_operation(htable, key, value, RAII_STRING);
}

RAII_INLINE hash_pair_t *insert_bool(hash_t *htable, const_t key, bool value) {
    return hash_operation(htable, key, &value, RAII_BOOL);
}

RAII_INLINE hash_pair_t *insert_char(hash_t *htable, const_t key, char value) {
    return hash_operation(htable, key, &value, RAII_CHAR);
}

RAII_INLINE hash_pair_t *insert_short(hash_t *htable, const_t key, short value) {
    return hash_operation(htable, key, &value, RAII_SHORT);
}

void_t hash_replace(hash_t *htable, const_t key, const_t value) {
    if (hash_should_grow(htable))
        hash_grow(htable);

    uint32_t hash_val = htable->key_ops.hash(key);
    size_t idx = hash_val % (u32)atomic_load(&htable->capacity);

    // // Probing for the next good index
    idx = hash_getidx(htable, idx, hash_val, key, PUT);

    hash_pair_t **buckets = (hash_pair_t **)atomic_load_explicit(&htable->buckets, memory_order_acquire);
    atomic_thread_fence(memory_order_seq_cst);
    // Update the new values
    if (buckets[idx]->type == RAII_CONST_CHAR) {
        buckets[idx]->type = RAII_OBJ;
        void_t data = buckets[idx]->extended->char_ptr;
        RAII_FREE(data);
    }

    if (buckets[idx]->type == RAII_PTR)
        htable->key_ops.free(buckets[idx]->value);

    buckets[idx]->extended->object = (void_t)value;
    buckets[idx]->value = buckets[idx]->extended;
    atomic_store_explicit(&htable->buckets, buckets, memory_order_release);
    return buckets[idx];
}

static RAII_INLINE bool hash_is_tombstone(hash_t *htable, size_t idx) {
    hash_pair_t *buckets = (hash_pair_t *)atomic_load(&htable->buckets[idx]);
    if (is_empty(buckets))
        return false;

    if (is_empty(buckets->key) && is_empty(buckets->value) && 0 == buckets->hash)
        return true;

    return false;
}

static RAII_INLINE void hash_put_tombstone(hash_t *htable, size_t idx) {
    if (!is_empty(atomic_get(void_t, &htable->buckets[idx]))) {
        hash_pair_t **buckets = (hash_pair_t **)atomic_load_explicit(&htable->buckets, memory_order_acquire);
        atomic_thread_fence(memory_order_seq_cst);
        buckets[idx]->hash = 0;
        buckets[idx]->key = nullptr;
        buckets[idx]->value = nullptr;
        buckets[idx]->type = RAII_NULL;
        atomic_store_explicit(&htable->buckets, buckets, memory_order_release);
    }
}

void_t hash_get(hash_t *htable, const_t key) {
    uint32_t hash_val = htable->key_ops.hash(key);
    size_t idx = hash_val % (u32)atomic_load(&htable->capacity);

    if (is_empty((void_t)atomic_load(&htable->buckets[idx])))
        return nullptr;

    idx = hash_getidx(htable, idx, hash_val, key, GET);

    return is_empty(atomic_get(void_t, &htable->buckets[idx]))
        ? nullptr
        : (atomic_get(hash_pair_t *, &htable->buckets[idx]))->value;
}

hash_pair_t *hash_get_pair(hash_t *htable, const_t key) {
    uint32_t hash_val = htable->key_ops.hash(key);
    size_t idx = hash_val % (u32)atomic_load(&htable->capacity);

    if (is_empty((void_t)atomic_load(&htable->buckets[idx])))
        return nullptr;

    idx = hash_getidx(htable, idx, hash_val, key, GET);

    return is_empty(atomic_get(void_t, &htable->buckets[idx]))
        ? nullptr
        : (atomic_get(hash_pair_t *, &htable->buckets[idx]));
}

RAII_INLINE bool hash_pair_is_null(hash_pair_t *pair) {
    return is_empty(pair) || is_empty(pair->extended) || is_empty(pair->extended->object);
}

RAII_INLINE template_t hash_pair_value(hash_pair_t *pair) {
    if (!hash_pair_is_null(pair))
        return *pair->extended;

    return raii_values_empty->value;
}

RAII_INLINE string_t hash_pair_key(hash_pair_t *pair) {
    return (string_t)pair->key;
}

RAII_INLINE raii_type hash_pair_type(hash_pair_t *pair) {
    return pair->type;
}

bool hash_has(hash_t *htable, const_t key) {
    uint32_t hash_val = htable->key_ops.hash(key);
    size_t idx = hash_val % (u32)atomic_load(&htable->capacity);

    if (is_empty((void_t)atomic_load(&htable->buckets[idx])))
        return false;

    idx = hash_getidx(htable, idx, hash_val, key, GET);

    return is_empty(atomic_get(void_t, &htable->buckets[idx])) ? false : true;
}

void hash_delete(hash_t *htable, const_t key) {
    uint32_t hash_val = htable->key_ops.hash(key);
    size_t idx = hash_val % (u32)atomic_load(&htable->capacity);

    if (is_empty((void_t)atomic_load(&htable->buckets[idx])))
        return;

    idx = hash_getidx(htable, idx, hash_val, key, DEL);
    if (is_empty(atomic_get(void_t, &htable->buckets[idx])))
        return;

    hash_pair_t **buckets = (hash_pair_t **)atomic_load_explicit(&htable->buckets, memory_order_acquire);
    atomic_thread_fence(memory_order_seq_cst);
    RAII_FREE(buckets[idx]->extended);
    htable->key_ops.free(buckets[idx]->key);
    if (buckets[idx]->type == RAII_PTR)
        htable->key_ops.free(buckets[idx]->value);

    atomic_store_explicit(&htable->buckets, buckets, memory_order_release);
    atomic_fetch_sub(&htable->size, 1);

    hash_put_tombstone(htable, idx);
}

void hash_printer(hash_t *htable, print_key k, print_val v) {
    hash_pair_t *pair;
    u32 i, capacity = (u32)atomic_load(&htable->capacity);

    printf("Hash Capacity: %zu\n", (size_t)capacity);
    printf("Hash Size: %zu\n", (size_t)atomic_load(&htable->size));

    printf("Hash Buckets:\n");
    hash_pair_t **buckets = (hash_pair_t **)atomic_load(&htable->buckets);
    for (i = 0; i < capacity; i++) {
        pair = buckets[i];
        if (!is_empty(pair)) {
            printf("\tbucket[%d]:\n", i);
            if (hash_is_tombstone(htable, i)) {
                printf("\t\t TOMBSTONE");
            } else {
                printf("\t\thash=%" PRIu32 ", key=", pair->hash);
                k(pair->key);
                printf(", value=");
                v(pair->value);
            }
            printf("\n");
        }
    }
}

void_t hash_iter(hash_t *htable, void_t variable, hash_iter_func func) {
    hash_pair_t *pair;
    u32 i, capacity = (u32)atomic_load(&htable->capacity);
    hash_pair_t **buckets = (hash_pair_t **)atomic_load(&htable->buckets);
    for (i = 0; i < capacity; i++) {
        pair = buckets[i];
        if (!is_empty(pair))
            variable = func(variable, pair->key, pair->value);
    }

    return variable;
}

static size_t hash_getidx(hash_t *htable, size_t idx, uint32_t hash_val,
                          const_t key, enum ret_ops op) {
    do {
        if (op == PUT && hash_is_tombstone(htable, idx))
            break;

        if ((atomic_get(hash_pair_t *, &htable->buckets[idx]))->hash == hash_val &&
            htable->key_ops.eq(key, (atomic_get(hash_pair_t *, &htable->buckets[idx]))->key, htable->key_ops.arg)) {
            break;
        }

        htable->probing(htable, &idx);
    } while (!is_empty(atomic_get(void_t, &htable->buckets[idx])));
    return idx;
}

hash_pair_t *pair_create(uint32_t hash, const_t key, const_t value, raii_type op) {
    hash_pair_t *p = try_calloc(1, sizeof(hash_pair_t));
    p->type = op;
    if (op == RAII_STRING && simd_strlen((string)value) > (sizeof(template_t) - 1))
        p->type = RAII_CONST_CHAR;

    p->extended = value_create(value, op);
    p->hash = hash;
    p->value = p->extended;
    if (op == RAII_PTR)
        p->value = p->extended->object;

    p->key = (void_t)key;

    return p;
}

RAII_INLINE void hash_lp_idx(hash_t *htable, size_t *idx) {
    (*idx)++;
    if ((*idx) == (size_t)atomic_load(&htable->capacity))
        (*idx) = 0;
}

RAII_INLINE bool hash_string_eq(const_t data1, const_t data2, void_t arg) {
    string_t str1 = (string_t)data1;
    string_t str2 = (string_t)data2;
    return !(strcmp(str1, str2)) ? true : false;
}

RAII_INLINE void_t hash_string_cp(const_t data, void_t arg) {
    string_t input = (string_t)data;
    size_t input_length = simd_strlen(input);
    char *result = try_calloc(1, sizeof(*result) + input_length + 1);

    str_copy(result, input, input_length);
    return result;
}

// String operations
static RAII_INLINE uint32_t hash_fmix32(uint32_t h) {
    h ^= h >> 16;
    h *= 0x3243f6a9U;
    h ^= h >> 16;
    return h;
}

RAII_INLINE uint32_t djb2_hash(const_t data) {
    // djb2
    uint32_t hash = (const uint32_t)5381;
    string_t str = (string_t)data;
    char c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash_fmix32(hash);
}

RAII_INLINE void string_print(const_t data) {
    printf("%s", (string_t)data);
}

static RAII_INLINE void_t plain_cp(const_t data, void_t arg) {
    return (void_t)data;
}

static RAII_INLINE bool plain_eq(const_t data1, const_t data2, void_t arg) {
    return memcmp(data1, data2, sizeof(data2)) == 0 ? true : false;
}

static RAII_INLINE void plain_free(void_t data) {
}

val_ops_t val_ops_value = {plain_eq, plain_cp, plain_free, nullptr};

RAII_INLINE hash_t *hash_create(void) {
    return (hash_t *)hashtable_init(key_ops_string, val_ops_value, hash_lp_idx, 0);
}

RAII_INLINE hash_t *hash_create_ex(u32 size) {
    return (hash_t *)hashtable_init(key_ops_string, val_ops_value, hash_lp_idx, size);
}

RAII_INLINE void hash_set_capacity(u32 buckets) {
    atomic_thread_fence(memory_order_seq_cst);
    gq_result.capacity = hash_initial_capacity = buckets;
    hash_initial_override = true;
}

RAII_INLINE size_t hash_count(hash_t *htable) {
    return (size_t)atomic_load_explicit(&htable->size, memory_order_relaxed);
}

RAII_INLINE size_t hash_capacity(hash_t *htable) {
    return (size_t)atomic_load_explicit(&htable->capacity, memory_order_relaxed);
}

RAII_INLINE hash_pair_t *hash_buckets(hash_t *htable, u32 index) {
    return (hash_pair_t *)atomic_load_explicit(&htable->buckets[index], memory_order_relaxed);
}

RAII_INLINE void hash_print(hash_t *htable) {
    hash_printer(htable, string_print, string_print);
}

RAII_INLINE void hash_print_custom(hash_t *htable, print_key k, print_val v) {
    hash_printer(htable, k, v);
}
