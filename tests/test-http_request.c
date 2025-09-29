#include "url_http.h"
#include "test_assert.h"

TEST(http_request) {
    http_t *parser = http_for(nullptr, 1.1);
    ASSERT_TRUE((type_of(parser) == RAII_HTTPINFO));

    string request = str_concat(6, "GET /index.html HTTP/1.1" CRLF,
                                  "Host: url.com" CRLF,
                                  "Accept: */*" CRLF,
                                  "User-Agent: http_client" CRLF,
                                  "X-Powered-By: Ze" CRLF,
                                  "Connection: close" CRLF CRLF);

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

    parse_http(HTTP_REQUEST, parser, raw);
	ASSERT_STR(request, http_request(parser, HTTP_GET, "url.com/index.html", nullptr, nullptr, 2,
		kv(head_by, "Ze"),
		kv(head_conn, "close"))
	);

	ASSERT_STR("http://url.com/index.html", http_get_url(parser));
    ASSERT_STR("PHP-SOAP/BeSimpleSoapClient", http_get_header(parser, "User-Agent"));
    ASSERT_STR("url.com:80", http_get_header(parser, "Host"));
    ASSERT_STR("*/*", http_get_header(parser, "Accept"));
    ASSERT_STR("deflate, gzip", http_get_header(parser, "Accept-Encoding"));
    ASSERT_STR("text/xml; charset=utf-8", http_get_header(parser, "Content-Type"));
    ASSERT_STR("1108", http_get_header(parser, "Content-Length"));
    ASSERT_STR("100-continue", http_get_header(parser, "Expect"));
    ASSERT_STR("utf-8", http_get_var(parser, "Content-Type", "charset"));
	ASSERT_STR("", http_get_var(parser, "Expect", "charset"));

    ASSERT_FALSE(http_has_header(parser, "Pragma"));

    ASSERT_STR("<b>hello world</b>", http_get_body(parser));
    ASSERT_STR("one", http_get_param(parser, "free"));
    ASSERT_STR("POST", http_get_method(parser));
    ASSERT_STR("/path", http_get_path(parser));
    ASSERT_STR("HTTP/1.1", http_get_protocol(parser));

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