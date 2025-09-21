#include "url_http.h"

#ifdef _WIN32
#include <sys/utime.h>

#define SLASH '\\'
#define DIR_SEP	';'
#define IS_SLASH(c)	((c) == '/' || (c) == '\\')
#define IS_SLASH_P(c)	(*(c) == '/' || \
        (*(c) == '\\' && !IsDBCSLeadByte(*(c-1))))

/* COPY_ABS_PATH is 2 under Win32 because by chance both regular absolute paths
   in the file system and UNC paths need copying of two characters */
#define COPY_ABS_PATH(path) 2
#define IS_UNC_PATH(path, len) \
	(len >= 2 && IS_SLASH(path[0]) && IS_SLASH(path[1]))
#define IS_ABS_PATH(path, len) \
	(len >= 2 && (/* is local */isalpha(path[0]) && path[1] == ':' || /* is UNC */IS_SLASH(path[0]) && IS_SLASH(path[1])))

#else
#include <dirent.h>

#define SLASH '/'

#ifdef __riscos__
#define DIR_SEP  ';'
#else
#define DIR_SEP  ':'
#endif

#define IS_SLASH(c)	((c) == '/')
#define IS_SLASH_P(c)	(*(c) == '/')
#endif

static raii_type scheme_type(string scheme) {
    if (is_str_in("http,tcp,ws,ftp", scheme)) {
        return RAII_SCHEME_TCP;
    } else if (is_str_in("https,tls,ssl,wss,ftps", scheme)) {
        return RAII_SCHEME_TLS;
    } else if (is_str_in("file,unix", scheme)) {
        return RAII_SCHEME_PIPE;
    } else if (is_str_eq(scheme, "udp")) {
        return RAII_SCHEME_UDP;
    } else {
		return -RAII_SCHEME_INVALID;
    }
}

static int htoi(string s) {
    int value;
    int c;

    c = ((unsigned char *)s)[0];
    if (isupper(c))
        c = tolower(c);
    value = (c >= '0' && c <= '9' ? c - '0' : c - 'a' + 10) * 16;

    c = ((unsigned char *)s)[1];
    if (isupper(c))
        c = tolower(c);
    value += c >= '0' && c <= '9' ? c - '0' : c - 'a' + 10;

    return (value);
}

#ifdef _WIN32
static bool _is_basename_start(string_t start, string_t pos) {
    if (pos - start >= 1
        && *(pos - 1) != '/'
        && *(pos - 1) != '\\') {
        if (pos - start == 1) {
            return true;
        } else if (*(pos - 2) == '/' || *(pos - 2) == '\\') {
            return true;
        } else if (*(pos - 2) == ':'
                   && _is_basename_start(start, pos - 2)) {
            return true;
        }
    }

    return false;
}

/* Returns the filename component of the path */
string basename(string_t s) {
    string c;
    string_t comp, cend;
    size_t inc_len, cnt;
    size_t len = simd_strlen(s);
    int state;

    comp = cend = c = (string)s;
    cnt = len;
    state = 0;
    while (cnt > 0) {
        inc_len = (*c == '\0' ? 1 : mblen(c, cnt));
        switch (inc_len) {
            case -2:
            case -1:
                inc_len = 1;
                break;
            case 0:
                goto quit_loop;
            case 1:
                if (*c == '/' || *c == '\\') {
                    if (state == 1) {
                        state = 0;
                        cend = c;
                    }
                    /* Catch relative paths in c:file.txt style. They're not to confuse
                       with the NTFS streams. This part ensures also, that no drive
                       letter traversing happens. */
                } else if ((*c == ':' && (c - comp == 1))) {
                    if (state == 0) {
                        comp = c;
                        state = 1;
                    } else {
                        cend = c;
                        state = 0;
                    }
                } else {
                    if (state == 0) {
                        comp = c;
                        state = 1;
                    }
                }
                break;
            default:
                if (state == 0) {
                    comp = c;
                    state = 1;
                }
                break;
        }
        c += inc_len;
        cnt -= inc_len;
    }

quit_loop:
    if (state == 1) {
        cend = c;
    }

    len = cend - comp;
    return str_trim(comp, len);
}
#endif

