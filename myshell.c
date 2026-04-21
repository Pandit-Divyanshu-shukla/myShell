/*
 * ============================================================
 *   MyShell — Enhanced Custom Shell
 *   Features:
 *     - System info   : sysinfo, meminfo, diskinfo
 *     - File utils    : search, countlines, preview
 *     - Aliases       : alias, unalias, aliases
 *     - History       : history, !N recall
 *     - Bookmarks     : bookmark, bookmarks, goto
 *     - Color output  : ANSI color-coded prompt & messages
 *     - Arrow keys    : up/down history navigation (raw mode)
 *     - Autocomplete  : Tab key completes commands & filenames
 *     - Redirection   : cmd > file, cmd >> file
 *     - Pipes         : cmd1 | cmd2 (single pipe)
 * ============================================================
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <fcntl.h>
#include <dirent.h>
#include <termios.h>
#include <errno.h>
#include <pwd.h>
#include <signal.h>
#include <glob.h>

/* ── Constants ─────────────────────────────────────────── */
#define MY_MAX_INPUT    2048
#undef  MAX_INPUT
#define MAX_INPUT       MY_MAX_INPUT
#define MAX_ARGS        128
#define MAX_HISTORY     100
#define MAX_ALIASES     64
#define MAX_BOOKMARKS   32
#define MAX_PATH        1024
#define HIST_FILE       ".myshell_history"
#define ALIAS_FILE      ".myshell_aliases"
#define BOOKMARK_FILE   ".myshell_bookmarks"

/* ── ANSI color macros ──────────────────────────────────── */
#define RESET       "\033[0m"
#define BOLD        "\033[1m"
#define RED         "\033[31m"
#define GREEN       "\033[32m"
#define YELLOW      "\033[33m"
#define BLUE        "\033[34m"
#define MAGENTA     "\033[35m"
#define CYAN        "\033[36m"
#define WHITE       "\033[37m"
#define BRED        "\033[1;31m"
#define BGREEN      "\033[1;32m"
#define BYELLOW     "\033[1;33m"
#define BBLUE       "\033[1;34m"
#define BCYAN       "\033[1;36m"
#define BWHITE      "\033[1;37m"

/* ── Data structures ────────────────────────────────────── */
typedef struct {
    char name[64];
    char value[MAX_INPUT];
} Alias;

typedef struct {
    char name[64];
    char path[MAX_PATH];
} Bookmark;

/* ── Globals ────────────────────────────────────────────── */
static char  *history[MAX_HISTORY];
static int    history_count = 0;

static Alias    aliases[MAX_ALIASES];
static int      alias_count = 0;

static Bookmark bookmarks[MAX_BOOKMARKS];
static int      bookmark_count = 0;

static struct termios orig_termios;   /* saved terminal state */
static int last_exit_code = 0;

/* ══════════════════════════════════════════════════════════
   TERMINAL RAW MODE (for arrow keys & Tab)
   ══════════════════════════════════════════════════════════ */
static void disable_raw_mode(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

static void enable_raw_mode(void) {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    raw.c_iflag &= ~(IXON | ICRNL);
    raw.c_cc[VMIN]  = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

/* ══════════════════════════════════════════════════════════
   PERSISTENCE — load/save history, aliases, bookmarks
   ══════════════════════════════════════════════════════════ */
static char *get_home_path(const char *filename) {
    static char path[MAX_PATH];
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    snprintf(path, sizeof(path), "%s/%s", home, filename);
    return path;
}

static void load_history(void) {
    FILE *f = fopen(get_home_path(HIST_FILE), "r");
    if (!f) return;
    char line[MAX_INPUT];
    while (fgets(line, sizeof(line), f) && history_count < MAX_HISTORY) {
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) > 0)
            history[history_count++] = strdup(line);
    }
    fclose(f);
}

static void save_history(void) {
    FILE *f = fopen(get_home_path(HIST_FILE), "w");
    if (!f) return;
    for (int i = 0; i < history_count; i++)
        fprintf(f, "%s\n", history[i]);
    fclose(f);
}

static void add_history(const char *line) {
    if (!line || strlen(line) == 0) return;
    /* avoid consecutive duplicates */
    if (history_count > 0 && strcmp(history[history_count-1], line) == 0) return;
    if (history_count == MAX_HISTORY) {
        free(history[0]);
        memmove(history, history+1, (MAX_HISTORY-1) * sizeof(char*));
        history_count--;
    }
    history[history_count++] = strdup(line);
}

static void load_aliases(void) {
    FILE *f = fopen(get_home_path(ALIAS_FILE), "r");
    if (!f) return;
    char line[MAX_INPUT];
    while (fgets(line, sizeof(line), f) && alias_count < MAX_ALIASES) {
        line[strcspn(line, "\n")] = 0;
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        strncpy(aliases[alias_count].name,  line,   63);
        strncpy(aliases[alias_count].value, eq + 1, MAX_INPUT - 1);
        alias_count++;
    }
    fclose(f);
}

