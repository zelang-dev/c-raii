#ifndef _URL_HTTP_H_
#define _URL_HTTP_H_

#include "hashtable.h"

#if defined(_WIN32) || defined(_WIN64)
#   include <compat/dirent.h>
#else
#   include <dirent.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct form_data_s form_data_t;
typedef struct cookie_s cookie_t;
typedef struct response_s response_t;
typedef struct http_s http_t;
typedef hash_t hash_http_t;
typedef struct fileinfo_s {
    raii_type type;
    string_t dirname;
    string_t base;
    string_t extension;
    string_t filename;
} fileinfo_t;

typedef struct url_s {
    raii_type type;
	bool is_rejected;
	bool is_autofreeable;
	unsigned short port;
	string scheme;
    string user;
    string pass;
    string host;
    string path;
    string query;
    string fragment;
} url_t, uri_t;

typedef enum {
    URL_SCHEME,
    URL_HOST,
    URL_PORT,
    URL_USER,
    URL_PASS,
    URL_PATH,
    URL_QUERY,
    URL_FRAGMENT,
} url_key;

typedef enum {
    // Informational 1xx
    STATUS_CONTINUE = 100,
    STATUS_SWITCHING_PROTOCOLS = 101,
    STATUS_PROCESSING = 102,
    STATUS_EARLY_HINTS = 103,
    // Successful 2xx
    STATUS_OK = 200,
    STATUS_CREATED = 201,
    STATUS_ACCEPTED = 202,
    STATUS_NON_AUTHORITATIVE_INFORMATION = 203,
    STATUS_NO_CONTENT = 204,
    STATUS_RESET_CONTENT = 205,
    STATUS_PARTIAL_CONTENT = 206,
    STATUS_MULTI_STATUS = 207,
    STATUS_ALREADY_REPORTED = 208,
    STATUS_IM_USED = 226,
    // Redirection 3xx
    STATUS_MULTIPLE_CHOICES = 300,
    STATUS_MOVED_PERMANENTLY = 301,
    STATUS_FOUND = 302,
    STATUS_SEE_OTHER = 303,
    STATUS_NOT_MODIFIED = 304,
    STATUS_USE_PROXY = 305,
    STATUS_RESERVED = 306,
    STATUS_TEMPORARY_REDIRECT = 307,
    STATUS_PERMANENT_REDIRECT = 308,
    // Client Errors 4xx
    STATUS_BAD_REQUEST = 400,
    STATUS_UNAUTHORIZED = 401,
    STATUS_PAYMENT_REQUIRED = 402,
    STATUS_FORBIDDEN = 403,
    STATUS_NOT_FOUND = 404,
    STATUS_METHOD_NOT_ALLOWED = 405,
    STATUS_NOT_ACCEPTABLE = 406,
    STATUS_PROXY_AUTHENTICATION_REQUIRED = 407,
    STATUS_REQUEST_TIMEOUT = 408,
    STATUS_CONFLICT = 409,
    STATUS_GONE = 410,
    STATUS_LENGTH_REQUIRED = 411,
    STATUS_PRECONDITION_FAILED = 412,
    STATUS_PAYLOAD_TOO_LARGE = 413,
    STATUS_URI_TOO_LONG = 414,
    STATUS_UNSUPPORTED_MEDIA_TYPE = 415,
    STATUS_RANGE_NOT_SATISFIABLE = 416,
    STATUS_EXPECTATION_FAILED = 417,
    STATUS_IM_A_TEAPOT = 418,
    STATUS_MISDIRECTED_REQUEST = 421,
    STATUS_UNPROCESSABLE_ENTITY = 422,
    STATUS_LOCKED = 423,
    STATUS_FAILED_DEPENDENCY = 424,
    STATUS_TOO_EARLY = 425,
    STATUS_UPGRADE_REQUIRED = 426,
    STATUS_PRECONDITION_REQUIRED = 428,
    STATUS_TOO_MANY_REQUESTS = 429,
    STATUS_REQUEST_HEADER_FIELDS_TOO_LARGE = 431,
    STATUS_UNAVAILABLE_FOR_LEGAL_REASONS = 451,
    // Server Errors 5xx
    STATUS_INTERNAL_SERVER_ERROR = 500,
    STATUS_NOT_IMPLEMENTED = 501,
    STATUS_BAD_GATEWAY = 502,
    STATUS_SERVICE_UNAVAILABLE = 503,
    STATUS_GATEWAY_TIMEOUT = 504,
    STATUS_VERSION_NOT_SUPPORTED = 505,
    STATUS_VARIANT_ALSO_NEGOTIATES = 506,
    STATUS_INSUFFICIENT_STORAGE = 507,
    STATUS_LOOP_DETECTED = 508,
    STATUS_NOT_EXTENDED = 510,
    STATUS_NETWORK_AUTHENTICATION_REQUIRED = 511
} http_status;

