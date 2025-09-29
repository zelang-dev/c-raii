#include "url_http.h"

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
static char http_timestamp[81] = nil;

#define PROXY_CONNECTION "proxy-connection"
#define CONNECTION "connection"
#define CONTENT_LENGTH "content-length"
#define TRANSFER_ENCODING "transfer-encoding"
#define UPGRADE "upgrade"
#define CHUNKED "chunked"
#define KEEP_ALIVE "keep-alive"
#define CLOSE "close"

struct cookie_s {
	bool httpOnly;
	bool secure;
	int maxAge;
	string value;
	string path;
	string expiries;
	string domain;
	string sameSite;
};

struct response_s {
	raii_type type;
	/* The current response status */
	http_status status;
	/* The protocol version */
	double version;
	/* The current response body */
	string body;
	/* The unchanged data from server */
	string raw;
	/* The protocol */
	string protocol;
	/* The current headers */
	hash_http_t *headers;
};

struct form_data_s {
	size_t bodysize;
	string body;
	string filename;
	string disposition;
	string type;
	string encoding;
};

struct http_s {
	raii_type type;
	/* Is Multipart `form_data` in header response? */
	bool is_multipart;
	/* This parser ~instance~ state,
	either `RESPONSE` or `REQUEST` behaviour. */
	http_parser_type action;
	/* The current response status */
	http_status status;
	/* The requested status code */
	http_status code;
	/* The protocol version */
	double version;
	string hostname;
	/* The unchanged data from server */
	string raw;
	/* The current response body */
	string body;
	/* The protocol */
	string protocol;
	/* The requested status message */
	string message;
	/* The requested method */
	string method;
	/* The requested path */
	string path;
	/* The requested uri */
	string uri;
	/* The multi-part `boundary` name */
	string boundary;
	/* Array of multi-part `disposition` names */
	arrays_t names;
	/* Array of set-cookie `session` names */
	arrays_t cookies;
	/* Parser, `request/response` staging allocations,
	WILL be freed at exit, and before `parse_http` execution. */
	arrays_t garbage;
	/* The current headers
	and `response` headers to send */
	hash_http_t *headers;
	/* The request params */
	hash_http_t *parameters;
	/* The multi-part dispositions, `form_data_t` */
	hash_http_t *dispositions;
	/* The set-cookie sessions, `cookie_t` */
	hash_http_t *sessions;
};

static void http_clear(http_t *this) {
	foreach(arr in this->garbage) {
		free(arr.object);
	}

	array_delete(this->garbage);
	this->garbage = nullptr;
}

static RAII_INLINE void_t concat_headers(void_t line, string_t key, const_t value) {
	void_t lines = line;
	lines = str_cat_ex(nullptr, 5, (string)lines, key, ": ", value, CRLF);
	free(line);

    return lines;
}

static RAII_INLINE void_t concat_cookies(void_t line, string_t key, const_t value) {
	void_t lines = line;
	cookie_t *cookie = (cookie_t *)value;
	lines = str_cat_ex(nullptr, 5, (string)lines, key, "=", cookie->value, "; ");
	free(line);

    return lines;
}

/* Return date string ahead in standard format for `Set-Cookie` headers */
string http_date_ahead(u32 days, u32 hours, u32 minutes, u32 seconds) {
	time_t time_ahead, current_time;
	struct tm *tm1, result;
	u32 date_ahead = 0;

	if (days) date_ahead = (24 * 3600 * days);
	if (hours) date_ahead = date_ahead + (hours * 3600);
	if (minutes) date_ahead = date_ahead + (minutes * 60);
	if (seconds) date_ahead = date_ahead + seconds;

	// Get the current time in seconds since the epoch
	time(&current_time);

	// Add time ahead
	time_ahead = current_time + date_ahead;
	return http_std_date(time_ahead);
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
		return http_timestamp;
	}

	snprintf(http_timestamp, sizeof(http_timestamp), "%s, %02d %s %04d %02d:%02d:%02d GMT",
		day_short_names[tm1->tm_wday],
		tm1->tm_mday,
		mon_short_names[tm1->tm_mon],
		tm1->tm_year + 1900,
		tm1->tm_hour, tm1->tm_min, tm1->tm_sec);

	return http_timestamp;
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

