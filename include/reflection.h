#ifndef REFLECTION_H_
#define REFLECTION_H_

#include "raii.h"

/*
 * A reflection library for C
 *
 * Modified from: https://github.com/loganek/mkcreflect
 * ----------------------------------------------------------------------------
 */

#ifdef __cplusplus
extern "C" {
#endif

#undef interface
typedef raii_type reflect_types;
typedef void_t interface;
typedef struct reflect_value_s {
    reflect_types type;
    string_t value_type;
    string_t name;
    size_t size;
    size_t offset;
    bool is_signed;
    int array_size;
} reflect_value_t;

typedef struct reflect_kind_s {
    reflect_types type;
    void_t instance;
    string_t name;
    size_t num_fields;
    size_t size;
    size_t packed_size;
    reflect_value_t *fields;
} reflect_kind_t;

typedef struct reflect_field_s {
    reflect_types data_type;
    const char *field_type;
    const char *field_name;
    size_t size;
    size_t offset;
    bool is_signed;
    int array_size;
} reflect_field_t;

typedef struct reflect_type_s {
    reflect_types data_type;
    void *instance;
    const char *name;
    size_t fields_count;
    size_t size;
    size_t packed_size;
    reflect_field_t *fields;
} reflect_type_t;

#define RE_EXPAND_(X) X
#define RE_EXPAND_VA_(...) __VA_ARGS__
#define RE_FOREACH_1_(FNC, USER_DATA, ARG) FNC(ARG, USER_DATA)
#define RE_FOREACH_2_(FNC, USER_DATA, ARG, ...) \
    FNC(ARG, USER_DATA) RE_EXPAND_(RE_FOREACH_1_(FNC, USER_DATA, __VA_ARGS__))
#define RE_FOREACH_3_(FNC, USER_DATA, ARG, ...) \
    FNC(ARG, USER_DATA) RE_EXPAND_(RE_FOREACH_2_(FNC, USER_DATA, __VA_ARGS__))
#define RE_FOREACH_4_(FNC, USER_DATA, ARG, ...) \
    FNC(ARG, USER_DATA) RE_EXPAND_(RE_FOREACH_3_(FNC, USER_DATA, __VA_ARGS__))
#define RE_FOREACH_5_(FNC, USER_DATA, ARG, ...) \
    FNC(ARG, USER_DATA) RE_EXPAND_(RE_FOREACH_4_(FNC, USER_DATA, __VA_ARGS__))
#define RE_FOREACH_6_(FNC, USER_DATA, ARG, ...) \
    FNC(ARG, USER_DATA) RE_EXPAND_(RE_FOREACH_5_(FNC, USER_DATA, __VA_ARGS__))
#define RE_FOREACH_7_(FNC, USER_DATA, ARG, ...) \
    FNC(ARG, USER_DATA) RE_EXPAND_(RE_FOREACH_6_(FNC, USER_DATA, __VA_ARGS__))
#define RE_FOREACH_8_(FNC, USER_DATA, ARG, ...) \
    FNC(ARG, USER_DATA) RE_EXPAND_(RE_FOREACH_7_(FNC, USER_DATA, __VA_ARGS__))
#define RE_FOREACH_9_(FNC, USER_DATA, ARG, ...) \
    FNC(ARG, USER_DATA) RE_EXPAND_(RE_FOREACH_8_(FNC, USER_DATA, __VA_ARGS__))
#define RE_FOREACH_10_(FNC, USER_DATA, ARG, ...) \
    FNC(ARG, USER_DATA) RE_EXPAND_(RE_FOREACH_9_(FNC, USER_DATA, __VA_ARGS__))
#define RE_FOREACH_11_(FNC, USER_DATA, ARG, ...) \
    FNC(ARG, USER_DATA) RE_EXPAND_(RE_FOREACH_10_(FNC, USER_DATA, __VA_ARGS__))
#define RE_FOREACH_12_(FNC, USER_DATA, ARG, ...) \
    FNC(ARG, USER_DATA) RE_EXPAND_(RE_FOREACH_11_(FNC, USER_DATA, __VA_ARGS__))
#define RE_FOREACH_13_(FNC, USER_DATA, ARG, ...) \
    FNC(ARG, USER_DATA) RE_EXPAND_(RE_FOREACH_12_(FNC, USER_DATA, __VA_ARGS__))
#define RE_FOREACH_14_(FNC, USER_DATA, ARG, ...) \
    FNC(ARG, USER_DATA) RE_EXPAND_(RE_FOREACH_13_(FNC, USER_DATA, __VA_ARGS__))
#define RE_FOREACH_15_(FNC, USER_DATA, ARG, ...) \
    FNC(ARG, USER_DATA) RE_EXPAND_(RE_FOREACH_14_(FNC, USER_DATA, __VA_ARGS__))
#define RE_FOREACH_16_(FNC, USER_DATA, ARG, ...) \
    FNC(ARG, USER_DATA) RE_EXPAND_(RE_FOREACH_15_(FNC, USER_DATA, __VA_ARGS__))
#define RE_FOREACH_17_(FNC, USER_DATA, ARG, ...) \
    FNC(ARG, USER_DATA) RE_EXPAND_(RE_FOREACH_16_(FNC, USER_DATA, __VA_ARGS__))
#define RE_FOREACH_18_(FNC, USER_DATA, ARG, ...) \
    FNC(ARG, USER_DATA) RE_EXPAND_(RE_FOREACH_17_(FNC, USER_DATA, __VA_ARGS__))
#define RE_FOREACH_19_(FNC, USER_DATA, ARG, ...) \
    FNC(ARG, USER_DATA) RE_EXPAND_(RE_FOREACH_18_(FNC, USER_DATA, __VA_ARGS__))
#define RE_FOREACH_20_(FNC, USER_DATA, ARG, ...) \
    FNC(ARG, USER_DATA) RE_EXPAND_(RE_FOREACH_19_(FNC, USER_DATA, __VA_ARGS__))
#define RE_FOREACH_21_(FNC, USER_DATA, ARG, ...) \
    FNC(ARG, USER_DATA) RE_EXPAND_(RE_FOREACH_20_(FNC, USER_DATA, __VA_ARGS__))
#define RE_FOREACH_22_(FNC, USER_DATA, ARG, ...) \
    FNC(ARG, USER_DATA) RE_EXPAND_(RE_FOREACH_21_(FNC, USER_DATA, __VA_ARGS__))
#define RE_FOREACH_23_(FNC, USER_DATA, ARG, ...) \
    FNC(ARG, USER_DATA) RE_EXPAND_(RE_FOREACH_22_(FNC, USER_DATA, __VA_ARGS__))
#define RE_FOREACH_24_(FNC, USER_DATA, ARG, ...) \
    FNC(ARG, USER_DATA) RE_EXPAND_(RE_FOREACH_23_(FNC, USER_DATA, __VA_ARGS__))
#define RE_FOREACH_25_(FNC, USER_DATA, ARG, ...) \
    FNC(ARG, USER_DATA) RE_EXPAND_(RE_FOREACH_24_(FNC, USER_DATA, __VA_ARGS__))

#define RE_OVERRIDE_0(FNC, ...) FNC
#define RE_OVERRIDE_0_PLACEHOLDER
#define RE_OVERRIDE_1(_1, FNC, ...) FNC
#define RE_OVERRIDE_1_PLACEHOLDER RE_OVERRIDE_0_PLACEHOLDER, 1
#define RE_OVERRIDE_2(_1, _2, FNC, ...) FNC
#define RE_OVERRIDE_2_PLACEHOLDER RE_OVERRIDE_1_PLACEHOLDER, 2
#define RE_OVERRIDE_3(_1, _2, _3, FNC, ...) FNC
#define RE_OVERRIDE_3_PLACEHOLDER RE_OVERRIDE_2_PLACEHOLDER, 3
#define RE_OVERRIDE_4(_1, _2, _3, _4, FNC, ...) FNC
#define RE_OVERRIDE_4_PLACEHOLDER RE_OVERRIDE_3_PLACEHOLDER, 4
#define RE_OVERRIDE_5(_1, _2, _3, _4, _5, FNC, ...) FNC
#define RE_OVERRIDE_5_PLACEHOLDER RE_OVERRIDE_4_PLACEHOLDER, 5
#define RE_OVERRIDE_14(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, FNC, ...) FNC
#define RE_OVERRIDE_14_PLACEHOLDER RE_OVERRIDE_5_PLACEHOLDER, 6, 7, 8, 9, 10, 11, 12, 13, 14
#define RE_OVERRIDE_20(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, FNC, ...) FNC
#define RE_OVERRIDE_20_PLACEHOLDER RE_OVERRIDE_14_PLACEHOLDER, 15, 16, 17, 18, 19, 20
#define RE_OVERRIDE_25(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, FNC, ...) FNC
#define RE_OVERRIDE_25_PLACEHOLDER RE_OVERRIDE_20_PLACEHOLDER, 21, 22, 23, 24, 25

#define RE_FOREACH(FNC, USER_DATA, ...) \
    RE_EXPAND_(RE_OVERRIDE_25( __VA_ARGS__,	\
    RE_FOREACH_25_, \
    RE_FOREACH_24_, \
    RE_FOREACH_23_, \
    RE_FOREACH_22_, \
    RE_FOREACH_21_, \
    RE_FOREACH_20_, \
    RE_FOREACH_19_, \
    RE_FOREACH_18_, \
    RE_FOREACH_17_, \
    RE_FOREACH_16_, \
    RE_FOREACH_15_, \
    RE_FOREACH_14_, \
    RE_FOREACH_13_, \
    RE_FOREACH_12_, \
    RE_FOREACH_11_, \
    RE_FOREACH_10_, \
    RE_FOREACH_9_, \
    RE_FOREACH_8_, \
    RE_FOREACH_7_, \
    RE_FOREACH_6_, \
    RE_FOREACH_5_, \
    RE_FOREACH_4_, \
    RE_FOREACH_3_, \
    RE_FOREACH_2_, \
    RE_FOREACH_1_)(FNC, USER_DATA, __VA_ARGS__))

#define RE_DECLARE_SIMPLE_FIELD_(IGNORE, TYPE, FIELD_NAME) \
    TYPE FIELD_NAME;

#define RE_DECLARE_ARRAY_FIELD_(IGNORE, TYPE, FIELD_NAME, ARRAY_SIZE) \
    TYPE FIELD_NAME[ARRAY_SIZE];

#define RE_DECLARE_FIELD_(...) RE_EXPAND_(RE_OVERRIDE_4( \
    __VA_ARGS__, \
    RE_DECLARE_ARRAY_FIELD_, \
    RE_DECLARE_SIMPLE_FIELD_, \
    RE_OVERRIDE_4_PLACEHOLDER)(__VA_ARGS__))