typedef enum {
    /* Request Methods */
    HTTP_DELETE,
    HTTP_GET,
    HTTP_HEAD,
    HTTP_POST,
    HTTP_PUT,
    /* pathological */
    HTTP_CONNECT,
    HTTP_OPTIONS,
    HTTP_TRACE,
    /* webdav */
    HTTP_COPY,
    HTTP_LOCK,
    HTTP_MKCOL,
    HTTP_MOVE,
    HTTP_PROPFIND,
    HTTP_PROPPATCH,
    HTTP_SEARCH,
    HTTP_UNLOCK,
    /* subversion */
    HTTP_REPORT,
    HTTP_MKACTIVITY,
    HTTP_CHECKOUT,
    HTTP_MERGE,
    /* upnp */
    HTTP_MSEARCH,
    HTTP_NOTIFY,
    HTTP_SUBSCRIBE,
    HTTP_UNSUBSCRIBE,
    /* RFC-5789 */
    HTTP_PATCH,
    HTTP_PURGE
} http_method_type;

typedef enum {
    HTTP_REQUEST = HTTP_PURGE + 1,
    HTTP_RESPONSE,
    HTTP_BOTH
} http_parser_type;

typedef enum {
    F_CHUNKED = 1 << 0,
    F_CONNECTION_KEEP_ALIVE = 1 << 1,
    F_CONNECTION_CLOSE = 1 << 2,
    F_TRAILING = 1 << 3,
    F_UPGRADE = 1 << 4,
    F_SKIP_BODY = 1 << 5
} http_flags;

#define kv_custom(key, value) (head_custom), (key), (value)
typedef enum {
	/* For X-Powered-By: */
	head_by = HTTP_BOTH + 1,
	/* For Cookie:, or Set-Cookie: `; Path=; Expires=; Max-Age=; SameSite=; Domain=; HttpOnly; Secure` */
	head_cookie,
	/* For Strict-Transport-Security:
	`max-age=31536000; includeSubDomains; preload` */
	head_secure,
	/* For Connection: `keep-alive`, `close`, `etc...` */
	head_conn,
	/* For Authorization: Bearer */
	head_bearer,
	/* For Authorization: Basic */
	head_auth_basic,
	/* Internal, use `kv_custom("key", "value")` instead */
	head_custom,
} header_types;

/*
Parse a URL and return its components, return `NULL` for malformed URLs.

Modifed C code from PHP userland function
see https://php.net/manual/en/function.parse-url.php
*/
C_API url_t *parse_url(string_t str);

/*
Same as `parse_url`, but user MUST call `uri_free` to release allocated memory.

NOTE: Each field is separately allocated, MUST `nullptr` assign, if ~modify/free~.
*/
C_API uri_t *parse_uri(string_t str);
C_API void uri_free(uri_t *uri);
C_API string url_decode(string str, size_t len);
C_API string url_encode(char const *s, size_t len);
C_API fileinfo_t *pathinfo(string filepath);
C_API void parse_str(http_t *this, string lines, string sep, string part);

/*
Returns valid HTTP status codes reasons.

Verified 2020-05-22

see https://www.iana.org/assignments/http-status-codes/http-status-codes.xhtml
*/
C_API string_t http_status_str(uint16_t const status);

/* Return date string in standard format for http headers */
C_API string http_std_date(time_t t);

/* Return date string ahead in standard format for `Set-Cookie` headers */
C_API string http_date_ahead(u32 days, u32 hours, u32 minutes, u32 seconds);

/**
 * Parse `request/response` headers, and store.
 *
 * @param action either HTTP_RESPONSE or HTTP_REQUEST
 * @param this current `http_t` instance
 * @param headers raw message
 */
C_API void parse_http(http_parser_type action, http_t *this, string headers);

/**
 * Returns `http_t` instance, for simple generic handling/constructing
 * `request/response` messages.
 *
 * Following the https://tools.ietf.org/html/rfc2616.html specs.
 *
 * - For use with `http_response()` and `http_request()`.
 *
 * @param hostname for `Host:` header request,  will be ignored on `path/url` setting
 * @param protocol version for `HTTP/` header
 */
