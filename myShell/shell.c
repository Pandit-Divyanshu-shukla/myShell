#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>

int main() {
    char input[100];

    // Infinite loop → shell keeps running
    while (1) {
        // Print prompt
        printf("panditzzz>> ");

        // Take input from user
        fgets(input, sizeof(input), stdin);

        // Remove newline character
        input[strcspn(input, "\n")] = 0;

        // Array to store arguments (argv)
        char *args[10];
        int i = 0;

        // Tokenize input using space and tab
        char *token = strtok(input, " \t");

        while (token != NULL && i < 9) {
            args[i++] = token;
            token = strtok(NULL, " \t");
        }

        // NULL terminate args (VERY IMPORTANT for execvp)
        args[i] = NULL;

        // If empty input → skip
        if (args[0] == NULL) continue;

        // ================= BUILT-IN: cd =================
        if (strcmp(args[0], "cd") == 0) {
            if (args[1] == NULL) {
                // Go to HOME directory
                chdir(getenv("HOME"));  
            } else {
                // Change directory
                if (chdir(args[1]) != 0) {
                    perror("cd failed");
                }
            }
            continue; // skip fork
        }

        // ================= BUILT-IN: exit =================
        if (strcmp(args[0], "exit") == 0) {
            printf("Exiting the P-Shell\n");
            exit(0);
        }

        int redirect = 0;
        int append = 0;

        char* filename = NULL;

        for(int j=0; args[j]!=NULL; j++){
            if(strcmp(args[j],">") == 0){
                if (args[j+1] == NULL) {
                    printf("Error: No file specified\n");
                    break;
                }
                redirect = 1;
                filename = args[j+1];
                args[j] = NULL;
                break;
            }

            //append

            if(strcmp(args[j],">>") == 0){
                 if (args[j+1] == NULL) {
                    printf("Error: No file specified\n");
                    break;
                }
                redirect=1;
                append = 1;
                filename = args[j+1];
                args[j] = NULL;
                break;
            }

            
        }
        

        // ================= PROCESS CREATION =================
        pid_t pid = fork();

        if (pid == 0) {
            // CHILD PROCESS

            if(redirect){
                int fd;
                if(append){
                    fd = open(filename,O_WRONLY|O_CREAT|O_APPEND,0644);
                }else{
                    fd = open(filename,O_WRONLY|O_CREAT|O_TRUNC,0644);
                }

                if(fd<0){
                    perror("File Open Failed");
                    exit(1);
                }

                dup2(fd,STDOUT_FILENO);

                close(fd);
            }

            // Replace child with new program
            execvp(args[0], args);

            // Only runs if exec fails
            perror("execution failed");
            exit(1);
        } else {
            // PARENT PROCESS waits for child
            wait(NULL);
        }
    }
}