#ifdef _WIN32
/* Returns directory name component of path */
string dirname(string path) {
    size_t len = simd_strlen(path);
    string end = path + len - 1;
    unsigned int len_adjust = 0;

    /* Note that on Win32 CWD is per drive (heritage from CP/M).
     * This means dirname("c:foo") maps to "c:." or "c:" - which means CWD on C: drive.
     */
    if ((2 <= len) && isalpha((int)((u_string)path)[0]) && (':' == path[1])) {
        /* Skip over the drive spec (if any) so as not to change */
        path += 2;
        len_adjust += 2;
        if (2 == len) {
            /* Return "c:" on Win32 for dirname("c:").
             * It would be more consistent to return "c:."
             * but that would require making the string *longer*.
             */
            return path;
        }
    }

    if (len == 0) {
        /* Illegal use of this function */
        return nullptr;
    }

    /* Strip trailing slashes */
    while (end >= path && IS_SLASH_P(end)) {
        end--;
    }
    if (end < path) {
        /* The path only contained slashes */
        path[0] = SLASH;
        path[1] = '\0';
        return path;
    }

    /* Strip filename */
    while (end >= path && !IS_SLASH_P(end)) {
        end--;
    }
    if (end < path) {
        /* No slash found, therefore return '.' */
        path[0] = '.';
        path[1] = '\0';
        return path;
    }

    /* Strip slashes which came before the file name */
    while (end >= path && IS_SLASH_P(end)) {
        end--;
    }
    if (end < path) {
        path[0] = SLASH;
        path[1] = '\0';
        return path;
    }
    *(end + 1) = '\0';

    return path;
}
#endif

/* rfc1738:

   ...The characters ";",
   "/", "?", ":", "@", "=" and "&" are the characters which may be
   reserved for special meaning within a scheme...

   ...Thus, only alphanumerics, the special characters "$-_.+!*'(),", and
   reserved characters used for their reserved purposes may be used
   unencoded within a URL...

   For added safety, we only leave -_. unencoded.
 */
static unsigned char hex_chars[] = "0123456789ABCDEF";

static u_char_t tolower_map[256] = {
0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,
0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,
0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,
0x40,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,
0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x5b,0x5c,0x5d,0x5e,0x5f,
0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,
0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x7b,0x7c,0x7d,0x7e,0x7f,
0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8a,0x8b,0x8c,0x8d,0x8e,0x8f,
0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9a,0x9b,0x9c,0x9d,0x9e,0x9f,
0xa0,0xa1,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,0xa8,0xa9,0xaa,0xab,0xac,0xad,0xae,0xaf,
0xb0,0xb1,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xbb,0xbc,0xbd,0xbe,0xbf,
0xc0,0xc1,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0xca,0xcb,0xcc,0xcd,0xce,0xcf,
0xd0,0xd1,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,0xdb,0xdc,0xdd,0xde,0xdf,
0xe0,0xe1,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,0xeb,0xec,0xed,0xee,0xef,
0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9,0xfa,0xfb,0xfc,0xfd,0xfe,0xff
};

#define tolower_ascii(c) (tolower_map[(unsigned char)(c)])

int binary_strcasecmp(string_t s1, size_t len1, string_t s2, size_t len2)
{
    size_t len;
    int c1, c2;

    if (s1 == s2) {
        return 0;
    }

    len = MIN(len1, len2);
    while (len--) {
        c1 = tolower_ascii(*(unsigned char *)s1++);
        c2 = tolower_ascii(*(unsigned char *)s2++);
        if (c1 != c2) {
            return c1 - c2;
        }
    }

    return (int)(len1 - len2);
}

#define string_equals_literal_ci(str, c) \
	(simd_strlen(str) == sizeof(c) - 1 && !binary_strcasecmp(str, simd_strlen(str), (c), sizeof(c) - 1))

static string_t binary_strcspn(string_t s, string_t e, string_t chars) {
    while (*chars) {
        string_t p = memchr(s, *chars, e - s);
        if (p) {
            e = p;
        }
        chars++;
    }
    return e;
}

