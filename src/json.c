#include "json.h"
/*
A JSON Validator in C

article: https://ryjo.codes/articles/a-json-validator-in-c.html

static functions and `is_string_json` modified from https://github.com/mrryanjohnston/validatejson
*/

static bool isString(string_t text, int *cursor, int length);
static bool is_jsonElement(string_t text, int *cursor, int length);
static RAII_INLINE bool isCharAndAdvanceCursor(string_t text, int *cursor, char c) {
    return text[*cursor] == c &&
        ++(*cursor);
}

static bool skipWhitespace(string_t text, int *cursor, int length) {
    while (
        *cursor < length && (
            isCharAndAdvanceCursor(text, cursor, ' ') ||
            isCharAndAdvanceCursor(text, cursor, '\t') ||
            isCharAndAdvanceCursor(text, cursor, '\r') ||
            isCharAndAdvanceCursor(text, cursor, '\n')
            )
        );
    return true;
}

static bool isEndOfObject(string_t text, int *cursor, int length) {
    (*cursor)++;
    skipWhitespace(text, cursor, length);
    return text[*cursor] == '}' ||
        isCharAndAdvanceCursor(text, cursor, ',') &&
        skipWhitespace(text, cursor, length) &&
        text[*cursor] == '"' &&
        isString(text, cursor, length) &&
        (*cursor)++ &&
        skipWhitespace(text, cursor, length) &&
        isCharAndAdvanceCursor(text, cursor, ':') &&
        is_jsonElement(text, cursor, length) &&
        isEndOfObject(text, cursor, length);
}

static bool isObject(string_t text, int *cursor, int length) {
    (*cursor)++;
    skipWhitespace(text, cursor, length);
    return text[*cursor] == '}' ||
        text[*cursor] == '"' &&
        isString(text, cursor, length) &&
        (*cursor)++ &&
        skipWhitespace(text, cursor, length) &&
        isCharAndAdvanceCursor(text, cursor, ':') &&
        is_jsonElement(text, cursor, length) &&
        isEndOfObject(text, cursor, length);
}

static bool isEndOfArray(string_t text, int *cursor, int length) {
    (*cursor)++;
    skipWhitespace(text, cursor, length);
    return text[*cursor] == ']' ||
        isCharAndAdvanceCursor(text, cursor, ',') &&
        is_jsonElement(text, cursor, length) &&
        isEndOfArray(text, cursor, length);
}

static bool isArray(string_t text, int *cursor, int length) {
    (*cursor)++;
    skipWhitespace(text, cursor, length);
    return text[*cursor] == ']' ||
        is_jsonElement(text, cursor, length) &&
        isEndOfArray(text, cursor, length);
}

static RAII_INLINE bool isBoolean(string_t text, int *cursor, int length) {
    return (
        strncmp(text + (*cursor), "true", 4) == 0 ||
        strncmp(text + (*cursor), "null", 4) == 0
        ) &&
        (*cursor = (*cursor) + 3) ||
        strncmp(text + (*cursor), "false", 5) == 0 &&
        (*cursor = (*cursor) + 4);
}

static bool isString(string_t text, int *cursor, int length) {
    int x;
    (*cursor)++;
    while (
        *cursor < length &&
        text[*cursor] != '"'
        ) {
        if (text[*cursor] == '\\') {
            (*cursor)++;
            if (text[*cursor] == 'u') {
                if ((*cursor) + 4 > length) return false;
                // From https://github.com/zserge/jsmn/blob/25647e692c7906b96ffd2b05ca54c097948e879c/jsmn.h#L241-L251
                for (x = 0; x < 4; (*cursor)++ && x++) {
                    int c = text[(*cursor) + 1];
                    if (!(
                        (c >= 48 && c <= 57) || /* 0-9 */
                        (c >= 65 && c <= 70) || /* A-F */
                        (c >= 97 && c <= 102)   /* a-f */
                        )) return false;
                }
            }
        }
        (*cursor)++;
    }
    return text[*cursor] == '"';
}

static bool isAtLeastOneInteger(string_t text, int *cursor, int length) {
    if (
        text[*cursor] < 48 ||
        text[*cursor] > 57
        ) return false;
    do (*cursor)++;
    while (
        *cursor < length &&
        text[*cursor] >= 48 &&
        text[*cursor] <= 57
        );
    return true;
}

