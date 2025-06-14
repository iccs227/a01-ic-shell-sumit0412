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

#define MAX_CMD_BUFFER 255
#define MAX_ARGS 64

char last_command[MAX_CMD_BUFFER];
int script_mode = 0;

void parse_command(char* input, char* argv[]) {
    int argc = 0;
    char* token = strtok(input, " \t\n");
    
    while (token != NULL && argc < MAX_ARGS - 1) {
        argv[argc++] = token;
        token = strtok(NULL, " \t\n");
    }
    argv[argc] = NULL;
}

void handle_echo(char* input) {
    char* echo_text = input + 5;
    printf("%s", echo_text);
}

void handle_double_bang() {
    if (strlen(last_command) > 0) {
        if (!script_mode) {
            printf("%s", last_command);
        }
        
        char temp[MAX_CMD_BUFFER];
        strcpy(temp, last_command);
        temp[strcspn(temp, "\n")] = 0;
        
        if (strncmp(temp, "echo ", 5) == 0) {
            handle_echo(last_command);
        }
        else if (strcmp(temp, "!!") != 0) {
            strcpy(last_command, temp);
            strcat(last_command, "\n");
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
        
        if (strncmp(cmd_copy, "echo ", 5) == 0) {
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
            printf("bad command\n");
        }
    }
    
    if (script_mode) {
        fclose(input_stream);
    }
    
    return 0;
}
