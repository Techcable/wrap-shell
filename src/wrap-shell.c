#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#undef NDEBUG // Always debug
#include <assert.h>
#include <unistd.h>
// TODO: Can we (or should we) include this on Mac too?
#ifndef __APPLE__
    #include <sys/wait.h>
#endif

struct parsed_flags {
    bool prefer_xonsh;
    bool prefer_fish;
    bool verbose;
    bool fallback_to_zsh;
    char* python_bin;
};

// This is my argparse library
// It aint much bit its honest work
#include "plain/argparse.h"

const char *HELP = "wrap-shell - The simple shell manager\n\n"
    "Will find and run the user's prefered shell, with an optional fallback\n"
    "Source: https://github.com/Techacble/wrap-shell\n"
    "\n"
    "Options:\n"
    "  --prefer-xonsh, --xonsh, -x  --- Attempts to find and run `xonsh` instead of the user's default shell\n"
    "\n"
    "  --prefer-fish, --fish --- Attempts to find and run `fish` instead of the user's default shell\n"
    "\n"
    "  --verbose, -v --- Print verbose information\n"
    "\n"
    "  --fallback-to-zsh, --fallback, -f --- After xonsh exits, fallback to running `zsh`\n"
    "\n"
    "  --python-bin [path] - The path to the python binary to use.\n";

/**
 * Settings for searching the path
 */
struct path_search_settings {
    // Avoid homebrew on Apple computers
    //
    // By default, homebrew is preferred (effecitvely put at start of path)
    bool avoid_homebrew;
};
static const struct path_search_settings DEFAULT_PATH_SEARCH_SETTINGS = {0};
/**
 * Search the path for the specified executable, returning NULL
 * if not found.
 * 
 * The resulting path is dynamically allocated and must be freed.
 */
static char *search_path(const char *bin_name, struct path_search_settings settings);
/**
 * Check that the binary path exists, and is executable by the current use.
 * 
 * Returns true if the access check succeeds, false if it doesn.t
 */
static inline bool check_binary_path(const char *binary_path) {
    assert(binary_path != NULL);
    return access(binary_path, R_OK|X_OK) == 0;
}

enum shell_kind {
    // Marker for missing shell
    SHELL_MISSING = 0,
    SHELL_XONSH,
    SHELL_FISH,
    SHELL_ZSH,
    SHELL_SH,
    SHELL_KIND_COUNT,
};
#define MAX_SHELL_ARGS 8
struct detected_shell {
    // The path to the binary
    //
    // This is dynamically allocated
    char *binary;
    // Remember argv[0] is binary name
    char *argv[MAX_SHELL_ARGS];
    int argc;
    enum shell_kind kind;
};
static const char *shell_kind_name(enum shell_kind kind) {
    static const char *SHELL_NAMES[SHELL_KIND_COUNT] = {
        "missing",
        "xonsh",
        "fish",
        "zsh",
        "sh"
    };
    if (kind >= 0 && kind < sizeof(SHELL_NAMES)) {
        return SHELL_NAMES[kind];
    } else {
        return "<invalid>";
    }
}
static bool shell_is_missing(struct detected_shell *shell) {
    bool missing = shell->kind == SHELL_MISSING;
    if (!missing) assert(shell->binary != NULL);
    return missing;
}
struct detected_shell default_shell() {
    struct path_search_settings search_settings = {
        // Actually want to avoid it in this case :)
        .avoid_homebrew = true,
    };
    struct detected_shell res = {.argc = 1};
    // prefer zsh
    char *zsh_path = search_path("zsh", search_settings);
    if (zsh_path != NULL) {
        res.binary = zsh_path;
        res.kind = SHELL_ZSH;
        return res;
    }
    // fallback to `sh`
    char *fallback_path = search_path("sh", search_settings);
    if (fallback_path != NULL) {
        res.binary = fallback_path;
        res.kind = SHELL_SH; // probably bash
        return res;
    }
    // nothing more to try :(
    res.binary = NULL;
    res.kind = SHELL_MISSING;
    return res;
}
struct detected_shell xonsh_shell(char* python_bin) {
    struct detected_shell res = {.argc = 3, .kind = SHELL_XONSH, .binary = python_bin};
    if (!check_binary_path(python_bin)) {
        res.kind = SHELL_MISSING;
        res.binary = NULL;
        return res;
    }
    res.argv[1] = "-m";
    res.argv[2] = "xonsh";
    return res;
}
struct detected_shell fish_shell() {
    char *fish_path = search_path("fish", DEFAULT_PATH_SEARCH_SETTINGS);
    struct detected_shell res = {.argc = 1, .kind = SHELL_FISH, .binary = fish_path};
    if (fish_path == NULL) {
        res.kind = SHELL_MISSING;
    }
    return res;
}
int exec_shell(struct detected_shell *shell) {
    assert(!shell_is_missing(shell));
    assert(shell->argc >= 1); // always need one argc for binary name
    assert(shell->argc < MAX_SHELL_ARGS); // Check for overflow
    fflush(stderr);
    // Discard constness
    shell->argv[0] = (char*) shell->binary;
    shell->argv[shell->argc] = NULL;
    int ret = execv(shell->binary, shell->argv);
    if (ret == 0) {
        fprintf(stderr, "ERROR: execv should never return succesfully\n");
        return 0;
    } else {
        perror("Unexpected error executing shell");
        return 1;
    }
}

