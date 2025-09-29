#include "raii.h"

#if defined(__linux__) && defined(__GNUC__)
#define CODE_SECTION section(text)
#else
#define CODE_SECTION
#endif

static RAII_INLINE int64_t _copySign(int64_t src, uint64_t dst) {
    // This is better than `src > 0 ? dst : -dst` that is using cmov
    uint64_t m = ~(src >> 63); // all 1s if >= 0 (opposite of abs)
    return (dst + m) ^ m; // flip if src negative
}

RAII_INLINE uintptr_t cast(string_t src) {
    uintptr_t ret;
    memcpy(&ret, src, sizeof(uintptr_t));
    return ret;
}

RAII_INLINE uintptr_t cast8(string_t s, uint32_t len) {
    RAII_ASSERT(len <= 8);

    // int 64 of s
    uint64_t x = cast(s);

    uint64_t mask = (1ull << (len * 8)) - 1;
    mask |= -(len == 8); // fill 1's if len == 8

    return x & mask;
}

RAII_INLINE uintptr_t extend(char c) {
    uintptr_t ret;
    memset(&ret, c, sizeof(uintptr_t));
    return ret;
}

RAII_INLINE int countr_zero(uintptr_t mask) {
#ifdef _WIN32
    unsigned long result;
#ifdef _X86_
    _BitScanForward(&result, mask);
#else
    _BitScanForward64(&result, mask);
#endif
    return (int)result;
#else
#if defined(__i386__) || defined(__i486__) || defined(__i586__) || defined(__i686__)
    return __builtin_ctzl(mask);
#else
    return __builtin_ctzll(mask);
#endif
#endif
}

RAII_INLINE int countl_zero(uintptr_t mask) {
#ifdef _WIN32
    unsigned long result;
#ifdef _X86_
    _BitScanReverse(&result, mask);
#else
    _BitScanReverse64(&result, mask);
#endif
    return (int)result;
#else
#if defined(__i386__) || defined(__i486__) || defined(__i586__) || defined(__i686__)
    return __builtin_clzl(mask);
#else
    return __builtin_clzll(mask);
#endif
#endif
}

RAII_INLINE int countr_index(uintptr_t mask) {
    return (mask == 0) ? 0 : countr_zero(mask) + 1;
}

RAII_INLINE bool haszero(uint64_t x) {
    uint64_t a = 0x7f7f7f7f7f7f7f7full;
    uint64_t l = 0x0101010101010101ull;
    return (x - l) & ~x & ~a;
}

RAII_INLINE bool hasbyte(uint64_t x, uint8_t c) {
    return haszero(x ^ extend(c));
}

RAII_INLINE uint32_t _memchr8(bool Printable, bool Exists, bool Reverse, string_t s, uint8_t c) {
#if defined(__arm__) || defined(_X86_) || defined(__i386__) || defined(__i486__) || defined(__i586__) || defined(__i686__)
    // int 32 of all c's
    uint32_t m = extend(c);

    // int 32 of s
    uint32_t x = cast(s);

    // remove c's from string
    // so now we have to find first zero byte
    x ^= m;

    uint32_t a = 0x7f7f7f7ful;
#else
    // int 64 of all c's
    uint64_t m = extend(c);

    // int 64 of s
    uint64_t x = cast(s);

    // remove c's from string
    // so now we have to find first zero byte
    x ^= m;

    uint64_t a = 0x7f7f7f7f7f7f7f7full;
#endif

    // set the high bit in non-zero bytes
    if (Printable) {
        x += a;
    } else {
        x = ((x & a) + a) | x;
    }

    // flip to set the high bit in zero bytes, and clear other high bits
    x = ~x;

    // clear all bits except the high bit of the zero byte
    x &= ~a;

    // find the high bit, from right (little endian)
    if (Exists) {
        if (!Reverse) {
            // ctz returns 7, 15, 23, etc
            return countr_zero(x) / 8;
        } else {
            // clz returns 0, 8, 16, etc
            return 7 - countl_zero(x) / 8;
        }
    } else {
        if (!Reverse) {
            // ffs returns + 1, so that's going to be 8, 16, 24, etc
            return (countr_index(x) - 8) / 8;
        } else {
            // clz returns 0, 8, 16, 24, 32, 40, 48, 56
            // x == 0 returns 63 that we increment to 64 to return -1
            return 7 - (countl_zero(x | 1) + 1) / 8;
        }
    }
}

