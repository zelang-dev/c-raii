#include "raii.h"
#include "test_assert.h"

TEST(memchr) {
    ASSERT_LEQ(memchr8("12345678=90", '='), -1);
    ASSERT_LEQ(memchr8("1234567=890", '='), 7);
    ASSERT_LEQ(memchr8("123456=7890", '='), 6);
    ASSERT_LEQ(memchr8("12345=67890", '='), 5);
    ASSERT_LEQ(memchr8("1234=567890", '='), 4);
    ASSERT_LEQ(memchr8("123=4567890", '='), 3);
    ASSERT_LEQ(memchr8("12=34567890", '='), 2);
    ASSERT_LEQ(memchr8("1=234567890", '='), 1);
    ASSERT_LEQ(memchr8("=1234567890", '='), 0);

    ASSERT_LEQ(memchr8("1234=", '='), 4);
    ASSERT_LEQ(memchr8("123=4", '='), 3);
    ASSERT_LEQ(memchr8("12=34", '='), 2);
    ASSERT_LEQ(memchr8("1=234", '='), 1);
    ASSERT_LEQ(memchr8("=1234", '='), 0);

    ASSERT_LEQ(memchr8("===", '='), 0);
    ASSERT_LEQ(memchr8("==", '='), 0);
    ASSERT_LEQ(memchr8("=", '='), 0);

    //                    123456789 123456789 123456789
    ASSERT_LEQ(simd_memchr("1234567890abcdefghij=", 20, '='), -1);
    ASSERT_LEQ(simd_memchr("12345678=90abcdefghi", 20, '='), 8);
    ASSERT_LEQ(simd_memchr("1234=567890abcdefghi", 20, '='), 4);
}

TEST(cast8) {
    ASSERT_XEQ(cast8("1234567890", 0), 0);
    ASSERT_UEQ(cast8("1234567890", 1), 0x31ull);
    ASSERT_UEQ(cast8("1234567890", 2), 0x3231ull);
    ASSERT_UEQ(cast8("1234567890", 3), 0x333231ull);
    ASSERT_UEQ(cast8("1234567890", 4), 0x34333231ull);
    ASSERT_UEQ(cast8("1234567890", 5), 0x3534333231ull);
    ASSERT_UEQ(cast8("1234567890", 6), 0x363534333231ull);
    ASSERT_UEQ(cast8("1234567890", 7), 0x37363534333231ull);
    ASSERT_UEQ(cast8("1234567890", 8), 0x3837363534333231ull);
}

