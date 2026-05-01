#include <unistd.h>
#include <string.h>
#include "shell.h"

int main() {
    char input[MAX_INPUT];
    char *args[MAX_ARGS];
    char *left[MAX_ARGS], *right[MAX_ARGS];

    while (1) {
        read_input(input);
        parse_input(input, args);

        if (args[0] == NULL) continue;

        if (handle_builtin(args)) continue;

        char *filename = NULL;
        int append = 0;
        int redirect = handle_redirection(args, &filename, &append);

        if (handle_pipe(args, left, right)) {
            execute_pipe(left, right);
            continue;
        }

        execute_command(args, redirect, filename, append);
    }
}