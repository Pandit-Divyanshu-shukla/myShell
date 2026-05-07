#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "shell.h"

/**
 * @brief Executes command with optional redirection.
 */
void execute_command(char **args, int redirect, char *filename, int append, int background) {
    pid_t pid = fork();

    if (pid == 0) {
        if (redirect) {
            int fd = append ?
                open(filename, O_WRONLY | O_CREAT | O_APPEND, 0644) :
                open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);

            if (fd < 0) {
                perror("file error");
                exit(1);
            }

            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

        execvp(args[0], args);
        perror("exec failed");
        exit(1);
    } else {
        if(background){
            printf("Background Process is Running with PID : %d\n",pid);
        }else{
            waitpid(pid, NULL, 0);
        }   
    }
}
