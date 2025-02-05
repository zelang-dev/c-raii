#include "url_http.h"
#include "test_assert.h"

TEST(http_request) {
    http_t *parser = http_for(HTTP_REQUEST, nullptr, 1.1);
    ASSERT_TRUE((type_of(parser) == RAII_HTTPINFO));

    string request = str_concat(13, "GET /index.html HTTP/1.1", CRLF,
                                  "Host: url.com", CRLF,
                                  "Accept: */*", CRLF,
                                  "User-Agent: http_client", CRLF,
                                  "X-Powered-By: Ze", CRLF,
                                  "Connection: close", CRLF, CRLF);

    char raw[] = "POST /path?free=one&open=two HTTP/1.1\n\
 User-Agent: PHP-SOAP/BeSimpleSoapClient\n\
 Host: url.com:80\n\
 Accept: */*\n\
 Accept-Encoding: deflate, gzip\n\
 Content-Type:text/xml; charset=utf-8\n\
 Content-Length: 1108\n\
 Expect: 100-continue\n\
 \n\n\
 <b>hello world</b>";

    parse_http(parser, raw);
    ASSERT_STR(request, http_request(parser, HTTP_GET, "http://url.com/index.html", nullptr, nullptr, nullptr, "x-powered-by=Ze;"));
    ASSERT_STR("http://url.com/index.html", parser->uri);
    ASSERT_STR("PHP-SOAP/BeSimpleSoapClient", get_header(parser, "User-Agent"));
    ASSERT_STR("url.com:80", get_header(parser, "Host"));
    ASSERT_STR("*/*", get_header(parser, "Accept"));
    ASSERT_STR("deflate, gzip", get_header(parser, "Accept-Encoding"));
    ASSERT_STR("text/xml; charset=utf-8", get_header(parser, "Content-Type"));
    ASSERT_STR("1108", get_header(parser, "Content-Length"));
    ASSERT_STR("100-continue", get_header(parser, "Expect"));
    ASSERT_STR("utf-8", get_variable(parser, "Content-Type", "charset"));
    ASSERT_STR("", get_variable(parser, "Expect", "charset"));

    ASSERT_FALSE(has_header(parser, "Pragma"));

    ASSERT_STR("<b>hello world</b>", parser->body);
    ASSERT_STR("one", get_parameter(parser, "free"));
    ASSERT_STR("POST", parser->method);
    ASSERT_STR("/path", parser->path);
    ASSERT_STR("HTTP/1.1", parser->protocol);

    raii_destroy();
    return 0;
}

TEST(list) {
    int result = 0;

    EXEC_TEST(http_request);

    return result;
}

int main(int argc, char **argv) {
    TEST_FUNC(list());
}