static void save_aliases(void) {
    FILE *f = fopen(get_home_path(ALIAS_FILE), "w");
    if (!f) return;
    for (int i = 0; i < alias_count; i++)
        fprintf(f, "%s=%s\n", aliases[i].name, aliases[i].value);
    fclose(f);
}

static void load_bookmarks(void) {
    FILE *f = fopen(get_home_path(BOOKMARK_FILE), "r");
    if (!f) return;
    char line[MAX_INPUT];
    while (fgets(line, sizeof(line), f) && bookmark_count < MAX_BOOKMARKS) {
        line[strcspn(line, "\n")] = 0;
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        strncpy(bookmarks[bookmark_count].name, line,   63);
        strncpy(bookmarks[bookmark_count].path, eq + 1, MAX_PATH - 1);
        bookmark_count++;
    }
    fclose(f);
}

static void save_bookmarks(void) {
    FILE *f = fopen(get_home_path(BOOKMARK_FILE), "w");
    if (!f) return;
    for (int i = 0; i < bookmark_count; i++)
        fprintf(f, "%s=%s\n", bookmarks[i].name, bookmarks[i].path);
    fclose(f);
}

/* ══════════════════════════════════════════════════════════
   BANNER
   ══════════════════════════════════════════════════════════ */
static void print_banner(void) {
    printf(BCYAN "\n");
    printf("  ╔══════════════════════════════════════════════╗\n");
    printf("  ║          ✦  MyShell  — Enhanced  ✦          ║\n");
    printf("  ║   \"Focus beats talent when talent doesn't\"  ║\n");
    printf("  ║                  focus.                     ║\n");
    printf("  ╚══════════════════════════════════════════════╝\n");
    printf(RESET);
    printf(YELLOW "  Type " BWHITE "help" YELLOW " for a list of built-in commands.\n\n" RESET);
}

/* ══════════════════════════════════════════════════════════
   PROMPT
   ══════════════════════════════════════════════════════════ */
static void print_prompt(void) {
    char cwd[MAX_PATH];
    if (getcwd(cwd, sizeof(cwd)) == NULL)
        strcpy(cwd, "?");

    /* shorten home dir to ~ */
    const char *home = getenv("HOME");
    char display[MAX_PATH];
    if (home && strncmp(cwd, home, strlen(home)) == 0)
        snprintf(display, sizeof(display), "~%s", cwd + strlen(home));
    else
        strncpy(display, cwd, sizeof(display));

    const char *user = getenv("USER");
    if (!user) user = "user";

    /* exit-code indicator */
    const char *indicator = (last_exit_code == 0)
        ? BGREEN "✔" RESET : BRED "✘" RESET;

    printf("\n" BBLUE "%s" RESET "@" BCYAN "myshell" RESET " " BYELLOW "%s" RESET " %s\n"
           BWHITE ">>> " RESET, user, display, indicator);
    fflush(stdout);
}

/* ══════════════════════════════════════════════════════════
   AUTOCOMPLETE
   ══════════════════════════════════════════════════════════ */
static const char *builtin_names[] = {
    "cd", "exit", "help", "history", "alias", "unalias", "aliases",
    "bookmark", "bookmarks", "goto", "sysinfo", "meminfo", "diskinfo",
    "search", "countlines", "preview", NULL
};

/* collect matches for prefix, store in matches[], return count */
static int collect_matches(const char *prefix, char matches[][MAX_PATH], int max) {
    int n = 0;
    size_t plen = strlen(prefix);

    /* built-ins */
    for (int i = 0; builtin_names[i] && n < max; i++)
        if (strncmp(builtin_names[i], prefix, plen) == 0)
            strncpy(matches[n++], builtin_names[i], MAX_PATH - 1);

    /* filesystem entries */
    const char *slash = strrchr(prefix, '/');
    char dir_part[MAX_PATH] = ".";
    const char *file_prefix = prefix;
    if (slash) {
        size_t dlen = slash - prefix + 1;
        strncpy(dir_part, prefix, dlen);
        dir_part[dlen] = '\0';
        file_prefix = slash + 1;
    }
    size_t fplen = strlen(file_prefix);

    DIR *d = opendir(dir_part);
    if (d) {
        struct dirent *ent;
        while ((ent = readdir(d)) != NULL && n < max) {
            if (ent->d_name[0] == '.' && file_prefix[0] != '.') continue;
            if (strncmp(ent->d_name, file_prefix, fplen) == 0) {
                char full[MAX_PATH];
                if (slash)
                    snprintf(full, sizeof(full), "%.*s%s",
                             (int)(slash - prefix + 1), prefix, ent->d_name);
                else
                    strncpy(full, ent->d_name, MAX_PATH - 1);
                /* append / for directories */
                struct stat st;
                if (stat(full, &st) == 0 && S_ISDIR(st.st_mode))
                    strncat(full, "/", MAX_PATH - strlen(full) - 1);
                strncpy(matches[n++], full, MAX_PATH - 1);
            }
        }
        closedir(d);
    }
    return n;
}

