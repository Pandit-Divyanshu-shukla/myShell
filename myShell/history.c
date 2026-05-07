#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "shell.h"

#define MAX_HISTORY 5

char history[MAX_HISTORY][MAX_INPUT];
int history_cnt = 0;

//Taking Const so that somehow input dont't change
void add_history(const char* input){
    if(input == NULL || strlen(input) == 0) return;
    strcpy(history[history_cnt%MAX_HISTORY], input);
    history_cnt++;
}

void show_history(){
    int start = history_cnt<MAX_HISTORY ? 0: history_cnt-MAX_HISTORY;

    for(int i=start; i<history_cnt; i++){
        printf("%d %s\n", i + 1, history[i % MAX_HISTORY]);
    }
}