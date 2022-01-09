#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#undef NDEBUG // Always debug
#include <assert.h>
#include <unistd.h>

struct parsed_flags {
    bool prefer_xonsh;
    bool verbose;
    bool fallback_to_zsh;
    char* python_bin;
};

// This is my argparse library
// It aint much bit its honest work
#include "idiot_argparse.h"

const char *HELP = "run-shell - The simple shell manager\n\n"
    "Will find and run the user's prefered shell, with an optional fallback\n"
    "\n"
    "Options:\n"
    "  --prefer-xonsh, --xonsh, -x  --- Attempts to find and run `xonsh` instead of the user's default shell\n"
    "\n"
    "  --verbose, -v --- Print verbose information\n"
    "\n"
    "  --fallback-to-zsh, --fallback, -f --- After xonsh exits, fallback to running `zsh`\n"
    "\n"
    "  --python-bin [path] - The path to the python binary to use.\n";


enum shell_kind {
    SHELL_XONSH,
    SHELL_ZSH,
    SHELL_OTHER
};
#define MAX_SHELL_ARGS 8
struct detected_shell {
    char *binary;
    // Remember argv[0] is binary name
    char *argv[MAX_SHELL_ARGS];
    int argc;
    enum shell_kind kind;
};
struct detected_shell default_shell() {
    struct detected_shell res = {.argc = 1, .kind = SHELL_ZSH};
    res.binary = "/usr/bin/zsh";
    if (access(res.binary, R_OK|X_OK) != 0) {
        res.binary = "/bin/zsh";
    }
    if (access(res.binary, R_OK|X_OK) != 0) {
        res.binary = "/bin/sh";
        // Probably bash
        res.kind = SHELL_OTHER;
    }
    return res;
}
struct detected_shell xonsh_shell(char* python_bin) {
    assert(python_bin != NULL);
    struct detected_shell res = {.argc = 3, .kind = SHELL_XONSH, .binary = python_bin};
    res.argv[1] = "-m";
    res.argv[2] = "xonsh";
    return res;
}
void verify_shell(struct detected_shell *shell) {
    assert(shell->binary != NULL);
    if (access(shell->binary, R_OK|X_OK) != 0) {
        fprintf(stderr, "Unable to access %s - Does it exist?\n", shell->binary);
        exit(1);
    }
}
int exec_shell(struct detected_shell *shell) {
    assert(shell->binary != NULL);
    assert(shell->argc < MAX_SHELL_ARGS); // Check for overflow
    verify_shell(shell);
    fflush(stderr);
    shell->argv[0] = shell->binary;
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

int main(int argc, char* argv[]) {
    struct arg_parser parser = init_args(argc, argv);
    struct parsed_flags flags = {};
    while (has_flag_args(&parser)) {
        static const char* XONSH_ALASES[] = {"xonsh", NULL};
        static const char* FALLBACK_ALIASES[] = {"fallback", NULL};
        static const struct arg_config XONSH_CONFIG = {.flag = true, .short_name = "x", .aliases = XONSH_ALASES};
        static const struct arg_config VEBROSE_CONFIG = {.flag = true, .short_name = "v"};
        static const struct arg_config FALLBACK_CONFIG = {.flag = true, .short_name = "f", .aliases = FALLBACK_ALIASES};
        static const struct arg_config HELP_CONFIG = {.flag = true, .short_name = "h"};
        if (match_arg(&parser, "prefer-xonsh", &XONSH_CONFIG)) {
            flags.prefer_xonsh = true;
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
    struct detected_shell shell;
    if (flags.prefer_xonsh) {
        shell = xonsh_shell(flags.python_bin != NULL ? flags.python_bin : "/usr/bin/python3");
    } else {
        shell = default_shell();
    }
    verify_shell(&shell);
    pid_t child_pid;
    if (flags.fallback_to_zsh && shell.kind == SHELL_XONSH) {
        if (flags.verbose) {
            fprintf(stderr, "NOTE: Forking process to enable zsh fallback\n");
        }
        struct detected_shell fallback_shell = default_shell();
        verify_shell(&fallback_shell);
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