#define RE_DECLARE_FIELD(X, USER_DATA) RE_DECLARE_FIELD_ X

#define RE_SIZEOF_(IGNORE, C_TYPE, ...) +sizeof(C_TYPE)
#define RE_SIZEOF(X, USER_DATA) RE_SIZEOF_ X

#define RE_SUM(...) +1

#define RE_IS_TYPE_SIGNED_(C_TYPE) (C_TYPE)-1 < (C_TYPE)1
#define RE_IS_SIGNED_FUNC(C_TYPE) 0
#define RE_IS_SIGNED_PTR(C_TYPE) 0
#define RE_IS_SIGNED_OBJ(C_TYPE) 0
#define RE_IS_SIGNED_STRUCT(C_TYPE) 0
#define RE_IS_SIGNED_UNION(C_TYPE) 0
#define RE_IS_SIGNED_CHAR_P(C_TYPE) RE_IS_TYPE_SIGNED_(C_TYPE)
#define RE_IS_SIGNED_UCHAR_P(C_TYPE) RE_IS_TYPE_SIGNED_(C_TYPE)
#define RE_IS_SIGNED_UCHAR(C_TYPE) RE_IS_TYPE_SIGNED_(C_TYPE)
#define RE_IS_SIGNED_BOOL(C_TYPE) RE_IS_TYPE_SIGNED_(C_TYPE)
#define RE_IS_SIGNED_MAXSIZE(C_TYPE) RE_IS_TYPE_SIGNED_(C_TYPE)
#define RE_IS_SIGNED_CONST_CHAR(C_TYPE) RE_IS_TYPE_SIGNED_(C_TYPE)
#define RE_IS_SIGNED_STRING(C_TYPE) RE_IS_TYPE_SIGNED_(C_TYPE)
#define RE_IS_SIGNED_INT(C_TYPE) RE_IS_TYPE_SIGNED_(C_TYPE)
#define RE_IS_SIGNED_INTEGER(C_TYPE) RE_IS_TYPE_SIGNED_(C_TYPE)
#define RE_IS_SIGNED_UINT(C_TYPE) RE_IS_TYPE_SIGNED_(C_TYPE)
#define RE_IS_SIGNED_LLONG(C_TYPE) RE_IS_TYPE_SIGNED_(C_TYPE)
#define RE_IS_SIGNED_ENUM(C_TYPE) RE_IS_TYPE_SIGNED_(C_TYPE)
#define RE_IS_SIGNED_LONG(C_TYPE) RE_IS_TYPE_SIGNED_(C_TYPE)
#define RE_IS_SIGNED_FLOAT(C_TYPE) RE_IS_TYPE_SIGNED_(C_TYPE)
#define RE_IS_SIGNED_DOUBLE(C_TYPE) RE_IS_TYPE_SIGNED_(C_TYPE)

