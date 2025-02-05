#include "url_http.h"
#include "test_assert.h"

TEST(parse_http) {
    http_t *parser = http_for(HTTP_RESPONSE, nullptr, 1.1);

    char raw[] = "HTTP/1.1 200 OK\n\
Date: Tue, 12 Apr 2016 13:58:01 GMT\n\
Server: Apache/2.2.14 (Ubuntu)\n\
X-Powered-By: PHP/5.3.14 ZendServer/5.0\n\
Set-Cookie: ZDEDebuggerPresent=php,phtml,php3; path=/\n\
Set-Cookie: PHPSESSID=6sf8fa8rlm8c44avk33hhcegt0; path=/; HttpOnly\n\
Expires: Thu, 19 Nov 1981 08:52:00 GMT\n\
Cache-Control: no-store, no-cache, must-revalidate, post-check=0, pre-check=0\n\
Pragma: no-cache\n\
Vary: Accept-Encoding\n\
Content-Encoding: gzip\n\
Content-Length: 192\n\
Content-Type: text/xml\n\n";

    parse_http(parser, raw);
    ASSERT_TRUE((type_of(parser) == RAII_HTTPINFO));

    ASSERT_STR("Tue, 12 Apr 2016 13:58:01 GMT", get_header(parser, "Date"));
    ASSERT_STR("Apache/2.2.14 (Ubuntu)", get_header(parser, "Server"));
    ASSERT_STR("PHP/5.3.14 ZendServer/5.0", get_header(parser, "X-Powered-By"));
    ASSERT_STR("PHPSESSID=6sf8fa8rlm8c44avk33hhcegt0; path=/; HttpOnly", get_header(parser, "Set-Cookie"));
    ASSERT_STR("Thu, 19 Nov 1981 08:52:00 GMT", get_header(parser, "Expires"));
    ASSERT_STR("no-store, no-cache, must-revalidate, post-check=0, pre-check=0", get_header(parser, "Cache-Control"));
    ASSERT_STR("no-cache", get_header(parser, "Pragma"));
    ASSERT_STR("Accept-Encoding", get_header(parser, "Vary"));
    ASSERT_STR("gzip", get_header(parser, "Content-Encoding"));
    ASSERT_STR("192", get_header(parser, "Content-Length"));
    ASSERT_STR("text/xml", get_header(parser, "Content-Type"));
    ASSERT_STR("HTTP/1.1", parser->protocol);
    ASSERT_STR("OK", parser->message);
    ASSERT_STR("/", get_variable(parser, "Set-Cookie", "path"));

    ASSERT_EQ(200, parser->code);

    ASSERT_TRUE(has_header(parser, "Pragma"));
    ASSERT_TRUE(has_flag(parser, "Cache-Control", "no-store"));
    ASSERT_TRUE(has_variable(parser, "Set-Cookie", "PHPSESSID"));

    raii_destroy();
    return 0;
}

TEST(list) {
    int result = 0;

    EXEC_TEST(parse_http);

    return result;
}

int main(int argc, char **argv) {
    TEST_FUNC(list());
}