/* find longest common prefix among matches */
static void longest_common_prefix(char matches[][MAX_PATH], int n, char *out, size_t outlen) {
    if (n == 0) { out[0] = '\0'; return; }
    strncpy(out, matches[0], outlen - 1);
    for (int i = 1; i < n; i++) {
        int j = 0;
        while (out[j] && matches[i][j] && out[j] == matches[i][j]) j++;
        out[j] = '\0';
    }
}

/* ══════════════════════════════════════════════════════════
   INPUT with arrow keys, Tab autocomplete
   ══════════════════════════════════════════════════════════ */
static void read_input(char *buf, int maxlen) {
    int len = 0;
    int hist_idx = history_count;   /* pointing past end = current line */
    char saved[MAX_INPUT] = "";     /* save current typing when browsing */
    buf[0] = '\0';

    enable_raw_mode();

    while (1) {
        char c;
        if (read(STDIN_FILENO, &c, 1) <= 0) break;

        /* ── Enter ── */
        if (c == '\r' || c == '\n') {
            write(STDOUT_FILENO, "\n", 1);
            break;
        }

        /* ── Ctrl-D (EOF) ── */
        if (c == 4) {
            disable_raw_mode();
            printf("\nExiting...\n");
            save_history(); save_aliases(); save_bookmarks();
            exit(0);
        }

        /* ── Ctrl-C ── */
        if (c == 3) {
            write(STDOUT_FILENO, "^C\n", 3);
            len = 0; buf[0] = '\0';
            break;
        }

        /* ── Backspace ── */
        if (c == 127 || c == '\b') {
            if (len > 0) {
                len--;
                buf[len] = '\0';
                write(STDOUT_FILENO, "\b \b", 3);
            }
            continue;
        }

        /* ── Tab autocomplete ── */
        if (c == '\t') {
            /* find start of current token */
            int tok_start = len;
            while (tok_start > 0 && buf[tok_start-1] != ' ') tok_start--;
            char token[MAX_PATH];
            strncpy(token, buf + tok_start, len - tok_start);
            token[len - tok_start] = '\0';

            char matches[128][MAX_PATH];
            int n = collect_matches(token, matches, 128);

            if (n == 1) {
                /* unique match — complete it */
                int add = strlen(matches[0]) - strlen(token);
                if (len + add < maxlen - 1) {
                    strncat(buf, matches[0] + strlen(token), add);
                    len += add;
                    write(STDOUT_FILENO, matches[0] + strlen(token), add);
                }
            } else if (n > 1) {
                /* multiple matches — print them */
                write(STDOUT_FILENO, "\n", 1);
                for (int i = 0; i < n; i++)
                    printf(CYAN "  %s\n" RESET, matches[i]);
                fflush(stdout);
                /* re-print prompt + buffer */
                print_prompt();
                write(STDOUT_FILENO, buf, len);

                /* complete as far as possible */
                char lcp[MAX_PATH];
                longest_common_prefix(matches, n, lcp, sizeof(lcp));
                int add = strlen(lcp) - strlen(token);
                if (add > 0 && len + add < maxlen - 1) {
                    strncat(buf, lcp + strlen(token), add);
                    len += add;
                    write(STDOUT_FILENO, lcp + strlen(token), add);
                }
            }
            continue;
        }

        /* ── Escape sequences (arrow keys) ── */
        if (c == '\033') {
            char seq[3];
            if (read(STDIN_FILENO, &seq[0], 1) <= 0) continue;
            if (read(STDIN_FILENO, &seq[1], 1) <= 0) continue;
            if (seq[0] == '[') {
                if (seq[1] == 'A') {          /* UP */
                    if (hist_idx == history_count)
                        strncpy(saved, buf, MAX_INPUT - 1);
                    if (hist_idx > 0) {
                        hist_idx--;
                        /* clear line */
                        for (int i = 0; i < len; i++) write(STDOUT_FILENO, "\b \b", 3);
                        strncpy(buf, history[hist_idx], maxlen - 1);
                        len = strlen(buf);
                        write(STDOUT_FILENO, buf, len);
                    }
                } else if (seq[1] == 'B') {   /* DOWN */
                    if (hist_idx < history_count) {
                        hist_idx++;
                        for (int i = 0; i < len; i++) write(STDOUT_FILENO, "\b \b", 3);
                        if (hist_idx == history_count)
                            strncpy(buf, saved, maxlen - 1);
                        else
                            strncpy(buf, history[hist_idx], maxlen - 1);
                        len = strlen(buf);
                        write(STDOUT_FILENO, buf, len);
                    }
                }
            }
            continue;
        }

        /* ── Regular character ── */
        if (len < maxlen - 1) {
            buf[len++] = c;
            buf[len]   = '\0';
            write(STDOUT_FILENO, &c, 1);
        }
    }

    disable_raw_mode();
}

