#include "raii.h"

#if defined(__linux__) && defined(__GNUC__)
#define CODE_SECTION __attribute__ ((section (".text#")))
#else
#define CODE_SECTION
#endif

// Copy the sign from src to dst that is unsigned.
// *** dst is up to 63 bit
static RAII_INLINE int64_t _copySign(int64_t src, uint64_t dst) {
    // This is better than `src > 0 ? dst : -dst` that is using cmov
    uint64_t m = ~(src >> 63); // all 1s if >= 0 (opposite of abs)
    return (dst + m) ^ m; // flip if src negative
}

RAII_INLINE uintptr_t cast(const char *src) {
    uintptr_t ret;
    memcpy(&ret, src, sizeof(ret));
    return ret;
}

// Get the uint _cast_ of a string of up to 8 chars
RAII_INLINE uintptr_t cast8(const char *s, uint32_t len) {
    RAII_ASSERT(len <= 8);

    // int 64 of s
    uint64_t x = cast(s);

    uint64_t mask = (1ull << (len * 8)) - 1;
    mask |= -(len == 8); // fill 1's if len == 8

    return x & mask;
}

RAII_INLINE uintptr_t extend(char c) {
    uintptr_t ret;
    memset(&ret, c, sizeof(ret));
    return ret;
}

/* Returns the number of trailing 0-bits in x,
starting at the least significant bit position.*/
RAII_INLINE int countr_zero(uintptr_t mask) {
#ifdef _WIN32
    unsigned long result;
    _BitScanForward64(&result, mask);
    return (int)result;
#else
    return __builtin_ctzll(mask);
#endif
}

/* Returns the number of leading 0-bits in x,
starting at the most significant bit position. */
RAII_INLINE int countl_zero(uintptr_t mask) {
#ifdef _WIN32
    unsigned long result;
    _BitScanReverse64(&result, mask);
    return (int)result;
#else
    return __builtin_clzll(mask);
#endif
}

/* Returns the index of the least significant 1-bit in x,
or the value zero if x is zero. */
RAII_INLINE int countr_index(uintptr_t mask) {
    return (mask == 0) ? 0 : countr_zero(mask) + 1;
}

/* Check if word has zero byte */
RAII_INLINE bool haszero(uint64_t x) {
    uint64_t a = 0x7f7f7f7f7f7f7f7full;
    uint64_t l = 0x0101010101010101ull;
    return (x - l) & ~x & ~a;
}

/* Check if word has some byte */
RAII_INLINE bool hasbyte(uint64_t x, uint8_t c) {
    return haszero(x ^ (uint64_t)extend(c));
}

// Find char in string. Support all options.
RAII_INLINE uint32_t _memchr8(bool Printable, bool Exists, bool Reverse, const char *s, uint8_t c) {
    // int 64 of all c's
    uint64_t m = extend(c);

    // int 64 of s
    uint64_t x = cast(s);

    // remove c's from string
    // so now we have to find first zero byte
    x ^= m;

    uint64_t a = 0x7f7f7f7f7f7f7f7full;

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

// Find char in binary string of 8 chars
RAII_INLINE uint32_t memchr8(const char *s, uint8_t c) {
    return _memchr8(false, false, false, s, c);
}

// Find char in binary string of 8 chars
// * The string is known to contain the char
RAII_INLINE uint32_t memchr8k(const char *s, uint8_t c) {
    return _memchr8(false, true, false, s, c);
}

// Find char, in reverse, in const binary string
static RAII_INLINE uint32_t _memrchr(bool Printable, bool Known, const char *s, uint32_t len, uint8_t c) {
    const char *p = s + len;

    // If shorter than 8 bytes, we have to mask away c bytes past len
    uint32_t partLen = (len & 7) ? (len & 7) : 8;
    uint64_t partMask = len < 8 ? ~0ull >> (64 - partLen * 8) : 0ull;
    uint64_t first = cast(p) & ~(partMask & extend(c));

    // Advance to leave multiple of 8 bytes
    p -= partLen;

    // Check first 8 bytes
    if (hasbyte(first, c)) {
        return (p - s) + _memchr8(Printable, true, true, (char *)&first, c);
    }

    // Check words for that byte
    for (;;) {
        if (hasbyte(cast(p), c)) {
            return (p - s) + _memchr8(Printable, true, true, p, c);
        }
        p -= 8;
        if (!Known) {
            if (p == s)
                return -1;
        }
    }
}

// Find char in const binary string
RAII_INLINE uint32_t _memchr(bool Printable, bool Known, const char *s, uint32_t len, uint8_t c) {
    const char *p = s;
    const char *end = s + len;

    // If shorter than 8 bytes, we have to mask away c bytes past len
    uint32_t partLen = (len & 7) ? (len & 7) : 8;
    uint64_t partMask = len < 8 ? ~0ull >> (64 - partLen * 8) : 0ull;
    uint64_t first = cast(p) & ~(partMask & extend(c));

    // Check first 8 bytes
    if (hasbyte(first, c))
        return _memchr8(Printable, true, false, (char *)&first, c);

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
                return -1;
        }
    }
}

// Convert uint, of less than 100, to %02u, as int 16
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

// Convert uint, of less than 100, to %02u
RAII_INLINE void utoa2p_ex(uint64_t x, char *s) {
    uint16_t t = utoa2p(x);
    memcpy(s, &t, sizeof(uint16_t));
}

