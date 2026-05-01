# MyShell — Custom Unix Shell in C

## Overview

MyShell is a lightweight Unix-like shell implemented in C. It supports execution of system commands, built-in commands, piping, and output redirection using low-level system calls.

---

## Features

* Execute external commands (`ls`, `pwd`, `echo`, etc.)
* Built-in commands:

  * `cd` (change directory)
  * `exit` (terminate shell)
* Output redirection:

  * `>` (overwrite)
  * `>>` (append)
* Pipe support:

  * `command1 | command2`
* Modular architecture (multi-file design)
* Process management using `fork()` and `wait()`

---

## Concepts Used

* Process creation: `fork()`
* Program execution: `execvp()`
* Inter-process communication: `pipe()`
* File descriptor manipulation: `dup2()`
* File handling: `open()`
* Command parsing using `strtok()`
* Built-in vs external command handling

---

## Project Structure

```
.
├── shell.c        # Main loop
├── input.c        # Input handling
├── parser.c       # Command parsing
├── builtin.c      # Built-in commands
├── execute.c      # Command execution + redirection
├── pipe.c         # Pipe handling
├── shell.h        # Function declarations
├── Makefile       # Build automation
```

---

## Build & Run

### Using Makefile (recommended)

```bash
make
./shell
```

### Manual Compilation

```bash
gcc shell.c input.c parser.c builtin.c execute.c pipe.c -o shell
./shell
```

---

## Example Usage

```bash
ls
pwd
cd ..
ls > out.txt
cat out.txt
ls | grep .c
```

---

## Limitations

* Single pipe supported (no multiple pipe chaining yet)
* No input redirection (`<`)
* No background process support (`&`)
* No command history or autocomplete

---

## Future Improvements

* Multiple pipe support (`cmd1 | cmd2 | cmd3`)
* Input redirection (`<`)
* Background execution (`&`)
* Command history (arrow keys)
* Signal handling (Ctrl+C)

---

## Key Learning

This project demonstrates how a shell works internally by combining process creation, execution, file descriptors, and inter-process communication.

---

## Author

Divyanshu Shukla