/* ══════════════════════════════════════════════════════════
   ALIAS EXPANSION
   ══════════════════════════════════════════════════════════ */
static void expand_alias(char *input, int maxlen) {
    /* only expand the first word */
    char first[64] = {0};
    int i = 0;
    while (input[i] && input[i] != ' ') { first[i] = input[i]; i++; }
    first[i] = '\0';

    for (int j = 0; j < alias_count; j++) {
        if (strcmp(aliases[j].name, first) == 0) {
            char tmp[MAX_INPUT];
            snprintf(tmp, sizeof(tmp), "%s%s", aliases[j].value, input + i);
            strncpy(input, tmp, maxlen - 1);
            return;
        }
    }
}

/* ══════════════════════════════════════════════════════════
   REDIRECTION PARSING
   detect > and >> in args, returns fd or -1 if none
   strips redirection tokens from args
   ══════════════════════════════════════════════════════════ */
static int parse_redirection(char **args, int *append_flag) {
    *append_flag = 0;
    for (int i = 0; args[i]; i++) {
        if (strcmp(args[i], ">>") == 0 && args[i+1]) {
            *append_flag = 1;
            int fd = open(args[i+1],
                          O_WRONLY | O_CREAT | O_APPEND, 0644);
            args[i] = NULL;   /* terminate args here */
            return fd;
        }
        if (strcmp(args[i], ">") == 0 && args[i+1]) {
            int fd = open(args[i+1],
                          O_WRONLY | O_CREAT | O_TRUNC, 0644);
            args[i] = NULL;
            return fd;
        }
    }
    return -1;
}

/* ══════════════════════════════════════════════════════════
   PARSE INPUT → args array
   ══════════════════════════════════════════════════════════ */
static void parse_input(char *input, char **args) {
    int i = 0;
    args[i] = strtok(input, " \t");
    while (args[i] != NULL && i < MAX_ARGS - 1) {
        i++;
        args[i] = strtok(NULL, " \t");
    }
}

/* ══════════════════════════════════════════════════════════
   PIPE SPLIT
   ══════════════════════════════════════════════════════════ */
static int parse_pipe(char *input, char **left, char **right) {
    char *p = strchr(input, '|');
    if (!p) return 0;
    *p = '\0'; p++;
    while (*p == ' ') p++;
    *left  = input;
    *right = p;
    return 1;
}

/* ══════════════════════════════════════════════════════════
   EXECUTE COMMAND (with optional redirection fd)
   ══════════════════════════════════════════════════════════ */
static int execute_command(char **args, int redir_fd) {
    pid_t pid = fork();
    if (pid == 0) {
        if (redir_fd >= 0) {
            dup2(redir_fd, STDOUT_FILENO);
            close(redir_fd);
        }
        if (execvp(args[0], args) < 0) {
            fprintf(stderr, BRED "myshell: %s: %s\n" RESET,
                    args[0], strerror(errno));
        }
        exit(1);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        if (redir_fd >= 0) close(redir_fd);
        return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    }
    return 1;
}

/* ══════════════════════════════════════════════════════════
   EXECUTE PIPED COMMANDS
   ══════════════════════════════════════════════════════════ */
static int execute_piped(char **args1, char **args2) {
    int pipefd[2];
    pipe(pipefd);

    pid_t p1 = fork();
    if (p1 == 0) {
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]); close(pipefd[1]);
        execvp(args1[0], args1);
        fprintf(stderr, BRED "myshell: %s: %s\n" RESET, args1[0], strerror(errno));
        exit(1);
    }

    pid_t p2 = fork();
    if (p2 == 0) {
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[1]); close(pipefd[0]);
        execvp(args2[0], args2);
        fprintf(stderr, BRED "myshell: %s: %s\n" RESET, args2[0], strerror(errno));
        exit(1);
    }

    close(pipefd[0]); close(pipefd[1]);
    int s1, s2;
    waitpid(p1, &s1, 0);
    waitpid(p2, &s2, 0);
    return WIFEXITED(s2) ? WEXITSTATUS(s2) : 1;
}

/* ══════════════════════════════════════════════════════════
   BUILT-IN: sysinfo
   ══════════════════════════════════════════════════════════ */
