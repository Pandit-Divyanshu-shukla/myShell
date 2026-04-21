#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>


int main(){
    char input[100];
    while(1){
        printf("panditzzz>>");
        // fgets(buffer, size, stream);
        fgets(input,sizeof(input),stdin);
        // strcspn(buffer,char to find)  -> returns index of char founds in buffer
        input[strcspn(input, "\n")] = 0;

        // argument list
        char *args[10];
        //Updated Parser
        int i=0;

        char *token = strtok(input," \t");

        while(token!=NULL && i<9){
            args[i++] = token;
            token = strtok(NULL," \t"); 
        }

        args[i] = NULL;

        // args[i] = strtok(input," ");

        // //extracting token (Old Parser)
        // while(args[i] != NULL){
        //     i++;
        //     args[i] = strtok(NULL," ");
        // }

        if(args[0] == NULL) continue;

        // CD
        if(strcmp(args[0],"cd")==0){
            if(args[1]==NULL){
                chdir("origin");
            }else{
                if (chdir(args[1]) != 0){ // return 0 when changed directory
                    perror("No Folder with Name is Present");
                }  
            }
            continue;
            // Means the command below fork and execvp can be skipped
        }

        //Exit
        if(strcmp(args[0],"exit")==0){
            printf("Exiting the P-Shell");
            exit(0);
        }

        pid_t pid = fork();

        if(pid==0){
            //child
            execvp(args[0],args);
            // execvp("ls", ["ls", "-l", NULL])
            // ls -l

            //Only runs when execp failed

            perror("execution Failed");
            exit(1);
        }else{
            wait(NULL);
        }
    }
}