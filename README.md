# MyShell — Custom Unix Shell in C

## Overview

MyShell is a lightweight Unix-like shell implemented in C. It supports execution of system commands, built-in commands, piping, redirection, background processes, and inter-process communication using low-level POSIX system calls.

---

## Features

* Execute external commands (`ls`, `pwd`, `echo`, etc.)
* Built-in commands:

  * `cd` (change directory)
  * `exit` (terminate shell)
  * `history` (show previously executed commands)
* Output redirection:

  * `>` (overwrite)
  * `>>` (append)
* Pipe support:

  * `command1 | command2`
* Background process execution:

  * `command &`
* Command history support
* Modular multi-file architecture
* Process management using `fork()` and `waitpid()`

---

## Concepts Used

* Process creation: `fork()`
* Program execution: `execvp()`
* Inter-process communication (IPC): `pipe()`
* File descriptor manipulation: `dup2()`
* File handling: `open()`
* Process synchronization: `wait()` / `waitpid()`
* Command parsing using `strtok()`
* Background execution handling
* Built-in vs external command handling

---

## Project Structure

```text
.
├── shell.c         # Main shell loop
├── input.c         # Input handling
├── parser.c        # Command parsing
├── builtin.c       # Built-in commands
├── execute.c       # Command execution and redirection
├── pipe.c          # Pipe handling
├── history.c       # Command history management
├── background.c    # Background process handling
├── shell.h         # Shared declarations
├── Makefile        # Build automation
├── README.md
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
gcc shell.c input.c parser.c builtin.c execute.c pipe.c history.c background.c -o shell
./shell
```

---

## Example Usage

### Basic Commands

```bash
ls
pwd
echo Hello
```

### Built-in Commands

```bash
cd ..
history
exit
```

### Output Redirection

```bash
ls > out.txt
echo Hello >> out.txt
```

### Pipe

```bash
ls | grep .c
echo hello | wc -c
```

### Background Process

```bash
sleep 5 &
```

---

## Current Limitations

* Single pipe supported (multiple pipe chaining not implemented yet)
* No input redirection (`<`)
* No autocomplete support
* No arrow-key navigation for history
* Background zombie process cleanup not implemented

---

## Future Improvements

* Multiple pipe support (`cmd1 | cmd2 | cmd3`)
* Input redirection (`<`)
* Signal handling (`Ctrl + C`)
* Command history navigation using arrow keys
* Zombie process handling using `SIGCHLD`
* Environment variable support

---

## Key Learnings

This project helped in understanding:

* How Unix shells execute commands internally
* Process creation and management
* Inter-process communication using pipes
* File descriptor redirection
* Foreground vs background execution
* Modular systems programming in C

---

## Author

Divyanshu Shukla