static void cmd_sysinfo(void) {
    struct sysinfo si;
    if (sysinfo(&si) != 0) { perror("sysinfo"); return; }

    char hostname[256] = "unknown";
    gethostname(hostname, sizeof(hostname));

    printf(BCYAN "\n  ╔══════════════ System Info ══════════════╗\n" RESET);
    printf(CYAN  "  ║" RESET BWHITE " Hostname  : " RESET "%-30s" CYAN "║\n" RESET, hostname);
    printf(CYAN  "  ║" RESET BWHITE " Uptime    : " RESET "%-3ldd %02ldh %02ldm        " CYAN "      ║\n" RESET,
           si.uptime/86400, (si.uptime%86400)/3600, (si.uptime%3600)/60);

    long total_mb = si.totalram  * si.mem_unit / (1024*1024);
    long free_mb  = si.freeram   * si.mem_unit / (1024*1024);
    long used_mb  = total_mb - free_mb;
    char ram_total[64], ram_used[64], ram_free[64];
    snprintf(ram_total, sizeof(ram_total), "%ld MB", total_mb);
    snprintf(ram_used,  sizeof(ram_used),  "%ld MB  (%.1f%%)", used_mb, 100.0*used_mb/total_mb);
    snprintf(ram_free,  sizeof(ram_free),  "%ld MB", free_mb);
    printf(CYAN  "  ║" RESET BWHITE " RAM Total : " RESET "%-30s" CYAN "║\n" RESET, ram_total);
    printf(CYAN  "  ║" RESET BWHITE " RAM Used  : " RESET "%-30s" CYAN "║\n" RESET, ram_used);
    printf(CYAN  "  ║" RESET BWHITE " RAM Free  : " RESET "%-30s" CYAN "║\n" RESET, ram_free);
    printf(CYAN  "  ║" RESET BWHITE " Processes : " RESET "%-30d" CYAN "║\n" RESET, (int)si.procs);
    printf(BCYAN "  ╚═════════════════════════════════════════╝\n\n" RESET);
}

/* ══════════════════════════════════════════════════════════
   BUILT-IN: meminfo
   ══════════════════════════════════════════════════════════ */
static void cmd_meminfo(void) {
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) { perror("meminfo"); return; }
    printf(BYELLOW "\n  ── /proc/meminfo (top entries) ──\n" RESET);
    char line[256];
    int shown = 0;
    while (fgets(line, sizeof(line), f) && shown < 10) {
        printf(YELLOW "  %s" RESET, line);
        shown++;
    }
    fclose(f);
    printf("\n");
}

/* ══════════════════════════════════════════════════════════
   BUILT-IN: diskinfo [path]
   ══════════════════════════════════════════════════════════ */
static void cmd_diskinfo(char *path) {
    if (!path) path = ".";
    struct statvfs sv;
    if (statvfs(path, &sv) != 0) { perror("diskinfo"); return; }

    unsigned long long total = (unsigned long long)sv.f_blocks * sv.f_frsize;
    unsigned long long free_ = (unsigned long long)sv.f_bfree  * sv.f_frsize;
    unsigned long long used  = total - free_;
    double pct = (total > 0) ? 100.0 * used / total : 0;

    /* ASCII bar */
    int bar_width = 30;
    int filled    = (int)(pct / 100.0 * bar_width);
    char bar[64];
    int bi = 0;
    for (int i = 0; i < filled;              i++) { bar[bi++]='#'; }
    for (int i = filled; i < bar_width;      i++) { bar[bi++]='-'; }
    bar[bi] = '\0';

    char s_total[64], s_used[64], s_free[64];
    snprintf(s_total, sizeof(s_total), "%.2f GB", total/1073741824.0);
    snprintf(s_used,  sizeof(s_used),  "%.2f GB (%.1f%%)", used/1073741824.0, pct);
    snprintf(s_free,  sizeof(s_free),  "%.2f GB", free_/1073741824.0);

    printf(BGREEN "\n  ╔═══════════════ Disk Info ═══════════════╗\n" RESET);
    printf(GREEN  "  ║" RESET BWHITE " Path  : " RESET "%-33s" GREEN "║\n" RESET, path);
    printf(GREEN  "  ║" RESET BWHITE " Total : " RESET "%-33s" GREEN "║\n" RESET, s_total);
    printf(GREEN  "  ║" RESET BWHITE " Used  : " RESET "%-33s" GREEN "║\n" RESET, s_used);
    printf(GREEN  "  ║" RESET BWHITE " Free  : " RESET "%-33s" GREEN "║\n" RESET, s_free);
    printf(GREEN  "  ║ " RESET "[%s] " GREEN "           ║\n" RESET, bar);
    printf(BGREEN "  ╚═════════════════════════════════════════╝\n\n" RESET);
}

/* ══════════════════════════════════════════════════════════
   BUILT-IN: search <pattern> [dir]
   ══════════════════════════════════════════════════════════ */