RAII_INLINE uint32_t memchr8(string_t s, uint8_t c) {
    return _memchr8(false, false, false, s, c);
}

RAII_INLINE uint32_t memchr8k(string_t s, uint8_t c) {
    return _memchr8(false, true, false, s, c);
}

static RAII_INLINE uint32_t _memrchr(bool Printable, bool Known, string_t s, uint32_t len, uint8_t c) {
    string_t p = s + len;

    // If shorter than 8 bytes, we have to mask away c bytes past len
    uint32_t partLen = (len & 7) ? (len & 7) : 8;
    uint64_t partMask = len < 8 ? ~0ull >> (64 - partLen * 8) : 0ull;
    uint64_t first = cast(p) & ~(partMask & extend(c));

    // Advance to leave multiple of 8 bytes
    p -= partLen;

    // Check first 8 bytes
    if (hasbyte(first, c)) {
        return (p - s) + _memchr8(Printable, true, true, (string)&first, c);
    }

    // Check words for that byte
    for (;;) {
        if (hasbyte(cast(p), c)) {
            return (p - s) + _memchr8(Printable, true, true, p, c);
        }
        p -= 8;
        if (!Known) {
            if (p == s)
                return RAII_ERR;
        }
    }
}

static RAII_INLINE uint32_t _memchr(bool Printable, bool Known, string_t s, uint32_t len, uint8_t c) {
    string_t p = s;
    string_t end = s + len;

    // If shorter than 8 bytes, we have to mask away c bytes past len
#if defined(__arm__) || defined(_X86_) || defined(__i386__) || defined(__i486__) || defined(__i586__) || defined(__i686__)
    uint16_t partLen = (len & 3) ? (len & 3) : 4;
    uint32_t partMask = len < 4 ? ~0ul >> (32 - partLen * 4) : 0ul;
    uint32_t first = cast(p) & ~(partMask & extend(c));
#else
    uint32_t partLen = (len & 7) ? (len & 7) : 8;
    uint64_t partMask = len < 8 ? ~0ull >> (64 - partLen * 8) : 0ull;
    uint64_t first = cast(p) & ~(partMask & extend(c));
#endif

    // Check first 8 bytes
    if (hasbyte(first, c))
        return _memchr8(Printable, true, false, (string)&first, c);

    // Advance to leave multiple of 8 bytes
    p += partLen;

    // Check words for that byte
    for (;;) {
        if (hasbyte(cast(p), c)) {
            return (p - s) + _memchr8(Printable, true, false, p, c);
        }
        p += 8;
        if (!Known) {
            if (p == end)
                return RAII_ERR;
        }
    }
}

RAII_INLINE uint16_t utoa2p(uint64_t x) {
    static const CODE_SECTION uint8_t pairs[50] = { // 0..49, little endian
        0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90,
        0x01, 0x11, 0x21, 0x31, 0x41, 0x51, 0x61, 0x71, 0x81, 0x91,
        0x02, 0x12, 0x22, 0x32, 0x42, 0x52, 0x62, 0x72, 0x82, 0x92,
        0x03, 0x13, 0x23, 0x33, 0x43, 0x53, 0x63, 0x73, 0x83, 0x93,
        0x04, 0x14, 0x24, 0x34, 0x44, 0x54, 0x64, 0x74, 0x84, 0x94,
    };

    uint32_t b50 = -(uint32_t)(x >= 50); // x >= 50 ? ~0 : 0;
    uint32_t x2 = x - (50u & b50);       // x2 = x % 50;
    uint16_t t = pairs[x2] + (b50 & 5);  // t = pairs[x % 50] + 5 in low nibble if x > 50

    // move upper nibble to next byte and add '00'
    return ((t | (t << 4)) & 0x0f0f) | 0x3030;
}

RAII_INLINE void utoa2p_ex(uint64_t x, string s) {
    uint16_t t = utoa2p(x);
    memcpy(s, &t, sizeof(uint16_t));
}

