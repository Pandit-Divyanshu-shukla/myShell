#include <stdio.h>
#include <string.h>

#include "shell.h"


/**
 * @brief Parses input string into arguments array.
 */
void parse_input(char *input, char **args) {
    int i = 0;
    char *token = strtok(input, " \t");

    while (token != NULL && i < MAX_ARGS - 1) {
        args[i++] = token;
        token = strtok(NULL, " \t");
    }
    args[i] = NULL;
}