TEST(atoi) {
    ASSERT_LEQ(atou8("1234567890", 0), 0);
    ASSERT_LEQ(atou8("1234567890", 1), 1);
    ASSERT_LEQ(atou8("1234567890", 2), 12);
    ASSERT_LEQ(atou8("1234567890", 3), 123);
    ASSERT_LEQ(atou8("1234567890", 4), 1234);
    ASSERT_LEQ(atou8("1234567890", 5), 12345);
    ASSERT_LEQ(atou8("1234567890", 6), 123456);
    ASSERT_LEQ(atou8("1234567890", 7), 1234567);
    ASSERT_LEQ(atou8("1234567890", 8), 12345678);

    // long
    ASSERT_UEQ(simd_atou("12345678901234567890", 0), 0ull);
    ASSERT_UEQ(simd_atou("12345678901234567890", 1), 1ull);
    ASSERT_UEQ(simd_atou("12345678901234567890", 2), 12ull);
    ASSERT_UEQ(simd_atou("12345678901234567890", 3), 123ull);
    ASSERT_UEQ(simd_atou("12345678901234567890", 4), 1234ull);
    ASSERT_UEQ(simd_atou("12345678901234567890", 5), 12345ull);
    ASSERT_UEQ(simd_atou("12345678901234567890", 6), 123456ull);
    ASSERT_UEQ(simd_atou("12345678901234567890", 7), 1234567ull);
    ASSERT_UEQ(simd_atou("12345678901234567890", 8), 12345678ull);
    ASSERT_UEQ(simd_atou("12345678901234567890", 9), 123456789ull);
    ASSERT_UEQ(simd_atou("12345678901234567890", 10), 1234567890ull);
    ASSERT_UEQ(simd_atou("12345678901234567890", 11), 12345678901ull);
    ASSERT_UEQ(simd_atou("12345678901234567890", 12), 123456789012ull);
    ASSERT_UEQ(simd_atou("12345678901234567890", 13), 1234567890123ull);
    ASSERT_UEQ(simd_atou("12345678901234567890", 14), 12345678901234ull);
    ASSERT_UEQ(simd_atou("12345678901234567890", 15), 123456789012345ull);
    ASSERT_UEQ(simd_atou("12345678901234567890", 16), 1234567890123456ull);
    ASSERT_UEQ(simd_atou("12345678901234567890", 17), 12345678901234567ull);
    ASSERT_UEQ(simd_atou("12345678901234567890", 18), 123456789012345678ull);
    ASSERT_UEQ(simd_atou("12345678901234567890", 19), 1234567890123456789ull);
    ASSERT_UEQ(simd_atou("12345678901234567890", 20), 12345678901234567890ull);

    // long signed w/o sign
    ASSERT_XEQ(simd_atoi("12345678901234567890", 0), 0);
    ASSERT_UEQ(simd_atoi("12345678901234567890", 1), 1ll);
    ASSERT_UEQ(simd_atoi("12345678901234567890", 2), 12ll);
    ASSERT_UEQ(simd_atoi("12345678901234567890", 3), 123ll);
    ASSERT_UEQ(simd_atoi("12345678901234567890", 4), 1234ll);
    ASSERT_UEQ(simd_atoi("12345678901234567890", 5), 12345ll);
    ASSERT_UEQ(simd_atoi("12345678901234567890", 6), 123456ll);
    ASSERT_UEQ(simd_atoi("12345678901234567890", 7), 1234567ll);
    ASSERT_UEQ(simd_atoi("12345678901234567890", 8), 12345678ll);
    ASSERT_UEQ(simd_atoi("12345678901234567890", 9), 123456789ll);
    ASSERT_UEQ(simd_atoi("12345678901234567890", 10), 1234567890ll);
    ASSERT_UEQ(simd_atoi("12345678901234567890", 11), 12345678901ll);
    ASSERT_UEQ(simd_atoi("12345678901234567890", 12), 123456789012ll);
    ASSERT_UEQ(simd_atoi("12345678901234567890", 13), 1234567890123ll);
    ASSERT_UEQ(simd_atoi("12345678901234567890", 14), 12345678901234ll);
    ASSERT_UEQ(simd_atoi("12345678901234567890", 15), 123456789012345ll);
    ASSERT_UEQ(simd_atoi("12345678901234567890", 16), 1234567890123456ll);
    ASSERT_UEQ(simd_atoi("12345678901234567890", 17), 12345678901234567ll);
    ASSERT_UEQ(simd_atoi("12345678901234567890", 18), 123456789012345678ll);
    ASSERT_UEQ(simd_atoi("12345678901234567890", 19), 1234567890123456789ll);

    // long signed +
    ASSERT_UEQ(simd_atoi("+12345678901234567890", 2), 1ll);
    ASSERT_UEQ(simd_atoi("+12345678901234567890", 3), 12ll);
    ASSERT_UEQ(simd_atoi("+12345678901234567890", 4), 123ll);
    ASSERT_UEQ(simd_atoi("+12345678901234567890", 5), 1234ll);
    ASSERT_UEQ(simd_atoi("+12345678901234567890", 6), 12345ll);
    ASSERT_UEQ(simd_atoi("+12345678901234567890", 7), 123456ll);
    ASSERT_UEQ(simd_atoi("+12345678901234567890", 8), 1234567ll);
    ASSERT_UEQ(simd_atoi("+12345678901234567890", 9), 12345678ll);
    ASSERT_UEQ(simd_atoi("+12345678901234567890", 10), 123456789ll);
    ASSERT_UEQ(simd_atoi("+12345678901234567890", 11), 1234567890ll);
    ASSERT_UEQ(simd_atoi("+12345678901234567890", 12), 12345678901ll);
    ASSERT_UEQ(simd_atoi("+12345678901234567890", 13), 123456789012ll);
    ASSERT_UEQ(simd_atoi("+12345678901234567890", 14), 1234567890123ll);
    ASSERT_UEQ(simd_atoi("+12345678901234567890", 15), 12345678901234ll);
    ASSERT_UEQ(simd_atoi("+12345678901234567890", 16), 123456789012345ll);
    ASSERT_UEQ(simd_atoi("+12345678901234567890", 17), 1234567890123456ll);
    ASSERT_UEQ(simd_atoi("+12345678901234567890", 18), 12345678901234567ll);
    ASSERT_UEQ(simd_atoi("+12345678901234567890", 19), 123456789012345678ll);
    ASSERT_UEQ(simd_atoi("+12345678901234567890", 20), 1234567890123456789ll);

    // long signed -
    ASSERT_UEQ(simd_atoi("-12345678901234567890", 2), -1ll);
    ASSERT_UEQ(simd_atoi("-12345678901234567890", 3), -12ll);
    ASSERT_UEQ(simd_atoi("-12345678901234567890", 4), -123ll);
    ASSERT_UEQ(simd_atoi("-12345678901234567890", 5), -1234ll);
    ASSERT_UEQ(simd_atoi("-12345678901234567890", 6), -12345ll);
    ASSERT_UEQ(simd_atoi("-12345678901234567890", 7), -123456ll);
    ASSERT_UEQ(simd_atoi("-12345678901234567890", 8), -1234567ll);
    ASSERT_UEQ(simd_atoi("-12345678901234567890", 9), -12345678ll);
    ASSERT_UEQ(simd_atoi("-12345678901234567890", 10), -123456789ll);
    ASSERT_UEQ(simd_atoi("-12345678901234567890", 11), -1234567890ll);
    ASSERT_UEQ(simd_atoi("-12345678901234567890", 12), -12345678901ll);
    ASSERT_UEQ(simd_atoi("-12345678901234567890", 13), -123456789012ll);
    ASSERT_UEQ(simd_atoi("-12345678901234567890", 14), -1234567890123ll);
    ASSERT_UEQ(simd_atoi("-12345678901234567890", 15), -12345678901234ll);
    ASSERT_UEQ(simd_atoi("-12345678901234567890", 16), -123456789012345ll);
    ASSERT_UEQ(simd_atoi("-12345678901234567890", 17), -1234567890123456ll);
    ASSERT_UEQ(simd_atoi("-12345678901234567890", 18), -12345678901234567ll);
    ASSERT_UEQ(simd_atoi("-12345678901234567890", 19), -123456789012345678ll);
    ASSERT_UEQ(simd_atoi("-12345678901234567890", 20), -1234567890123456789ll);
}

