#include "json.h"
#include "test_assert.h"

TEST(json_encode) {
    string_t json = "{\n\
    \"name\": \"John Smith\",\n\
    \"age\": 25,\n\
    \"address\": {\n\
        \"city\": \"Cupertino\"\n\
    },\n\
    \"contact\": {\n\
        \"emails\": [\n\
            \"email@example.com\",\n\
            \"email2@example.com\"\n\
        ]\n\
    }\n\
}";

    json_t *encoded = json_decode(json_for("ss", "email@example.com", "email2@example.com"), false);
    ASSERT_TRUE(is_json(encoded));
    encoded = json_encode("si.s.v",
                          kv("name", "John Smith"),
                          kv("age", 25),
                          kv("address.city", "Cupertino"),
                          kv("contact.emails", encoded));

    ASSERT_STR(json, json_serialize(encoded, true));

    return 0;
}

TEST(list) {
    int result = 0;

    EXEC_TEST(json_encode);

    raii_destroy();
    return result;
}

int main(int argc, char **argv) {
    TEST_FUNC(list());
}