#define RE_IS_SIGNED_(DATA_TYPE, CTYPE) RE_IS_SIGNED_##DATA_TYPE(CTYPE)

#define RE_ARRAY_FIELD_INFO_(TYPE_NAME, DATA_TYPE, C_TYPE, FIELD_NAME, ARRAY_SIZE) \
    RAII_##DATA_TYPE, #C_TYPE, #FIELD_NAME, sizeof(C_TYPE) * ARRAY_SIZE, offsetof(TYPE_NAME, FIELD_NAME), \
    RE_IS_SIGNED_(DATA_TYPE, C_TYPE), ARRAY_SIZE

#define RE_SIMPLE_FIELD_INFO_(TYPE_NAME, DATA_TYPE, C_TYPE, FIELD_NAME) \
    RAII_##DATA_TYPE, #C_TYPE, #FIELD_NAME, sizeof(C_TYPE), offsetof(TYPE_NAME, FIELD_NAME), \
    RE_IS_SIGNED_(DATA_TYPE, C_TYPE), -1

#define RE_FIELD_INFO_(...) \
{ \
    RE_EXPAND_(RE_OVERRIDE_5( \
    __VA_ARGS__, \
    RE_ARRAY_FIELD_INFO_, \
    RE_SIMPLE_FIELD_INFO_, \
    RE_OVERRIDE_5_PLACEHOLDER)(__VA_ARGS__)) \
},

