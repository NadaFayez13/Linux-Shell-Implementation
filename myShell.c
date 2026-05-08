#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

#define MAX_INPUT 1024
#define MAX_ARGS 64
#define MAX_HISTORY 100

// history
char history[MAX_HISTORY][MAX_INPUT];
int history_count = 0;
pid_t fg_pid = -1;

// -------------------------
// Helper functions
// -------------------------
void print_user_error(const char *msg) {
    fprintf(stderr, "myShell: %s\n", msg);
}

void print_sys_error(const char *context) {
    fprintf(stderr, "myShell: %s: %s\n", context, strerror(errno));
}

void print_file_error(const char *file) {
    fprintf(stderr, "myShell: cannot open '%s': %s\n", file, strerror(errno));
}

void print_exec_error(const char *cmd) {
    if (cmd == NULL || strlen(cmd) == 0) {
        print_user_error("missing command");
        return;
    }

    if (errno == ENOENT) {
        fprintf(stderr, "myShell: %s: command not found\n", cmd);
    } else {
        fprintf(stderr, "myShell: %s: %s\n", cmd, strerror(errno));
    }
}

void flush_extra_input(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {
    }
}

int is_special_token(const char *token) {
    return token &&
           (strcmp(token, ">") == 0 ||
            strcmp(token, "<") == 0 ||
            strcmp(token, "|") == 0 ||
            strcmp(token, "&") == 0);
}

// SIGCHLD HANDLER (Zombie Handling)
void sigchld_handler(int sig) {
    (void)sig;
    int saved_errno = errno;
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        // silently reap finished background children
    }

    errno = saved_errno;
}

void add_to_history(const char *cmd) {
    if (cmd == NULL || strlen(cmd) == 0) return;

    if (history_count < MAX_HISTORY) {
        strncpy(history[history_count], cmd, MAX_INPUT - 1);
        history[history_count][MAX_INPUT - 1] = '\0';
        history_count++;
    } else {
        for (int i = 1; i < MAX_HISTORY; i++) {
            strcpy(history[i - 1], history[i]);
        }
        strncpy(history[MAX_HISTORY - 1], cmd, MAX_INPUT - 1);
        history[MAX_HISTORY - 1][MAX_INPUT - 1] = '\0';
    }
}

// ==========================================
// (Pipes)
// ==========================================
void execute_pipe_system(char *args[], int pipe_idx) {
    char *left_side[MAX_ARGS]; 
    char *right_side[MAX_ARGS];

    if (pipe_idx == 0 || args[pipe_idx + 1] == NULL) {
        print_user_error("invalid null command in pipe");
        return;   
    }

    for (int j = pipe_idx + 1; args[j] != NULL; j++) {
        if (strcmp(args[j], "|") == 0) {
            print_user_error("multiple pipes are not supported");
            return;  
        }
    }

    // Divide args into left and right side
    for (int i = 0; i < pipe_idx; i++) {
        left_side[i] = args[i];
    }
    left_side[pipe_idx] = NULL;

    int k = 0;
    for (int i = pipe_idx + 1; args[i] != NULL; i++) {
        right_side[k++] = args[i];
    }
    right_side[k] = NULL;

    if (left_side[0] == NULL || right_side[0] == NULL) {
        print_user_error("invalid pipe syntax");
        return;
    }

    int fd[2];
    if (pipe(fd) == -1) {
        print_sys_error("pipe");
        return;
    }

    pid_t pid1 = fork();
    if (pid1 < 0) {
        print_sys_error("fork");
        close(fd[0]);
        close(fd[1]);
        return;
    }

    if (pid1 == 0) {
        // signals
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
// redirction
        if (dup2(fd[1], STDOUT_FILENO) == -1) {
            print_sys_error("dup2");
            _exit(EXIT_FAILURE);
        }

        close(fd[0]);
        close(fd[1]);

        execvp(left_side[0], left_side);
        print_exec_error(left_side[0]);
        _exit(errno == ENOENT ? 127 : EXIT_FAILURE);
    }

    pid_t pid2 = fork();
    if (pid2 < 0) {
        print_sys_error("fork");
        close(fd[0]);
        close(fd[1]);
        waitpid(pid1, NULL, 0);
        return;
    }

    if (pid2 == 0) {
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);

        if (dup2(fd[0], STDIN_FILENO) == -1) {
            print_sys_error("dup2");
            _exit(EXIT_FAILURE);
        }

        close(fd[0]);
        close(fd[1]);

        execvp(right_side[0], right_side);
        print_exec_error(right_side[0]);
        _exit(errno == ENOENT ? 127 : EXIT_FAILURE);
    }

    close(fd[0]);
    close(fd[1]);

    int status;

    if (waitpid(pid1, &status, WUNTRACED) == -1 && errno != ECHILD) {
        print_sys_error("waitpid");
    } else if (WIFSTOPPED(status)) {
        printf("\nStopped\n");
    }

    if (waitpid(pid2, &status, WUNTRACED) == -1 && errno != ECHILD) {
        print_sys_error("waitpid");
    } else if (WIFSTOPPED(status)) {
        printf("\nStopped\n");
    }
}

