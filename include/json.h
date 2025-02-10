
#ifndef _JSON_H
#define _JSON_H

#if defined(_WIN32) || defined(_WIN64)
#   include "compat/dirent.h"
#else
#   include <dirent.h>
#endif

#include "raii.h"
#include "parson.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct json_value_t json_t;

/* Checks for valid `JSON` string as defined by [RFC 8259](https://datatracker.ietf.org/doc/html/rfc8259). */
C_API bool is_string_json(string_t text);

/* Check if validated by json type */
C_API bool is_json(json_t *);

/**
* @param value Serialization of value to string.
* @param is_pretty Pretty serialization, if set `true`.
*/
C_API string json_serialize(json_t *, bool is_pretty);

/**
* @param text Parses first JSON value in a text, returns NULL in case of error.
* @param is_commented Ignores comments (/ * * / and //), if set `true`.
*/
C_API json_t *json_decode(string_t, bool is_commented);

/**
* Creates json value `object` using a format like `printf` for each value to key.
*
* @param desc format string:
* * @param `.` indicate next format character will use dot function to record value for key name with dot,
* * @param `a` begin array encoding, every item `value` will be appended, until '`e`' is place in format desc,
* * @param `e` end array encoding,
* * @param `n` record `null` value for key, *DO NOT PLACE `NULL` IN ARGUMENTS*,
* * @param `f` record `float/double` number for key,
* * @param `d` record `signed` number for key,
* * @param `i` record `unsigned` number for key,
* * @param `b` record `boolean` value for key,
* * @param `s` record `string` value for key,
* * @param `v` record `JSON_Value` for key,
* @param arguments use ~macro~ `kv(key,value)` for pairs, *DO NOT PROVIDE FOR NULL, ONLY KEY*
*/
C_API json_t *json_encode(string_t desc, ...);

/**
* Creates json string `array`, using a format like `printf` for each value to index.
*
* @param desc format string:
* * @param `n` record `null` value for index, *DO NOT PLACE `NULL` IN ARGUMENTS*,
* * @param `f` record `float/double` number for index,
* * @param `d` record `signed` number for index,
* * @param `i` record `unsigned` number for index,
* * @param `b` record `boolean` value for index,
* * @param `s` record `string` value for index,
* * @param `v` record `JSON_Value` for index,
* @param arguments indexed by `desc` format order, *DO NOT PROVIDE FOR NULL*
*/
C_API string json_for(string_t desc, ...);

C_API string json_read_file(string_t filename);
#ifdef __cplusplus
}
#endif

#endif