static void cmd_search(char *pattern, char *dir) {
    if (!pattern) { printf(BRED "Usage: search <pattern> [dir]\n" RESET); return; }
    if (!dir) dir = ".";
    printf(BYELLOW "\n  Searching for '%s' in '%s'...\n\n" RESET, pattern, dir);

    char cmd[MAX_INPUT];
    snprintf(cmd, sizeof(cmd), "find %s -name '%s' 2>/dev/null", dir, pattern);
    FILE *fp = popen(cmd, "r");
    if (!fp) { perror("search"); return; }

    char line[MAX_PATH];
    int found = 0;
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = 0;
        printf(GREEN "  ✔ %s\n" RESET, line);
        found++;
    }
    pclose(fp);

    if (found == 0)
        printf(YELLOW "  No matches found.\n" RESET);
    else
        printf(BGREEN "\n  %d result(s) found.\n" RESET, found);
    printf("\n");
}

/* ══════════════════════════════════════════════════════════
   BUILT-IN: countlines <file>
   ══════════════════════════════════════════════════════════ */
static void cmd_countlines(char *file) {
    if (!file) { printf(BRED "Usage: countlines <file>\n" RESET); return; }
    FILE *f = fopen(file, "r");
    if (!f) { fprintf(stderr, BRED "countlines: %s: %s\n" RESET, file, strerror(errno)); return; }
    int lines = 0, bytes = 0;
    int c;
    while ((c = fgetc(f)) != EOF) {
        bytes++;
        if (c == '\n') lines++;
    }
    fclose(f);
    printf(BBLUE "\n  File      : " RESET "%s\n", file);
    printf(BBLUE "  Lines     : " RESET "%d\n", lines);
    printf(BBLUE "  Bytes     : " RESET "%d\n\n", bytes);
}

/* ══════════════════════════════════════════════════════════
   BUILT-IN: preview <file> [lines]
   ══════════════════════════════════════════════════════════ */
static void cmd_preview(char *file, char *nstr) {
    if (!file) { printf(BRED "Usage: preview <file> [lines]\n" RESET); return; }
    int n = nstr ? atoi(nstr) : 10;
    if (n <= 0) n = 10;
    FILE *f = fopen(file, "r");
    if (!f) { fprintf(stderr, BRED "preview: %s: %s\n" RESET, file, strerror(errno)); return; }

    printf(BYELLOW "\n  ── Preview: %s (first %d lines) ──\n\n" RESET, file, n);
    char line[1024];
    int ln = 1;
    while (ln <= n && fgets(line, sizeof(line), f)) {
        printf(CYAN "  %4d │ " RESET "%s", ln++, line);
        if (line[strlen(line)-1] != '\n') printf("\n");
    }
    fclose(f);
    printf(YELLOW "\n  ──────────────────────────────\n\n" RESET);
}

/* ══════════════════════════════════════════════════════════
   BUILT-IN: history [N] / !N
   ══════════════════════════════════════════════════════════ */
static void cmd_history(char *nstr) {
    int n = nstr ? atoi(nstr) : history_count;
    if (n > history_count) n = history_count;
    printf(BYELLOW "\n  ── Command History ──\n" RESET);
    int start = history_count - n;
    for (int i = start; i < history_count; i++)
        printf(CYAN "  %4d  " RESET "%s\n", i+1, history[i]);
    printf("\n");
}

/* expand !N → returns pointer to history entry or NULL */
static const char *expand_history_ref(const char *input) {
    if (input[0] == '!' && input[1] >= '1' && input[1] <= '9') {
        int n = atoi(input + 1) - 1;
        if (n >= 0 && n < history_count)
            return history[n];
        printf(BRED "  myshell: !%d: event not found\n" RESET, n+1);
    }
    return NULL;
}

/* ══════════════════════════════════════════════════════════
   BUILT-IN: alias / unalias / aliases
   ══════════════════════════════════════════════════════════ */
static void cmd_alias(char *name, char *value) {
    if (!name || !value) {
        printf(BRED "Usage: alias <name> <expansion>\n" RESET);
        return;
    }
    /* update if exists */
    for (int i = 0; i < alias_count; i++) {
        if (strcmp(aliases[i].name, name) == 0) {
            strncpy(aliases[i].value, value, MAX_INPUT - 1);
            printf(GREEN "  Alias updated: " RESET "%s → %s\n", name, value);
            return;
        }
    }
    if (alias_count >= MAX_ALIASES) { printf(BRED "  Alias table full.\n" RESET); return; }
    strncpy(aliases[alias_count].name,  name,  63);
    strncpy(aliases[alias_count].value, value, MAX_INPUT - 1);
    alias_count++;
    printf(GREEN "  Alias set: " RESET "%s → %s\n", name, value);
}