// execute external commands (Redirection)
void execute(char *args[], int background) {
    if (args[0] == NULL || strlen(args[0]) == 0) {
        print_user_error("missing command");
        return;
    }

    pid_t pid = fork();

    if (pid < 0) {
        print_sys_error("fork");
        return;
    }

    if (pid == 0) { // Child process
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);

        for (int j = 0; args[j] != NULL; j++) {

            // Output Redirection (>)
            if (strcmp(args[j], ">") == 0) {
                if (args[j + 1] == NULL || is_special_token(args[j + 1])) {
                    fprintf(stderr, "myShell: syntax error near unexpected token '%s'\n",
                            args[j + 1] ? args[j + 1] : "newline");
                    _exit(EXIT_FAILURE);
                }

                int fd = open(args[j + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) {
                    print_file_error(args[j + 1]);
                    _exit(EXIT_FAILURE);
                }

                if (dup2(fd, STDOUT_FILENO) == -1) {
                    print_sys_error("dup2");
                    close(fd);
                    _exit(EXIT_FAILURE);
                }

                if (close(fd) == -1) {
                    print_sys_error("close");
                    _exit(EXIT_FAILURE);
                }

                args[j] = NULL;
            }

            // Input Redirection (<)
            else if (strcmp(args[j], "<") == 0) {
                if (args[j + 1] == NULL || is_special_token(args[j + 1])) {
                    fprintf(stderr, "myShell: syntax error near unexpected token '%s'\n",
                            args[j + 1] ? args[j + 1] : "newline");
                    _exit(EXIT_FAILURE);
                }

                int fd = open(args[j + 1], O_RDONLY);
                if (fd < 0) {
                    print_file_error(args[j + 1]);
                    _exit(EXIT_FAILURE);
                }

                if (dup2(fd, STDIN_FILENO) == -1) {
                    print_sys_error("dup2");
                    close(fd);
                    _exit(EXIT_FAILURE);
                }

                if (close(fd) == -1) {
                    print_sys_error("close");
                    _exit(EXIT_FAILURE);
                }

                args[j] = NULL;
            }
        }

        if (args[0] == NULL || strlen(args[0]) == 0) {
            print_user_error("missing command");
            _exit(EXIT_FAILURE);
        }

        execvp(args[0], args);
        print_exec_error(args[0]);
        _exit(errno == ENOENT ? 127 : EXIT_FAILURE);
    }

    else {
        if (background) {
            printf("Started background process with PID %d\n", pid);
        } else {
            fg_pid = pid;

            int status;
            if (waitpid(pid, &status, WUNTRACED) == -1) {
                if (errno != ECHILD) {
                    print_sys_error("waitpid");
                }
            } else {
                if (WIFSTOPPED(status)) {
                    printf("\nStopped\n");
                }
            }

            fg_pid = -1;
        }
    }
}

