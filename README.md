# MyShell — Custom Unix Shell in C

## Features
- Execute system commands (ls, pwd, etc.)
- Built-in commands:
  - cd
  - exit
- Fork + exec implementation

## Concepts Used
- Process creation (fork)
- Program execution (execvp)
- System calls
- Basic command parsing

## How to Run
```bash
gcc shell.c -o shell
./shell
