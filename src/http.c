#include "url_http.h"

#if defined(_WIN32) || defined(_WIN64)
#   include "compat/dirent.h"
#else
#   include <dirent.h>
#endif

#if defined(_WIN32) || defined(_WIN64)
struct tm *gmtime_r(const time_t *timer, struct tm *buf) {
    int r = gmtime_s(buf, timer);
    if (r)
        return NULL;

    return buf;
}
#endif

static string_t method_strings[] = {
"DELETE", "GET", "HEAD", "POST", "PUT", "CONNECT", "OPTIONS", "TRACE", "COPY", "LOCK", "MKCOL", "MOVE", "PROPFIND", "PROPPATCH", "SEARCH", "UNLOCK", "REPORT", "MKACTIVITY", "CHECKOUT", "MERGE", "M-SEARCH", "NOTIFY", "SUBSCRIBE", "UNSUBSCRIBE", "PATCH", "PURGE"
};

static string_t const mon_short_names[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static string_t const day_short_names[] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

static char http_server_name[NAME_MAX] = HTTP_SERVER;
static char http_agent_name[NAME_MAX] = HTTP_AGENT;
static char http_std_timestamp[81] = nil;

#define PROXY_CONNECTION "proxy-connection"
#define CONNECTION "connection"
#define CONTENT_LENGTH "content-length"
#define TRANSFER_ENCODING "transfer-encoding"
#define UPGRADE "upgrade"
#define CHUNKED "chunked"
#define KEEP_ALIVE "keep-alive"
#define CLOSE "close"

RAII_INLINE void_t concat_headers(void_t line, string_t key, const_t value) {
	void_t lines = line;
	lines = str_cat_ex(nullptr, 5, (string)lines, key, ": ", value, CRLF);
	RAII_FREE(line);

    return lines;
}

/* Return date string in standard format for http headers */
string http_std_date(time_t t) {
	struct tm *tm1, tm_buf;
	if (t == 0) {
		time_t timer;
		t = time(&timer);
	}

	tm1 = gmtime_r(&t, &tm_buf);
	if (!tm1) {
		return http_std_timestamp;
	}

	snprintf(http_std_timestamp, sizeof(http_std_timestamp), "%s, %02d %s %04d %02d:%02d:%02d GMT",
		day_short_names[tm1->tm_wday],
		tm1->tm_mday,
		mon_short_names[tm1->tm_mon],
		tm1->tm_year + 1900,
		tm1->tm_hour, tm1->tm_min, tm1->tm_sec);

	return (http_std_timestamp);
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

	arrays_t p_headers = str_explode(this->raw, LFLF);
	count = $size(p_headers);
	this->body = count > 1 && !is_empty(p_headers[1].char_ptr)
		? trim(p_headers[1].char_ptr)
		: nullptr;

	string *lines = (count > 0 && !is_empty(p_headers[0].char_ptr))
		? str_split_ex(nullptr, p_headers[0].char_ptr, "\n", &count)
		: nullptr;

	if (count >= 1) {
		this->headers = hashtable_init(key_ops_string, val_ops_string, hash_lp_idx, SCRAPE_SIZE);
		deferring((func_t)hash_free, this->headers);
		string *parts = nullptr;
		string *params = nullptr;
		string delim = nullptr;
		for (x = 0; x < count; x++) {
			// clean the line
			string line = lines[x];
			bool found = false;
			if (is_str_in(line, ": ") || is_str_in(line, ":")) {
				delim = is_str_in(line, ": ") ? ": " : ":";
				found = true;
				parts = str_split_ex(nullptr, line, delim, nullptr);
			} else if (is_str_in(line, "HTTP/")) {
				params = str_split(line, " ", nullptr);
				if (this->action == HTTP_REQUEST) {
					this->method = trim(params[0]);
					this->path = trim(params[1]);
					this->protocol = trim(params[2]);
				} else if (this->action == HTTP_RESPONSE) {
					this->protocol = trim(params[0]);
					this->code = atoi(params[1]);
					this->message = trim(params[2]);
				}
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

    this->action = action;
    this->code = STATUS_OK;
    this->hostname = hostname;
    this->version = protocol;
    this->type = RAII_HTTPINFO;

    return this;
}

RAII_INLINE void http_user_agent(string name) {
	if (snprintf(http_agent_name, sizeof(http_agent_name), "%s", name))	return;
}

RAII_INLINE void http_server_agent(string name) {
	if (snprintf(http_server_name, sizeof(http_server_name), "%s", name)) return;
}

string http_response(http_t *this, string body, http_status status,
	string type, u32 header_pairs, ...) {
	va_list extras;
	header_types k;
	string key, val, lines;
	bool found, force_cap;
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
		? str_cat_ex(nullptr, 7,
			"<h1>",
			http_server_name,
			": ",
			simd_itoa(status, scrape3),
			" - ",
			http_status_str(status),
			"</h1>")
		: body;

	// Create a string out of the response data
	lines = str_cat_ex(nullptr, 19,
		// response status
		"HTTP/", (is_empty(this->protocol) ? gcvt(this->version, 2, scrape) : this->protocol), " ",
		simd_itoa(this->status, scrape3), " ", http_status_str(this->status), CRLF,
		// set initial headers
		"Date: ", http_std_date(0), CRLF,
		"Content-Type: ", (is_empty(type) ? "text/html" : type), "; charset=utf-8" CRLF,
		"Content-Length: ", simd_itoa(simd_strlen(this->body), scrape2), CRLF,
		"Server: ", http_server_name, CRLF
	);

	if (header_pairs > 0) {
		va_start(extras, header_pairs);
		for (i = 0; i < (int)header_pairs; i++) {
			found = false;
			force_cap = false;
			k = va_arg(extras, header_types);
			val = va_arg(extras, string);
			switch (k) {
				case head_cookie:
					found = true;
					key = "Set-Cookie";
					break;
				case head_secure:
					found = true;
					key = "Strict-Transport-Security";
					break;
				case head_conn:
					found = true;
					key = "Connection";
					break;
				case head_custom:
					found = true;
					force_cap = true;
					key = val;
					val = va_arg(extras, string);
					break;
				case head_by:
					found = true;
					key = "X-Powered-By";
					break;
			}

			if (found) {
				put_header(this, key, val, force_cap);
			}
		}
		va_end(extras);
	}

	// add the headers
	lines = hash_iter(this->header, lines, concat_headers);

	// Build a response header string based on the current line data.
	string headerString = str_concat(3, (string)lines, CRLF, this->body);
	RAII_FREE(lines);
	if (is_empty(body))
		RAII_FREE(this->body);

	return headerString;
}

string http_request(http_t *this, http_method method, string path, string type,
	string body_data, u32 header_pairs, ...) {
	va_list extras;
	header_types k;
	string key, val;
	char scrape[SCRAPE_SIZE];
	bool found;
	int x, count = 0;
	string localpath = path;

	if (is_str_in(path, "://"))
		this->uri = path;
	else
		this->uri = str_concat(2, (is_empty(this->hostname) ? "http://" : this->hostname), path);

	url_t *url_array = parse_uri(this->uri);
	string hostname = !is_empty(url_array->host) ? url_array->host : this->hostname;
	if (!is_empty(url_array->path)) {
		localpath = url_array->path;
		if (!is_empty(url_array->query)) {
			RAII_FREE(url_array->path);
			url_array->path = str_cat_ex(nullptr, 3, localpath, "?", url_array->query);
			RAII_FREE(localpath);
			localpath = url_array->path;
		}

		if (!is_empty(url_array->fragment)) {
			RAII_FREE(url_array->path);
			url_array->path = str_cat_ex(nullptr, 3, localpath, "#", url_array->fragment);
			RAII_FREE(localpath);
		}
	}

	string headers, header = str_cat_ex(nullptr, 13, method_strings[method], " ",
		(is_empty(path) ? "/" : url_array->path),
		" HTTP/", gcvt(this->version, 2, scrape), CRLF,
		"Host: ", hostname, CRLF,
		"Accept: */*" CRLF,
		"User-Agent: ", http_agent_name, CRLF
	);

	if (!is_empty(body_data)) {
		headers = str_cat_ex(nullptr, 7, header,
			"Content-Type: ", (is_empty(type) ? "text/html" : type), "; charset=utf-8" CRLF,
			"Content-Length: ", simd_itoa(simd_strlen(body_data), scrape), CRLF
		);
		RAII_FREE(header);
		header = headers;
	}

	if (header_pairs > 0) {
		va_start(extras, header_pairs);
		for (x = 0; x < (int)header_pairs; x++) {
			found = false;
			k = va_arg(extras, header_types);
			val = va_arg(extras, string);
			switch (k) {
				case head_cookie:
					found = true;
					key = "Set-Cookie";
					break;
				case head_secure:
					found = true;
					key = "Strict-Transport-Security";
					break;
				case head_conn:
					found = true;
					key = "Connection";
					break;
				case head_custom:
					found = true;
					key = word_toupper(val, '-');
					val = va_arg(extras, string);
					break;
				case head_by:
					found = true;
					key = "X-Powered-By";
					break;
			}

			if (found) {
				headers = str_cat_ex(nullptr, 5, header, key, ": ", val, CRLF);
				RAII_FREE(header);
				header = headers;
			}
		}
		va_end(extras);
	}

	headers = str_cat_ex(nullptr, 2, header, CRLF);
	RAII_FREE(header);
	header = headers;

	if (!is_empty(body_data)) {
		headers = str_cat_ex(nullptr, 2, header, body_data);
		RAII_FREE(header);
	}

	uri_free(url_array);
	deferring(RAII_FREE, headers);
	return headers;
}


RAII_INLINE string get_header(http_t *this, string key) {
    if (has_header(this, key)) {
        return hash_get((hash_t *)this->headers, key);
    }

    return "";
}

string get_variable(http_t *this, string key, string var) {
	int x, has_sect = 0, count = 1;
	string *sections, line;
	if (has_variable(this, key, var)) {
		line = get_header(this, key);
		if (is_str_in(line, "; ")) {
			sections = str_split_ex(nullptr, line, "; ", &count);
			has_sect = 1;
		} else {
			sections = (string *)line;
		}

		for (x = 0; x < count; x++) {
			string parts = sections[x], *variable = str_split_ex(nullptr, parts, "=", nullptr);
			if (is_str_eq(variable[0], var)) {
				if (has_sect)
					RAII_FREE(sections);

				deferring(RAII_FREE, variable);
				return !is_empty(variable[1]) ? variable[1] : nullptr;
			}
			RAII_FREE(variable);
		}
	}

	if (has_sect)
		RAII_FREE(sections);

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

    hash_put_str(this->header, temp, value);
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