int protect_against_failure(pid_t child_pid, struct parsed_flags *flags, struct detected_shell *fallback_shell);


/**
 * Checks if the specified pointer is null.
 * 
 * If true, it exits the program with an out of memory error.
 */
static inline void check_oom(void *res) {
    if (res == NULL) {
        fputs("FATAL: Allocation failure", stderr);
        exit(1);
    }
}


// TODO: Some of this could be moved to plainlibs
/**
 * Join the specified paths together, with a slash '/' in between.
 * 
 * If the second path begins with a slash, it is assumed to be absolute
 * and the first argument will be completely ignored.
 * 
 * It is the caller's responsibility to free the resulting memory.
 * 
 * Exits on allocation failure (see check_oom).
 */
static char *join_path_alloc(const char *first, const char *second) {
    assert(first != NULL);
    assert(second != NULL);
    size_t first_len = strlen(first);
    size_t second_len = strlen(second);
    // second is absolute
    if (second_len > 0 && second[0] == '/') {
        return strdup(second);
    }
    size_t result_size = first_len + second_len;
    bool needs_slash = first_len == 0 || first[first_len - 1] != '/';
    if (needs_slash) result_size += 1;
    char *res = malloc(result_size + 1); // need null terminator
    check_oom(res);
    size_t i = 0;
    memcpy(res, first, first_len);
    i += first_len;
    assert(i == first_len);
    if (needs_slash) res[i++] = '/';
    memcpy(res + i, second, second_len);
    i += second_len;
    assert(i == result_size);
    res[i] = '\0';
    return res;
}

struct raw_path_search_res {
    int dir_idx;
    // Will be NULL if missing, non-null if found
    char *binary_path;
};
static struct raw_path_search_res search_path_raw(const char *const *binary_dirs, int num_dirs, const char *binary_name) {
    struct raw_path_search_res res = {0};
    for (int i = 0; i < num_dirs; i++) {
        const char *bin_dir = binary_dirs[i];
        if (access(bin_dir, F_OK) != 0) continue; // doesn't exist
        // Ensure it's actually a homebrew binary, and not something else
        char *binary_path = join_path_alloc(bin_dir, binary_name);
        if (check_binary_path(binary_path)) {
            // success! we've found a path
            res.dir_idx = i;
            res.binary_path = binary_path;
            return res;
        } else {
            free(binary_path);
        }
    }
    assert(res.binary_path == NULL);
    return res;
}

/**
 * If the user has an Apple computer, check for a homebrew binary directory.
 * 
 * The resulting location is statically allocated (no need to free it).
 * 
 * Returns NULL if unable to find path to homebrew binary (or the user is not on a Mac).
 */
const char *detect_homebrew_bin() {
    #ifdef __APPLE__
        #define POSSIBLE_HOMEBREW_PATHS 2
        static const char *HOMEBREW_BIN_PATH[POSSIBLE_HOMEBREW_PATHS] = {
            "/opt/homebrew/bin", // New M1 Mac homebrew bin
            "/usr/local/bin", // Traditional x86 homebrew bin
        };
        struct raw_path_search_res res = search_path_raw(
            HOMEBREW_BIN_PATH,
            POSSIBLE_HOMEBREW_PATHS,
            "brew"
        );
        if (res.binary_path != NULL) {
            free(res.binary_path); // only interested in index
            return HOMEBREW_BIN_PATH[res.dir_idx];
        } else {
            return NULL; // detected nothing
        }
    #else
        return NULL;
    #endif
}