void sigint_handler(int sig) {
    (void)sig;

    if (fg_pid > 0) {
        kill(fg_pid, SIGINT);
        write(STDOUT_FILENO, "\n", 1);
    } else {
        write(STDOUT_FILENO, "\nmyShell> ", 10);
    }
}

int main() {
    signal(SIGINT, sigint_handler); // shell ignore Ctrl+C
    signal(SIGTSTP, SIG_IGN);
    signal(SIGCHLD, sigchld_handler); // Install SIGCHLD handler for zombie cleanup

    char input[MAX_INPUT];
    char *args[MAX_ARGS];

    while (1) {
        printf("myShell> ");
        fflush(stdout);

        if (fgets(input, MAX_INPUT, stdin) == NULL) {
            if (ferror(stdin)) {
                print_sys_error("fgets");
            }
            printf("\n");
            break;
        }

        // detect too long input
        if (strchr(input, '\n') == NULL && !feof(stdin)) {
            flush_extra_input();
            print_user_error("input too long");
            continue;
        }

        // remove \n
        input[strcspn(input, "\n")] = 0;

        if (strlen(input) == 0) continue;

        add_to_history(input);

        // parsing
        int i = 0;
        char input_copy[MAX_INPUT];
        strcpy(input_copy, input);

        char *token = strtok(input_copy, " ");
        int too_many_args = 0;

        while (token != NULL) {
            if (i >= MAX_ARGS - 1) {
                too_many_args = 1;
                break;
            }
            args[i++] = token;
            token = strtok(NULL, " ");
        }
        args[i] = NULL;

        if (too_many_args) {
            print_user_error("too many arguments");
            continue;
        }

        if (args[0] == NULL) continue;

        // ==========================================
        // Detect pipe before executing any command
        // ==========================================
        int pipe_idx = -1;
        int pipe_count = 0;

        for (int j = 0; args[j] != NULL; j++) {
            if (strcmp(args[j], "|") == 0) {
                pipe_count++;
                pipe_idx = j;
            }
        }

        if (pipe_count > 1) {
            print_user_error("multiple pipes are not supported");
            continue;
        }

        if (pipe_idx != -1) {
            execute_pipe_system(args, pipe_idx);
            continue;
        }
        // ==========================================

        // ======================
        // BUILT-IN COMMANDS
        // ======================

        // exit
        if (strcmp(args[0], "exit") == 0) {
            break;
        }

        // cd
        else if (strcmp(args[0], "cd") == 0) {
            if (args[1] == NULL) {
                print_user_error("expected argument to cd");
            } else if (args[2] != NULL) {
                print_user_error("cd: too many arguments");
            } else {
                if (chdir(args[1]) != 0) {
                    print_sys_error("cd");
                }
            }
        }

        // pwd
        else if (strcmp(args[0], "pwd") == 0) {
            char cwd[MAX_INPUT];

            if (getcwd(cwd, sizeof(cwd)) != NULL) {
                printf("%s\n", cwd);
            } else {
                print_sys_error("pwd");
            }
        }

        // history
        else if (strcmp(args[0], "history") == 0) {
            for (int j = 0; j < history_count; j++) {
                printf("%d  %s\n", j + 1, history[j]);
            }
        }

        // external commands
        else {
            // BACKGROUND EXECUTION (&)
            int background = 0;

            // Case 1: "&" is a separate token
            if (i > 0 && strcmp(args[i - 1], "&") == 0) {
                background = 1;
                args[i - 1] = NULL;
            }
            // Case 2: "&" attached to last argument (e.g. sleep 5&)
            else if (i > 0) {
                int len = strlen(args[i - 1]);
                if (len > 0 && args[i - 1][len - 1] == '&') {
                    background = 1;
                    args[i - 1][len - 1] = '\0';
                }
            }

            if (args[0] == NULL || strlen(args[0]) == 0) {
                print_user_error("missing command before '&'");
                continue;
            }

            execute(args, background);
        }
    }

    return 0;
}