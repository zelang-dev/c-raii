#include "raii.h"
#include "test_assert.h"

TEST(raii_encode64) {
    {
        // Success
        const char string[] = "IMProject is a very cool project!";
        u_string base64 = raii_encode64(string);
        ASSERT_STR("SU1Qcm9qZWN0IGlzIGEgdmVyeSBjb29sIHByb2plY3Qh", base64);
        ASSERT_TRUE(is_base64(base64));
    }
    {
        // Success
        const char string[] = "Hehe";
        u_string base64 = raii_encode64(string);
        ASSERT_STR("SGVoZQ==", base64);
        ASSERT_TRUE(is_base64(base64));
    }
    {
        // Success
        const char string[] = "jc";
        u_string base64 = raii_encode64(string);
        ASSERT_STR("amM=", base64);
        ASSERT_TRUE(is_base64(base64));
    }

    return 0;
}

TEST(raii_decode64) {
    {
        // Success
        char text[] = "IMProject is a very cool project!";
        u_string base64 = "SU1Qcm9qZWN0IGlzIGEgdmVyeSBjb29sIHByb2plY3Qh";
        u_string data = raii_decode64(base64);
        ASSERT_STR(text, data);
    }
    {
        // Success
        char text[] = "B?E(H+MbQeThWmZq4t7w!z%C*F)J@NcR";
        const char base64[] = "Qj9FKEgrTWJRZVRoV21acTR0N3cheiVDKkYpSkBOY1I=";
        u_string data = raii_decode64(base64);
        ASSERT_STR(text, data);
    }
    {
        // Success
        char text[] = "Hehe";
        const char base64[] = "SGVoZQ==";
        u_string data = raii_decode64(base64);
        ASSERT_STR(text, data);
    }
    {
        const char base64[] = "SGVoZQ1==";
        u_string data = raii_decode64(base64);
        ASSERT_NULL(data);
    }

    return 0;
}

TEST(list) {
    int result = 0;

    EXEC_TEST(raii_encode64);
    EXEC_TEST(raii_decode64);

    raii_deferred_clean();
    return result;
}

int main(int argc, char **argv) {
    TEST_FUNC(list());
}
