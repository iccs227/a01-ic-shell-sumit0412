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
#define MAX_JOBS 64

typedef enum {
    RUNNING,
    STOPPED,
    DONE
} job_status_t;

typedef struct {
    int job_id;
    pid_t pid;
    pid_t pgid;
    job_status_t status;
    char command[MAX_CMD_BUFFER];
    int is_background;
} job_t;

char last_command[MAX_CMD_BUFFER];
int script_mode = 0;
int last_exit_status = 0;
pid_t foreground_pid = 0;
pid_t shell_pgid;
job_t jobs[MAX_JOBS];
int next_job_id = 1;

void init_jobs() {
    for (int i = 0; i < MAX_JOBS; i++) {
        jobs[i].job_id = 0;
        jobs[i].pid = 0;
        jobs[i].pgid = 0;
        jobs[i].status = DONE;
        jobs[i].command[0] = '\0';
        jobs[i].is_background = 0;
    }
}

int add_job(pid_t pid, pid_t pgid, char* command, int is_background) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].job_id == 0) {
            jobs[i].job_id = next_job_id++;
            jobs[i].pid = pid;
            jobs[i].pgid = pgid;
            jobs[i].status = RUNNING;
            strncpy(jobs[i].command, command, MAX_CMD_BUFFER - 1);
            jobs[i].command[MAX_CMD_BUFFER - 1] = '\0';
            jobs[i].is_background = is_background;
            return jobs[i].job_id;
        }
    }
    return -1;
}

job_t* find_job_by_id(int job_id) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].job_id == job_id) {
            return &jobs[i];
        }
    }
    return NULL;
}

job_t* find_job_by_pid(pid_t pid) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].pid == pid) {
            return &jobs[i];
        }
    }
    return NULL;
}

void remove_job(int job_id) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].job_id == job_id) {
            jobs[i].job_id = 0;
            jobs[i].pid = 0;
            jobs[i].pgid = 0;
            jobs[i].status = DONE;
            jobs[i].command[0] = '\0';
            jobs[i].is_background = 0;
            break;
        }
    }
}

void cleanup_jobs() {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].job_id > 0 && jobs[i].status == DONE) {
            remove_job(jobs[i].job_id);
        }
    }
}

void sigchld_handler(int sig) {
    pid_t pid;
    int status;
    
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
        job_t* job = find_job_by_pid(pid);
        if (job != NULL) {
            if (WIFEXITED(status) || WIFSIGNALED(status)) {
                job->status = DONE;
                if (job->is_background) {
                    printf("\n[%d]+  Done                    %s", job->job_id, job->command);
                    fflush(stdout);
                }
                last_exit_status = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
            } else if (WIFSTOPPED(status)) {
                job->status = STOPPED;
                printf("\n[%d]+  Stopped                 %s", job->job_id, job->command);
                fflush(stdout);
            }
        }
    }
}

void sigint_handler(int sig) {
    if (foreground_pid > 0) {
        kill(-foreground_pid, SIGINT);
    } else {
        printf("\n");
        if (!script_mode) {
            printf("icsh $ ");
            fflush(stdout);
        }
    }
}

void sigtstp_handler(int sig) {
    if (foreground_pid > 0) {
        kill(-foreground_pid, SIGTSTP);
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

int is_background_command(char* input) {
    char* trimmed = input;
    while (*trimmed && (*trimmed == ' ' || *trimmed == '\t')) trimmed++;
    
    int len = strlen(trimmed);
    while (len > 0 && (trimmed[len-1] == ' ' || trimmed[len-1] == '\t' || trimmed[len-1] == '\n')) {
        len--;
    }
    
    return (len > 0 && trimmed[len-1] == '&');
}

void execute_external_command(char* input) {
    pid_t pid;
    int status;
    char* argv[MAX_ARGS];
    char cmd_copy[MAX_CMD_BUFFER];
    char* input_file = NULL;
    char* output_file = NULL;
    int append_mode = 0;
    int background = is_background_command(input);
    
    strcpy(cmd_copy, input);
    
    if (background) {
        int len = strlen(cmd_copy);
        while (len > 0 && (cmd_copy[len-1] == '&' || cmd_copy[len-1] == ' ' || cmd_copy[len-1] == '\t' || cmd_copy[len-1] == '\n')) {
            cmd_copy[len-1] = '\0';
            len--;
        }
    }
    
    parse_redirection(cmd_copy, argv, &input_file, &output_file, &append_mode);
    
    pid = fork();
    
    if (pid < 0) {
        perror("Fork failed");
        return;
    }
    else if (pid == 0) {
        if (background) {
            setpgid(0, 0);
        } else {
            setpgid(0, 0);
        }
        
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);
        
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
        setpgid(pid, pid);
        
        if (background) {
            int job_id = add_job(pid, pid, input, 1);
            printf("[%d] %d\n", job_id, pid);
        } else {
            foreground_pid = pid;
            tcsetpgrp(STDIN_FILENO, pid);
            
            waitpid(pid, &status, WUNTRACED);
            
            tcsetpgrp(STDIN_FILENO, shell_pgid);
            foreground_pid = 0;
            
            if (WIFEXITED(status)) {
                last_exit_status = WEXITSTATUS(status);
            }
            else if (WIFSIGNALED(status)) {
                last_exit_status = 128 + WTERMSIG(status);
            }
            else if (WIFSTOPPED(status)) {
                int job_id = add_job(pid, pid, input, 0);
                printf("\n[%d]+  Stopped                 %s", job_id, input);
                last_exit_status = 128 + WSTOPSIG(status);
            }
        }
    }
}

void handle_jobs() {
    cleanup_jobs();
    
    int found_jobs = 0;
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].job_id > 0 && jobs[i].status != DONE) {
            char* status_str = (jobs[i].status == RUNNING) ? "Running" : "Stopped";
            char marker = '+';
            
            printf("[%d]%c  %s                    %s", jobs[i].job_id, marker, status_str, jobs[i].command);
            found_jobs = 1;
        }
    }
    
    if (!found_jobs) {
        printf("");
    }
    
    last_exit_status = 0;
}