RAII_INLINE uint32_t itoa8(int32_t x, string buf) {
    // Handle negatives
    bool neg = x < 0;
    *buf = '-'; // Always write
    buf += neg; // But advance only if negative
    x = abs(x);

    uint64_t tmp = 0;
    int n = 0;

    // Convert pairs of digits
    while (x >= 100) {
        n += 2;
        tmp <<= 16;
        tmp |= utoa2p(x % 100);
        x /= 100;
    }

    // Last pair - no need to divide any more
    n += 2;
    tmp <<= 16;
    tmp |= utoa2p(x);

    // If last pair is 0<N> we want to remove this "0"
    n -= x < 10;
    tmp >>= x < 10 ? 8 : 0;

    // Copy to provided buffer
    memcpy(buf, &tmp, 8);
    buf[n] = '\0';

    return n + neg;
}

RAII_INLINE string simd_itoa(int64_t x, string buf) {
    // Handle negatives
    bool neg = x < 0;
    *buf = '-'; // Always write
    buf += neg; // But advance only if negative

#if defined(__APPLE__) || defined(__MACH__)
    x = llabs(x);
#else
    x = abs(x);
#endif

    char tmp[20];
    string p = tmp + 20;

    while (x >= 100) {
        p -= 2;
        utoa2p_ex(x % 100, p);
        x /= 100;
    }

    p -= 2;
    utoa2p_ex(x, p);

    p += x < 10;

    uint32_t len = tmp + 20 - p;

    memcpy(buf, p, 20);
    buf[len] = '\0';

    return buf;
}

RAII_INLINE uint32_t atou8(string_t s, uint32_t len) {
    RAII_ASSERT(len <= 8);

    // int 64 of s. "12345678" --> 0x3837363534333231
    uint64_t x = cast(s);

    // apply len. len of 2 --> 0x3231000000000000
    x <<= 64 - len * 8;
    x &= -(uint64_t)(len > 0);

    // add ones and tens, in int8's, from 0x0[2]0[1] to 0x00[12]
    x = (x & 0x0f0f0f0f0f0f0f0full) * ((1ull << 8) * 10 + 1) >> 8;

    // add int16's, from 0x00[34]00[12] to 0x0000[1234]
    x = (x & 0x00ff00ff00ff00ffull) * ((1ull << 16) * 100 + 1) >> 16;

    // add int32's, from 0x0000[1234]0000[5678] to 0x[12345678]
    x = (x & 0x0000ffff0000ffffull) * ((1ull << 32) * 10000 + 1) >> 32;

    return x;
}

RAII_INLINE uint64_t simd_atou(string_t s, uint32_t len) {
    RAII_ASSERT(len <= 20);
    uint64_t x = 0;
    if (len > 8) {
        uint32_t lh = len % 8;
        x = atou8(s, lh);
        x *= 100000000;
        len -= lh;
        s += lh;
        if (len > 8) {
            x += atou8(s, 8);
            x *= 100000000;
            len -= 8;
            s += 8;
        }
    }
    return x + atou8(s, len);
}

RAII_INLINE int64_t simd_atoi(string_t s, uint32_t len) {
    bool neg = !!len & (*s == '-');
    bool ls = !!len & (*s == '-' || *s == '+');
    s += ls;

    int64_t x = simd_atou(s, len - ls);

    return neg ? -x : x;
}

RAII_INLINE uint32_t htou8(string_t s, uint32_t len) {
    RAII_ASSERT(len <= 8);

    // int 64 of s. "12345678" --> 0x3837363534333231
    uint64_t x = cast(s);

    // apply len. len of 2 --> 0x3231000000000000
    x <<= 64 - len * 8;

    // handle length of 0. remove all bits if zero
    x &= -(uint64_t)(len > 0);

    // change a-f to to number. 0x41 --> 0x0a
    x += ((x & 0x4040404040404040ull) >> 6) * 9;

    // add ones and 0x10's, in int8's, from 0x0[f]0[1] to 0x00[1f]
    x = (x & 0x0f0f0f0f0f0f0f0full) * ((1ull << 12) + 1) >> 8;

    // add int16's, from 0x00[ed]00[1f] to 0x0000[1fed]
    x = (x & 0x00ff00ff00ff00ffull) * ((1ull << 24) + 1) >> 16;

    // add int32's, from 0x0000[1fed]0000[cba9] to 0x[1fedcba9]
    x = (x & 0x0000ffff0000ffffull) * ((1ull << 48) + 1) >> 32;

    return x;
}