void parse_str(http_t *this, string lines, string sep, string part) {
	if (is_empty(this->parameters)) {
		this->parameters = hashtable_init(key_ops_string, val_ops_string, hash_lp_idx, SCRAPE_SIZE);
		deferring((func_t)hash_free, this->parameters);
	}

	if (is_empty((void_t)part))
        part = "=";

    int i = 0, x;
    string *token = str_split_ex(nullptr, lines, sep, &i);
    for (x = 0; x < i; x++) {
        string *parts = str_split_ex(nullptr, token[x], part, nullptr);
        if (!is_empty(parts)) {
			hash_put_str(this->parameters, trim(parts[0]), trim(parts[1]));
            free(parts);
        }
    }

    if (!is_empty(token))
        free(token);
}

static void parse_multipart(http_t *this) {
	if (this->is_multipart) {
		string key, value, line, parameters, delim = nullptr, keyname = nullptr,
			*boundary = nullptr, *boundaries = nullptr, *parts = nullptr, *params = nullptr,
			*lines = nullptr, *filenames = nullptr, *filename = nullptr;
		int x, y, sects = 0, names = 0, count = 0, pieces = 0;
		form_data_t *multipart;
		char scrape[SCRAPE_SIZE];
		bool found;

		snprintf(scrape, SCRAPE_SIZE, "--%s", this->boundary);
		boundary = str_split_ex(nullptr, this->raw, scrape, &count);
		$append(this->garbage, boundary);
		if (--count >= 2 && is_str_eq(boundary[count], "--")) {
			if (!is_empty(this->dispositions)) {
				hash_free(this->dispositions);
				this->dispositions = nullptr;
			}

			this->dispositions = hash_create_ex(SCRAPE_SIZE);
			if (is_empty(this->names))
				this->names = arrays();
			else
				$reset(this->names);

			for (x = 1; x < count; x++) {
				boundaries = str_split_ex(nullptr, boundary[x], LFLF, &pieces);
				$append(this->garbage, boundaries);
				if (pieces > 1) {
					multipart = calloc(1, sizeof(form_data_t));
					if (is_empty(multipart))
						break;

					$append(this->garbage, multipart);
					multipart->body = trim(boundaries[1]);
					// Calculate each multipart body size by memory region used
					size_t headsize = ((uintptr_t)boundaries[1] - (uintptr_t)boundaries[0]);
					size_t boundarysize = ((uintptr_t)boundary[x + 1] - (uintptr_t)boundary[x]);
					multipart->bodysize = boundarysize - headsize - simd_strlen(scrape) - 1;
					lines = str_split_ex(nullptr, trim(boundaries[0]), "\n", &sects);
					if (sects >= 1) {
						for (y = 0; y < sects; y++) {
							line = lines[y];
							found = false;
							if (is_str_in(line, ": ") || is_str_in(line, ":")) {
								delim = is_str_in(line, ": ") ? ": " : ":";
								found = true;
								parts = str_split_ex(nullptr, line, delim, nullptr);
							}

							if (found && !is_empty(parts)) {
								key = trim(parts[0]);
								value = trim(parts[1]);
								if (is_str_eq(key, "Content-Disposition")) {
									params = str_split_ex(nullptr, value, "; ", &names);
									$append(this->garbage, params);
									multipart->disposition = params[0];
									filenames = str_split_ex(nullptr, params[1], "\"", nullptr);
									$append(this->garbage, filenames);
									keyname = trim(filenames[1]);
									$append_string(this->names, keyname);
									if (names > 2) {
										filename = str_split_ex(nullptr, params[2], "\"", nullptr);
										$append(this->garbage, filename);
										multipart->filename = trim(filename[1]);
									}
								} else if (is_str_eq(key, "Content-Type")) {
									multipart->type = value;
								} else if (is_str_eq(key, "Content-Transfer-Encoding")) {
									multipart->encoding = value;
								}

								free(parts);
								parts = nullptr;
							}
						}

						if (keyname) {
							hash_put(this->dispositions, keyname, multipart);
							keyname = nullptr;
						}
					}
					free(lines);
				}
			}
		}
	}
}