static bool isExponent(string_t text, int *cursor, int length) {
    return (
        text[*cursor] != 'e' &&
        text[*cursor] != 'E'
        ) ||
        (*cursor)++ &&
        (
            isCharAndAdvanceCursor(text, cursor, '-') ||
            isCharAndAdvanceCursor(text, cursor, '+') ||
            true
            ) &&
        isAtLeastOneInteger(text, cursor, length);
}

static RAII_INLINE bool isFraction(string_t text, int *cursor, int length) {
    return text[*cursor] != '.' ||
        (*cursor)++ &&
        isAtLeastOneInteger(text, cursor, length);
}

static RAII_INLINE bool isNumber(string_t text, int *cursor, int length) {
    return isAtLeastOneInteger(text, cursor, length) &&
        isFraction(text, cursor, length) &&
        isExponent(text, cursor, length) &&
        (
            text[*cursor] == '}' ||
            text[*cursor] == ']' ||
            text[*cursor] == ',' ||
            text[*cursor] == ' ' ||
            text[*cursor] == '\t' ||
            text[*cursor] == '\r' ||
            text[*cursor] == '\n' ||
            text[*cursor] == '\0'
            ) &&
        (*cursor)--;
}

static bool is_jsonElement(string_t text, int *cursor, int length) {
    skipWhitespace(text, cursor, length);
    switch (text[*cursor]) {
        case '"':
            return isString(text, cursor, length);
        case '[':
            return isArray(text, cursor, length);
        case '{':
            return isObject(text, cursor, length);
        case 't':
        case 'f':
        case 'n':
            return isBoolean(text, cursor, length);
        case '-':
            (*cursor)++;
        default:
            return isNumber(text, cursor, length);
    }
}

static RAII_INLINE bool is_jsonString(string_t text, int *cursor, int length) {
    return is_jsonElement(text, cursor, length) &&
        (
            (*cursor) == length ||
            ++(*cursor) &&
            skipWhitespace(text, cursor, length) &&
            (*cursor) == length
            );
}

RAII_INLINE bool is_string_json(string_t text) {
    int cursor = 0;

    return is_jsonString(text, &cursor, simd_strlen(text));
}

RAII_INLINE bool is_json(json_t *schema) {
    return (is_empty(schema) || json_value_get_type(schema) == JSONError) ? false : true;
}

RAII_INLINE string json_serialize(json_t *json, bool is_pretty) {
    string json_string = nullptr;
    if (!is_empty(json)) {
        if (is_pretty)
            json_string = json_serialize_to_string_pretty((const json_t *)json);
        else
            json_string = json_serialize_to_string((const json_t *)json);

        deferring((func_t)json_free_serialized_string, json_string);
    }

    return json_string;
}

RAII_INLINE json_t *json_decode(string_t text, bool is_commented) {
    if (is_commented)
        return json_parse_string_with_comments(text);
    else
        return json_parse_string(text);
}