RAII_INLINE uint64_t simd_htou(string_t s, uint32_t len) {
    RAII_ASSERT(len <= 16);
    uint64_t x = 0;
    if (len > 8) {
        uint32_t lh = len - 8;
        x = htou8(s, lh);
        x <<= 32;
        len -= lh;
        s += lh;
    }
    return x + htou8(s, len);
}

RAII_INLINE size_t simd_strlen(string_t str) {
	if (is_empty((void_t)str))
		return 0;

	string_t char_ptr;
    const uintptr_t *longword_ptr;
    uintptr_t longword, himagic, lomagic;

    /* Handle the first few characters by reading one character at a time.
       Do this until CHAR_PTR is aligned on a longword boundary.  */
    for (char_ptr = str; ((uintptr_t)char_ptr
                          & (sizeof(longword) - 1)) != 0;
         ++char_ptr)
        if (*char_ptr == '\0')
            return char_ptr - str;

    /* All these elucidatory comments refer to 4-byte longwords,
       but the theory applies equally well to 8-byte longwords.  */

    longword_ptr = (uintptr_t *)char_ptr;

    /* Bits 31, 24, 16, and 8 of this number are zero.  Call these bits
       the "holes."  Note that there is a hole just to the left of
       each byte, with an extra at the end:
       bits:  01111110 11111110 11111110 11111111
       bytes: AAAAAAAA BBBBBBBB CCCCCCCC DDDDDDDD
       The 1-bits make sure that carries propagate to the next 0-bit.
       The 0-bits provide holes for carries to fall into.  */
    himagic = 0x80808080L;
    lomagic = 0x01010101L;
    if (sizeof(longword) > 4) {
        /* 64-bit version of the magic.  */
        /* Do the shift in two steps to avoid a warning if long has 32 bits.  */
        himagic = ((himagic << 16) << 16) | himagic;
        lomagic = ((lomagic << 16) << 16) | lomagic;
    }

    RAII_ASSERT(!(sizeof(longword) > 8));

    /* Instead of the traditional loop which tests each character,
       we will test a longword at a time.  The tricky part is testing
       if *any of the four* bytes in the longword in question are zero.  */
    for (;;) {
        longword = *longword_ptr++;

        if (((longword - lomagic) & ~longword & himagic) != 0) {
            /* Which of the bytes was the zero?  If none of them were, it was
               a misfire; continue the search.  */

            string_t cp = (string_t )(longword_ptr - 1);

            if (cp[0] == 0)
                return cp - str;
            if (cp[1] == 0)
                return cp - str + 1;
            if (cp[2] == 0)
                return cp - str + 2;
            if (cp[3] == 0)
                return cp - str + 3;
            if (sizeof(longword) > 4) {
                if (cp[4] == 0)
                    return cp - str + 4;
                if (cp[5] == 0)
                    return cp - str + 5;
                if (cp[6] == 0)
                    return cp - str + 6;
                if (cp[7] == 0)
                    return cp - str + 7;
            }
        }
    }
}

RAII_INLINE string simd_memchr(string_t s, uint8_t c, uint32_t len) {
#if defined(__arm__) || defined(_X86_) || defined(__i386__) || defined(__i486__) || defined(__i586__) || defined(__i686__)
    string ptr = (string)s;
    return (string)memchr(ptr, c, simd_strlen(s));
#endif
    return (string)s + _memchr(false, false, s, len, c);
}

RAII_INLINE string simd_memrchr(string_t s, uint8_t c, uint32_t len) {
#if defined(_WIN32) || defined(__arm__) || defined(__i386__) || defined(__i486__) || defined(__i586__) || defined(__i686__)
    return (string)str_memrchr((const_t)s, (int)c, (size_t)len);
#else
    return (string)s + _memrchr(false, false, s, len, c);
#endif
}

RAII_INLINE uint32_t memchrk(string_t s, uint32_t len, uint8_t c) {
    return _memchr(false, true, s, len, c);
}