#define RE_FIELD_INFO(X, USER_DATA) \
    RE_FIELD_INFO_(USER_DATA, RE_EXPAND_VA_ X)

#define RE_DEFINE_METHOD(TYPE_NAME, ...) \
    reflect_type_t* reflect_##TYPE_NAME() \
    { \
        static reflect_field_t fields_info[RE_FOREACH(RE_SUM, 0, __VA_ARGS__)] = \
        { \
            RE_FOREACH(RE_FIELD_INFO, TYPE_NAME, __VA_ARGS__) \
        }; \
        static reflect_type_t type_info = \
        { \
            RAII_STRUCT, \
            NULL, \
            #TYPE_NAME, \
            RE_FOREACH(RE_SUM, 0, __VA_ARGS__), \
            sizeof(TYPE_NAME), \
            RE_FOREACH(RE_SIZEOF, 0, __VA_ARGS__), \
            fields_info \
        }; \
        return &type_info; \
    }

#define RE_DEFINE_PROTO(TYPE_NAME) C_API reflect_type_t* reflect_##TYPE_NAME(void);

#define RE_DEFINE_STRUCT(TYPE_NAME, ...) \
    typedef struct \
    { \
        RE_FOREACH(RE_DECLARE_FIELD, 0, __VA_ARGS__) \
    } TYPE_NAME; \
    RE_DEFINE_PROTO(TYPE_NAME) \
    RE_DEFINE_METHOD(TYPE_NAME, __VA_ARGS__)