void handle_fg(char* input) {
    char* arg = input + 3;
    while (*arg == ' ' || *arg == '\t') arg++;
    
    if (*arg != '%') {
        printf("Usage: fg %%<job_id>\n");
        return;
    }
    
    int job_id = atoi(arg + 1);
    job_t* job = find_job_by_id(job_id);
    
    if (job == NULL || job->status == DONE) {
        printf("fg: job %d not found\n", job_id);
        return;
    }
    
    char clean_cmd[MAX_CMD_BUFFER];
    strcpy(clean_cmd, job->command);
    clean_cmd[strcspn(clean_cmd, "\n")] = 0;
    printf("%s\n", clean_cmd);
    
    if (job->status == STOPPED) {
        kill(-job->pgid, SIGCONT);
    }
    
    job->status = RUNNING;
    job->is_background = 0;
    
    foreground_pid = job->pgid;
    tcsetpgrp(STDIN_FILENO, job->pgid);
    
    int status;
    waitpid(job->pid, &status, WUNTRACED);
    
    tcsetpgrp(STDIN_FILENO, shell_pgid);
    foreground_pid = 0;
    
    if (WIFEXITED(status) || WIFSIGNALED(status)) {
        remove_job(job_id);
        last_exit_status = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
    } else if (WIFSTOPPED(status)) {
        job->status = STOPPED;
        job->is_background = 0;
    }
}

void handle_bg(char* input) {
    char* arg = input + 3;
    while (*arg == ' ' || *arg == '\t') arg++;
    
    if (*arg != '%') {
        printf("Usage: bg %%<job_id>\n");
        return;
    }
    
    int job_id = atoi(arg + 1);
    job_t* job = find_job_by_id(job_id);
    
    if (job == NULL || job->status == DONE) {
        printf("bg: job %d not found\n", job_id);
        return;
    }
    
    if (job->status != STOPPED) {
        printf("bg: job %d is not stopped\n", job_id);
        return;
    }
    
    job->status = RUNNING;
    job->is_background = 1;
    
    char clean_cmd[MAX_CMD_BUFFER];
    strcpy(clean_cmd, job->command);
    clean_cmd[strcspn(clean_cmd, "\n")] = 0;
    printf("[%d]+ %s &\n", job->job_id, clean_cmd);
    
    kill(-job->pgid, SIGCONT);
    last_exit_status = 0;
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
    
    shell_pgid = getpid();
    if (setpgid(shell_pgid, shell_pgid) < 0) {
        perror("Couldn't put the shell in its own process group");
        exit(1);
    }
    
    tcsetpgrp(STDIN_FILENO, shell_pgid);
    
    signal(SIGINT, sigint_handler);
    signal(SIGTSTP, sigtstp_handler);
    signal(SIGCHLD, sigchld_handler);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    
    init_jobs();
    
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
        cleanup_jobs();
        
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
        else if (strcmp(cmd_copy, "jobs") == 0) {
            handle_jobs();
        }
        else if (strncmp(cmd_copy, "fg ", 3) == 0) {
            handle_fg(cmd_copy);
        }
        else if (strncmp(cmd_copy, "bg ", 3) == 0) {
            handle_bg(cmd_copy);
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
