#include "raii.h"
#include "test_assert.h"

TEST(cli_getopt) {
	cli_message_set("\tgarbage -n nothing -first=hello --last=world -bool bar=45\n", false);
	ASSERT_TRUE(is_cli_getopt(nullptr, true));
	ASSERT_STR("garbage", cli_getopt());
	ASSERT_TRUE(is_cli_getopt("-n", false));
	ASSERT_STR("nothing", cli_getopt());
	ASSERT_TRUE(is_cli_getopt("-first", true));
	ASSERT_STR("hello", cli_getopt());
	ASSERT_TRUE(is_cli_getopt("--last", true));
	ASSERT_STR("world", cli_getopt());
	ASSERT_TRUE(is_cli_getopt("-bool", true));
	ASSERT_STR("-bool", cli_getopt());
	ASSERT_TRUE(is_cli_getopt("bar", true));
	ASSERT_XEQ(45, atoi(cli_getopt()));
	ASSERT_FALSE(is_cli_getopt("help", true));

	raii_destroy();
    return 0;
}

TEST(list) {
    int result = 0;

	EXEC_TEST(cli_getopt);

    return result;
}

int main(int argc, char **argv) {
    TEST_FUNC(list());
}
