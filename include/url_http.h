#ifndef _URL_HTTP_H_
#define _URL_HTTP_H_

#include "hashtable.h"

#define CRLF "\r\n"

#ifdef __cplusplus
extern "C"
{
#endif

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
    unsigned short port;
    string scheme;
    string user;
    string pass;
    string host;
    string path;
    string query;
    string fragment;
} url_t;

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
} http_method;

typedef enum {
    HTTP_REQUEST,
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

typedef struct cookie_s {
    bool httpOnly;
    bool secure;
    bool strict;
    int expiry;
    int maxAge;
    string domain;
    string lifetime;
    string name;
    string path;
    string value;
    string sameSite;
} cookie_t;

typedef struct http_s {
    raii_type type;
    http_parser_type action;
    /* The current response status */
    http_status status;
    /* The current response body */
    string body;
    /* The unchanged data from server */
    string raw;
    /* The protocol */
    string protocol;
    /* The protocol version */
    double version;
    /* The requested status code */
    http_status code;
    /* The requested status message */
    string message;
    /* The requested method */
    string method;
    /* The requested path */
    string path;
    /* The requested uri */
    string uri;
    string hostname;
    /* The cookie headers */
    cookie_t *cookies;
    /* The current headers */
    hash_http_t *headers;
    /* The request params */
    hash_http_t *parameters;
    /* Response headers to send */
    hash_http_t *header;
} http_t;


/*
Parse a URL and return its components, return `NULL` for malformed URLs.

Modifed C code from PHP userland function
see https://php.net/manual/en/function.parse-url.php
*/
C_API url_t *parse_url(char const *str);
C_API string url_decode(string str, size_t len);
C_API string url_encode(char const *s, size_t len);
C_API fileinfo_t *pathinfo(string filepath);
C_API hash_http_t *parse_str(string lines, string sep, string part);

/*
Returns valid HTTP status codes reasons.

Verified 2020-05-22

see https://www.iana.org/assignments/http-status-codes/http-status-codes.xhtml
*/
C_API string_t http_status_str(uint16_t const status);

/* Return date string in standard format for http headers */
C_API string http_std_date(time_t t);

/* Parse/prepare server headers, and store. */
C_API void parse_http(http_t *this, string headers);

/**
 * Returns `http_t` instance, for simple generic handling/constructing **http** request/response
 * messages, following the https://tools.ietf.org/html/rfc2616.html specs.
 *
 * - For use with `http_response()` and `http_request()`.
 *
 * - `action` either HTTP_RESPONSE or HTTP_REQUEST
 * - `hostname` for `Host:` header request,  will be ignored on `path/url` setting
 * - `protocol` version for `HTTP/` header
 */
C_API http_t *http_for(http_parser_type action, string hostname, double protocol);

/**
 * Construct a new response string.
 *
 * - `body` defaults to `Not Found`, if `status` empty
 * - `status` defaults to `STATUS_NO_FOUND`, if `body` empty, otherwise `STATUS_OK`
 * - `type`
 * - `extras` additional headers - associative like "x-power-by: whatever" as `key=value;...`
 */
C_API string http_response(http_t *this, string body, http_status status, string type, string extras);

/**
 * Construct a new request string.
 *
 * - `extras` additional headers - associative like "x-power-by: whatever" as `key=value;...`
 */
C_API string http_request(http_t *this,
                          http_method method,
                          string path,
                          string type,
                          string connection,
                          string body_data,
                          string extras);

/**
 * Return a request header `content`.
 */
C_API string get_header(http_t *this, string key);

/**
 * Return a request header content `variable` value.
 *
 * - `key` header to check for
 * - `var` variable to find
 */
C_API string get_variable(http_t *this, string key, string var);

/**
 * Return a request parameter `value`.
 */
C_API string get_parameter(http_t *this, string key);

/**
 * Add or overwrite an response header parameter.
 */
C_API void put_header(http_t *this, string key, string value, bool force_cap);
C_API bool has_header(http_t *this, string key);
C_API bool has_variable(http_t *this, string key, string var);
C_API bool has_flag(http_t *this, string key, string flag);
C_API bool has_parameter(http_t *this, string key);

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