static string replace_ctrl_ex(string str, size_t len) {
    unsigned char *s = (unsigned char *)str;
    unsigned char *e = (unsigned char *)str + len;

    if (!str) {
        return (nullptr);
    }

    while (s < e) {

        if (iscntrl(*s)) {
            *s = '_';
        }
        s++;
    }

    return (str);
}

static RAII_INLINE string uri_dup(const_t src, size_t len) {
	string ptr = (string)try_calloc(1, len + 1);

	return memcpy(ptr, src, len);
}

static url_t *url_parse_ex2(char const *str, size_t length, bool *has_port, bool autofree) {
	url_t *ret = (autofree) ? calloc_local(1, sizeof(url_t)) : try_calloc(1, sizeof(uri_t));
	char port_buf[6];
	char const *s, *e, *p, *pp, *ue;

	ret->is_rejected = false;
	ret->is_autofreeable = autofree;

    *has_port = 0;
    s = str;
	ue = s + length;


    /* parse scheme */
    if ((e = memchr(s, ':', length)) && e != s) {
        /* validate scheme */
        p = s;
        while (p < e) {
            /* scheme = 1*[ lowalpha | digit | "+" | "-" | "." ] */
            if (!isalpha(*p) && !isdigit(*p) && *p != '+' && *p != '.' && *p != '-') {
                if (e + 1 < ue && e < binary_strcspn(s, ue, "?#")) {
                    goto parse_port;
                } else if (s + 1 < ue && *s == '/' && *(s + 1) == '/') { /* relative-scheme URL */
                    s += 2;
                    e = 0;
                    goto parse_host;
                } else {
                    goto just_path;
                }
            }
            p++;
        }

		if (e + 1 == ue) { /* only scheme is available */
			ret->scheme = (autofree) ? str_trim(s, (e - s)) : uri_dup(s, (e - s));
			replace_ctrl_ex(ret->scheme, simd_strlen(ret->scheme));
            return ret;
        }

        /*
         * certain schemas like mailto: and zlib: may not have any / after them
         * this check ensures we support those.
         */
        if (*(e + 1) != '/') {
            /* check if the data we get is a port this allows us to
             * correctly parse things like a.com:80
             */
            p = e + 1;
            while (p < ue && isdigit(*p)) {
                p++;
            }

            if ((p == ue || *p == '/') && (p - e) < 7) {
                goto parse_port;
			}

			ret->scheme = (autofree) ? str_trim(s, (e - s)) : uri_dup(s, (e - s));
            replace_ctrl_ex(ret->scheme, simd_strlen(ret->scheme));

            s = e + 1;
            goto just_path;
		} else {
			ret->scheme = (autofree) ? str_trim(s, (e - s)) : uri_dup(s, (e - s));
            replace_ctrl_ex(ret->scheme, simd_strlen(ret->scheme));

            if (e + 2 < ue && *(e + 2) == '/') {
                s = e + 3;
                if (string_equals_literal_ci(ret->scheme, "file")) {
                    if (e + 3 < ue && *(e + 3) == '/') {
                        /* support windows drive letters as in:
                           file:///c:/somedir/file.txt
                        */
                        if (e + 5 < ue && *(e + 5) == ':') {
                            s = e + 4;
                        }
                        goto just_path;
                    }
                }
            } else {
                s = e + 1;
                goto just_path;
            }
        }
    } else if (e) { /* no scheme; starts with colon: look for port */
    parse_port:
        p = e + 1;
        pp = p;

        while (pp < ue && pp - p < 6 && isdigit(*pp)) {
            pp++;
        }

        if (pp - p > 0 && pp - p < 6 && (pp == ue || *pp == '/')) {
            long port;
            string end;
            memcpy(port_buf, p, (pp - p));
            port_buf[pp - p] = '\0';
            port = strtol(port_buf, &end, 10);
            if (port >= 0 && port <= 65535 && end != port_buf) {
                *has_port = 1;
                ret->port = (unsigned short)port;
                if (s + 1 < ue && *s == '/' && *(s + 1) == '/') { /* relative-scheme URL */
                    s += 2;
                }
			} else {
				ret->is_rejected = true;
				return (autofree) ? NULL : ret;
            }
		} else if (p == pp && pp == ue) {
			ret->is_rejected = true;
			return (autofree) ? NULL : ret;
        } else if (s + 1 < ue && *s == '/' && *(s + 1) == '/') { /* relative-scheme URL */
            s += 2;
        } else {
            goto just_path;
        }
    } else if (s + 1 < ue && *s == '/' && *(s + 1) == '/') { /* relative-scheme URL */
        s += 2;
    } else {
        goto just_path;
    }

parse_host:
    e = binary_strcspn(s, ue, "/?#");

    /* check for login and password */
    if ((p = str_memrchr(s, '@', (e - s)))) {
		if ((pp = memchr(s, ':', (p - s)))) {
			ret->user = (autofree) ? str_trim(s, (pp - s)) : uri_dup(s, (pp - s));
            replace_ctrl_ex(ret->user, simd_strlen(ret->user));

            pp++;
			ret->pass = (autofree) ? str_trim(pp, (p - pp)) : uri_dup(pp, (p - pp));
            replace_ctrl_ex(ret->pass, simd_strlen(ret->pass));
        } else {
			ret->user = (autofree) ? str_trim(s, (p - s)) : uri_dup(s, (p - s));
            replace_ctrl_ex(ret->user, simd_strlen(ret->user));
        }

        s = p + 1;
    }

    /* check for port */
    if (s < ue && *s == '[' && *(e - 1) == ']') {
        /* Short circuit portscan,
           we're dealing with an
           IPv6 embedded address */
        p = NULL;
    } else {
        p = str_memrchr(s, ':', (e - s));
    }

    if (p) {
        if (!ret->port) {
            p++;
			if (e - p > 5) { /* port cannot be longer then 5 characters */
				ret->is_rejected = true;
				return (autofree) ? NULL : ret;
            } else if (e - p > 0) {
                long port;
                string end;
                memcpy(port_buf, p, (e - p));
                port_buf[e - p] = '\0';
                port = strtol(port_buf, &end, 10);
                if (port >= 0 && port <= 65535 && end != port_buf) {
                    *has_port = 1;
                    ret->port = (unsigned short)port;
				} else {
					ret->is_rejected = true;
					return (autofree) ? NULL : ret;
                }
            }
            p--;
        }
    } else {
        p = e;
    }

    /* check if we have a valid host, if we don't reject the string as url */
	if ((p - s) < 1) {
		ret->is_rejected = true;
		return (autofree) ? NULL : ret;
    }

	ret->host = (autofree) ? str_trim(s, (p - s)) : uri_dup(s, (p - s));
    replace_ctrl_ex(ret->host, simd_strlen(ret->host));

    if (e == ue) {
        return ret;
    }

    s = e;

just_path:

    e = ue;
    p = memchr(s, '#', (e - s));
    if (p) {
        p++;
        if (p < e) {
			ret->fragment = (autofree) ? str_trim(p, (e - p)) : uri_dup(p, (e - p));
            replace_ctrl_ex(ret->fragment, simd_strlen(ret->fragment));
        }
        e = p - 1;
    }

    p = memchr(s, '?', (e - s));
    if (p) {
        p++;
        if (p < e) {
			ret->query = (autofree) ? str_trim(p, (e - p)) : uri_dup(p, (e - p));
            replace_ctrl_ex(ret->query, simd_strlen(ret->query));
        }
        e = p - 1;
    }

    if (s < e || s == ue) {
		ret->path = (autofree) ? str_trim(s, (e - s)) : uri_dup(s, (e - s));
        replace_ctrl_ex(ret->path, simd_strlen(ret->path));
    }

    return ret;
}