C_API string_t reflect_kind(void_t);
C_API void reflect_set_field(reflect_type_t *, int, void_t value);
C_API void reflect_get_field(reflect_type_t *, int, void_t out);
C_API void reflect_with(reflect_type_t *, void_t value);

C_API size_t reflect_num_fields(reflect_type_t *type);
C_API const char *reflect_type_of(reflect_type_t *type);
C_API reflect_types reflect_type_enum(reflect_type_t *type);
C_API size_t reflect_type_size(reflect_type_t *type);
C_API size_t reflect_packed_size(reflect_type_t *type);
C_API reflect_field_t *reflect_value_of(reflect_type_t *type);

C_API const char *reflect_field_type(reflect_type_t *, int);
C_API const char *reflect_field_name(reflect_type_t *, int);
C_API size_t reflect_field_size(reflect_type_t *, int);
C_API size_t reflect_field_offset(reflect_type_t *, int);
C_API bool reflect_field_is_signed(reflect_type_t *, int);
C_API int reflect_field_array_size(reflect_type_t *, int);
C_API reflect_types reflect_field_enum(reflect_type_t *, int);
C_API bool is_reflection(void_t self);
C_API void println(int n_of_args, ...);
C_API void shuttingdown(void);

/**
* Creates a reflection structure and reflection function as `reflect_*TYPE_NAME*()`.
* Allows the inspect of data structures at runtime:
* - field types
* - field names
* - size of array fields
* - size of field
*
* `TYPE_NAME` - name of your structure
* (`DATA_TYPE`, `C_TYPE`, `FIELD_NAME`[, `ARRAY_SIZE`]) - comma-separated list of fields in the structure
*
* - DATA_TYPE - type of field (INTEGER, STRING, ENUM, PTR, FLOAT, DOUBLE, or STRUCT)
* - C_TYPE - type of the field (e.g. int, uint64, char, etc.)
* - FIELD_NAME - name of the field
* - ARRAY_SIZE - size of array, if a field is an array
*/
#define reflect_struct(TYPE_NAME, ...) RE_DEFINE_STRUCT(TYPE_NAME, (ENUM, raii_type, type), __VA_ARGS__)

/**
* Creates a reflection function as `reflect_*TYPE_NAME*()`.
*
* Allows the inspect of data structures at runtime:
* - field types
* - field names
* - size of array fields
* - size of field
*
* `TYPE_NAME` - name of your structure
* (`DATA_TYPE`, `C_TYPE`, `FIELD_NAME`[, `ARRAY_SIZE`]) - comma-separated list of fields in the structure
*
* - DATA_TYPE - type of field (INTEGER, STRING, ENUM, PTR, FLOAT, DOUBLE, or STRUCT)
* - C_TYPE - type of the field (e.g. int, uint64, char, etc.)
* - FIELD_NAME - name of the field
* - ARRAY_SIZE - size of array, if a field is an array
*/
#define reflect_func(TYPE_NAME, ...) RE_DEFINE_METHOD(TYPE_NAME, (ENUM, raii_type, type), __VA_ARGS__)

/**
* Creates a reflection function proto as `extern reflect_type_t *reflect_*TYPE_NAME*(void);`
*/
#define reflect_proto(TYPE_NAME) RE_DEFINE_PROTO(TYPE_NAME)

