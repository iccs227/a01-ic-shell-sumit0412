/* ICCS227: Project 1: icsh
 * Name: Sumit Sachdev
 * StudentID: 6480523
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>

#define MAX_CMD_BUFFER 255
#define MAX_ARGS 64

char last_command[MAX_CMD_BUFFER];
int script_mode = 0;
int last_exit_status = 0;
pid_t foreground_pid = 0;

void sigint_handler(int sig) {
    if (foreground_pid > 0) {
        kill(foreground_pid, SIGINT);
    }
    printf("\n");
}

void sigtstp_handler(int sig) {
    if (foreground_pid > 0) {
        kill(foreground_pid, SIGTSTP);
    }
}

void parse_command(char* input, char* argv[]) {
    int argc = 0;
    char* token = strtok(input, " \t\n");
    
    while (token != NULL && argc < MAX_ARGS - 1) {
        argv[argc++] = token;
        token = strtok(NULL, " \t\n");
    }
    argv[argc] = NULL;
}

int parse_redirection(char* input, char* argv[], char** input_file, char** output_file, int* append_mode) {
    int argc = 0;
    char* token = strtok(input, " \t\n");
    
    *input_file = NULL;
    *output_file = NULL;
    *append_mode = 0;
    
    while (token != NULL && argc < MAX_ARGS - 1) {
        if (strcmp(token, "<") == 0) {
            token = strtok(NULL, " \t\n");
            if (token != NULL) {
                *input_file = token;
            }
        }
        else if (strcmp(token, ">") == 0) {
            token = strtok(NULL, " \t\n");
            if (token != NULL) {
                *output_file = token;
                *append_mode = 0;
            }
        }
        else if (strcmp(token, ">>") == 0) {
            token = strtok(NULL, " \t\n");
            if (token != NULL) {
                *output_file = token;
                *append_mode = 1;
            }
        }
        else {
            argv[argc++] = token;
        }
        token = strtok(NULL, " \t\n");
    }
    argv[argc] = NULL;
    return argc;
}

int has_redirection(char* input) {
    return (strstr(input, " > ") != NULL || strstr(input, " < ") != NULL || strstr(input, " >> ") != NULL);
}

void execute_external_command(char* input) {
    pid_t pid;
    int status;
    char* argv[MAX_ARGS];
    char cmd_copy[MAX_CMD_BUFFER];
    char* input_file = NULL;
    char* output_file = NULL;
    int append_mode = 0;
    
    strcpy(cmd_copy, input);
    parse_redirection(cmd_copy, argv, &input_file, &output_file, &append_mode);
    
    pid = fork();
    
    if (pid < 0) {
        perror("Fork failed");
        return;
    }
    else if (pid == 0) {
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        
        if (input_file != NULL) {
            int fd_in = open(input_file, O_RDONLY);
            if (fd_in < 0) {
                perror(input_file);
                exit(1);
            }
            dup2(fd_in, STDIN_FILENO);
            close(fd_in);
        }
        
        if (output_file != NULL) {
            int fd_out;
            if (append_mode) {
                fd_out = open(output_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
            } else {
                fd_out = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            }
            if (fd_out < 0) {
                perror(output_file);
                exit(1);
            }
            dup2(fd_out, STDOUT_FILENO);
            close(fd_out);
        }
        
        execvp(argv[0], argv);
        perror(argv[0]);
        exit(127);
    }
    else {
        foreground_pid = pid;
        waitpid(pid, &status, WUNTRACED);
        foreground_pid = 0;
        
        if (WIFEXITED(status)) {
            last_exit_status = WEXITSTATUS(status);
        }
        else if (WIFSIGNALED(status)) {
            last_exit_status = 128 + WTERMSIG(status);
        }
        else if (WIFSTOPPED(status)) {
            printf("\n[1]+  Stopped                 %s", input);
            last_exit_status = 128 + WSTOPSIG(status);
        }
    }
}

void handle_echo(char* input) {
    char* echo_text = input + 5;
    
    if (strncmp(echo_text, "$?", 2) == 0) {
        printf("%d\n", last_exit_status);
    } else {
        printf("%s", echo_text);
    }
    last_exit_status = 0;
}

void handle_double_bang() {
    if (strlen(last_command) > 0) {
        if (!script_mode) {
            printf("%s", last_command);
        }
        
        char temp[MAX_CMD_BUFFER];
        strcpy(temp, last_command);
        temp[strcspn(temp, "\n")] = 0;
        
        if (strncmp(temp, "echo ", 5) == 0 && !has_redirection(temp)) {
            handle_echo(last_command);
        }
        else if (strcmp(temp, "!!") != 0) {
            execute_external_command(last_command);
        }
    }
}

void handle_exit(char* input) {
    int exit_code = 0;
    char* exit_arg = input + 5;
    exit_arg[strcspn(exit_arg, "\n")] = 0;
    
    if (strlen(exit_arg) > 0) {
        exit_code = atoi(exit_arg) & 0xFF;
    }
    
    if (!script_mode) {
        printf("bye\n");
    }
    exit(exit_code);
}

int main(int argc, char* argv[]) {
    char buffer[MAX_CMD_BUFFER];
    FILE* input_stream = stdin;
    
    signal(SIGINT, sigint_handler);
    signal(SIGTSTP, sigtstp_handler);
    
    if (argc > 1) {
        input_stream = fopen(argv[1], "r");
        if (input_stream == NULL) {
            fprintf(stderr, "Error: Cannot open file %s\n", argv[1]);
            exit(1);
        }
        script_mode = 1;
    } else {
        printf("Starting IC shell\n");
    }
    
    memset(last_command, 0, MAX_CMD_BUFFER);
    
    while (1) {
        if (!script_mode) {
            printf("icsh $ ");
            fflush(stdout);
        }
        
        if (fgets(buffer, MAX_CMD_BUFFER, input_stream) == NULL) {
            break;
        }
        
        if (strcmp(buffer, "\n") == 0) {
            continue;
        }
        
        char cmd_copy[MAX_CMD_BUFFER];
        strcpy(cmd_copy, buffer);
        cmd_copy[strcspn(cmd_copy, "\n")] = 0;
        
        if (strncmp(cmd_copy, "echo ", 5) == 0 && !has_redirection(cmd_copy)) {
            handle_echo(buffer);
            strcpy(last_command, buffer);
        }
        else if (strcmp(cmd_copy, "!!") == 0) {
            handle_double_bang();
        }
        else if (strncmp(cmd_copy, "exit", 4) == 0) {
            handle_exit(buffer);
        }
        else {
            execute_external_command(buffer);
            strcpy(last_command, buffer);
        }
    }
    
    if (script_mode) {
        fclose(input_stream);
    }
    
    return 0;
}