RAII_INLINE uint32_t pmemchr(string_t s, uint32_t len, uint8_t c) {
    return _memchr(true, false, s, len, c);
}

RAII_INLINE uint32_t pmemchrk(string_t s, uint32_t len, uint8_t c) {
    return _memchr(true, true, s, len, c);
}

RAII_INLINE uint32_t memrchrk(string_t s, uint32_t len, uint8_t c) {
    return _memrchr(false, true, s, len, c);
}

RAII_INLINE uint32_t pmemrchr(string_t s, uint32_t len, uint8_t c) {
    return _memrchr(true, false, s, len, c);
}

RAII_INLINE uint32_t pmemrchrk(string_t s, uint32_t len, uint8_t c) {
    return _memrchr(true, true, s, len, c);
}

RAII_INLINE double simd_atod(string_t s, uint32_t len) {
    // Get int part
    int ilen = pmemchr(s, len, '.');

    // If no decimal dot, return the int
    if (ilen == -1) {
        return simd_atoi(s, len);
    }

    // Do the int part
    int64_t ipart = simd_atoi(s, ilen);
    s += ilen + 1;
    len -= ilen + 1;

    // Int of decimal part
    int64_t dpart = simd_atou(s, len);

    // To add the two parts we need matching signs
    dpart = _copySign(ipart, dpart);

    // Array of 20 * 8 = 160 bytes
    static const CODE_SECTION double scales[20] = {
        1e-1, 1e-2, 1e-3, 1e-4, 1e-5, 1e-6, 1e-7, 1e-8, 1e-9, 1e-10,
        1e-11, 1e-12, 1e-13, 1e-14, 1e-15, 1e-16, 1e-17, 1e-18, 1e-19, 1e-20};

    return ipart + dpart * scales[len];
}

RAII_INLINE uint64_t _utoap(int N, uint64_t x, string s) {
    int i;
    RAII_ASSERT(N <= 8);

    uint64_t tmp = utoa2p(x % 100);

    for (i = 0; i < N - 2; i += 2) {
        x /= 100;
        tmp <<= 16;
        tmp |= utoa2p(x % 100);
    }

    tmp >>= (N & 1) * 8;
    memcpy(s, &tmp, 8);

    return x;
}

RAII_INLINE string utoap(int N, uint64_t x, string s) {
    if (N <= 8) {
        _utoap(N, x, s);
    } else if (N <= 16) {
        x = _utoap(N - 8, x, s + 8);
        x /= (N & 1) ? 10 : 100;
        _utoap(8, x, s);
    } else {
        x = _utoap(N - 16, x, s + 16);
        x /= (N & 1) ? 10 : 100;
        x = _utoap(8, x, s + 8);
        _utoap(8, x / 100, s);
    }

    s[N] = '\0';
    return s;
}

RAII_INLINE const_t str_memrchr(const_t s, int c, size_t n) {
    u_string_t e;
    if (0 == n) {
        return nullptr;
    }

    for (e = (u_string_t)s + n - 1; e >= (u_string_t)s; e--) {
        if (*e == (u_char_t)c) {
            return (const_t)e;
        }
    }

    return nullptr;
}

int strpos(string_t text, string pattern) {
    size_t c, d, e, text_length, pattern_length, position = RAII_ERR;
	if (is_empty(pattern) || is_empty((void_t)text))
		return RAII_ERR;

	text_length = simd_strlen(text);
	pattern_length = simd_strlen(pattern);

    if (pattern_length > text_length)
        return RAII_ERR;

    for (c = 0; c <= text_length - pattern_length; c++) {
        position = e = c;
        for (d = 0; d < pattern_length; d++)
            if (pattern[d] == text[e])
                e++;
            else
                break;

        if (d == pattern_length)
            return (int)position;
    }

    return RAII_ERR;
}