static void cmd_unalias(char *name) {
    if (!name) { printf(BRED "Usage: unalias <name>\n" RESET); return; }
    for (int i = 0; i < alias_count; i++) {
        if (strcmp(aliases[i].name, name) == 0) {
            memmove(&aliases[i], &aliases[i+1], (alias_count-i-1)*sizeof(Alias));
            alias_count--;
            printf(GREEN "  Alias removed: " RESET "%s\n", name);
            return;
        }
    }
    printf(YELLOW "  Alias not found: %s\n" RESET, name);
}

static void cmd_list_aliases(void) {
    if (alias_count == 0) { printf(YELLOW "  No aliases defined.\n" RESET); return; }
    printf(BYELLOW "\n  ── Aliases ──\n" RESET);
    for (int i = 0; i < alias_count; i++)
        printf(CYAN "  %-15s" RESET " → %s\n", aliases[i].name, aliases[i].value);
    printf("\n");
}

/* ══════════════════════════════════════════════════════════
   BUILT-IN: bookmark / bookmarks / goto
   ══════════════════════════════════════════════════════════ */
static void cmd_bookmark(char *name) {
    char cwd[MAX_PATH];
    if (getcwd(cwd, sizeof(cwd)) == NULL) { perror("getcwd"); return; }
    if (!name) name = "default";

    /* update if exists */
    for (int i = 0; i < bookmark_count; i++) {
        if (strcmp(bookmarks[i].name, name) == 0) {
            strncpy(bookmarks[i].path, cwd, MAX_PATH - 1);
            printf(GREEN "  Bookmark updated: " RESET "%s → %s\n", name, cwd);
            return;
        }
    }
    if (bookmark_count >= MAX_BOOKMARKS) { printf(BRED "  Bookmark table full.\n" RESET); return; }
    strncpy(bookmarks[bookmark_count].name, name, 63);
    strncpy(bookmarks[bookmark_count].path, cwd,  MAX_PATH - 1);
    bookmark_count++;
    printf(GREEN "  Bookmarked: " RESET "%s → %s\n", name, cwd);
}

static void cmd_list_bookmarks(void) {
    if (bookmark_count == 0) { printf(YELLOW "  No bookmarks.\n" RESET); return; }
    printf(BYELLOW "\n  ── Bookmarks ──\n" RESET);
    for (int i = 0; i < bookmark_count; i++)
        printf(CYAN "  %-15s" RESET " → %s\n", bookmarks[i].name, bookmarks[i].path);
    printf("\n");
}

static void cmd_goto(char *name) {
    if (!name) { printf(BRED "Usage: goto <bookmark_name>\n" RESET); return; }
    for (int i = 0; i < bookmark_count; i++) {
        if (strcmp(bookmarks[i].name, name) == 0) {
            if (chdir(bookmarks[i].path) != 0)
                perror("goto");
            else
                printf(GREEN "  Jumped to: " RESET "%s\n", bookmarks[i].path);
            return;
        }
    }
    printf(YELLOW "  Bookmark not found: %s\n" RESET, name);
}

/* ══════════════════════════════════════════════════════════
   BUILT-IN: help
   ══════════════════════════════════════════════════════════ */
static void cmd_help(void) {
    printf(BCYAN "\n  ╔═══════════════════════════════════════════════════════╗\n" RESET);
    printf(BCYAN "  ║              MyShell Built-in Commands                ║\n" RESET);
    printf(BCYAN "  ╠═══════════════════════════════════════════════════════╣\n" RESET);

    struct { const char *cmd; const char *desc; } cmds[] = {
        {"cd <dir>",                  "Change directory"},
        {"exit",                      "Exit the shell"},
        {"help",                      "Show this help"},
        {"history [N]",               "Show last N commands"},
        {"!N",                        "Re-run command number N"},
        {"alias <n> <val>",           "Create an alias"},
        {"unalias <n>",               "Remove an alias"},
        {"aliases",                   "List all aliases"},
        {"bookmark [name]",           "Bookmark current directory"},
        {"bookmarks",                 "List all bookmarks"},
        {"goto <name>",               "Jump to bookmarked directory"},
        {"sysinfo",                   "Show CPU/RAM/uptime info"},
        {"meminfo",                   "Show /proc/meminfo"},
        {"diskinfo [path]",           "Show disk usage for path"},
        {"search <pat> [dir]",        "Find files matching pattern"},
        {"countlines <file>",         "Count lines & bytes in file"},
        {"preview <file> [N]",        "Preview first N lines of file"},
        {NULL, NULL}
    };
    for (int i = 0; cmds[i].cmd; i++)
        printf(CYAN "  ║  %-26s" RESET " %-30s" CYAN "║\n" RESET,
               cmds[i].cmd, cmds[i].desc);

    printf(BCYAN "  ╠═══════════════════════════════════════════════════════╣\n" RESET);
    printf(CYAN  "  ║  Redirection: cmd > file  |  cmd >> file             ║\n" RESET);
    printf(CYAN  "  ║  Pipe:        cmd1 | cmd2                            ║\n" RESET);
    printf(CYAN  "  ║  Tab:         autocomplete  |  ↑↓: history           ║\n" RESET);
    printf(BCYAN "  ╚═══════════════════════════════════════════════════════╝\n\n" RESET);
}

