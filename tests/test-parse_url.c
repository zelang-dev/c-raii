#include "url_http.h"
#include "test_assert.h"

TEST(parse_url) {
    url_t *url = parse_url("http://secret:hideout@zelang.dev:80/this/is/a/very/deep/directory/structure/and/file.html?lots=1&of=2&parameters=3&too=4&here=5#some_page_ref123");
    ASSERT_TRUE((type_of(url) == RAII_SCHEME_TCP));
    ASSERT_STR("http", url->scheme);
    ASSERT_STR("zelang.dev", url->host);
    ASSERT_STR("secret", url->user);
    ASSERT_STR("hideout", url->pass);
    ASSERT_EQ(80, url->port);
    ASSERT_STR("/this/is/a/very/deep/directory/structure/and/file.html", url->path);
    ASSERT_STR("lots=1&of=2&parameters=3&too=4&here=5", url->query);
    ASSERT_STR("some_page_ref123", url->fragment);

    fileinfo_t *fileinfo = pathinfo(url->path);
    ASSERT_TRUE((type_of(fileinfo) == RAII_URLINFO));
    ASSERT_STR("/this/is/a/very/deep/directory/structure/and", fileinfo->dirname);
    ASSERT_STR("and", fileinfo->base);
    ASSERT_STR("html", fileinfo->extension);
    ASSERT_STR("file.html", fileinfo->filename);

    raii_destroy();
    return 0;
}

TEST(list) {
    int result = 0;

    EXEC_TEST(parse_url);

    return result;
}

int main(int argc, char **argv) {
    TEST_FUNC(list());
}