TEST(htou) {
    ASSERT_LEQ(htou8("123456789abcdef0", 0), 0);
    ASSERT_LEQ(htou8("123456789abcdef0", 1), 0x1);
    ASSERT_LEQ(htou8("123456789abcdef0", 2), 0x12);
    ASSERT_LEQ(htou8("123456789abcdef0", 3), 0x123);
    ASSERT_LEQ(htou8("123456789abcdef0", 4), 0x1234);
    ASSERT_LEQ(htou8("123456789abcdef0", 5), 0x12345);
    ASSERT_LEQ(htou8("123456789abcdef0", 6), 0x123456);
    ASSERT_LEQ(htou8("123456789aBCDEf0", 7), 0x1234567);
    ASSERT_LEQ(htou8("123456789aBCDEf0", 8), 0x12345678);

    ASSERT_UEQ(simd_htou("123456789abcdef0", 0), 0ull);
    ASSERT_UEQ(simd_htou("123456789abcdef0", 1), 0x1ull);
    ASSERT_UEQ(simd_htou("123456789abcdef0", 2), 0x12ull);
    ASSERT_UEQ(simd_htou("123456789abcdef0", 3), 0x123ull);
    ASSERT_UEQ(simd_htou("123456789abcdef0", 4), 0x1234ull);
    ASSERT_UEQ(simd_htou("123456789abcdef0", 5), 0x12345ull);
    ASSERT_UEQ(simd_htou("123456789abcdef0", 6), 0x123456ull);
    ASSERT_UEQ(simd_htou("123456789aBCDEf0", 7), 0x1234567ull);
    ASSERT_UEQ(simd_htou("123456789aBCDEf0", 8), 0x12345678ull);
    ASSERT_UEQ(simd_htou("123456789abCDEF0", 9), 0x123456789ull);
    ASSERT_UEQ(simd_htou("123456789abCDEF0", 10), 0x123456789aull);
    ASSERT_UEQ(simd_htou("123456789abCDEF0", 11), 0x123456789abull);
    ASSERT_UEQ(simd_htou("123456789abCDEF0", 12), 0x123456789abcull);
    ASSERT_UEQ(simd_htou("123456789abCDEF0", 13), 0x123456789abcdull);
    ASSERT_UEQ(simd_htou("123456789ABcdef0", 14), 0x123456789abcdeull);
    ASSERT_UEQ(simd_htou("123456789ABcdef0", 15), 0x123456789abcdefull);
    ASSERT_UEQ(simd_htou("123456789ABcdef0", 16), 0x123456789abcdef0ull);

    ASSERT_LEQ(htou8("abcdef..", 1), 0xa);
    ASSERT_LEQ(htou8("abcdef..", 2), 0xab);
    ASSERT_LEQ(htou8("abcdef..", 3), 0xabc);
    ASSERT_LEQ(htou8("abcdef..", 4), 0xabcd);
    ASSERT_LEQ(htou8("abcdef..", 5), 0xabcde);
    ASSERT_LEQ(htou8("abcdef..", 6), 0xabcdef);

    ASSERT_LEQ(htou8("ABCDEF..", 1), 0xa);
    ASSERT_LEQ(htou8("ABCDEF..", 2), 0xab);
    ASSERT_LEQ(htou8("ABCDEF..", 3), 0xabc);
    ASSERT_LEQ(htou8("ABCDEF..", 4), 0xabcd);
    ASSERT_LEQ(htou8("ABCDEF..", 5), 0xabcde);
    ASSERT_LEQ(htou8("ABCDEF..", 6), 0xabcdef);

    ASSERT_LEQ(htou8("abef0189", 5), 0xabef0);
    ASSERT_LEQ(htou8("abef0189", 6), 0xabef01);
    ASSERT_LEQ(htou8("abef0189", 7), 0xabef018);
    ASSERT_LEQ(htou8("abef0189", 8), 0xabef0189);

    ASSERT_LEQ(htou8("1234abef", 5), 0x1234a);
    ASSERT_LEQ(htou8("1234abef", 6), 0x1234ab);
    ASSERT_LEQ(htou8("1234abef", 7), 0x1234abe);
    ASSERT_LEQ(htou8("1234abef", 8), 0x1234abef);
}

