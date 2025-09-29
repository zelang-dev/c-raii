#include "url_http.h"
#include "json.h"
#include "test_assert.h"

#if defined(_WIN32) || defined(_WIN64)
#   define TESTDIR "../../tests"
#else
#   define TESTDIR "../tests"
#endif

TEST(parse_multipart) {
	int unused;
    http_t *parser = http_for(nullptr, 1.1);

#if defined(_WIN32) || defined(_WIN64)
	unused = chdir("Debug");
#endif

	unused = chdir(TESTDIR);
	string raw = json_read_file("multibody/bad_multipart.txt");

	parse_http(HTTP_RESPONSE, parser, raw);
    ASSERT_TRUE((type_of(parser) == RAII_HTTPINFO));

	ASSERT_STR("curl/7.21.2 (x86_64-apple-darwin)",
		http_get_header(parser, "User-Agent"));
	ASSERT_STR("localhost:8080", http_get_header(parser, "Host"));
	ASSERT_STR("*/*", http_get_header(parser, "Accept"));
	ASSERT_STR("1143", http_get_header(parser, "Content-Length"));
	ASSERT_STR("100-continue", http_get_header(parser, "Expect"));
	ASSERT_STR("line one",
		http_get_header(parser, "X-Multi-Line"));
	ASSERT_TRUE(http_is_multipart(parser));
	ASSERT_STR("multipart/form-data; boundary=----------------------------83ff53821b7c",
		http_get_header(parser, "Content-Type"));
	ASSERT_STR("----------------------------83ff53821b7c",
		http_get_boundary(parser));

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