string *str_split_ex(memory_t *defer, string_t s, string_t delim, int *count) {
    if (is_str_eq(s, ""))
        return nullptr;

    if (is_empty((void_t)delim))
        delim = " ";

    void *data;
    string _s = (string)s;
    string_t *ptrs;
    size_t ptrsSize, nbWords = 1, sLen = simd_strlen(s), delimLen = simd_strlen(delim);

    while ((_s = strstr(_s, delim))) {
        _s += delimLen;
        ++nbWords;
    }

    ptrsSize = (nbWords + 1) * sizeof(string);
    if (defer)
        ptrs = data = calloc_full(defer, 1, ptrsSize + sLen + 1, free);
    else
        ptrs = data = try_calloc(1, ptrsSize + sLen + 1);

    if (data) {
        *ptrs = _s = str_copy((string)data + ptrsSize, s, sLen);
        if (nbWords > 1) {
            while ((_s = strstr(_s, delim))) {
                *_s = '\0';
                _s += delimLen;
                *++ptrs = _s;
            }
        }

        *++ptrs = nullptr;
        if (count)
            *count = (int)nbWords;
    }

    return data;
}

string str_cat_ex(memory_t *defer, int num_args, ...) {
    va_list args;

    va_start(args, num_args);
    string s = str_concat_ex(defer, num_args, args);
    va_end(args);

    return s;
}

string str_concat_ex(memory_t *defer, int num_args, va_list ap_copy) {
    size_t strsize = 0;
    int i;
    va_list ap;

    va_copy(ap, ap_copy);
    for (i = 0; i < num_args; i++)
        strsize += simd_strlen(va_arg(ap, string));

    string res = try_calloc(1, strsize + 1);
    strsize = 0;

    va_copy(ap, ap_copy);
    for (i = 0; i < num_args; i++) {
        string s = va_arg(ap, string);
        str_copy(res + strsize, s, 0);
        strsize += simd_strlen(s);
    }
    va_end(ap);

    if (defer)
        raii_deferred(defer, free, res);

    return res;
}

string str_replace_ex(memory_t *defer, string_t haystack, string_t needle, string_t replace) {
    if (!haystack || !needle || !replace)
        return nullptr;

    string result;
    size_t i, cnt = 0;
    size_t newWlen = simd_strlen(replace);
    size_t oldWlen = simd_strlen(needle);

    for (i = 0; haystack[i] != '\0'; i++) {
        if (strstr(&haystack[i], needle) == &haystack[i]) {
            cnt++;

            i += oldWlen - 1;
        }
    }

    if (cnt == 0)
        return nullptr;

    if (defer)
        result = (string)calloc_full(defer, 1, i + cnt * (newWlen - oldWlen) + 1, free);
    else
        result = (string)try_calloc(1, i + cnt * (newWlen - oldWlen) + 1);

    i = 0;
    while (*haystack) {
        if (strstr(haystack, needle) == haystack) {
            str_copy(&result[i], replace, newWlen);
            i += newWlen;
            haystack += oldWlen;
        } else {
            result[i++] = *haystack++;
        }
    }

    result[i] = '\0';
    return result;
}

RAII_INLINE string str_copy(string dest, string_t src, size_t len) {
    return memcpy(dest, src, (len ? len : simd_strlen(src)));
}

RAII_INLINE string str_memdup_ex(memory_t *defer, const_t src, size_t len) {
    string ptr = (string)calloc_full(defer, 1, len + 1, free);

    return memcpy(ptr, src, len);
}

RAII_INLINE string str_trim(string_t str, size_t length) {
    return str_memdup_ex(get_scope(), str, length);
}

RAII_INLINE string str_dup(string_t str) {
    return str_trim(str, simd_strlen(str));
}

RAII_INLINE string *str_split(string_t s, string_t delim, int *count) {
    return str_split_ex(get_scope(), s, delim, count);
}

string str_concat(int num_args, ...) {
    va_list args;

    va_start(args, num_args);
    string s = str_concat_ex(get_scope(), num_args, args);
    va_end(args);

    return s;
}

RAII_INLINE string str_replace(string_t haystack, string_t needle, string_t replace) {
    return str_replace_ex(get_scope(), haystack, needle, replace);
}

