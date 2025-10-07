#include "raii.h"

static int command_line_argc;
static char **command_line_argv;
static bool command_line_set = false;
static bool command_line_ordered = false;
static u32 command_line_index = 1;
static int command_line_required = 1;
static string command_line_message = nullptr;
static string command_line_option = nullptr;

static void usage(string_t program, string message) {
	cerr(CLR_LN"Usage: %s OPTIONS\n%s"CLR_LN, program, (is_empty(message) ? "" : message));
	exit_scope();
}

RAII_INLINE void cli_message_set(string_t message, int minium, bool is_ordered) {
	if (!command_line_set) {
		command_line_set = true;
		command_line_message = (string)message;
		command_line_required = minium;
		command_line_ordered = is_ordered;
	}
}

RAII_INLINE string cli_getopt(void) {
	return command_line_option;
}

bool is_cli_getopt(string_t flag, bool is_single) {
	string unknown = nullptr, *flags = nullptr;
	bool show_help = false, is_unknown = false, is_split = false;
	int i = 0;

	// Parse command-line flags
	if (command_line_argc > command_line_required) {
		for (i = command_line_index; i < command_line_argc; i++) {
			if (is_single && is_str_in(command_line_argv[i], "=")) {
				flags = str_split_ex(nullptr, command_line_argv[i], "=", nullptr);
				is_split = true;
			}

			if (flag == nullptr && i == 1) {
				if (is_split)
					free(flags);

				command_line_option = command_line_argv[i];
				if (command_line_ordered)
					command_line_index = i + 1;

				return true;
			} else if (is_str_eq(command_line_argv[i], flag) || (is_split && is_str_eq(flags[0], flag))) {
				if (is_split) {
					command_line_option = flags[1];
					deferring(free, flags);
				} else {
					command_line_option = is_single ? command_line_argv[i] : command_line_argv[++i];
				}

				if (command_line_ordered)
					command_line_index = i + 1;
				return true;
			} else if (is_str_in("-h, --h, ?, help", command_line_argv[i])) {
				show_help = true;
				break;
			} else {
				is_unknown = true;
				unknown = command_line_argv[i];
			}

			if (is_split) {
				free(flags);
				flags = nullptr;
				is_split = false;
			}
		}
	}

	if (is_split && !is_empty(flags))
		free(flags);

	if (is_unknown)
		cerr("\nUnknown flag provided: %s", unknown);

	if (show_help || is_unknown || command_line_argc <= 1)
		usage(command_line_argv[0], command_line_message);

	return false;
}

RAII_INLINE void cli_arguments_set(int argc, char **argv) {
	command_line_argc = argc;
	command_line_argv = argv;
}