json_t *json_encode(string_t desc, ...) {
    int count = (int)simd_strlen(desc);
    json_t *json_root = json_value_init_object();
    deferring((func_t)json_value_free, json_root);
    JSON_Object *json_object = json_value_get_object(json_root);

    va_list argp;
    string key, value_char;
    int value_bool, i;
    JSON_Status status = JSONSuccess;
    json_t *value_any = nullptr;
    JSON_Array *value_array = nullptr;
    double value_float = 0;
    int64_t value_int = 0;
    size_t value_max = 0;
    bool is_dot = false, is_array = false, is_double = false, is_int = false, is_max = false;

    va_start(argp, desc);
    for (i = 0; i < count; i++) {
        if (status == JSONFailure)
            return nullptr;

        switch (*desc++) {
            case '.':
                is_dot = true;
                break;
            case 'e':
                if (is_array) {
                    is_array = false;
                    value_array = nullptr;
                    is_dot = false;
                }
                break;
            case 'a':
                if (!is_array) {
                    key = va_arg(argp, string);
                    status = json_object_set_value(json_object, key, json_value_init_array());
                    value_array = json_object_get_array(json_object, key);
                    is_array = true;
                    is_dot = false;
                }
                break;
            case 'n':
                if (!is_array)
                    key = va_arg(argp, string);

                if (is_array)
                    status = json_array_append_null(value_array);
                else if (is_dot)
                    status = json_object_dotset_null(json_object, key);
                else
                    status = json_object_set_null(json_object, key);
                is_dot = false;
                break;
            case 'd':
                is_int = true;
            case 'f':
                if (!is_int)
                    is_double = true;
            case 'i':
                if (!is_double && !is_int)
                    is_max = true;

                if (!is_array)
                    key = va_arg(argp, string);

                if (is_double)
                    value_float = va_arg(argp, double);
                else if (is_int)
                    value_int = va_arg(argp, int64_t);
                else
                    value_max = va_arg(argp, size_t);

                if (is_array)
                    status = json_array_append_number(value_array, (is_double ? value_float
                                                                    : is_int ? (int)value_int
                                                                    : (unsigned long)value_max));
                else if (is_dot)
                    status = json_object_dotset_number(json_object, key, (is_double ? value_float
                                                                          : is_int ? (int)value_int
                                                                          : (unsigned long)value_max));
                else
                    status = json_object_set_number(json_object, key, (is_double ? value_float
                                                                       : is_int ? (int)value_int
                                                                       : (unsigned long)value_max));

                is_dot = false;
                is_double = false;
                is_int = false;
                is_max = false;
                break;
            case 'b':
                if (!is_array)
                    key = va_arg(argp, string);

                value_bool = va_arg(argp, int);
                if (is_array)
                    status = json_array_append_boolean(value_array, value_bool);
                else if (is_dot)
                    status = json_object_dotset_boolean(json_object, key, value_bool);
                else
                    status = json_object_set_boolean(json_object, key, value_bool);
                is_dot = false;
                break;
            case 's':
                if (!is_array)
                    key = va_arg(argp, string);

                value_char = va_arg(argp, string);
                if (is_array)
                    status = json_array_append_string(value_array, value_char);
                else if (is_dot)
                    status = json_object_dotset_string(json_object, key, value_char);
                else
                    status = json_object_set_string(json_object, key, value_char);
                is_dot = false;
                break;
            case 'v':
                if (!is_array)
                    key = va_arg(argp, string);

                value_any = va_arg(argp, json_t *);
                if (is_array)
                    status = json_array_append_value(value_array, value_any);
                else if (is_dot)
                    status = json_object_dotset_value(json_object, key, value_any);
                else
                    status = json_object_set_value(json_object, key, value_any);
                is_dot = false;
                break;
            default:
                break;
        }
    }
    va_end(argp);

    return json_root;
}

string json_for(string_t desc, ...) {
    int count = (int)simd_strlen(desc);
    json_t *json_root = json_value_init_object();
    deferring((func_t)json_value_free, json_root);
    JSON_Object *json_object = json_value_get_object(json_root);
    JSON_Status status = json_object_set_value(json_object, "array", json_value_init_array());
    JSON_Array *value_array = json_object_get_array(json_object, "array");

    va_list argp;
    string value_char;
    int value_bool, i;
    json_t *value_any = nullptr;
    double value_float = 0;
    int64_t value_int = 0;
    size_t value_max = 0;
    bool is_double = false, is_int = false, is_max = false;

    va_start(argp, desc);
    for (i = 0; i < count; i++) {
        if (status == JSONFailure)
            return nullptr;

        switch (*desc++) {
            case 'n':
                status = json_array_append_null(value_array);
                break;
            case 'd':
                is_int = true;
            case 'f':
                if (!is_int)
                    is_double = true;
            case 'i':
                if (!is_double && !is_int)
                    is_max = true;

                if (is_double)
                    value_float = va_arg(argp, double);
                else if (is_int)
                    value_int = va_arg(argp, int64_t);
                else
                    value_max = va_arg(argp, size_t);

                status = json_array_append_number(value_array, (is_double ? value_float
                                                                : is_int ? (int)value_int
                                                                : (unsigned long)value_max));
                is_double = false;
                is_int = false;
                is_max = false;
                break;
            case 'b':
                value_bool = va_arg(argp, int);
                status = json_array_append_boolean(value_array, value_bool);
                break;
            case 's':
                value_char = va_arg(argp, string);
                status = json_array_append_string(value_array, value_char);
                break;
            case 'v':
                value_any = va_arg(argp, json_t *);
                status = json_array_append_value(value_array, value_any);
                break;
            default:
                break;
        }
    }
    va_end(argp);

    return json_serialize(json_array_get_wrapping_value(value_array), false);
}
