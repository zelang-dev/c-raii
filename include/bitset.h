
#ifndef _BITSET_H_
#define _BITSET_H_

#include "raii.h"

typedef struct bits_s *bits_t;
C_API bits_t bitset_create(i32 length);
C_API i32 bitset_size(bits_t set);
C_API i32 bitset_count(bits_t set);
C_API void bitset_free(bits_t set);
C_API u64 bitset_ullong(bits_t set);
C_API string bitset_to_string(bits_t set);
C_API i32 bitset_get(bits_t set, i32 n);
C_API void bitset_set(bits_t set, i32 n);
C_API void bitset_reset(bits_t set, i32 n);
C_API void bitset_flip(bits_t set, i32 n);
C_API i32 bitset_lt(bits_t s, bits_t t);
C_API i32 bitset_eq(bits_t s, bits_t t);
C_API i32 bitset_leq(bits_t s, bits_t t);
C_API void bitset_map(bits_t set, void apply(i32 n, i32 bit, void_t cl), void_t cl);
C_API bits_t bitset_union(bits_t s, bits_t t);
C_API bits_t bitset_inter(bits_t s, bits_t t);
C_API bits_t bitset_minus(bits_t s, bits_t t);
C_API bits_t bitset_diff(bits_t s, bits_t t);

#endif
