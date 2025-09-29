#ifndef _HASHTABLE_H_
#define _HASHTABLE_H_

#include "raii.h"

#define HASH_LOAD_FACTOR (0.75)
#define HASH_GROWTH_FACTOR (1<<2)
#ifndef HASH_INIT_CAPACITY
#   define HASH_INIT_CAPACITY (1<<10)
#endif

typedef struct key_ops_s {
    uint32_t(*hash)(const_t data);
    bool (*eq)(const_t data1, const_t data2, void_t arg);
    void_t(*cp)(const_t data, void_t arg);
    void (*free)(void_t data);
    void_t arg;
} key_ops_t;

typedef struct val_ops_s {
    bool (*eq)(const_t data1, const_t data2, void_t arg);
    void_t(*cp)(const_t data, void_t arg);
    void (*free)(void_t data);
    void_t arg;
} val_ops_t;

typedef void_t(*hash_iter_func)(void_t variable, string_t key, const_t value);
typedef void (*probe_func)(hash_t *htable, size_t *from_idx);
typedef void (*print_key)(const_t k);
typedef void (*print_val)(const_t v);

/* DJB2 string hashing */
C_API uint32_t djb2_hash(const_t data);
/* General index probing */
C_API void hash_lp_idx(hash_t *, size_t *idx);
/* General string compare */
C_API bool hash_string_eq(const_t, const_t, void_t arg);
/* General string copy */
C_API void_t hash_string_cp(const_t, void_t arg);

C_API hash_t *hashtable_init(key_ops_t key_ops, val_ops_t val_ops, probe_func probing, u32 cap);
C_API hash_t *hash_create(void);
C_API hash_t *hash_create_ex(u32);

C_API void_t hash_put(hash_t *, const_t key, const_t value);
C_API void_t hash_put_str(hash_t *htable, const_t key, string value);
C_API void_t hash_put_obj(hash_t *htable, const_t key, const_t value);

C_API hash_pair_t *insert_func(hash_t *htable, const_t key, raii_func_args_t value);
C_API hash_pair_t *insert_unsigned(hash_t *htable, const_t key, size_t value);
C_API hash_pair_t *insert_signed(hash_t *htable, const_t key, int64_t value);
C_API hash_pair_t *insert_double(hash_t *htable, const_t key, double value);
C_API hash_pair_t *insert_string(hash_t *htable, const_t key, string value);
C_API hash_pair_t *insert_bool(hash_t *htable, const_t key, bool value);
C_API hash_pair_t *insert_char(hash_t *htable, const_t key, char value);
C_API hash_pair_t *insert_short(hash_t *htable, const_t key, short value);

C_API hash_pair_t *hash_get_pair(hash_t *, const_t key);
C_API template_t *hash_get_value(hash_t *htable, const_t key);
C_API void_t hash_get(hash_t *, const_t key);
C_API void_t hash_iter(hash_t *, void_t variable, hash_iter_func func);
C_API void_t hash_replace(hash_t *, const_t key, const_t value);
C_API size_t hash_count(hash_t *);
C_API size_t hash_capacity(hash_t *);
C_API hash_pair_t *hash_buckets(hash_t *, u32 index);
C_API template_t hash_pair_value(hash_pair_t *);
C_API string_t hash_pair_key(hash_pair_t *);
C_API raii_type hash_pair_type(hash_pair_t *);
C_API bool hash_pair_is_null(hash_pair_t *);
C_API bool hash_has(hash_t *, const_t key);
C_API void hash_free(hash_t *);
C_API void hash_set_capacity(u32 buckets);
C_API void hash_delete(hash_t *, const_t key);
C_API void hash_print(hash_t *);
C_API void hash_print_custom(hash_t *, print_key, print_val);

C_API key_ops_t key_ops_string;
C_API val_ops_t val_ops_value;
C_API val_ops_t val_ops_string;

#endif