void parse_http(http_parser_type action, http_t *this, string headers) {
	string key, value, line, parameters, delim = nullptr,
		*sessions ,*messages, *lines = nullptr, *parts = nullptr;
	int i = 0, x = 0, z = 0, count = 0;
	bool is_multi_set = false, is_cookie_set = false, found = false;
	if (!is_empty(this->garbage))
		http_clear(this);

	if (!is_empty(this->cookies))
		$reset(this->cookies);

	if (!is_empty(this->sessions)) {
		hash_free(this->sessions);
		this->sessions = nullptr;
	}

	messages = str_split_ex(nullptr, headers, LFLF, &count);
	this->garbage = arrays_ex(1, messages);
	this->raw = headers;
	this->action = action;
	this->body = count > 1 && !is_empty(messages[1])
		? trim(messages[1])
		: nullptr;

	lines = (count > 0 && !is_empty(messages[0]))
		? str_split_ex(nullptr, messages[0], "\n", &count)
		: nullptr;

	if (count >= 1) {
		if (is_empty(this->headers)) {
			this->headers = hashtable_init(key_ops_string, val_ops_string, hash_lp_idx, SCRAPE_SIZE);
			deferring((func_t)hash_free, this->headers);
		}

		for (x = 0; x < count; x++) {
			// clean the line
			line = lines[x];
			found = false;
			is_cookie_set = false;
			if (is_str_in(line, ": ") || is_str_in(line, ":")) {
				delim = is_str_in(line, ": ") ? ": " : ":";
				found = true;
				parts = str_split_ex(nullptr, line, delim, nullptr);
			} else if (is_str_in(line, "HTTP/")) {
				string *params = str_split_ex(nullptr, line, " ", nullptr);
				$append(this->garbage, params);
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
				key = trim(parts[0]);
				value = trim(parts[1]);
				if (!is_multi_set && is_str_eq(key, "Content-Type")
					&& is_str_in(value, "multipart/form-data; boundary=")) {
					is_multi_set = true;
					string *para = str_split_ex(nullptr, value, "; boundary=", nullptr);
					this->is_multipart = true;
					this->body = nullptr;
					this->boundary = para[1];
					$append(this->garbage, para);
				} else if (is_str_eq(key, "Set-Cookie")) {
					if (is_empty(this->sessions))
						this->sessions = hash_create_ex(SCRAPE_SIZE);

					is_cookie_set = true;
					cookie_t *cookie = try_calloc(1, sizeof(cookie_t));
					$append(this->garbage, cookie);
					if (is_str_in(value, "Secure"))
						cookie->secure = true;

					if (is_str_in(value, "HttpOnly"))
						cookie->httpOnly = true;

					string *sess = str_split_ex(nullptr, value, "; ", &z);
					string session_name = nullptr;
					for (i = 0; i < z; i++) {
						sessions = str_split_ex(nullptr, sess[i], "=", nullptr);
						$append(this->garbage, sessions);
						if (!is_empty(sessions) && !is_empty(sessions[0])) {
							if (is_str_eq(sessions[0], "Path")) {
								cookie->path = trim(sessions[1]);
							} else if (is_str_eq(sessions[0], "Expires")) {
								cookie->expiries = sessions[1];
							} else if (is_str_eq(sessions[0], "Max-Age")) {
								cookie->maxAge = simd_atoi(sessions[1], 20);
							} else if (is_str_eq(sessions[0], "SameSite")) {
								cookie->sameSite = sessions[1];
							} else if (is_str_eq(sessions[0], "Domain")) {
								cookie->domain = sessions[1];
							} else if (!is_empty(sessions[1]) && is_empty(session_name)) {
								session_name = sessions[0];
								cookie->value = sessions[1];
								if (is_empty(this->cookies))
									this->cookies = arrays();

								$append_string(this->cookies, session_name);
							}
						}
					}

					if (session_name) {
						hash_put(this->sessions, session_name, cookie);
					}

					free(sess);
				}

				hash_put_str(this->headers, key, value);
			}

			if (!is_empty(parts)) {
				free(parts);
				parts = nullptr;
			}
		}

		parse_multipart(this);
		// split path and parameters string
		if (is_str_in(this->path, "?")) {
			string *param = str_split_ex(nullptr, this->path, "?", nullptr);
			this->path = param[0];
			parameters = param[1];
			$append(this->garbage, param);

			// parse the parameters
			if (!is_empty(parameters)) {
				parse_str(this, parameters, "&", nullptr);
			}
		}

		if (!is_empty(lines))
			free(lines);
	}
}

