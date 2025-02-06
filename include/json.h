
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
*
* - '`.`' indicate next format character will use dot function to record value for key name with dot,
*
* - '`a`' begin array encoding, every item `value` will be appended, until '`e`' is place in format desc,
*
* - '`e`' end array encoding,
*
* - '`n`' record `null` value for key, *DO NOT PLACE `NULL` IN ARGUMENTS*,
*
* - '`f`' record `float/double` number for key,
*
* - '`d`' record `signed` number for key,
*
* - '`i`' record `unsigned` number for key,
*
* - '`b`' record `boolean` value for key,
*
* - '`s`' record `string` value for key,
*
* - '`v`' record `JSON_Value` for key,
*
* @param arguments use `kv(key,value)` for pairs,
*
* - DO NOT PROVIDE FOR `NULL`, ONLY KEY
*/
C_API json_t *json_encode(string_t desc, ...);

/**
* Creates json value string `array` using a format like `printf` for each value to index.
*
* @param desc format string:
*
* - '`n`' record `null` value for index, ~DO NOT PLACE `NULL` IN ARGUMENTS~,
*
* - '`f`' record `float/double` number for index,,
*
* - '`d`' record `signed` number for index,
*
* - '`i`' record `unsigned` number for index,
*
* - '`b`' record `boolean` value for index,,
*
* - '`s`' record `string` value for index,,
*
* - '`v`' record `JSON_Value` for index,
*
* @param arguments indexed by `desc` format order, ~DO NOT PROVIDE FOR NULL~
*/
C_API string json_for(string_t desc, ...);

#ifdef __cplusplus
}
#endif

#endif