#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "shell.h"
/**
 * @brief Handles built-in commands like cd and exit.
 * @return 1 if handled, 0 otherwise
 */
int handle_builtin(char **args) {
    if (args[0] == NULL) return 1;

    if (strcmp(args[0], "history") == 0) {
        show_history();
        return 1;
    }

    if (strcmp(args[0], "cd") == 0) {
        if (args[1] == NULL)
            chdir(getenv("HOME"));
        else if (chdir(args[1]) != 0)
            perror("cd failed");
        return 1;
    }

    if (strcmp(args[0], "exit") == 0) {
        printf("Exiting shell\n");
        exit(0);
    }

    return 0;
}