#include "reflection.h"

string_t reflect_kind(void_t value) {
    reflect_types res = (reflect_types)type_of(value);
    if (res == RAII_STRUCT) {
        if (is_str_eq(reflect_type_of((reflect_type_t *)value), "var_t")) {
            char out[SCRAPE_SIZE];
            reflect_get_field((reflect_type_t *)value, 0, out);
            res = c_int(out);
        }
    }

    switch (res) {
        case RAII_STRUCT:
            return "struct";
        case RAII_UNION:
            return "union";
        case RAII_CONST_CHAR:
            return "const char *";
        case RAII_STRING:
        case RAII_CHAR_P:
            return "char *";
        case RAII_UCHAR_P:
            return "unsigned char *";
        case RAII_INTEGER:
        case RAII_INT:
            return "int";
        case RAII_UINT:
            return "unsigned int";
        case RAII_MAXSIZE:
            return "unsigned long long";
        case RAII_LLONG:
            return "long long";
        case RAII_ENUM:
            return "enum";
        case RAII_UCHAR:
        case RAII_BOOL:
            return "unsigned char";
        case RAII_FLOAT:
            return "float";
        case RAII_DOUBLE:
            return "double";
        case RAII_OBJ:
            return "* object(struct)";
        case RAII_PTR:
            return "* ptr";
        case RAII_FUNC:
            return "*(*)(*) callable";
        case RAII_REFLECT_TYPE:
            return "<> reflect";
    }

    return "Unknown error";
}

RAII_INLINE void reflect_with(reflect_type_t *type, void_t value) {
    type->instance = value;
}

RAII_INLINE reflect_field_t *reflect_value_of(reflect_type_t *type) {
    return type->fields;
}

RAII_INLINE size_t reflect_num_fields(reflect_type_t *type) {
    return type->fields_count;
}

RAII_INLINE string_t reflect_type_of(reflect_type_t *type) {
    return type->name;
}

RAII_INLINE reflect_types reflect_type_enum(reflect_type_t *type) {
    return type->data_type;
}

RAII_INLINE size_t reflect_type_size(reflect_type_t *type) {
    return type->size;
}

RAII_INLINE size_t reflect_packed_size(reflect_type_t *type) {
    return type->packed_size;
}

RAII_INLINE string_t reflect_field_type(reflect_type_t *type, int slot) {
    return (type->fields + slot)->field_type;
}

RAII_INLINE string_t reflect_field_name(reflect_type_t *type, int slot) {
    return (type->fields + slot)->field_name;
}

RAII_INLINE size_t reflect_field_size(reflect_type_t *type, int slot) {
    return (type->fields + slot)->size;
}

RAII_INLINE size_t reflect_field_offset(reflect_type_t *type, int slot) {
    return (type->fields + slot)->offset;
}

RAII_INLINE bool reflect_field_is_signed(reflect_type_t *type, int slot) {
    return (type->fields + slot)->is_signed;
}

RAII_INLINE int reflect_field_array_size(reflect_type_t *type, int slot) {
    return (type->fields + slot)->array_size;
}

RAII_INLINE reflect_types reflect_field_enum(reflect_type_t *type, int slot) {
    return (type->fields + slot)->data_type;
}

RAII_INLINE void reflect_set_field(reflect_type_t *variable, int slot, void_t value) {
    memcpy((string)variable->instance + (variable->fields + slot)->offset, value, (variable->fields + slot)->size);
}

RAII_INLINE void reflect_get_field(reflect_type_t *variable, int slot, void_t out) {
    memcpy(out, (string)variable->instance + (variable->fields + slot)->offset, (variable->fields + slot)->size);
}

RAII_INLINE bool is_reflection(void_t self) {
    return ((reflect_kind_t *)self)->type
        && ((reflect_kind_t *)self)->fields
        && ((reflect_kind_t *)self)->name
        && ((reflect_kind_t *)self)->num_fields
        && ((reflect_kind_t *)self)->size
        && ((reflect_kind_t *)self)->packed_size;
}

void println(int n_of_args, ...) {
    va_list argp;
    void_t list;
    int i, type;

    va_start(argp, n_of_args);
    for (i = 0; i < n_of_args; i++) {
        list = va_arg(argp, void_t);
        /*
        if (is_type(((map_t *)list), RAII_MAP_STRUCT)) {
            type = ((map_t *)list)->item_type;
            foreach(item in list) {
                if (type == RAII_LLONG)
#ifdef _WIN32
                    printf("%ld ", (long)has(item).long_long);
#else
                    printf("%lld ", has(item).long_long);
#endif
                else if (type == RAII_STRING)
                    printf("%s ", has(item).str);
                else if (type == RAII_OBJ)
                    printf("%p ", has(item).object);
                else if (type == RAII_BOOL)
                    printf(has(item).boolean ? "true " : "false ");
            }
        } else*/
        if (is_reflection(list)) {
            reflect_type_t *kind = (reflect_type_t *)list;
            printf("[ %d, %s, %zu, %zu, %zu ]\n",
                   reflect_type_enum(kind),
                   reflect_type_of(kind),
                   reflect_num_fields(kind),
                   reflect_type_size(kind),
                   reflect_packed_size(kind)
            );
            for (i = 0; i < reflect_num_fields(kind); i++) {
                printf("  -  %d, %s, %s, %zu, %zu, %d, %d\n",
                       reflect_field_enum(kind, i),
                       reflect_field_type(kind, i),
                       reflect_field_name(kind, i),
                       reflect_field_size(kind, i),
                       reflect_field_offset(kind, i),
                       reflect_field_is_signed(kind, i),
                       reflect_field_array_size(kind, i)
                );
            }
        } else {
            printf("%s ", raii_value(list).char_ptr);
        }
    }
    va_end(argp);
    puts("");
}

RAII_INLINE void shuttingdown(void) {
    raii_deferred_free(get_scope());
}

reflect_func(_awaitable_t,
             (UINT, rid_t, cid),
             (UNION, waitgroup_t, wg)
)

reflect_func(var_t,
             (PTR, void_t, value)
)

reflect_func(raii_array_t,
             (PTR, void_t, base),
             (MAXSIZE, size_t, elements)
)

reflect_func(defer_t,
             (STRUCT, raii_array_t, base)
)

reflect_func(defer_func_t,
             (FUNC, func_t, func),
             (PTR, void_t, data),
             (PTR, void_t, check)
)

reflect_func(object_t,
             (PTR, void_t, value),
             (FUNC, func_t, dtor)
)

reflect_func(_result_t,
             (BOOL, bool, is_ready),
             (UINT, rid_t, id),
             (STRUCT, raii_values_t *, result)
)

reflect_func(worker_t,
             (INTEGER, int, id),
             (STRUCT, unique_t *, local),
             (PTR, void_t, arg),
             (FUNC, thrd_func_t, func),
             (STRUCT, promise *, value),
             (STRUCT, raii_deque_t *, queue)
)

reflect_func(promise,
             (INT, int, id),
             (UCHAR, atomic_flag, done),
             (UCHAR, atomic_spinlock, mutex),
             (STRUCT, memory_t *, scope),
             (STRUCT, raii_values_t *, result)
)