void http_free(http_t *this) {
	if (is_type(this, RAII_HTTPINFO)) {
		this->type = RAII_ERR;
		if (!is_empty(this->sessions)) {
			hash_free(this->sessions);
			this->sessions = nullptr;
		}

		if (!is_empty(this->dispositions)) {
			hash_free(this->dispositions);
			this->dispositions = nullptr;
		}

		if (!is_empty(this->garbage))
			http_clear(this);

		free(this);
	}
}

http_t *http_for(string hostname, double protocol) {
    http_t *this = try_calloc(1, sizeof(http_t));

    this->names = nullptr;
    this->cookies = nullptr;
    this->garbage = nullptr;
    this->sessions = nullptr;
    this->dispositions = nullptr;
    this->is_multipart = false;
    this->code = STATUS_OK;
    this->status = STATUS_NO_CONTENT;
    this->hostname = hostname;
    this->version = protocol;
    this->type = RAII_HTTPINFO;
	deferring((func_t)http_free, this);

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
	string key, val, lines, body_data;
	bool found, force_cap;
	char scrape[SCRAPE_SIZE];
	char scrape2[SCRAPE_SIZE];
	char scrape3[SCRAPE_SIZE];
	char auth[NAME_MAX];
	int x, i = 0;

	if (!is_empty(this->headers)) {
		hash_delete(this->headers, "Date");
		hash_delete(this->headers, "Content-Length");
		hash_delete(this->headers, "Content-Type");
		hash_delete(this->headers, "Host");
		hash_delete(this->headers, "Accept");
		hash_delete(this->headers, "User-Agent");
		hash_delete(this->headers, "Server");
	}

	if (!is_zero(status)) {
		this->status = status;
	}

	if (is_empty(body) && is_zero(status)) {
		this->status = status = STATUS_NOT_FOUND;
	}

	body_data = is_empty(body)
		? str_cat_ex(nullptr, 7,
			"<h1>", http_server_name, ": ", simd_itoa(status, scrape3), " - ", http_status_str(status), "</h1>")
		: body;

	// Create a string out of the response data
	lines = str_cat_ex(nullptr, 19,
		// response status
		"HTTP/", (is_empty(this->protocol) ? gcvt(this->version, 2, scrape) : this->protocol), " ",
		simd_itoa(this->status, scrape3), " ", http_status_str(this->status), CRLF,
		// set initial headers
		"Date: ", http_std_date(0), CRLF,
		"Content-Type: ", (is_empty(type) ? "text/html" : type), "; charset=utf-8" CRLF,
		"Content-Length: ", simd_itoa(simd_strlen(body_data), scrape2), CRLF,
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
				case head_bearer:
					found = true;
					key = "Authorization";
					snprintf(auth, NAME_MAX, "Bearer %s", val);
					val = auth;
					break;
				case head_auth_basic:
					found = true;
					key = "Authorization";
					snprintf(auth, NAME_MAX, "Basic %s", val);
					val = auth;
					break;
				case head_by:
					found = true;
					key = "X-Powered-By";
					break;
				case head_custom:
					found = true;
					force_cap = true;
					key = val;
					val = va_arg(extras, string);
					break;
			}

			if (found) {
				http_put_header(this, key, val, force_cap);
			}
		}
		va_end(extras);
	}

	// add the current stored response headers
	lines = hash_iter(this->headers, lines, concat_headers);

	// Build a response header string based on the current line data.
	string resp = str_cat_ex(nullptr, 3, (string)lines, CRLF, body_data);
	free(lines);
	if (is_empty(body))
		free(body_data);

	if (is_empty(this->garbage))
		this->garbage = arrays_ex(1, resp);
	else
		$append(this->garbage, resp);

	return resp;
}