#define c_int(data) raii_value((data)).integer
#define c_long(data) raii_value((data)).s_long
#define c_llong(data) raii_value((data)).long_long
#define c_unsigned_int(data) raii_value((data)).u_int
#define c_unsigned_long(data) raii_value((data)).u_long
#define c_size_t(data) raii_value((data)).max_size
#define c_buffer(data) raii_value((data)).buffer
#define c_char(data) raii_value((data)).schar
#define c_char_ptr(data) raii_value((data)).char_ptr
#define c_bool(data) raii_value((data)).boolean
#define c_float(data) raii_value((data)).point
#define c_double(data) raii_value((data)).precision
#define c_unsigned_char(data) raii_value((data)).uchar
#define c_char_ptr_ptr(data) raii_value((data)).array
#define c_unsigned_char_ptr(data) raii_value((data)).uchar_ptr
#define c_short(data) raii_value((data)).s_short
#define c_unsigned_short(data) raii_value((data)).u_short
#define c_ptr(data) raii_value((data)).object
#define c_callable(data) raii_value((data)).func
#define c_object(data) c_ptr(data)
#define c_func(data) c_callable(data)
#define c_cast(type, data) (type)raii_value((data)).object

#define as_var(variable, variable_type, data, enum_type) var_t *variable = (var_t *)calloc_local(1, sizeof(var_t)); \
    variable->type = enum_type; \
    variable->value = (variable_type *)calloc_local(1, sizeof(variable_type) + sizeof(data)); \
    memcpy(variable->value, &data, sizeof(data))

#define as_list(variable, variable_type, data, enum_type) var_t *variable = (var_t *)calloc_local(1, sizeof(var_t)); \
    variable->type = enum_type; \
    variable->value = (interface)array_copy((variable_type)variable->value, data);

#define as_string(variable, data) as_var(variable, char, data, RAII_STRING)
#define as_size(variable, data) as_var(variable, size_t, data, RAII_MAXSIZE)
#define as_int(variable, data) as_var(variable, int, data, RAII_INTEGER)
#define as_uchar(variable, data) as_var(variable, unsigned char, data, RAII_UCHAR)
#define as_array(variable, data) as_list(variable, arrays_t, data, RAII_ARRAY)
#define as_range(variable, data) as_list(variable, ranges_t, data, RAII_RANGE)
#define as_range_char(variable, data) as_list(variable, ranges_t, data, RAII_RANGE_CHAR)

#define as_reflect(variable, type, value)   reflect_type_t* variable = reflect_##type(); \
    variable->instance = value;

#define as_ref(variable, type, data, enum_type) as_var(variable, type, data, enum_type); \
    as_reflect(variable##_r, var_t, variable)

#define as_list_ref(variable, type, data, enum_type) as_list(variable, type, data, enum_type); \
    as_reflect(variable##_r, var_t, variable)

#define as_string_ref(variable, type, data) as_ref(variable, type, data, RAII_STRING)
#define as_size_ref(variable, type, data) as_ref(variable, type, data, RAII_MAXSIZE)
#define as_int_ref(variable, type, data) as_ref(variable, type, data, RAII_INT)
#define as_uchar_ref(variable, type, data) as_ref(variable, type, data, RAII_UCHAR)
#define as_float_ref(variable, type, data) as_ref(variable, type, data, RAII_FLOAT)
#define as_double_ref(variable, type, data) as_ref(variable, type, data, RAII_DOUBLE)
#define as_array_ref(variable, type, data) as_list_ref(variable, type, data, RAII_ARRAY)
#define as_range_ref(variable, type, data) as_list_ref(variable, type, data, RAII_RANGE)
#define as_range_char_ref(variable, type, data) as_list_ref(variable, type, data, RAII_RANGE_CHAR)

#define as_instance(variable, variable_type)    \
    variable_type *variable = (variable_type *)calloc_local(1, sizeof(variable_type));  \
    variable->type = RAII_STRUCT;

#define as_instance_ref(variable, type) as_instance(variable, type) \
    as_reflect(variable##_r, type, variable)

reflect_proto(awaitable_t);
reflect_proto(result_t);
reflect_proto(worker_t);
reflect_proto(var_t);
reflect_proto(raii_array_t);
reflect_proto(defer_t);
reflect_proto(defer_func_t);
reflect_proto(object_t);
reflect_proto(promise);

#ifdef __cplusplus
}
#endif

#endif /* REFLECTION_H_ */