RAII_INLINE arrays_t str_explode(string_t s, string_t delim) {
    if (is_empty((void_t)s) || is_str_eq(s, ""))
        return nullptr;

    if (is_empty((void_t)delim))
        delim = " ";

    arrays_t data = arrays();
    string first = nullptr, _s = (string)s;
    string_t *ptrs;
    bool is_first = true;
    size_t ptrsSize, nbWords = 0, sLen = simd_strlen(s), delimLen = simd_strlen(delim);

    while ((_s = strstr(_s, delim))) {
        _s += delimLen;
        nbWords++;
    }

    if (nbWords > 0) {
        ptrsSize = nbWords * sizeof(string);
        ptrs = calloc_full(get_scope(), 1, ptrsSize + sLen + 1, free);
        first = _s = str_copy((string)ptrs, s, sLen);
        while ((_s = strstr(_s, delim))) {
            *_s = '\0';
            if (is_first) {
                is_first = false;
                $append_string(data, first);
            }

            _s += delimLen;
            $append_string(data, _s);
        }
    }

    return data;
}

string str_repeat(string str, int mult) {
    string result;
    size_t result_len, len = simd_strlen(str);

    if (mult < 0) {
        raii_panic("must be greater than or equal to 0");
    }

    /* Don't waste our time if it's empty */
    /* ... or if the multiplier is zero */
    if (len == 0 || mult == 0)
        return "";

    /* Initialize the result string */
    result_len = len * mult;
    result = calloc_local(1, result_len + 1);

    /* Heavy optimization for situations where input string is 1 byte long */
    if (len == 1) {
        memset(result, *str, mult);
    } else {
        string_t s, ee;
        string e;
        ptrdiff_t l = 0;
        memcpy(result, str, len);
        s = result;
        e = (string)s + len;
        ee = s + result_len;

        while (e < ee) {
            l = (e - s) < (ee - e) ? (e - s) : (ee - e);
            memmove(e, s, l);
            e += l;
        }
    }

    return result;
}

string str_pad(string str, int length, string pad, str_pad_type pad_type) {
    size_t pad_str_len, num_pad_chars, i;
    size_t len = 0, left_pad = 0, right_pad = 0, input_len = simd_strlen(str);
    string result = nullptr;

    if (is_empty(pad)) {
        pad = " ";
        pad_str_len = 1;
    } else {
        pad_str_len = simd_strlen(pad);
    }

    if (!pad_type)
        pad_type = STR_PAD_RIGHT; /* The padding type value */

    /* If resulting string turns out to be shorter than input string,
       we simply copy the input and return. */
    if (length < 0 || (size_t)length <= input_len) {
        return str;
    }

    if (pad_str_len == 0) {
        raii_panic("must be a non-empty string");
    }

    if (pad_type < STR_PAD_LEFT || pad_type > STR_PAD_BOTH) {
        raii_panic("must be STR_PAD_LEFT, STR_PAD_RIGHT, or STR_PAD_BOTH");
    }

    num_pad_chars = length - input_len;
    result = calloc_local(1, input_len + num_pad_chars + 1);

    /* We need to figure out the left/right padding lengths. */
    switch (pad_type) {
        case STR_PAD_RIGHT:
            left_pad = 0;
            right_pad = num_pad_chars;
            break;

        case STR_PAD_LEFT:
            left_pad = num_pad_chars;
            right_pad = 0;
            break;

        case STR_PAD_BOTH:
            left_pad = num_pad_chars / 2;
            right_pad = num_pad_chars - left_pad;
            break;
    }

    /* First we pad on the left. */
    for (i = 0; i < left_pad; i++)
        result[len++] = pad[i % pad_str_len];

    /* Then we copy the input string. */
    memcpy((result + len), str, input_len);
    len += input_len;

    /* Finally, we pad on the right. */
    for (i = 0; i < right_pad; i++)
        result[len++] = pad[i % pad_str_len];

    return result;
}

RAII_INLINE string str_toupper(string s, size_t len) {
    u_string c;
    u_string_t e;

    c = (u_string)s;
    e = (u_string)c + len;

    while (c < e) {
        *c = toupper(*c);
        c++;
    }

    return s;
}

RAII_INLINE string str_tolower(string s, size_t len) {
    u_string c;
    u_string_t e;

    c = (u_string)s;
    e = c + len;

    while (c < e) {
        *c = tolower(*c);
        c++;
    }
    return s;
}