string http_request(http_t *this, http_method_type method, string path, string type,
	string body_data, u32 header_pairs, ...) {
	va_list extras;
	header_types k;
	string key, val;
	char scrape[SCRAPE_SIZE];
	char auth[NAME_MAX] = nil;
	bool found = true;
	int x, count = 0;
	string localpath = path;

	this->uri = path;
	if (!is_str_in(path, "://")) {
		found = false;
		this->uri = str_cat_ex(nullptr, 2, (is_empty(this->hostname) ? "http://" : this->hostname), path);
	}

	if (is_empty(this->garbage) && !found)
		this->garbage = arrays_ex(1, this->uri);
	else if (!found)
		$append(this->garbage, this->uri);

	url_t *url_array = parse_uri(this->uri);
	string hostname = !is_empty(url_array->host) ? url_array->host : this->hostname;
	if (!is_empty(url_array->path)) {
		localpath = url_array->path;
		if (!is_empty(url_array->query)) {
			free(url_array->path);
			url_array->path = str_cat_ex(nullptr, 3, localpath, "?", url_array->query);
			free(localpath);
			localpath = url_array->path;
		}

		if (!is_empty(url_array->fragment)) {
			free(url_array->path);
			url_array->path = str_cat_ex(nullptr, 3, localpath, "#", url_array->fragment);
			free(localpath);
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
		free(header);
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
					key = "Cookie";
					break;
				case head_secure:
					found = true;
					key = "Strict-Transport-Security";
					break;
				case head_conn:
					found = true;
					key = "Connection";
					break;
				case head_bearer:
					found = true;
					key = "Authorization";
					snprintf(auth, NAME_MAX, "Bearer %s", val);
					val = auth;
					break;
				case head_auth_basic:
					found = true;
					key = "Authorization";
					snprintf(auth, NAME_MAX, "Basic %s", val);
					val = auth;
					break;
				case head_by:
					found = true;
					key = "X-Powered-By";
					break;
				case head_custom:
					found = true;
					key = word_toupper(val, '-');
					val = va_arg(extras, string);
					break;
			}

			if (found) {
				headers = str_cat_ex(nullptr, 5, header, key, ": ", val, CRLF);
				free(header);
				header = headers;
			}
		}
		va_end(extras);
	}

	// add the current stored `set-cookie` response headers
	if (!is_empty(this->sessions) && !is_empty(this->cookies) && $size(this->cookies) >= 1) {
		headers = str_cat_ex(nullptr, 2, header, CRLF "Cookie: ");
		free(header);
		header = headers;
		headers = hash_iter(this->sessions, header, concat_cookies);
		headers[simd_strlen(headers) - 2] = ' ';
		header = headers;
		headers = str_cat_ex(nullptr, 2, header, CRLF);
		free(header);
		header = headers;
	}

	headers = str_cat_ex(nullptr, 2, header, CRLF);
	free(header);
	header = headers;

	if (!is_empty(body_data)) {
		headers = str_cat_ex(nullptr, 2, header, body_data);
		free(header);
	}

	uri_free(url_array);
	if (is_empty(this->garbage))
		this->garbage = arrays_ex(1, headers);
	else
		$append(this->garbage, headers);

	return headers;
}

RAII_INLINE bool http_multi_has(http_t *this, string name) {
	if (is_empty(this->dispositions))
		return false;

	return hash_has(this->dispositions, name);
}

RAII_INLINE size_t http_multi_length(http_t *this, string name) {
	if (is_empty(this->dispositions))
		return 0;

	form_data_t *form = (form_data_t *)hash_get_value(this->dispositions, name)->object;
	return is_empty(form) ? 0 : form->bodysize;
}

RAII_INLINE string http_multi_body(http_t *this, string name) {
	if (is_empty(this->dispositions))
		return nullptr;

	form_data_t *form = (form_data_t *)hash_get_value(this->dispositions, name)->object;
	return is_empty(form) ? nullptr : form->body;
}

RAII_INLINE string http_multi_disposition(http_t *this, string name) {
	if (is_empty(this->dispositions))
		return nullptr;

	form_data_t *form = (form_data_t *)hash_get_value(this->dispositions, name)->object;
	return is_empty(form) ? nullptr : form->disposition;
}

RAII_INLINE string http_multi_type(http_t *this, string name) {
	if (is_empty(this->dispositions))
		return nullptr;

	form_data_t *form = (form_data_t *)hash_get_value(this->dispositions, name)->object;
	return is_empty(form) ? nullptr : form->type;
}