/* ══════════════════════════════════════════════════════════
   HANDLE BUILT-INS — returns 1 if handled, 0 if not
   ══════════════════════════════════════════════════════════ */
static int handle_builtin(char **args) {
    if (!args[0]) return 1;

    if (strcmp(args[0], "exit") == 0) {
        printf(YELLOW "  Goodbye!\n" RESET);
        save_history(); save_aliases(); save_bookmarks();
        exit(0);
    }
    if (strcmp(args[0], "help") == 0)    { cmd_help();                          return 1; }
    if (strcmp(args[0], "history") == 0) { cmd_history(args[1]);                return 1; }
    if (strcmp(args[0], "aliases") == 0) { cmd_list_aliases();                  return 1; }
    if (strcmp(args[0], "bookmarks") == 0){ cmd_list_bookmarks();               return 1; }
    if (strcmp(args[0], "sysinfo") == 0) { cmd_sysinfo();                       return 1; }
    if (strcmp(args[0], "meminfo") == 0) { cmd_meminfo();                       return 1; }
    if (strcmp(args[0], "diskinfo") == 0){ cmd_diskinfo(args[1]);               return 1; }
    if (strcmp(args[0], "search") == 0)  { cmd_search(args[1], args[2]);        return 1; }
    if (strcmp(args[0], "countlines")==0){ cmd_countlines(args[1]);             return 1; }
    if (strcmp(args[0], "preview") == 0) { cmd_preview(args[1], args[2]);       return 1; }
    if (strcmp(args[0], "bookmark") == 0){ cmd_bookmark(args[1]);               return 1; }
    if (strcmp(args[0], "goto") == 0)    { cmd_goto(args[1]);                   return 1; }

    if (strcmp(args[0], "alias") == 0) {
        /* join args[2..] back for the value */
        if (args[1] && args[2]) {
            char val[MAX_INPUT] = "";
            for (int i = 2; args[i]; i++) {
                if (i > 2) strncat(val, " ", sizeof(val)-strlen(val)-1);
                strncat(val, args[i], sizeof(val)-strlen(val)-1);
            }
            cmd_alias(args[1], val);
        } else {
            printf(BRED "Usage: alias <name> <expansion>\n" RESET);
        }
        return 1;
    }
    if (strcmp(args[0], "unalias") == 0) { cmd_unalias(args[1]);               return 1; }

    if (strcmp(args[0], "cd") == 0) {
        const char *dest = args[1] ? args[1] : getenv("HOME");
        if (!dest) dest = "/";
        if (chdir(dest) != 0)
            fprintf(stderr, BRED "cd: %s: %s\n" RESET, dest, strerror(errno));
        return 1;
    }

    return 0;
}

/* ══════════════════════════════════════════════════════════
   MAIN LOOP
   ══════════════════════════════════════════════════════════ */
int main(void) {
    /* ignore Ctrl-C in main shell process */
    signal(SIGINT, SIG_IGN);

    load_history();
    load_aliases();
    load_bookmarks();

    print_banner();

    char input[MAX_INPUT];
    char *args1[MAX_ARGS], *args2[MAX_ARGS];

    while (1) {
        print_prompt();
        read_input(input, MAX_INPUT);

        if (strlen(input) == 0) continue;

        /* history expansion (!N) */
        const char *hexp = expand_history_ref(input);
        if (hexp) {
            printf(YELLOW "  → %s\n" RESET, hexp);
            strncpy(input, hexp, MAX_INPUT - 1);
        }

        /* alias expansion */
        expand_alias(input, MAX_INPUT);

        /* save to history */
        add_history(input);

        /* pipe? */
        char *left, *right;
        char input_copy[MAX_INPUT];
        strncpy(input_copy, input, MAX_INPUT - 1);

        if (parse_pipe(input_copy, &left, &right)) {
            char lbuf[MAX_INPUT], rbuf[MAX_INPUT];
            strncpy(lbuf, left,  sizeof(lbuf)-1);
            strncpy(rbuf, right, sizeof(rbuf)-1);
            parse_input(lbuf, args1);
            parse_input(rbuf, args2);
            last_exit_code = execute_piped(args1, args2);
        } else {
            parse_input(input, args1);
            if (!args1[0]) continue;

            /* built-in? */
            if (handle_builtin(args1)) continue;

            /* redirection */
            int append = 0;
            int rfd = parse_redirection(args1, &append);

            last_exit_code = execute_command(args1, rfd);
        }
    }

    return 0;
}