static url_t *url_parse_ex(char const *str, size_t length, bool autofree) {
    bool has_port;
    return url_parse_ex2(str, length, &has_port, autofree);
}

url_t *parse_url(string_t str) {
    url_t *url = url_parse_ex(str, simd_strlen(str), true);
    if (!is_empty(url))
        url->type = scheme_type(url->scheme);

    return url;
}

void uri_free(uri_t *uri) {
	if (is_type(uri, RAII_SCHEME_TLS)
		|| is_type(uri, RAII_SCHEME_TCP)
		|| is_type(uri, RAII_SCHEME_UDP)
		|| is_type(uri, RAII_SCHEME_PIPE)
		|| (is_type(uri, -RAII_SCHEME_INVALID) && uri->is_rejected)) {
		if (uri->is_autofreeable)
			return;

		uri->type = RAII_INVALID;
		uri->port = RAII_ERR;
		uri->is_rejected = false;
		if (!is_empty(uri->scheme))
			RAII_FREE(uri->scheme);

		if (!is_empty(uri->user))
			RAII_FREE(uri->user);

		if (!is_empty(uri->pass))
			RAII_FREE(uri->pass);

		if (!is_empty(uri->host))
			RAII_FREE(uri->host);

		if (!is_empty(uri->path))
			RAII_FREE(uri->path);

		if (!is_empty(uri->query))
			RAII_FREE(uri->query);

		if (!is_empty(uri->fragment))
			RAII_FREE(uri->fragment);

		RAII_FREE(uri);
	}
}