RAII_INLINE bool http_multi_is_file(http_t *this, string name) {
	if (is_empty(this->dispositions))
		return nullptr;

	form_data_t *form = (form_data_t *)hash_get_value(this->dispositions, name)->object;
	return is_empty(form) ? false : !is_empty(form->filename);
}

RAII_INLINE string http_multi_filename(http_t *this, string name) {
	if (is_empty(this->dispositions))
		return nullptr;

	form_data_t *form = (form_data_t *)hash_get_value(this->dispositions, name)->object;
	return is_empty(form) ? nullptr : form->filename;
}

RAII_INLINE size_t http_multi_count(http_t *this) {
	return $size(this->names);
}

RAII_INLINE arrays_t http_multi_names(http_t *this) {
	return this->names;
}

RAII_INLINE bool http_is_multipart(http_t *this) {
	return this->is_multipart;
}

RAII_INLINE bool http_cookie_is_multi(http_t *this) {
	return is_empty(this->cookies) ? false : $size(this->cookies) > 1;
}

RAII_INLINE arrays_t http_cookie_names(http_t *this) {
	return is_empty(this->cookies) ? nullptr : this->cookies;
}

RAII_INLINE size_t http_cookie_count(http_t *this) {
	return is_empty(this->cookies) ? 0 : $size(this->cookies);
}

RAII_INLINE bool http_cookie_is_secure(http_t *this, string name) {
	if (is_empty(this->sessions))
		return false;

	cookie_t *cookie = (cookie_t *)hash_get_value(this->sessions, name)->object;
	return is_empty(cookie) ? false : cookie->secure;
}

RAII_INLINE bool http_cookie_is_httpOnly(http_t *this, string name) {
	if (is_empty(this->sessions))
		return false;

	cookie_t *cookie = (cookie_t *)hash_get_value(this->sessions, name)->object;
	return is_empty(cookie) ? false : cookie->httpOnly;
}

RAII_INLINE size_t http_cookie_maxAge(http_t *this, string name) {
	if (is_empty(this->sessions))
		return 0;

	cookie_t *cookie = (cookie_t *)hash_get_value(this->sessions, name)->object;
	return is_empty(cookie) ? 0 : (size_t)cookie->maxAge;
}

RAII_INLINE string http_get_cookie(http_t *this, string name) {
	if (is_empty(this->sessions))
		return nullptr;

	cookie_t *cookie = (cookie_t *)hash_get_value(this->sessions, name)->object;
	return is_empty(cookie) ? nullptr : cookie->value;
}

RAII_INLINE string http_cookie_expiries(http_t *this, string name) {
	if (is_empty(this->sessions))
		return nullptr;

	cookie_t *cookie = (cookie_t *)hash_get_value(this->sessions, name)->object;
	return is_empty(cookie) ? nullptr : cookie->expiries;
}

RAII_INLINE string http_cookie_domain(http_t *this, string name) {
	if (is_empty(this->sessions))
		return nullptr;

	cookie_t *cookie = (cookie_t *)hash_get_value(this->sessions, name)->object;
	return is_empty(cookie) ? nullptr : cookie->domain;
}

RAII_INLINE string http_cookie_sameSite(http_t *this, string name) {
	if (is_empty(this->sessions))
		return nullptr;

	cookie_t *cookie = (cookie_t *)hash_get_value(this->sessions, name)->object;
	return is_empty(cookie) ? nullptr : cookie->sameSite;
}

RAII_INLINE string http_cookie_path(http_t *this, string name) {
	if (is_empty(this->sessions))
		return nullptr;

	cookie_t *cookie = (cookie_t *)hash_get_value(this->sessions, name)->object;
	return is_empty(cookie) ? nullptr : cookie->path;
}

RAII_INLINE string http_get_protocol(http_t *this) {
	return this->protocol;
}

RAII_INLINE string http_get_boundary(http_t *this) {
	return this->boundary;
}

RAII_INLINE string http_get_path(http_t *this) {
	return this->path;
}

RAII_INLINE double http_get_version(http_t *this) {
	return this->version;
}

RAII_INLINE string http_get_method(http_t *this) {
	return this->method;
}

RAII_INLINE http_status http_get_code(http_t *this) {
	return this->code;
}

