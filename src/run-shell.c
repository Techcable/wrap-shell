#include <stdbool.h>
#include <string.h>
#include <assert.h>


struct parsed_flags {
    bool prefer_xonsh;
    bool verbose;
    bool fallback_regular;
    char* python_path;
};

struct arg_parser {
    int idx;
    int argc;
    char* argv[];
    char** current_value;
    bool finished;
};
struct arg_config {
    // Short name for this argument
    char* short_name;
    // Full length aliases (null terminated),
    const char** aliases;
    // True if this argument is a flag, false if it is a value
    bool flag;
}
struct arg_parser init_args(int argc, char* argv[]) {
    arg_parser parser = {
        .idx = 1, // NOTE: Arg 0 is program name
        .argv = argv,
        .argc = argc,
        .current_value = NULL
    };
    return parser;
}
inline char* consume_arg(struct arg_parser *parser) {
    assert(parser->idx < parser->argc); // Bounds check
    return parser->argv[parser->idx++];
}

bool match_arg(struct arg_parser *parser, const char *full_name, const struct arg_config config) {
    assert(parser != NULL);
    assert(full_name != NULL);
    if (is_finished(parser)) return false;
    char* current_arg = parser->argv[parser->idx];
    size_t len = strlen(current_arg);
    switch (len) {
        case 0:
        case 1:
            return false;
        case 2:
            if (current_arg[0] == '-') {
                if (current_arg[1] == '-') {
                    // Signals end of flags
                    consume_arg(parser);
                    parser->finished = true;
                    return false;
                }
                char actual_short = current_arg[1];
                if (actual_short == config.short_name) {

                } else {
                    // Some other flag
                }
            } else {
                // Must be a positional arg.
                // Mark as finished
                parser->finished = true;
                return false;
            }
    }
}
inline bool is_finished(struct arg_parser *parser) {
    return parser->finished || parser->idx >= parser->argc;
}

const char[] XONSH_ALASES = {"xonsh", NULL};
int main(int argc, char* argv[]) {
    struct arg_parser parser = init_args(argc, argv);
    struct parsed_flags flags = {};
    while (!is_finished(&parser)) {
        if (match_arg(&parser, "prefer-xonsh", {.flag = true, .short_name = "x", aliases = &XONSH_ALASES})) {

        } else if (match_arg(&parser, "verbose", {.flag = true, .short_name = "v"})) {

        } else if (match_arg(&parser, "")) {

        } else {

        }
    }
} 