static char *search_path(const char *binary_name, struct path_search_settings settings) {
    if (!settings.avoid_homebrew) {
        const char *homebrew_bin_dir = detect_homebrew_bin(); // borrowed res
        if (homebrew_bin_dir != NULL) {
            // owned res
            char *binary_path = join_path_alloc(homebrew_bin_dir, binary_name);
            if (access(binary_path, R_OK|X_OK) == 0) {
                return binary_path;
            } else {
                free(binary_path);
            }
        }
    }
    char *program_path = getenv("PATH"); // borrowed res
    char *end = NULL; // end of string (points to \0)
    if (program_path == NULL) {
        program_path = "/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/sbin";
    }
    int num_parts = 1;
    // count the total number of occurrences of `:` in the PATH variable
    // we always have one more than this (because ':' is seperator)
    {
        char *s = program_path;
        while (*s != '\0') {
            if (*s == ':') {
                num_parts += 1;
            }
            s += 1;
        }
        assert(*s == '\0');
        // also find end of string while we're at it
        end = s;
    }
    assert(num_parts > 0);
    const char **path_parts = calloc((size_t) num_parts, sizeof(char*));
    check_oom((void*) path_parts);
    // split along ':'
    {
        char *remaining = program_path;
        for (int i = 0; i < num_parts; i++) {
            char *part_end = remaining;
            while (true) {
                assert(part_end <= end);
                char c = *part_end;
                if (c == ':' || c == '\0') break;
                else part_end += 1;
            }
            assert(part_end <= end);
            ptrdiff_t part_size = part_end - remaining;
            assert(part_size >= 0);
            char *path_copy = strndup(remaining, (size_t) part_size);
            check_oom(path_copy); // technically this is required
            path_parts[i] = path_copy;
            remaining = part_end;
            if (*remaining == ':') remaining += 1; // consume
        }
        assert(remaining == end);
    }
    // actually find the path
    struct raw_path_search_res res = search_path_raw(path_parts, num_parts, binary_name);
    for (int i = 0; i < num_parts; i++) {
        const char *part = path_parts[i];
        if (part != NULL) free((void*) part);
    }
    free(path_parts);
    return res.binary_path;
}

/**
 * Detect the default path to the python interpreter.
 * 
 * The resulting path string is owned
 */
char *default_python_path() {
    char *res = search_path("python3", DEFAULT_PATH_SEARCH_SETTINGS);
    if (res == NULL) {
        fprintf(stderr, "Unable to detect system python\n");
        fprintf(stderr, "Please use a standard location or specify explicitly with --python-bin\n");
        exit(1);
    }
    return res;
}

static void exit_if_missing_shell(struct detected_shell *shell, enum shell_kind requested_kind) {
    assert(requested_kind != SHELL_MISSING);
    if (shell_is_missing(shell)) {
        fprintf(
            stderr,
            "ERROR: Unable to find fallback shell: %s\n",
            shell_kind_name(requested_kind)
        );
        exit(1);
    }
}