RAII_INLINE string word_toupper(string str, char sep) {
    size_t i, length = 0;

    length = simd_strlen(str);
    for (i = 0;i < length;i++) {// capitalize the first most letter
        if (i == 0) {
            str[i] = toupper(str[i]);
        } else if (str[i] == sep) {//check if the charater is the separator
            str[i + 1] = toupper(str[i + 1]);
            i += 1;  // skip the next charater
        } else {
            str[i] = tolower(str[i]); // lowercase reset of the characters
        }
    }

    return str;
}

RAII_INLINE string ltrim(string s) {
    while (isspace(*s)) s++;
    return s;
}

RAII_INLINE string rtrim(string s) {
    string back = s + strlen(s);
    while (isspace(*--back));
    *(back + 1) = '\0';
    return s;
}

RAII_INLINE string trim(string s) {
    return rtrim(ltrim(s));
}

static u_char_t base64_table[65] =
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static u_char_t base64_decode_table[256] = {
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x3e, 0x80, 0x80, 0x80, 0x3f,
    0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x80, 0x80,
    0x80, 0x00, 0x80, 0x80, 0x80, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
    0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12,
    0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24,
    0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30,
    0x31, 0x32, 0x33, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80};

static inline size_t str_base64_len(size_t decoded_len) {
    /* This counts the padding bytes (by rounding to the next multiple of 4). */
    return ((4u * decoded_len / 3u) + 3u) & ~3u;
}

RAII_INLINE bool is_base64(u_string_t src) {
    size_t i, len = simd_strlen((string_t)src);
    for (i = 0; i < len; i++) {
        if (base64_decode_table[src[i]] == 0x80)
            return false;
    }

    return true;
}

u_string str_encode64_ex(memory_t *defer, u_string_t src) {
    u_string out, pos;
    u_string_t end, begin;
    size_t olen;
    size_t len = simd_strlen((string_t)src);

    olen = str_base64_len(len) + 1 /* for NUL termination */;
    if (olen < len)
        return nullptr; /* integer overflow */

    out = calloc_full(defer, 1, olen, free);
    end = src + len;
    begin = src;
    pos = out;
    while (end - begin >= 3) {
        *pos++ = base64_table[begin[0] >> 2];
        *pos++ = base64_table[((begin[0] & 0x03) << 4) | (begin[1] >> 4)];
        *pos++ = base64_table[((begin[1] & 0x0f) << 2) | (begin[2] >> 6)];
        *pos++ = base64_table[begin[2] & 0x3f];
        begin += 3;
    }

    if (end - begin) {
        *pos++ = base64_table[begin[0] >> 2];
        if (end - begin == 1) {
            *pos++ = base64_table[(begin[0] & 0x03) << 4];
            *pos++ = '=';
        } else {
            *pos++ = base64_table[((begin[0] & 0x03) << 4) | (begin[1] >> 4)];
            *pos++ = base64_table[(begin[1] & 0x0f) << 2];
        }
        *pos++ = '=';
    }

    *pos = '\0';

    return out;
}

u_string str_decode64_ex(memory_t *defer, u_string_t src) {
    u_string out, pos;
    u8 block[4];
    size_t i, count, olen;
    int pad = 0;
    size_t len = simd_strlen((string_t)src);

    count = 0;
    for (i = 0; i < len; i++) {
        if (base64_decode_table[src[i]] != 0x80)
            count++;
    }

    if (count == 0 || count % 4)
        return nullptr;

    olen = (count / 4 * 3) + 1;
    pos = out = calloc_full(defer, 1, olen, free);

    count = 0;
    for (i = 0; i < len; i++) {
        u8 tmp = base64_decode_table[src[i]];
        if (tmp == 0x80)
            continue;

        if (src[i] == '=')
            pad++;
        block[count] = tmp;
        count++;
        if (count == 4) {
            *pos++ = (u8)((block[0] << 2) | (block[1] >> 4));
            *pos++ = (u8)((block[1] << 4) | (block[2] >> 2));
            *pos++ = (u8)((block[2] << 6) | block[3]);
            count = 0;
            if (pad) {
                if (pad == 1)
                    pos--;
                else if (pad == 2)
                    pos -= 2;
                else {
                    /* Invalid padding */
                    return nullptr;
                }
                break;
            }
        }
    }
    *pos = '\0';

    return out;
}
