#include "url_http.h"
#include "json.h"
#include "test_assert.h"

#if defined(_WIN32) || defined(_WIN64)
#   define TESTDIR "../../tests"
#else
#   define TESTDIR "../tests"
#endif

TEST(parse_multipart) {
    http_t *parser = http_for(nullptr, 1.1);
	int unused;

#if defined(_WIN32) || defined(_WIN64)
	unused = chdir("Debug");
#endif

	unused = chdir(TESTDIR);
	string raw = json_read_file("multibody/basic_multipart.txt");

	parse_http(HTTP_REQUEST, parser, raw);
    ASSERT_TRUE((type_of(parser) == RAII_HTTPINFO));

	ASSERT_STR("Mozilla/5.0 Gecko/2009042316 Firefox/3.0.10", http_get_header(parser, "User-Agent"));
	ASSERT_STR("aram", http_get_header(parser, "Host"));
	ASSERT_STR("text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8", http_get_header(parser, "Accept"));
	ASSERT_STR("514", http_get_header(parser, "Content-Length"));
	ASSERT_STR("keep-alive", http_get_header(parser, "Connection"));
	ASSERT_STR("300", http_get_header(parser, "Keep-Alive"));
	ASSERT_STR("en-us,en;q=0.5", http_get_header(parser, "Accept-Language"));
	ASSERT_STR("gzip,deflate", http_get_header(parser, "Accept-Encoding"));
	ASSERT_STR("ISO-8859-1,utf-8;q=0.7,*;q=0.7", http_get_header(parser, "Accept-Charset"));
	ASSERT_STR("http://aram/~martind/banner.htm", http_get_header(parser, "Referer"));
	ASSERT_STR("POST", http_get_method(parser));
	ASSERT_STR("/cgi-bin/qtest", http_get_path(parser));
	ASSERT_STR("HTTP/1.1", http_get_protocol(parser));

	ASSERT_TRUE(http_is_multipart(parser));
	ASSERT_STR("multipart/form-data; boundary=2a8ae6ad-f4ad-4d9a-a92c-6d217011fe0f",
		http_get_header(parser, "Content-Type"));
	ASSERT_STR("2a8ae6ad-f4ad-4d9a-a92c-6d217011fe0f",
		http_get_boundary(parser));

	ASSERT_XEQ(3, http_multi_count(parser));
	ASSERT_TRUE(http_multi_has(parser, "datafile1")
		&& http_multi_has(parser, "datafile2")
		&& http_multi_has(parser, "datafile3"));

	ASSERT_STR("GIF87a.............,...........D..;", http_multi_body(parser, "datafile3"));
	ASSERT_XEQ(35, http_multi_length(parser, "datafile3"));
	ASSERT_TRUE(http_multi_is_file(parser, "datafile2"));
	ASSERT_STR("g.gif", http_multi_filename(parser, "datafile2"));
	ASSERT_STR("image/gif", http_multi_type(parser, "datafile1"));
	ASSERT_STR("form-data", http_multi_disposition(parser, "datafile1"));

	unused = chdir("../build");
#if defined(_WIN32) || defined(_WIN64)
	unused = chdir("Debug");
#endif

	raii_destroy();
    return 0;
}

TEST(list) {
    int result = 0;

	EXEC_TEST(parse_multipart);

    return result;
}

int main(int argc, char **argv) {
    TEST_FUNC(list());
}