TEST(itoa) {
    int i;
    long long x;
    char itoa_ret[100];
    char test_buf[100];

    for (i = -100000; i < 100000; i++) {
        sprintf(test_buf, "%d", i);
        simd_itoa(i, itoa_ret);
        if (strcmp(itoa_ret, test_buf) != 0)
            ASSERT_STR(itoa_ret, test_buf);
    }

    for (x = LLONG_MIN;
         x < LLONG_MIN; x += 13371) {
        sprintf(test_buf, "%zi", x);
        simd_itoa(x, itoa_ret);
        if (strcmp(itoa_ret, test_buf) != 0)
            ASSERT_STR(itoa_ret, test_buf);
    }

    ASSERT_STR(utoap(1, 0, itoa_ret), "0");
    ASSERT_STR(utoap(2, 0, itoa_ret), "00");
    ASSERT_STR(utoap(3, 0, itoa_ret), "000");
    ASSERT_STR(utoap(4, 0, itoa_ret), "0000");
    ASSERT_STR(utoap(5, 0, itoa_ret), "00000");
    ASSERT_STR(utoap(6, 0, itoa_ret), "000000");
    ASSERT_STR(utoap(7, 0, itoa_ret), "0000000");
    ASSERT_STR(utoap(8, 0, itoa_ret), "00000000");
    ASSERT_STR(utoap(9, 0, itoa_ret), "000000000");
    ASSERT_STR(utoap(10, 0, itoa_ret), "0000000000");
    ASSERT_STR(utoap(11, 0, itoa_ret), "00000000000");
    ASSERT_STR(utoap(12, 0, itoa_ret), "000000000000");
    ASSERT_STR(utoap(13, 0, itoa_ret), "0000000000000");
    ASSERT_STR(utoap(14, 0, itoa_ret), "00000000000000");
    ASSERT_STR(utoap(15, 0, itoa_ret), "000000000000000");
    ASSERT_STR(utoap(16, 0, itoa_ret), "0000000000000000");
    ASSERT_STR(utoap(17, 0, itoa_ret), "00000000000000000");
    ASSERT_STR(utoap(18, 0, itoa_ret), "000000000000000000");
    ASSERT_STR(utoap(19, 0, itoa_ret), "0000000000000000000");

    ASSERT_STR(utoap(0, 7, itoa_ret), "");
    ASSERT_STR(utoap(1, 7, itoa_ret), "7");
    ASSERT_STR(utoap(2, 7, itoa_ret), "07");
    ASSERT_STR(utoap(3, 7, itoa_ret), "007");
    ASSERT_STR(utoap(4, 7, itoa_ret), "0007");
    ASSERT_STR(utoap(5, 7, itoa_ret), "00007");
    ASSERT_STR(utoap(6, 12345, itoa_ret), "012345");
    ASSERT_STR(utoap(7, 12345, itoa_ret), "0012345");
    ASSERT_STR(utoap(8, 12345, itoa_ret), "00012345");
    ASSERT_STR(utoap(9, 12345, itoa_ret), "000012345");
    ASSERT_STR(utoap(10, 12345, itoa_ret), "0000012345");
    ASSERT_STR(utoap(11, 12345, itoa_ret), "00000012345");
    ASSERT_STR(utoap(12, 12345678901, itoa_ret), "012345678901");
    ASSERT_STR(utoap(13, 12345678901, itoa_ret), "0012345678901");
    ASSERT_STR(utoap(14, 12345678901, itoa_ret), "00012345678901");
    ASSERT_STR(utoap(15, 12345678901, itoa_ret), "000012345678901");
    ASSERT_STR(utoap(16, 12345678901, itoa_ret), "0000012345678901");
    ASSERT_STR(utoap(17, 123456789012345, itoa_ret), "00123456789012345");
    ASSERT_STR(utoap(18, 123456789012345, itoa_ret), "000123456789012345");
    ASSERT_STR(utoap(19, 123456789012345, itoa_ret), "0000123456789012345");
    ASSERT_STR(utoap(20, 123456789012345, itoa_ret), "00000123456789012345");

    itoa8(0, itoa_ret); ASSERT_STR(itoa_ret, "0");
    itoa8(1, itoa_ret); ASSERT_STR(itoa_ret, "1");
    itoa8(12, itoa_ret); ASSERT_STR(itoa_ret, "12");
    itoa8(123, itoa_ret); ASSERT_STR(itoa_ret, "123");
    itoa8(1234, itoa_ret); ASSERT_STR(itoa_ret, "1234");
    itoa8(12345, itoa_ret); ASSERT_STR(itoa_ret, "12345");
    itoa8(123456, itoa_ret); ASSERT_STR(itoa_ret, "123456");
    itoa8(1234567, itoa_ret); ASSERT_STR(itoa_ret, "1234567");
    itoa8(12345678, itoa_ret); ASSERT_STR(itoa_ret, "12345678");
}

TEST(strlen) {
    const char *any = "Hello World!";

    size_t result = simd_strlen(any);
    ASSERT_UEQ((size_t)12, result);
}

int test_list() {
    int result = 0;

    EXEC_TEST(test_memchr);
    EXEC_TEST(test_cast8);
    EXEC_TEST(test_atoi);
    EXEC_TEST(test_htou);
    EXEC_TEST(test_itoa);
    EXEC_TEST(test_strlen);

    return result;
}

int main(int argc, char **argv) {
    ASSERT_FUNC(test_list());

    return 0;
}