#include "json.h"
#include "test_assert.h"

#if defined(_WIN32) || defined(_WIN64)
#   define TESTDIR "../../tests/valid"
#else
#   define TESTDIR "../tests/valid"
#endif

TEST(json_parse_file) {
    DIR *dirp;
    struct dirent *dp;
    json_t *encoded;
    int unused;

    dirp = opendir(TESTDIR);
    deferring((func_t)closedir, dirp);
    unused = chdir(TESTDIR);
    printf("Valid Json Folder:\n");
    while (!is_empty((dp = readdir(dirp)))) {
        if (dp->d_name[0] != '.' && !is_empty((encoded = json_parse_file(dp->d_name)))) {
            deferring((func_t)json_value_free, encoded);
            ASSERT_TRUE(is_json(encoded));
        }
    }

    return 0;
}

TEST(is_string_json) {
    DIR *dirp;
    struct dirent *dp;
    FILE *file;
    char buffer[10000];
    size_t n;
    int unused;

    unused = chdir("..");
    dirp = opendir("invalid");
    deferring((func_t)closedir, dirp);
    unused = chdir("invalid");
    printf("Invalid Json Folder:\n");
    while ((dp = readdir(dirp)) != NULL) {
        if (dp->d_name[0] != '.' && (file = fopen(dp->d_name, "r")) != NULL) {
            deferring((func_t)fclose, file);
            n = fread(buffer, sizeof(char), 10000, file);
            buffer[n] = '\0';
            ASSERT_FALSE(is_string_json(buffer));
        }
    }
    unused = chdir("../..");

    return 0;
}

TEST(list) {
    int result = 0;

    EXEC_TEST(json_parse_file);
    EXEC_TEST(is_string_json);

    raii_destroy();
    return result;
}

int main(int argc, char **argv) {
    TEST_FUNC(list());
}
