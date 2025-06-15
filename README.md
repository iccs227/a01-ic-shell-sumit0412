[![Review Assignment Due Date](https://classroom.github.com/assets/deadline-readme-button-22041afd0340ce965d47ae6ef1cefeee28c7c493a6346c4f15d667ab976d596c.svg)](https://classroom.github.com/a/WIXYXthJ)
# IC Shell (icsh)

This project is an implementation of a Linux shell called "icsh".

## Milestone 1: Interactive Mode & Built-in Commands
- Implemented an interactive command-line interface with a prompt.
- **`echo [message]`**: Prints the given message.
- **`!!`**: Re-executes the last command. Works consecutively and retains the last command even after empty inputs.
- **`exit [code]`**: Exits the shell with an optional exit code.

## Milestone 2: Scripting/Batch Mode
- The shell can execute commands from a script file (`./icsh <script_file>`).
- Suppresses the prompt in script mode.
- The shell exits after the script finishes execution.

## Milestone 3: External Program Execution
- Supports running external commands located in the system's `PATH` (e.g., `ls`, `grep`).
- Executes programs in the foreground, waiting for them to complete before showing a new prompt.

## Milestone 4: Signal Handling & Exit Status
- **`Ctrl+C` (SIGINT)**: Terminates the currently running foreground process.
- **`Ctrl+Z` (SIGTSTP)**: Stops the currently running foreground process.
- **`echo $?`**: Prints the exit status of the most recently terminated process. The behavior is consistent with standard shells after a process is terminated or stopped.

## Milestone 5: I/O Redirection
- **`<`**: Redirects standard input from a file.
- **`>`**: Redirects standard output to a file, overwriting the file if it exists.

## Milestone 6: Job Control
- **`&`**: Runs a command as a background job.
- **`jobs`**: Lists all current background jobs with their status (Running/Stopped).
- **`fg %<job_id>`**: Resumes a background job and brings it to the foreground.
- **`bg %<job_id>`**: Resumes a stopped background job and keeps it in the background.
- The shell tracks background processes and cleans up completed jobs.

## Milestone 7: Extra Features
- **`cd [path]`**: Added support for the `cd` command to change the current working directory. If no path is given, it changes to the `HOME` directory.
- **Command History**: Users can navigate through previous commands using the up and down arrow keys.
- **Append Redirection (`>>`)**: Appends the output of a command to a file without overwriting it.
- **Wildcard Expansion**: Supports `*` and `?` wildcards for filename expansion (globbing) in command arguments. 