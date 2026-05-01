#ifndef SHELL_H
#define SHELL_H


#define MAX_ARGS 10
#define MAX_INPUT 100

void read_input(char *input);
void parse_input(char *input, char **args);
int handle_builtin(char **args);
int handle_redirection(char **args, char **filename, int *append);
int handle_pipe(char **args, char **left, char **right);
void execute_command(char **args, int redirect, char *filename, int append);
void execute_pipe(char **left, char **right);

#endif