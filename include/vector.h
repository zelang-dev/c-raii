#ifndef _VECTOR_H
#define _VECTOR_H

#include "swar.h"

#ifdef __cplusplus
extern "C" {
#endif

C_API vectors_t vector_variant(void);
C_API vectors_t vector_for(vectors_t, size_t, ...);
C_API void vector_insert(vectors_t, size_t, void_t);
C_API void vector_clear(vectors_t);
C_API void vector_erase(vectors_t, size_t);
C_API size_t vector_size(vectors_t);
C_API size_t vector_capacity(vectors_t);
C_API memory_t *vector_scope(vectors_t);
C_API void vector_push_back(vectors_t, void_t);
C_API vectors_t vector_copy(memory_t *, vectors_t, vectors_t);
C_API bool is_vector(void_t);

/**
* Creates an scoped `vector/array/container` for arbitrary arguments passing
* into an single `paramater` function.
* - Use standard `array access` for retrieval of an `union` storage type.
*
* - MUST CALL `args_destructor_set()` to have memory auto released
*   within ~callers~ scoped `context`, will happen either at return/exist or panics.
*
* - OTHERWISE `memory leak` will be shown in DEBUG build.
*
* NOTE: `vector_for` does auto memory cleanup.
*
* @param count numbers of parameters, `0` will create empty `vector/array`.
* @param arguments indexed in given order.
*/
C_API args_t args_for(size_t, ...);
C_API args_t args_ex(size_t, va_list);
C_API void args_destructor_set(args_t);
C_API bool is_args(void_t);

/**
* Creates an scoped `vector/array/container` for arbitrary item types.
*
* - Use standard `array access` for retrieval of an `union` storage type.
*
* - MUST CALL `array_deferred_set` to have memory auto released
*   when given `scope` context return/exist or panics.
*
* - OTHERWISE `memory leak` will be shown in DEBUG build.
*
* @param count numbers of parameters, `0` will create empty `vector/array`.
* @param arguments indexed in given order.
*/
C_API arrays_t array_of(memory_t *, size_t, ...);
C_API arrays_t array_ex(memory_t *, size_t, va_list);
C_API arrays_t array_copy(arrays_t des, arrays_t src);
C_API void array_deferred_set(arrays_t, memory_t *);
C_API void array_append(arrays_t, void_t);
C_API value_t array_pop(arrays_t arr);
C_API value_t array_shift(arrays_t arr);
C_API void array_append_item(arrays_t arr, ...);
C_API void array_delete(arrays_t);
C_API void array_remove(arrays_t, size_t);
C_API void array_reset(arrays_t);
C_API bool is_array(void_t);

/* Returns a sequence of numbers, in a given range, this is same as `arrays_of`,
but `scoped` to either ~current~ call scope, `coroutine` or `thread` termination. */
C_API ranges_t range(int start, int stop);

/* Same as `range`, but increasing by given ~steps~. */
C_API ranges_t rangeing(int start, int stop, int steps);

/* Same as `range`, but return given ~string/text~, split as array of `char`. */
C_API ranges_t range_char(string_t text);

/* Returns `empty` array `scoped` to either ~current~ call scope,
`coroutine` or `thread` termination. */
C_API arrays_t arrays(void);

#define $append(arr, value) array_append((arrays_t)arr, (void_t)value)
#define $append_double(arr, value) array_append_item((arrays_t)arr, RAII_DOUBLE, (double)value)
#define $append_unsigned(arr, value) array_append_item((arrays_t)arr, RAII_MAXSIZE, (size_t)value)
#define $append_signed(arr, value) array_append_item((arrays_t)arr, RAII_LLONG, (int64_t)value)
#define $append_string(arr, value) array_append_item((arrays_t)arr, RAII_STRING, (string)value)
#define $append_func(arr, value) array_append_item((arrays_t)arr, RAII_FUNC, (raii_func_args_t)value)
#define $append_char(arr, value) array_append_item((arrays_t)arr, RAII_CHAR, (char)value)
#define $append_bool(arr, value) array_append_item((arrays_t)arr, RAII_BOOL, (bool)value)
#define $append_short(arr, value) array_append_item((arrays_t)arr, RAII_SHORT, (short)value)
#define $remove(arr, index) array_remove((arrays_t)arr, index)
#define $pop(arr) array_pop((arrays_t)arr)
#define $shift(arr) array_shift((arrays_t)arr)
#define $reset(arr) array_reset((arrays_t)arr)

C_API values_type get_arg(void_t);

#define vectorize(vec) vectors_t vec = vector_variant()
#define vector(vec, count, ...) vectors_t vec = vector_for(nullptr, count, __VA_ARGS__)

#define $push_back(vec, value) vector_push_back((vectors_t)vec, (void_t)value)
#define $insert(vec, index, value) vector_insert((vectors_t)vec, index, (void_t)value)
#define $clear(vec) vector_clear((vectors_t)vec)
#define $size(vec) vector_size((vectors_t)vec)
#define $capacity(vec) vector_capacity((vectors_t)vec)
#define $erase(vec, index) vector_erase((vectors_t)vec, index)

#define kv(key, value) (key), (value)
#define in ,
#define foreach_xp(X, A) X A
#define foreach_in(X, S) values_type X; int i##X;  \
    for (i##X = 0; i##X < (int)$size(S); i##X++)      \
        if ((X.object = S[i##X].object) || X.object == nullptr)
#define foreach_in_back(X, S) values_type X; int i##X; \
    for (i##X = (int)$size(S) - 1; i##X >= 0; i##X--)     \
        if ((X.object = S[i##X].object) || X.object == nullptr)

/* The `foreach(`item `in` vector/array`)` macro, similar to `C#`,
executes a statement or a block of statements for each element in
an instance of `vectors_t/args_t/arrays_t` */
#define foreach(...) foreach_xp(foreach_in, (__VA_ARGS__))
#define foreach_back(...) foreach_xp(foreach_in_back, (__VA_ARGS__))

#ifdef __cplusplus
    }
#endif
#endif /* _VECTOR_H */
