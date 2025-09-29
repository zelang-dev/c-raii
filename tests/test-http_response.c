#include "url_http.h"
#include "test_assert.h"

TEST(http_response) {
    http_t *parser = http_for(nullptr, 1.1);
    ASSERT_TRUE((type_of(parser) == RAII_HTTPINFO));

    string response = str_concat(9, "HTTP/1.1 200 OK" CRLF,
                                 "Date: ", http_std_date(0), CRLF,
                                 "Content-Type: text/html; charset=utf-8" CRLF,
                                 "Content-Length: 11" CRLF,
                                 "Server: http_server" CRLF,
                                 "X-Powered-By: Ze" CRLF CRLF,
                                 "hello world");
	ASSERT_STR(response, http_response(parser, "hello world", STATUS_OK, nullptr, 1,
		kv(head_by, "Ze"))
	);

    return 0;
}

TEST(list) {
    int result = 0;

    EXEC_TEST(http_response);

    raii_destroy();
    return result;
}

int main(int argc, char **argv) {
    TEST_FUNC(list());
}