C_API http_t *http_for(string hostname, double protocol);
C_API void http_free(http_t *this);

/* Set User agent name for request, max length = `NAME_MAX/MAX_PATH` */
C_API void http_user_agent(string);

/* Set Server name for response, max length = `NAME_MAX/MAX_PATH` */
C_API void http_server_agent(string);

/**
 * Construct a new response string.
 *
 * @param this current `http_t` instance
 * @param body defaults to `Not Found`, if `status` empty
 * @param status defaults to `STATUS_NO_FOUND`, if `body` empty, otherwise `STATUS_OK`
 * @param type defaults to `text/html; charset=utf-8`, if empty
 * @param header_pairs number of additional headers
 *
 * - `using:` header_types = `head_by, head_cookie, head_secure, head_conn, head_bearer, head_auth_basic`
 *
 * - `kv(header_types, "value")`
 *
 * - `or:` `kv_custom("key", "value")`
 */
C_API string http_response(http_t *this, string body, http_status status,
	string type, u32 header_pairs, ...);

/**
 * Construct a new request string.
 *
 * @param this current `http_t` instance
 * @param method
 * @param path
 * @param type defaults to `text/html; charset=utf-8`, if empty
 * @param body_data
 * @param header_pairs number of additional headers
 *
 * - `using:` header_types = `head_by, head_cookie, head_secure, head_conn, head_bearer, head_auth_basic`
 *
 * - `kv(header_types, "value")`
 *
 * - `or:` `kv_custom("key", "value")`
 */
C_API string http_request(http_t *this, http_method_type method, string path, string type,
	string body_data, u32 header_pairs, ...);

/**
 * Return a request/response header `content`.
 */
C_API string http_get_header(http_t *this, string key);

/**
 * Add or overwrite an request/response header parameter.
 */
C_API void http_put_header(http_t *this, string key, string value, bool force_cap);
C_API bool http_has_header(http_t *this, string key);

/**
 * Return a `request/response` header content `variable` value.
 *
 * - `key` header to check for
 * - `var` variable to find
 */
C_API string http_get_var(http_t *this, string key, string var);
C_API bool http_has_var(http_t *this, string key, string var);

C_API bool http_has_flag(http_t *this, string key, string flag);

/**
 * Return a request `variable` parameter `value`.
 */
C_API string http_get_param(http_t *this, string key);
C_API bool http_has_param(http_t *this, string key);

C_API http_status http_get_status(http_t *this);
C_API http_status http_get_code(http_t *this);
C_API double http_get_version(http_t *this);
C_API string http_get_message(http_t *this);
C_API string http_get_method(http_t *this);
C_API string http_get_body(http_t *this);
C_API string http_get_protocol(http_t *this);
C_API string http_get_url(http_t *this);
C_API string http_get_path(http_t *this);
C_API string http_get_boundary(http_t *this);

C_API bool http_cookie_is_multi(http_t *this);
C_API bool http_cookie_is_secure(http_t *this, string name);
C_API bool http_cookie_is_httpOnly(http_t *this, string name);
C_API size_t http_cookie_maxAge(http_t *this, string name);
C_API size_t http_cookie_count(http_t *this);
C_API arrays_t http_cookie_names(http_t *this);
C_API string http_get_cookie(http_t *this, string name);
C_API string http_cookie_expiries(http_t *this, string name);
C_API string http_cookie_domain(http_t *this, string name);
C_API string http_cookie_sameSite(http_t *this, string name);
C_API string http_cookie_path(http_t *this, string name);

C_API bool http_is_multipart(http_t *this);
C_API bool http_multi_has(http_t *this, string name);
C_API bool http_multi_is_file(http_t *this, string name);
C_API string http_multi_filename(http_t *this, string name);
C_API string http_multi_disposition(http_t *this, string name);
C_API string http_multi_type(http_t *this, string name);
C_API string http_multi_body(http_t *this, string name);
C_API size_t http_multi_length(http_t *this, string name);
C_API size_t http_multi_count(http_t *this);
C_API arrays_t http_multi_names(http_t *this);

#ifndef HTTP_AGENT
#define HTTP_AGENT "http_client"
#endif

#ifndef HTTP_SERVER
#define HTTP_SERVER "http_server"
#endif

#ifdef __cplusplus
}
#endif

#endif
