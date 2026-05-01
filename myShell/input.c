#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shell.h"


/**
 * @brief Reads user input from stdin.
 */
void read_input(char *input) {
    printf("panditzzz>> ");
    if (fgets(input, MAX_INPUT, stdin) == NULL) {
        printf("\nExiting...\n");
        exit(0);
    }
    input[strcspn(input, "\n")] = 0;
}