uri_t *parse_uri(string_t str) {
	uri_t *uri = url_parse_ex(str, simd_strlen(str), false);
	uri->type = scheme_type(uri->scheme);
	if (uri->is_rejected) {
		uri_free(uri);
		return nullptr;
	}

	return uri;
}

fileinfo_t *pathinfo(string filepath) {
    fileinfo_t *file = calloc_local(1, sizeof(fileinfo_t));
    size_t path_len = simd_strlen(filepath);
    string dir_name;
    string_t p;
    ptrdiff_t idx;

    dir_name = str_trim(filepath, path_len);
#if defined(__APPLE__) || defined(__MACH__)
    file->dirname = str_dup(dirname(dir_name));
    file->filename = str_dup(basename(dir_name));
    file->base = str_dup(basename((string)file->dirname));
#else
    dirname(dir_name);
    file->dirname = dir_name;
    file->base = basename((string)file->dirname);
    file->filename = basename((string)filepath);
#endif

    p = str_memrchr(file->filename, '.', simd_strlen(file->filename));
    if (p) {
        idx = p - file->filename;
        file->extension = file->filename + idx + 1;
    }

    file->type = RAII_URLINFO;
    return file;
}

string url_encode(char const *s, size_t len) {
    register unsigned char c;
    unsigned char *to;
    unsigned char const *from, *end;
    string start, ret;

    from = (unsigned char *)s;
    end = (unsigned char *)s + len;
    start = RAII_CALLOC(1, 3 * len);
    to = (unsigned char *)start;

    while (from < end) {
        c = *from++;

        if (c == ' ') {
            *to++ = '+';
        } else if ((c < '0' && c != '-' && c != '.') ||
                   (c < 'A' && c > '9') ||
                   (c > 'Z' && c < 'a' && c != '_') ||
                   (c > 'z')) {
            to[0] = '%';
            to[1] = hex_chars[c >> 4];
            to[2] = hex_chars[c & 15];
            to += 3;
        } else {
            *to++ = c;
        }
    }

    *to = '\0';
    size_t rlen = to - (unsigned char *)start;
    ret = calloc_local(1, rlen);
    memcpy(ret, start, rlen + 1);
    RAII_FREE(start);

    return ret;
}

string url_decode(string str, size_t len) {
    string dest = str;
    string data = str;

    while (len--) {
        if (*data == '+') {
            *dest = ' ';
        } else if (*data == '%' && len >= 2 && isxdigit((int)*(data + 1))
                   && isxdigit((int)*(data + 2))) {
            *dest = (char)htoi(data + 1);
            data += 2;
            len -= 2;
        } else {
            *dest = *data;
        }
        data++;
        dest++;
    }
    *dest = '\0';
    return dest;
}
