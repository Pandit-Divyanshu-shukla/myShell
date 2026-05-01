#include "shell.h"
#include <string.h>

int handle_redirection(char **args, char **filename, int *append) {
    for (int j = 0; args[j] != NULL; j++) {
        if (strcmp(args[j], ">") == 0) {
            *filename = args[j + 1];
            *append = 0;
            args[j] = NULL;
            return 1;
        }
        if (strcmp(args[j], ">>") == 0) {
            *filename = args[j + 1];
            *append = 1;
            args[j] = NULL;
            return 1;
        }
    }
    return 0;
}