// Convert signed int 32 to string of up to 8 bytes.
RAII_INLINE uint32_t itoa8(int32_t x, char *buf) {
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

// Convert signed int 64 to string. String buffer is at least 22 bytes.
// Returns length
// *** this feels inefficient :( ***
RAII_INLINE uint32_t simd_itoa(int64_t x, char *buf) {
    // Handle negatives
    bool neg = x < 0;
    *buf = '-'; // Always write
    buf += neg; // But advance only if negative
    x = _abs64(x);

    char tmp[20];
    char *p = tmp + 20;

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

    return len + neg;
}

// Parse uint from string of up to 8 chars
RAII_INLINE uint32_t atou8(const char *s, uint32_t len) {
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

// Parse uint64_t from string of up to 20 chars
// *** More than 20 char returns junk.
RAII_INLINE uint64_t simd_atou(const char *s, uint32_t len) {
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

// Parse _signed_ int from string of up to 20 chars. No spaces
RAII_INLINE int64_t simd_atoi(const char *s, uint32_t len) {
    bool neg = !!len & (*s == '-');
    bool ls = !!len & (*s == '-' || *s == '+');
    s += ls;

    int64_t x = simd_atou(s, len - ls);

    return neg ? -x : x;
}

// Parse hex int from string of up to 8 chars
RAII_INLINE uint32_t htou8(const char *s, uint32_t len) {
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

// Parse hex int from string of up to 16 chars
RAII_INLINE uint64_t simd_htou(const char *s, uint32_t len) {
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

// Find char in printable (chars < 128) string of 8 chars
RAII_INLINE uint32_t pmemchr8(const char *s, uint8_t c) {
    return _memchr8(true, false, false, s, c);
}

// Find char in printable (chars < 128) string of 8 chars
// * The string is known to contain the char
RAII_INLINE uint32_t pmemchr8k(const char *s, uint8_t c) {
    return _memchr8(true, true, false, s, c);
}

// Find zero byte in binary string
RAII_INLINE uint32_t simd_strlen(const char *s) {
    // check words for zero
    const char *p = s;
    while (!haszero(cast(p))) {
        p += 8;
    }

    return p - s + memchr8k(p, 8);
}

// Find zero byte in printable string
RAII_INLINE uint32_t simd_strlen_p(const char *s) {
    // check words for zero
    const char *p = s;
    while (!haszero(cast(p))) {
        p += 8;
    }

    return p - s + pmemchr8k(p, 8);
}

// Find char in string and trim it
RAII_INLINE uint32_t _trim8(bool Printable, bool Exists, const char *s, uint8_t c) {
    // int 64 of all c's
    uint64_t m = extend(c);

    // int 64 of s
    uint64_t x = cast(s);
    uint64_t xo = x;

    // remove c's from string
    // so now we have to find first zero byte
    x ^= m;

    uint64_t a = 0x7f7f7f7f7f7f7f7full;

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

    // set all bits under the lowest high bit
    x &= x - 1u;

    if (!Exists) {
        x >>= 7;
    } else if (Printable) {
        // complete the killed high bit
        x <<= 1u;
        x |= 1u;
    }

    return xo & x;
}

// Find char in binary string
RAII_INLINE uint32_t simd_memchr(const char *s, uint32_t len, uint8_t c) {
    return _memchr(false, false, s, len, c);
}

// Find char in binary string. Char c is known to be in s + len
RAII_INLINE uint32_t memchrk(const char *s, uint32_t len, uint8_t c) {
    return _memchr(false, true, s, len, c);
}

// Find char in printable string
RAII_INLINE uint32_t pmemchr(const char *s, uint32_t len, uint8_t c) {
    return _memchr(true, false, s, len, c);
}

// Find char in printable string. Char c is known to be in s + len
RAII_INLINE uint32_t pmemchrk(const char *s, uint32_t len, uint8_t c) {
    return _memchr(true, true, s, len, c);
}

// Find char in binary string
RAII_INLINE uint32_t simd_memrchr(const char *s, uint32_t len, uint8_t c) {
    return _memrchr(false, false, s, len, c);
}

// Find char in binary string. Char c is known to be in s + len
RAII_INLINE uint32_t memrchrk(const char *s, uint32_t len, uint8_t c) {
    return _memrchr(false, true, s, len, c);
}

// Find char in printable string
RAII_INLINE uint32_t pmemrchr(const char *s, uint32_t len, uint8_t c) {
    return _memrchr(true, false, s, len, c);
}

// Find char in printable string. Char c is known to be in s + len
RAII_INLINE uint32_t pmemrchrk(const char *s, uint32_t len, uint8_t c) {
    return _memrchr(true, true, s, len, c);
}

// Parse double from string
// *** More than 20 char integer part returns junk.
// *** Too much decimal char will get lost to precision
RAII_INLINE double simd_atod(const char *s, uint32_t len) {
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

// Convert uint to %0<N>u, N <= 8
RAII_INLINE uint64_t _utoap(int N, uint64_t x, char *s) {
    RAII_ASSERT(N <= 8);

    uint64_t tmp = utoa2p(x % 100);

    for (int i = 0; i < N - 2; i += 2) {
        x /= 100;
        tmp <<= 16;
        tmp |= utoa2p(x % 100);
    }

    tmp >>= (N & 1) * 8;
    memcpy(s, &tmp, 8);

    return x;
}

// Convert uint to %0<N>u, N <= 20
RAII_INLINE char *utoap(int N, uint64_t x, char *s) {
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