RAII_INLINE http_status http_get_status(http_t *this) {
	return this->status;
}

RAII_INLINE string http_get_message(http_t *this) {
	return this->message;
}

RAII_INLINE string http_get_body(http_t *this) {
	return this->body;
}

RAII_INLINE string http_get_url(http_t *this) {
	return this->uri;
}

RAII_INLINE string http_get_header(http_t *this, string key) {
	return is_empty(this->headers) ? nullptr : (string)hash_get(this->headers, key);
}

string http_get_var(http_t *this, string key, string var) {
	int x, has_sect = 0, count = 1;
	string *sections, line;
	if (http_has_var(this, key, var)) {
		line = http_get_header(this, key);
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
					free(sections);

				$append(this->garbage, variable);
				return !is_empty(variable[1]) ? variable[1] : nullptr;
			}
			free(variable);
		}
	}

	if (has_sect)
		free(sections);

	return "";
}

RAII_INLINE string http_get_param(http_t *this, string key) {
	return (string)hash_get(this->parameters, key);
}

void http_put_header(http_t *this, string key, string value, bool force_cap) {
    string temp = key;
    if (is_empty(this->headers)) {
        this->headers = hashtable_init(key_ops_string, val_ops_string, hash_lp_idx, SCRAPE_SIZE);
        deferring((func_t)hash_free, this->headers);
    }

    if (force_cap)
        temp = word_toupper(key, '-');

    hash_put_str(this->headers, temp, value);
}

RAII_INLINE bool http_has_header(http_t *this, string key) {
	return is_empty(this->headers) ? false : hash_has((hash_t *)this->headers, key);
}

RAII_INLINE bool http_has_var(http_t *this, string key, string var) {
    char temp[SCRAPE_SIZE] = {0};

    snprintf(temp, SCRAPE_SIZE, "%s%s", var, "=");
	return is_empty(this->headers) ? false : is_str_in(http_get_header(this, key), temp);
}

bool http_has_flag(http_t *this, string key, string flag) {
    char flag1[SCRAPE_SIZE] = {0};
    char flag2[SCRAPE_SIZE] = {0};
    char flag3[SCRAPE_SIZE] = {0};
    string value = http_get_header(this, key);

    snprintf(flag1, SCRAPE_SIZE, "%s%s", flag, ";");
    snprintf(flag2, SCRAPE_SIZE, "%s%s", flag, ",");
    snprintf(flag3, SCRAPE_SIZE, "%s%s", flag, "\r\n");
	return is_empty(this->headers) ? false : is_str_in(value, flag1) || is_str_in(value, flag2) || is_str_in(value, flag3);
}

RAII_INLINE bool http_has_param(http_t *this, string key) {
	return is_empty(this->parameters) ? false : hash_has(this->parameters, key);
}

string http_cookie(http_t *this, string_t key, string_t value, string_t path,
	string_t expire, int64_t maxage, string_t samesite, string_t domain, bool secure) {
	char maxage_str[22] = nil;
	string cookie, cookies = str_cat_ex(nullptr, 4, key, "=", value, (this->action == HTTP_RESPONSE ? "; " : ", "));

	cookie = cookies;
	if (path) {
		cookies = str_cat_ex(nullptr, 4, cookie, "Path=", path, "; ");
		free(cookie);
		cookie = cookies;
	}

	if (expire) {
		cookies = str_cat_ex(nullptr, 4, cookie, "Expires=", expire, "; ");
		free(cookie);
		cookie = cookies;
	}

	if (maxage) {
		cookies = str_cat_ex(nullptr, 4, cookie, "Max-Age=", simd_itoa(maxage, maxage_str), "; ");
		free(cookie);
		cookie = cookies;
	}

	if (domain) {
		cookies = str_cat_ex(nullptr, 4, cookie, "Domain=", domain, "; ");
		free(cookie);
		cookie = cookies;
	}

	if (samesite) {
		cookies = str_cat_ex(nullptr, 4, cookie, "SameSite=", samesite, "; ");
		free(cookie);
		cookie = cookies;
	}

	cookies = str_cat_ex(nullptr, 3, cookie, "HttpOnly", (secure ? "; Secure" CRLF : CRLF));
	free(cookie);
	$append(this->garbage, cookies);

	return cookies;
}