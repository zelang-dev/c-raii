
#include "bitset.h"

/*
Modified from https://github.com/drh/cii/blob/master/src/bit.c

"C Interfaces and Implementations: Techniques for Creating Reusable Software"
https://archive.org/details/davidr.hansoncinterfacesandimplementationszlib.org
*/

struct bits_s {
    raii_type type;
    i32 length;
    u8 *bytes;
    u64 *words;
    string to_string;
};

#define BPW (8*sizeof (u64))
#define nwords(len) ((((len) + BPW - 1)&(~(BPW-1)))/BPW)
#define nbytes(len) ((((len) + 8 - 1)&(~(8-1)))/8)
#define setop(sequal, snull, tnull, op)             \
	if (s == t) { RAII_ASSERT(s); return sequal; }  \
	else if (s == NULL) { RAII_ASSERT(t); return snull; }    \
	else if (t == NULL) return tnull;               \
	else {                                          \
		i32 i; bits_t set;                           \
		RAII_ASSERT(s->length == t->length);        \
		set = bitset_create(s->length);               \
		for (i = nwords(s->length); --i >= 0; )     \
			set->words[i] = s->words[i] op t->words[i]; \
		return set; }

u8 msb_mask[] = {
    0xFF, 0xFE, 0xFC, 0xF8,
    0xF0, 0xE0, 0xC0, 0x80
};

u8 lsb_mask[] = {
    0x01, 0x03, 0x07, 0x0F,
    0x1F, 0x3F, 0x7F, 0xFF
};

static RAII_INLINE bits_t copy(bits_t t) {
    bits_t set;
    RAII_ASSERT(t);
    set = bitset_create(t->length);
    if (t->length > 0)
        memcpy(set->bytes, t->bytes, nbytes(t->length));

    return set;
}

static RAII_INLINE i32 bitset_put(bits_t set, i32 n, i32 bit) {
    i32 prev;
    RAII_ASSERT(set);
    RAII_ASSERT(bit == 0 || bit == 1);
    RAII_ASSERT(0 <= n && n < set->length);
    prev = ((set->bytes[n / 8] >> (n % 8)) & 1);
    if (bit == 1)
        set->bytes[n / 8] |= 1 << (n % 8);
    else
        set->bytes[n / 8] &= ~(1 << (n % 8));

    return prev;
}

static RAII_INLINE void bitset_not(bits_t set, i32 lo, i32 hi) {
    RAII_ASSERT(set);
    RAII_ASSERT(0 <= lo && hi < set->length);
    RAII_ASSERT(lo <= hi);
    if (lo / 8 < hi / 8) {
        i32 i;
        set->bytes[lo / 8] ^= msb_mask[lo % 8];
        for (i = lo / 8 + 1; i < hi / 8; i++)
            set->bytes[i] ^= 0xFF;
        set->bytes[hi / 8] ^= lsb_mask[hi % 8];
    } else
        set->bytes[lo / 8] ^= (msb_mask[lo % 8] & lsb_mask[hi % 8]);
}

RAII_INLINE bits_t bitset_create(i32 length) {
    bits_t set = (bits_t)try_malloc(sizeof(struct bits_s));
    if (length > 0)
        set->words = try_calloc(nwords(length), sizeof(u64));
    else
        set->words = nullptr;

    set->bytes = (u8 *)set->words;
    set->length = length;
    set->to_string = nullptr;
    set->type = RAII_BITSET;

    return set;
}

RAII_INLINE bits_t bitset(i32 length, u64 ul) {
    bits_t bits = bitset_create(length);
    deferring((func_t)bitset_free, bits);

    *bits->words = ul;

    return bits;
}

RAII_INLINE void bitset_free(bits_t set) {
    if (is_type(set, RAII_BITSET)) {
        set->type = RAII_ERR;
        if (!is_empty(set->to_string))
            free(set->to_string);

        free(set->words);
        free(set);
    }
}

RAII_INLINE i32 bitset_size(bits_t set) {
    RAII_ASSERT(set);

    return set->length;
}

RAII_INLINE i32 bitset_count(bits_t set) {
    i32 length = 0, n;
    static char count[] = {0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4};
    RAII_ASSERT(set);
    for (n = nbytes(set->length); --n >= 0; ) {
        u8 c = set->bytes[n];
        length += count[c & 0xF] + count[c >> 4];
    }

    return length;
}

RAII_INLINE bool bitset_test(bits_t set, i32 n) {
    RAII_ASSERT(set);
    RAII_ASSERT(0 <= n && n < set->length);

    return (bool)((set->bytes[n / 8] >> (n % 8)) & 1);
}

RAII_INLINE u64 bitset_ullong(bits_t set) {
    return *set->words;
}

RAII_INLINE string bitset_to_string(bits_t set) {
    RAII_ASSERT(set);
    i32 i, max = set->length - 1;
    if (is_empty(set->to_string))
        set->to_string = try_calloc(1, sizeof(char) + set->length + 1);

    memset(set->to_string, '0', set->length);
    for (i = 0; i < set->length; i++) {
        if (bitset_test(set, max))
            set->to_string[i] = '1';

        max--;
    }

    return set->to_string;
}

RAII_INLINE void bitset_set(bits_t set, i32 n) {
    bitset_put(set, n, 1);
}

RAII_INLINE void bitset_reset(bits_t set, i32 n) {
    bitset_put(set, n, 0);
}

RAII_INLINE void bitset_flip(bits_t set, i32 n) {
    bitset_not(set, n, (set->length - 1));
}

RAII_INLINE void bitset_map(bits_t set,
                 void apply(i32 n, i32 bit, void_t cl), void_t cl) {
    i32 n;
    RAII_ASSERT(set);
    for (n = 0; n < set->length; n++)
        apply(n, ((set->bytes[n / 8] >> (n % 8)) & 1), cl);
}

RAII_INLINE i32 bitset_eq(bits_t s, bits_t t) {
    i32 i;
    RAII_ASSERT(s && t);
    RAII_ASSERT(s->length == t->length);
    for (i = nwords(s->length); --i >= 0; )
        if (s->words[i] != t->words[i])
            return 0;

    return 1;
}

RAII_INLINE i32 bitset_leq(bits_t s, bits_t t) {
    i32 i;
    RAII_ASSERT(s && t);
    RAII_ASSERT(s->length == t->length);
    for (i = nwords(s->length); --i >= 0; )
        if ((s->words[i] & ~t->words[i]) != 0)
            return 0;

    return 1;
}

RAII_INLINE i32 bitset_lt(bits_t s, bits_t t) {
    i32 i, lt = 0;
    RAII_ASSERT(s && t);
    RAII_ASSERT(s->length == t->length);
    for (i = nwords(s->length); --i >= 0; )
        if ((s->words[i] & ~t->words[i]) != 0)
            return 0;
        else if (s->words[i] != t->words[i])
            lt |= 1;

    return lt;
}

RAII_INLINE bits_t bitset_union(bits_t s, bits_t t) {
    setop(copy(t), copy(t), copy(s), | )
}

RAII_INLINE bits_t bitset_inter(bits_t s, bits_t t) {
    setop(copy(t), bitset_create(t->length), bitset_create(s->length), &)
}

RAII_INLINE bits_t bitset_minus(bits_t s, bits_t t) {
    setop(bitset_create(s->length), bitset_create(t->length), copy(s), &~)
}

RAII_INLINE bits_t bitset_diff(bits_t s, bits_t t) {
    setop(bitset_create(s->length), copy(t), copy(s), ^)
}
