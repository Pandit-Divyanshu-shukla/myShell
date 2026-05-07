#include "shell.h"
#include <string.h>

int handle_background(char **args){
    for( int i=0; args[i]!=NULL; i++){
        if(strcmp(args[i],"&")==0){
            // remove &
            args[i] = NULL;
            return 1;
        }
    }

    return 0;
}
