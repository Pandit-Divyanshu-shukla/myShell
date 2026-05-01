#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include "shell.h"

/**
 * @brief Detects and splits pipe into left and right commands.
 * @return 1 if pipe found, 0 otherwise
 */
int handle_pipe(char **args, char **left, char **right) {
    int idx = -1;

    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "|") == 0) {
            idx = i;
            break;
        }
    }

    if (idx == -1) return 0;

    for (int i = 0; i < idx; i++)
        left[i] = args[i];
    left[idx] = NULL;

    int k = 0;
    for (int j = idx + 1; args[j] != NULL; j++)
        right[k++] = args[j];
    right[k] = NULL;

    return 1;
}


/**
 * @brief Executes two commands connected via pipe.
 */
void execute_pipe(char **left, char **right) {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe failed");
        return;
    }

    pid_t p1 = fork();

    if (p1 == 0) {
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);

        execvp(left[0], left);
        perror("exec failed");
        exit(1);
    }

    pid_t p2 = fork();

    if (p2 == 0) {
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[1]);
        close(pipefd[0]);

        execvp(right[0], right);
        perror("exec failed");
        exit(1);
    }

    close(pipefd[0]);
    close(pipefd[1]);

    wait(NULL);
    wait(NULL);
}