int main(int argc, char* argv[]) {
    struct arg_parser parser = init_args(argc, argv);
    struct parsed_flags flags = {0};
    while (has_flag_args(&parser)) {
        static const char* XONSH_ALIASES[] = {"xonsh", NULL};
        static const char* FISH_ALIASES[] = {"fish", NULL};
        static const char* FALLBACK_ALIASES[] = {"fallback", NULL};
        static const struct arg_config XONSH_CONFIG = {.flag = true, .short_name = "x", .aliases = XONSH_ALIASES};
        static const struct arg_config FISH_CONFIG = {.flag = true, .aliases = FISH_ALIASES};
        static const struct arg_config VEBROSE_CONFIG = {.flag = true, .short_name = "v"};
        static const struct arg_config FALLBACK_CONFIG = {.flag = true, .short_name = "f", .aliases = FALLBACK_ALIASES};
        static const struct arg_config HELP_CONFIG = {.flag = true, .short_name = "h"};
        if (match_arg(&parser, "prefer-xonsh", &XONSH_CONFIG)) {
            flags.prefer_xonsh = true;
        } else if (match_arg(&parser, "prefer-fish", &FISH_CONFIG)) {
            flags.prefer_fish = true;
        } else if (match_arg(&parser, "verbose", &VEBROSE_CONFIG)) {
            flags.verbose = true;
        } else if (match_arg(&parser, "fallback-to-zsh", &FALLBACK_CONFIG)) {
            flags.fallback_to_zsh = true;
        } else if (match_arg(&parser, "help", &HELP_CONFIG)) {
            puts(HELP);
            return 0;
        } else if (match_arg(&parser, "python-bin", NULL)) {
            flags.python_bin = parser.current_value;
            assert(flags.python_bin != NULL);
        } else {
            fprintf(stderr, "Unknown flag %s\n", current_arg(&parser));
            return 1;
        }
    }
    // We have no positional arguments
    if (has_args(&parser)) {
        fprintf(stderr, "Unexpected positional argument: %s\n", current_arg(&parser));
        return 1;
    }
    enum shell_kind preferred_shell_kind;
    if (flags.prefer_xonsh) {
        if (flags.prefer_fish) {
            fputs("ERROR: Flags --fish and --xonsh are incompatible", stderr);
            exit(1);
        }
        preferred_shell_kind = SHELL_XONSH;
    } else if (flags.prefer_fish) {
        assert(!flags.prefer_xonsh); // checked above
        preferred_shell_kind = SHELL_FISH;
    } else {
        preferred_shell_kind = SHELL_ZSH;
    }
    struct detected_shell shell;
    switch (preferred_shell_kind) {
        case SHELL_XONSH:
            shell = xonsh_shell(flags.python_bin != NULL ? flags.python_bin : default_python_path());
            break;
        case SHELL_FISH:
            shell = fish_shell();
            break;
        case SHELL_ZSH:
            // TODO: default_shell() makes no distinction between zsh/fallback
            shell = default_shell();
            break;
        default:
            // Unimplemented shell
            // Logically speaking this is unreachable
            abort();
    }
    exit_if_missing_shell(&shell, preferred_shell_kind);
    pid_t child_pid;
    if (flags.fallback_to_zsh && shell.kind != SHELL_ZSH) {
        if (flags.verbose) {
            fprintf(stderr, "NOTE: Forking process to enable zsh fallback\n");
        }
        struct detected_shell fallback_shell = default_shell();
        exit_if_missing_shell(&fallback_shell, SHELL_SH);
        fflush(stderr);
        /*
         * A review for the non unix inclined.
         *
         * This will return -1 on error,
         * will return `0` if we are the child process
         *
         * If we are the parent, in which case 
         */
        child_pid = fork();
        if (child_pid < 0) {
            perror("fork");
            exit(1);
        }
        if (child_pid != 0) {
            // We are the parent
            return protect_against_failure(child_pid, &flags, &fallback_shell);
        }
    }
    // Execute the shell (exec replaces the current process)
    return exec_shell(&shell);
} 

int protect_against_failure(pid_t child_pid, struct parsed_flags *flags, struct detected_shell *fallback_shell) {
    assert(fallback_shell != NULL);
    assert(child_pid > 0);
    int status_info = 0;
    do {
        errno = 0;
        pid_t res = waitpid(child_pid, &status_info, 0);
        if (res < 0) {
            switch(errno) {
                case EINTR:
                    if (flags->verbose) {
                        fprintf(stderr, "Interrupted by signal\n");
                    }
                    continue;
                case ECHILD:
                default:
                    perror("Failed to wait for subprocess:");
                    fprintf(stderr, "\nThis is most likely an internal error\n");
                    return 1;
            }
        }
        if (res != child_pid) {
            fprintf(stderr, "Unexpected res from waitpid: %d\n", res);
            return 1;
        } else {}
    } while (!WIFEXITED(status_info) && !WIFSIGNALED(status_info));
    fprintf(stderr, "Falling back to fallback shell (%s):\n", fallback_shell->binary);
    fprintf(stderr, "  Original shell ");
    if (WIFEXITED(status_info)) {
        int code = WEXITSTATUS(status_info);
        if (code == 0) {
            fprintf(stderr, "exited succesfully\n");
        } else {
            fprintf(stderr, "failed with exit code %d\n", code);
        }
    } else if (WIFSIGNALED(status_info)) {
        int signum = WTERMSIG(status_info);
        fprintf(stderr, "was killed by signal %d\n", signum);
    }
    fprintf(stderr, "\n");
    return exec_shell(fallback_shell);
}
