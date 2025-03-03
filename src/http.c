#include "url_http.h"

static string_t method_strings[] = {
"DELETE", "GET", "HEAD", "POST", "PUT", "CONNECT", "OPTIONS", "TRACE", "COPY", "LOCK", "MKCOL", "MOVE", "PROPFIND", "PROPPATCH", "SEARCH", "UNLOCK", "REPORT", "MKACTIVITY", "CHECKOUT", "MERGE", "M-SEARCH", "NOTIFY", "SUBSCRIBE", "UNSUBSCRIBE", "PATCH", "PURGE"
};

static string_t const mon_short_names[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static string_t const day_short_names[] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

#define PROXY_CONNECTION "proxy-connection"
#define CONNECTION "connection"
#define CONTENT_LENGTH "content-length"
#define TRANSFER_ENCODING "transfer-encoding"
#define UPGRADE "upgrade"
#define CHUNKED "chunked"
#define KEEP_ALIVE "keep-alive"
#define CLOSE "close"

RAII_INLINE void_t concat_headers(void_t lines, string_t key, const_t value) {
    lines = str_concat(5, (string)lines, key, ": ", value, CRLF);

    return lines;
}

/* Return date string in standard format for http headers */
string http_std_date(time_t t) {
    struct tm *tm1, tm_buf;
    string str;
    if (t == 0) {
        time_t timer;
        t = time(&timer);
    }

    tm1 = gmtime_r(&t, &tm_buf);
    str = calloc_local(1, 81);
    str[0] = '\0';

    if (!tm1) {
        return str;
    }

    snprintf(str, 80, "%s, %02d %s %04d %02d:%02d:%02d GMT",
             day_short_names[tm1->tm_wday],
             tm1->tm_mday,
             mon_short_names[tm1->tm_mon],
             tm1->tm_year + 1900,
             tm1->tm_hour, tm1->tm_min, tm1->tm_sec);

    str[79] = 0;
    return (str);
}

string_t http_status_str(uint16_t const status) {
    switch (status) {
        // Informational 1xx
        case 100: return "Continue";
        case 101: return "Switching Protocols";
        case 102: return "Processing"; // RFC 2518, obsoleted by RFC 4918
        case 103: return "Early Hints";

            // Success 2xx
        case 200: return "OK";
        case 201: return "Created";
        case 202: return "Accepted";
        case 203: return "Non-Authoritative Information";
        case 204: return "No Content";
        case 205: return "Reset Content";
        case 206: return "Partial Content";
        case 207: return "Multi-Status"; // RFC 4918
        case 208: return "Already Reported";
        case 226: return "IM Used";

            // Redirection 3xx
        case 300: return "Multiple Choices";
        case 301: return "Moved Permanently";
        case 302: return "Moved Temporarily";
        case 303: return "See Other";
        case 304: return "Not Modified";
        case 305: return "Use Proxy";
        case 306: return "Reserved";
        case 307: return "Temporary Redirect";
        case 308: return "Permanent Redirect";

            // Client Error 4xx
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 402: return "Payment Required";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 406: return "Not Acceptable";
        case 407: return "Proxy Authentication Required";
        case 408: return "Request Time-out";
        case 409: return "Conflict";
        case 410: return "Gone";
        case 411: return "Length Required";
        case 412: return "Precondition Failed";
        case 413: return "Request Entity Too Large";
        case 414: return "Request-URI Too Large";
        case 415: return "Unsupported Media Type";
        case 416: return "Requested Range Not Satisfiable";
        case 417: return "Expectation Failed";
        case 418: return "I'm a teapot";               // RFC 2324
        case 422: return "Unprocessable Entity";       // RFC 4918
        case 423: return "Locked";                     // RFC 4918
        case 424: return "Failed Dependency";          // RFC 4918
        case 425: return "Unordered Collection";       // RFC 4918
        case 426: return "Upgrade Required";           // RFC 2817
        case 428: return "Precondition Required";      // RFC 6585
        case 429: return "Too Many Requests";          // RFC 6585
        case 431: return "Request Header Fields Too Large";// RFC 6585
        case 444: return "Connection Closed Without Response";
        case 451: return "Unavailable For Legal Reasons";
        case 499: return "Client Closed Request";

            // Server Error 5xx
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        case 504: return "Gateway Time-out";
        case 505: return "HTTP Version Not Supported";
        case 506: return "Variant Also Negotiates";    // RFC 2295
        case 507: return "Insufficient Storage";       // RFC 4918
        case 509: return "Bandwidth Limit Exceeded";
        case 510: return "Not Extended";               // RFC 2774
        case 511: return "Network Authentication Required"; // RFC 6585
        case 599: return "Network Connect Timeout Error";
        default: return nullptr;
    }
}

hash_http_t *parse_str(string lines, string sep, string part) {
    hash_http_t *this = hashtable_init(key_ops_string, val_ops_string, hash_lp_idx, SCRAPE_SIZE);
    deferring((func_t)hash_free, this);
    if (is_empty((void_t)part))
        part = "=";

    int i = 0, x;
    string *token = str_split_ex(nullptr, lines, sep, &i);
    for (x = 0; x < i; x++) {
        string *parts = str_split_ex(nullptr, token[x], part, nullptr);
        if (!is_empty(parts)) {
            hash_put_str(this, trim(parts[0]), trim(parts[1]));
            RAII_FREE(parts);
        }
    }

    if (!is_empty(token))
        RAII_FREE(token);

    return this;
}

void parse_http(http_t *this, string headers) {
    int x = 0, count = 0;
    this->raw = headers;

    string *p_headers = str_split(this->raw, "\n\n", &count);
    this->body = (count > 1 && !is_empty(p_headers[1]))
        ? trim(p_headers[1]) : nullptr;
    string *lines = (count > 0 && !is_empty(p_headers[0]))
        ? str_split_ex(nullptr, p_headers[0], "\n", &count) : nullptr;

    if (count >= 1) {
        this->headers = hashtable_init(key_ops_string, val_ops_string, hash_lp_idx, SCRAPE_SIZE);
        deferring((func_t)hash_free, this->headers);
        string *parts = nullptr;
        string *params = nullptr;
        for (x = 0; x < count; x++) {
            // clean the line
            string line = lines[x];
            bool found = false;
            if (is_str_in(line, ": ")) {
                found = true;
                parts = str_split_ex(nullptr, line, ": ", nullptr);
            } else if (is_str_in(line, ":")) {
                found = true;
                parts = str_split_ex(nullptr, line, ":", nullptr);
            } else if (is_str_in(line, "HTTP/") && this->action == HTTP_REQUEST) {
                params = str_split(line, " ", nullptr);
                this->method = trim(params[0]);
                this->path = trim(params[1]);
                this->protocol = trim(params[2]);
            } else if (is_str_in(line, "HTTP/") && this->action == HTTP_RESPONSE) {
                params = str_split(line, " ", nullptr);
                this->protocol = trim(params[0]);
                this->code = atoi(params[1]);
                this->message = trim(params[2]);
            }

            if (found && !is_empty(parts)) {
                hash_put_str((hash_t *)this->headers, trim(parts[0]), trim(parts[1]));
                RAII_FREE(parts);
            }
        }

        if (this->action == HTTP_REQUEST) {
            // split path and parameters string
            params = str_split(this->path, "?", nullptr);
            this->path = params[0];
            string parameters = params[1];

            // parse the parameters
            if (!is_empty(parameters)) {
                this->parameters = parse_str(parameters, "&", nullptr);
            }
        }

        if (!is_empty(lines))
            RAII_FREE(lines);
    }
}

http_t *http_for(http_parser_type action, string hostname, double protocol) {
    http_t *this = calloc_local(1, sizeof(http_t));

    this->parameters = nullptr;
    this->header = nullptr;
    this->path = nullptr;
    this->method = nullptr;
    this->cookies = nullptr;
    this->code = STATUS_OK;
    this->message = nullptr;
    this->action = action;
    this->hostname = hostname;
    this->version = protocol;
    this->type = RAII_HTTPINFO;

    return this;
}

string http_response(http_t *this, string body, http_status status, string type, string extras) {
    char scrape[SCRAPE_SIZE];
    char scrape2[SCRAPE_SIZE];
    char scrape3[SCRAPE_SIZE];
    int x, i = 0;
    if (!is_zero(status)) {
        this->status = status;
    }

    if (is_empty(body) && is_zero(status)) {
        this->status = status = STATUS_NOT_FOUND;
    }

    this->body = is_empty(body)
        ? str_concat(7,
                     "<h1>",
                     HTTP_SERVER,
                     ": ",
                     simd_itoa(status, scrape3),
                     " - ",
                     http_status_str(status),
                     "</h1>")
        : body;

    // Create a string out of the response data
    string lines = str_concat(20,
                              // response status
                              "HTTP/", (is_empty(this->protocol) ? gcvt(this->version, 2, scrape) : this->protocol), " ",
                              simd_itoa(this->status, scrape3), " ", http_status_str(this->status), CRLF,
                              // set initial headers
                              "Date: ", http_std_date(0), CRLF,
                              "Content-Type: ", (is_empty(type) ? "text/html" : type), "; charset=utf-8", CRLF,
                              "Content-Length: ", simd_itoa(simd_strlen(this->body), scrape2), CRLF,
                              "Server: ", HTTP_SERVER, CRLF
    );

    if (!is_empty(extras)) {
        string *token = str_split(extras, ";", &i);
        for (x = 0; x < i; x++) {
            string *parts = str_split(token[x], "=", nullptr);
            if (!is_empty(parts))
                put_header(this, parts[0], parts[1], true);
        }
    }

    // add the headers
    lines = hash_iter(this->header, lines, concat_headers);

    // Build a response header string based on the current line data.
    string headerString = str_concat(3, (string)lines, CRLF, this->body);

    return headerString;
}

string http_request(http_t *this,
                    http_method method,
                    string path,
                    string type,
                    string connection,
                    string body_data,
                    string extras
) {
    char scrape[SCRAPE_SIZE];
    this->uri = is_str_in(path, "://")
        ? path
        : str_concat(2, (is_empty(this->hostname) ? "http://" : this->hostname), path);
    url_t *url_array = parse_url(this->uri);
    string hostname = !is_empty(url_array->host) ? url_array->host : this->hostname;
    if (!is_empty(url_array->path)) {
        string path = url_array->path;
        if (!is_empty(url_array->query))
            path = str_concat(3, path, "?", url_array->query);

        if (!is_empty(url_array->fragment))
            path = str_concat(3, path, "#", url_array->fragment);
    } else {
        char path[] = "/";
    }

    string headers = str_concat(14, method_strings[method], " ", url_array->path,
                                " HTTP/", gcvt(this->version, 2, scrape), CRLF,
                                "Host: ", hostname, CRLF,
                                "Accept: */*", CRLF,
                                "User-Agent: ", HTTP_AGENT, CRLF
    );

    if (!is_empty(body_data)) {
        headers = str_concat(8, headers,
                             "Content-Type: ", (is_empty(type) ? "text/html" : type), "; charset=utf-8", CRLF,
                             "Content-Length: ", simd_itoa(simd_strlen(body_data), scrape), CRLF
        );
    }

    if (!is_empty(extras)) {
        int x, count = 0;
        string *lines = str_split(extras, ";", &count);
        for (x = 0; x < count; x++) {
            string *parts = str_split(lines[x], "=", nullptr);
            if (!is_empty(parts))
                headers = str_concat(5, headers, word_toupper(parts[0], '-'), ": ", parts[1], CRLF);
        }
    }

    headers = str_concat(5, headers,
                         "Connection: ", (is_empty(connection) ? "close" : connection), CRLF, CRLF
    );

    if (!is_empty(body_data))
        headers = str_concat(2, headers, body_data);

    return headers;
}

RAII_INLINE string get_header(http_t *this, string key) {
    if (has_header(this, key)) {
        return hash_get((hash_t *)this->headers, key);
    }

    return "";
}

string get_variable(http_t *this, string key, string var) {
    if (has_variable(this, key, var)) {
        int x, count = 1;
        string line = get_header(this, key);
        string *sections = (is_str_in(line, "; ")) ? str_split(line, "; ", &count) : (string *)line;
        for (x = 0; x < count; x++) {
            string parts = sections[x];
            string *variable = str_split(parts, "=", nullptr);
            if (is_str_eq(variable[0], var)) {
                return !is_empty(variable[1]) ? variable[1] : nullptr;
            }
        }
    }

    return "";
}

RAII_INLINE string get_parameter(http_t *this, string key) {
    if (has_parameter(this, key)) {
        return hash_get(this->parameters, key);
    }

    return "";
}

void put_header(http_t *this, string key, string value, bool force_cap) {
    string temp = key;
    if (is_empty(this->header)) {
        this->header = hashtable_init(key_ops_string, val_ops_string, hash_lp_idx, SCRAPE_SIZE);
        deferring((func_t)hash_free, this->header);
    }

    if (force_cap)
        temp = word_toupper(key, '-');

    hash_put_str(this->header, trim(temp), trim(value));
}

RAII_INLINE bool has_header(http_t *this, string key) {
    return hash_has((hash_t *)this->headers, key);
}

RAII_INLINE bool has_variable(http_t *this, string key, string var) {
    char temp[SCRAPE_SIZE] = {0};

    snprintf(temp, SCRAPE_SIZE, "%s%s", var, "=");
    return is_str_in(get_header(this, key), temp);
}

bool has_flag(http_t *this, string key, string flag) {
    char flag1[SCRAPE_SIZE] = {0};
    char flag2[SCRAPE_SIZE] = {0};
    char flag3[SCRAPE_SIZE] = {0};
    string value = get_header(this, key);

    snprintf(flag1, SCRAPE_SIZE, "%s%s", flag, ";");
    snprintf(flag2, SCRAPE_SIZE, "%s%s", flag, ",");
    snprintf(flag3, SCRAPE_SIZE, "%s%s", flag, "\r\n");
    return is_str_in(value, flag1) || is_str_in(value, flag2) || is_str_in(value, flag3);
}

RAII_INLINE bool has_parameter(http_t *this, string key) {
    return hash_has(this->parameters, key);
}
