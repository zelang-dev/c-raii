#ifndef _MAP_H_
#define _MAP_H_

#include "hashtable.h"

typedef _map_t *map_t;
typedef map_t slice_t;

C_API map_t maps(void);
C_API map_t map_create(void);
C_API map_t map_for(u32 num_of_pairs, ...);
C_API values_type map_get(map_t, string_t);
C_API void map_put(map_t, string_t, void_t);
C_API map_t map_insert(map_t, ...);
C_API values_type map_pop(map_t hash);
C_API u32 map_push(map_t hash, void_t value);
C_API u32 map_shift(map_t hash, void_t value);
C_API values_type map_unshift(map_t hash);
C_API void map_free(map_t);
C_API void_t map_remove(map_t, void_t);
C_API void_t map_delete(map_t, string_t);
C_API size_t map_count(map_t);

C_API map_iter_t *iter_create(map_t, bool forward);
C_API map_iter_t *iter_next(map_iter_t *iterator);
C_API map_iter_t *iter_remove(map_iter_t *iterator);
C_API string_t iter_key(map_iter_t *iterator);
C_API values_type iter_value(map_iter_t *iterator);
C_API raii_type iter_type(map_iter_t *iterator);

#define kv_object(key, value) RAII_OBJ, kv(key, (void_t)(value))
#define kv_func(key, value) RAII_FUNC, kv(key, (raii_func_args_t)(value))
#define kv_string(key, value) RAII_STRING, kv(key, (string)(value))
#define kv_short(key, value) RAII_SHORT, kv(key, (short)(value))
#define kv_char(key, value) RAII_CHAR, kv(key, (char)(value))
#define kv_bool(key, value) RAII_BOOL, kv(key, (bool)(value))
#define kv_signed(key, value) RAII_LLONG, kv(key, (int64_t)(value))
#define kv_unsigned(key, value) RAII_MAXSIZE, kv(key, (size_t)(value))
#define kv_double(key, value) RAII_DOUBLE, kv(key, (double)(value))
#define indic(X) iter_key(X)
#define has(X) iter_value(X)
#define foreach_in_map(X, S)    map_iter_t *(X), *i##X = iter_create((map_t)(S), true);  \
    for(X = i##X; X != nullptr; X = iter_next(X))
#define foreach_map(...) foreach_xp(foreach_in_map, (__VA_ARGS__))

#endif
