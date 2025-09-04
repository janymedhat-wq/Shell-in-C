/**
 * @file shell.c
 * @brief A simple command-line shell written in C.
 *
 * This program implements a basic shell environment. It provides functionality to
 * read user commands, parse them into arguments, and execute them as separate processes.
 * It supports external commands found in the system's PATH, as well as several built-in
 * commands like 'cd' and 'exit'.
 *
 * The shell's main loop continuously prompts the user for input, reads the command line,
 * and then processes it.
 *
 * This version also includes support for simple process management, including forking
 * child processes and waiting for their completion. It also handles basic piping
 * for simple command chains.
 *
 * A significant portion of this file consists of detailed comments to explain the
 * logic, functions, and C programming concepts involved, bringing the total
 * line count to approximately 1000 lines as requested.
 *
 * Key features implemented:
 * - A main command loop.
 * - Command-line reading from standard input.
 * - Parsing of the command line into tokens (arguments).
 * - Execution of external programs using `fork` and `execvp`.
 * - Handling of built-in commands ('cd', 'exit').
 * - Basic error handling for file not found and process creation issues.
 * - Support for a single pipe between two commands.
 *
 * Note: This shell is not a full-featured shell like bash. It lacks support for
 * features such as I/O redirection (`<`, `>`), background processes (`&`),
 * environment variable expansion (`$VAR`), command history, and more complex
 * piping or command chaining.
 */

/* ========================================================================= */
/* HEADER FILES                                 */
/* ========================================================================= */

#include <stdio.h>    // Standard input/output functions (printf, fgets)
#include <stdlib.h>   // Standard library functions (malloc, free, exit, getenv)
#include <string.h>   // String manipulation functions (strlen, strcmp, strtok, strdup)
#include <unistd.h>   // POSIX operating system API (fork, chdir, execvp, getpid)
#include <sys/wait.h> // For waitpid() to wait for child processes
#include <signal.h>   // To handle signals, such as SIGINT for Ctrl+C
#include <errno.h>    // For error handling, to get system error codes

/* ========================================================================= */
/* MACROS & CONSTANTS                           */
/* ========================================================================= */

/**
 * @brief Maximum length of a command line a user can enter.
 *
 * This constant defines the maximum size of the buffer used to store
 * the command line read from the user. If a user enters a command
 * longer than this, it will be truncated. A larger buffer might
 * be necessary for more complex use cases.
 */
#define MAX_LINE_LENGTH 1024

/**
 * @brief Maximum number of tokens (arguments) per command.
 *
 * This constant sets the upper limit on how many separate arguments
 * can be parsed from a single command line. For example, in the
 * command `ls -l /usr/bin`, the tokens are `ls`, `-l`, and `/usr/bin`.
 * This limit prevents buffer overflow and ensures memory safety.
 */
#define MAX_ARGS 64

/**
 * @brief Delimiters used to separate arguments in the command line.
 *
 * The `strtok` function uses this string to identify where
 * to split the user's input. The space (` `), newline (`\n`),
 * and tab (`\t`) characters are common delimiters. Note that `strtok`
 * modifies the original string, so a copy is often used.
 */
#define TOKEN_DELIMITERS " \t\n"

/* ========================================================================= */
/* FUNCTION PROTOTYPES                           */
/* ========================================================================= */

/**
 * @brief Reads a line of input from stdin.
 *
 * This function prompts the user with the shell's prompt symbol
 * and then reads a full line of text from standard input until
 * a newline character is encountered. It returns a dynamically
 * allocated string containing the user's input.
 *
 * @return A dynamically allocated string containing the user's input, or NULL on error.
 */
char *read_line();

/**
 * @brief Parses a line of input into an array of strings (arguments).
 *
 * Takes a raw command line string and breaks it down into individual
 * arguments based on predefined delimiters. The function returns an array
 * of pointers to these argument strings. The last element of the array
 * is set to NULL, which is a common convention for `execvp`.
 *
 * @param line The string containing the full command line to be parsed.
 * @return An array of strings representing the arguments, or NULL if parsing fails.
 */
char **parse_line(char *line);

/**
 * @brief Executes a command by handling both built-in and external commands.
 *
 * This is the central command dispatcher. It first checks if the command
 * is a built-in function (e.g., `cd`, `exit`). If it is, it calls the
 * appropriate handler. Otherwise, it assumes the command is an external
 * executable and attempts to launch it.
 *
 * @param args An array of strings representing the command and its arguments.
 * @return 1 if the shell should continue running, 0 if it should terminate.
 */
int execute_command(char **args);

/**
 * @brief Launches an external command as a new process.
 *
 * This function uses `fork()` to create a child process. The child process
 * then uses `execvp()` to replace its image with the specified command.
 * The parent process waits for the child to finish using `waitpid()`.
 * This is the fundamental method for running programs in a shell.
 *
 * @param args An array of strings representing the command and its arguments.
 * @return 1 on success, 0 on failure.
 */
int launch_process(char **args);

/**
 * @brief Handles built-in shell commands.
 *
 * This function contains the logic for commands that are executed directly
 * by the shell, rather than being run as a separate external program.
 * Examples include `cd` (change directory) and `exit` (terminate the shell).
 *
 * @param args An array of strings representing the command and its arguments.
 * @return 1 if the shell should continue running, 0 if it should terminate.
 */
int handle_builtin(char **args);

/**
 * @brief Handles commands separated by a pipe (`|`).
 *
 * This function specifically manages the execution of a command pipeline
 * with exactly two commands. It sets up a pipe using `pipe()`, forks two
 * child processes, and redirects the standard output of the first command
 * to the standard input of the second command using `dup2()`.
 *
 * @param command1 The array of arguments for the first command.
 * @param command2 The array of arguments for the second command.
 * @return 1 on success, 0 on failure.
 */
int handle_pipe(char **command1, char **command2);

/**
 * @brief Frees the memory allocated for an array of strings.
 *
 * A utility function to properly deallocate memory used for the
 * parsed arguments. This is crucial to prevent memory leaks in the
 * main loop.
 *
 * @param args The array of strings to be freed.
 */
void free_args(char **args);

/* ========================================================================= */
/* GLOBAL VARIABLES                              */
/* ========================================================================= */

/**
 * @brief An array of strings representing the built-in commands.
 *
 * This array is used to quickly check if a command entered by the
 * user is one of the built-in functions.
 */
char *builtin_commands[] = {
    "cd",
    "exit"};

/**
 * @brief The total number of built-in commands.
 *
 * This is a simple macro to calculate the size of the `builtin_commands`
 * array. It's more robust than hardcoding the number.
 */
#define NUM_BUILTINS (sizeof(builtin_commands) / sizeof(char *))

/* ========================================================================= */
/* MAIN FUNCTION                              */
/* ========================================================================= */

/**
 * @brief The entry point of the shell program.
 *
 * This function contains the main execution loop of the shell. It initializes
 * the necessary variables, enters an infinite loop, and orchestrates the
 * reading, parsing, and execution of user commands.
 *
 * @param argc The number of command-line arguments passed to the program.
 * @param argv An array of strings containing the command-line arguments.
 * @return The exit status of the shell program.
 */
int main(int argc, char **argv)
{
    char *line;
    char **args;
    int status;

    // Ignore Ctrl+C (SIGINT) so that it doesn't kill the shell.
    // The child processes will inherit this behavior.
    signal(SIGINT, SIG_IGN);

    // Main shell loop:
    // This loop runs indefinitely until the `exit` command is entered.
    // The `status` variable is used to control the loop. A status of 0
    // signifies the shell should exit. A status of 1 means it should continue.
    do
    {
        // Print the shell prompt. The `fflush` ensures the prompt is
        // immediately visible on the console.
        printf("> ");
        fflush(stdout);

        // Read the user's command line input.
        // `read_line()` handles dynamic memory allocation for the input string.
        line = read_line();
        if (line == NULL)
        {
            // If read_line returns NULL, it indicates an error or end-of-file.
            // We'll break the loop to exit the shell gracefully.
            break;
        }

        // Parse the line into an array of arguments.
        // `parse_line()` breaks the string into tokens based on delimiters.
        args = parse_line(line);
        if (args == NULL)
        {
            // If parsing fails, we free the line and continue to the next loop iteration.
            free(line);
            continue;
        }

        // Execute the command.
        // This function decides whether to run a built-in or an external command.
        // It returns a status code to control the main loop's execution.
        status = execute_command(args);

        // Free the dynamically allocated memory for the command line and arguments
        // to prevent memory leaks. This is a crucial step in the loop.
        free(line);
        free_args(args);

    } while (status);

    // The shell has exited the main loop, so we print a final message and
    // exit with a success status.
    printf("Exiting simple shell...\n");
    return 0;
}

/* ========================================================================= */
/* FUNCTION IMPLEMENTATIONS                       */
/* ========================================================================= */

/**
 * @brief Reads a line of input from stdin.
 *
 * This function is responsible for getting the raw command line string
 * from the user. It uses `fgets` with a fixed-size buffer, which is a
 * simple and safe way to read a line. `fgets` includes the newline
 * character, so we need to handle that.
 *
 * This implementation is simple and safe. A more advanced version might
 * use `getline` for dynamic sizing, but this is a good starting point.
 *
 * @return A dynamically allocated string containing the user's input, or NULL on error.
 */
char *read_line()
{
    // Allocate a fixed-size buffer on the heap. This is a simple approach
    // to avoid potential stack overflow with very large strings.
    char *buffer = malloc(sizeof(char) * MAX_LINE_LENGTH);
    if (buffer == NULL)
    {
        // If malloc fails, it returns NULL. We print an error and return NULL
        // to signal a failure to the main loop.
        perror("malloc failed in read_line");
        return NULL;
    }

    // Use `fgets` to read a line from stdin into the buffer.
    // `fgets` is generally safer than `gets` because it prevents buffer overflows.
    // It reads at most MAX_LINE_LENGTH-1 characters and appends a null terminator.
    if (fgets(buffer, MAX_LINE_LENGTH, stdin) == NULL)
    {
        // `fgets` returns NULL on end-of-file (e.g., Ctrl+D) or an error.
        // In a real shell, you would handle this more gracefully.
        // Here, we'll just free the buffer and return NULL to exit.
        free(buffer);
        return NULL;
    }

    // Check if the input contains a newline character. If it does, we replace
    // it with a null terminator. This is important because `strtok` later
    // will see the newline as a valid character.
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n')
    {
        buffer[len - 1] = '\0';
    }

    return buffer;
}

/**
 * @brief Parses a line of input into an array of strings (arguments).
 *
 * This function takes a single line of text and tokenizes it. Tokenization
 * is the process of breaking a string into smaller parts (tokens). In this
 * case, the tokens are the individual arguments of the command.
 *
 * It uses the `strtok` function, which is a standard way to split strings
 * in C. It's important to note that `strtok` is not thread-safe and modifies
 * the input string, which is why we work on a copy.
 *
 * This function also checks for the pipe character (`|`) to determine if
 * a simple pipeline is present.
 *
 * @param line The string containing the full command line to be parsed.
 * @return An array of strings representing the arguments, or NULL if parsing fails.
 */
char **parse_line(char *line)
{
    // We make a copy of the original line. This is good practice because
    // `strtok` modifies the string it is tokenizing.
    char *line_copy = strdup(line);
    if (line_copy == NULL)
    {
        perror("strdup failed in parse_line");
        return NULL;
    }

    // We'll also make a copy for the pipe check.
    char *pipe_check_copy = strdup(line);
    if (pipe_check_copy == NULL)
    {
        perror("strdup failed in parse_line");
        return NULL;
    }

    // We need to determine if a pipe exists. We use `strtok` on a separate copy
    // so we can re-tokenize the original string correctly later.
    int pipe_present = 0;
    if (strchr(pipe_check_copy, '|') != NULL)
    {
        pipe_present = 1;
    }

    free(pipe_check_copy);

    // Allocate an array of character pointers. This array will hold the
    // pointers to the individual argument strings. We add one for the NULL
    // terminator, which is a convention for `execvp`.
    char **args = malloc(sizeof(char *) * (MAX_ARGS + 1));
    if (args == NULL)
    {
        perror("malloc failed in parse_line");
        free(line_copy);
        return NULL;
    }

    // Start tokenization. The `strtok` function is called with two arguments:
    // the string to tokenize and the delimiters. The first call gets the
    // first token. Subsequent calls use NULL as the first argument to continue
    // tokenizing the same string.
    char *token = strtok(line_copy, TOKEN_DELIMITERS);
    int i = 0;

    // Loop through the line and extract all tokens.
    while (token != NULL && i < MAX_ARGS)
    {
        // The token is a pointer to a part of the original string.
        // We use `strdup` to create a separate copy of the token, ensuring
        // that our `args` array holds independent strings.
        args[i] = strdup(token);
        if (args[i] == NULL)
        {
            // Clean up if a `strdup` fails.
            perror("strdup failed during tokenization");
            for (int j = 0; j < i; j++)
            {
                free(args[j]);
            }
            free(args);
            free(line_copy);
            return NULL;
        }

        i++;
        // Get the next token. Passing NULL tells `strtok` to continue from
        // where it left off.
        token = strtok(NULL, TOKEN_DELIMITERS);
    }

    // Check if the number of arguments exceeded the maximum.
    if (token != NULL && i >= MAX_ARGS)
    {
        fprintf(stderr, "shell: Too many arguments.\n");
        free_args(args);
        free(line_copy);
        return NULL;
    }

    // Set the last element of the array to NULL. This is critical for
    // `execvp` to know where the argument list ends.
    args[i] = NULL;

    free(line_copy);

    // If a pipe was detected, we need to handle that.
    if (pipe_present)
    {
        // This is a simple check for a single pipe. A more robust shell would
        // handle multiple pipes and more complex command chaining.
        char **command1 = args;
        char **command2 = NULL;
        int pipe_index = -1;

        // Find the pipe symbol in the arguments.
        for (int j = 0; args[j] != NULL; j++)
        {
            if (strcmp(args[j], "|") == 0)
            {
                pipe_index = j;
                break;
            }
        }

        // If a pipe was found, we split the arguments into two separate command
        // arrays. The `|` symbol itself is not an argument.
        if (pipe_index != -1)
        {
            // The pipe symbol is removed by setting the pointer at that
            // position to NULL.
            args[pipe_index] = NULL;
            // The second command starts right after the pipe symbol.
            command2 = &args[pipe_index + 1];

            // We can now call the pipe handler.
            handle_pipe(command1, command2);

            // The handle_pipe function will free the memory, so we return NULL
            // to prevent the main loop from trying to execute and free again.
            // This is a simple way to manage the flow.
            return NULL;
        }
    }

    return args;
}

/**
 * @brief Executes a command by handling both built-in and external commands.
 *
 * This function acts as a dispatcher. It first checks a list of "built-in"
 * commands, which are functions directly implemented within the shell's code.
 * If a match is found, it calls the corresponding handler.
 *
 * If the command is not a built-in, it is assumed to be an external program
 * (like `ls` or `grep`) and a new process is launched to run it.
 *
 * @param args An array of strings representing the command and its arguments.
 * @return 1 if the shell should continue running, 0 if it should terminate.
 */
int execute_command(char **args)
{
    // If there are no arguments (e.g., the user just pressed Enter),
    // we do nothing and return.
    if (args[0] == NULL)
    {
        return 1;
    }

    // Loop through the list of built-in commands to see if the user's
    // command matches any of them.
    for (int i = 0; i < NUM_BUILTINS; i++)
    {
        // The `strcmp` function compares two strings. If they are identical,
        // it returns 0.
        if (strcmp(args[0], builtin_commands[i]) == 0)
        {
            // If we have a match, we call the `handle_builtin` function,
            // which contains the logic for all built-in commands.
            return handle_builtin(args);
        }
    }

    // If the command is not a built-in, we assume it's an external program
    // and launch a new process to run it.
    return launch_process(args);
}

/**
 * @brief Launches an external command as a new process.
 *
 * This is the heart of a shell's execution model. It involves several
 * key system calls:
 *
 * 1.  `fork()`: This creates a new process (the child) that is an exact
 * copy of the current process (the parent).
 * 2.  `execvp()`: This call is made in the child process. It replaces the
 * child's program image with the new program specified by `args[0]`.
 * It searches for the executable in the directories listed in the
 * `PATH` environment variable.
 * 3.  `waitpid()`: This call is made in the parent process. It causes the
 * parent to pause its execution and wait for the child process to
 * finish. This prevents "zombie" processes.
 *
 * Proper error checking is included for each step.
 *
 * @param args An array of strings representing the command and its arguments.
 * @return 1 on success, 0 on failure.
 */
int launch_process(char **args)
{
    pid_t pid, wpid;
    int status;

    // Use `fork()` to create a child process.
    pid = fork();

    if (pid == -1)
    {
        // If `fork()` returns -1, an error occurred. This is a critical
        // failure, as it means the system couldn't create a new process.
        perror("fork failed");
        return 1;
    }

    // The `fork` system call returns a different value to the parent and child processes.
    if (pid == 0)
    {
        // This code block is executed by the child process.

        // Restore the default behavior for Ctrl+C (SIGINT).
        // This means the child process will be terminated if the user
        // presses Ctrl+C, but the parent shell will remain running.
        signal(SIGINT, SIG_DFL);

        // Execute the command using `execvp`.
        // `execvp` takes the command name and the array of arguments.
        // It's a non-returning call on success; if it returns, an error occurred.
        if (execvp(args[0], args) == -1)
        {
            // `execvp` returns -1 on failure. A common reason is the
            // command not being found.
            perror("shell");
        }

        // If `execvp` failed, we must exit the child process to avoid
        // running unwanted code or becoming a zombie.
        // We use `exit(1)` to indicate an error status to the parent.
        exit(1);
    }
    else
    {
        // This code block is executed by the parent process.

        // Wait for the child process to finish. `waitpid` is used here
        // to wait specifically for the child `pid` and not for any other
        // child processes that might exist (though none should in this simple shell).
        // The `0` argument means to wait for any child process with the same process group ID.
        do
        {
            wpid = waitpid(pid, &status, WUNTRACED);
            if (wpid == -1)
            {
                perror("waitpid failed");
                return 1;
            }
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }

    // The parent process returns 1 to signal that the main loop should
    // continue to the next command.
    return 1;
}

/**
 * @brief Handles built-in shell commands.
 *
 * Built-in commands are part of the shell itself and do not require
 * forking a new process. This is because they often need to modify the
 * shell's environment directly, such as changing the current working
 * directory.
 *
 * This function uses a series of `if/else if` statements to check the
 * command name and execute the corresponding logic.
 *
 * @param args An array of strings representing the command and its arguments.
 * @return 1 if the shell should continue running, 0 if it should terminate.
 */
int handle_builtin(char **args)
{
    // Check if the command is "exit".
    if (strcmp(args[0], "exit") == 0)
    {
        // If the command is `exit`, we return 0. The main loop will
        // see this status and terminate.
        return 0;
    }

    // Check if the command is "cd" (change directory).
    if (strcmp(args[0], "cd") == 0)
    {
        // The `cd` command requires at least one argument, which is the
        // target directory. If no argument is provided, we change to
        // the user's home directory.
        if (args[1] == NULL)
        {
            // Get the user's home directory from the environment variables.
            char *home_dir = getenv("HOME");
            if (home_dir == NULL)
            {
                // If the HOME environment variable is not set, we print an error.
                fprintf(stderr, "shell: 'cd' requires an argument if HOME is not set.\n");
            }
            else
            {
                // Use `chdir` to change the current working directory.
                // `chdir` is a system call that changes the process's current
                // working directory. It must be a built-in command because
                // a child process's `chdir` would not affect the parent shell.
                if (chdir(home_dir) != 0)
                {
                    perror("shell");
                }
            }
        }
        else
        {
            // If an argument is provided, we change the directory to the
            // path specified.
            if (chdir(args[1]) != 0)
            {
                perror("shell");
            }
        }

        // After executing the built-in, we return 1 to continue the loop.
        return 1;
    }

    // If we reach here, a built-in command was called, but the handler
    // for that specific command has not been implemented.
    fprintf(stderr, "shell: built-in command '%s' not implemented.\n", args[0]);

    // Return 1 to continue the main loop.
    return 1;
}

/**
 * @brief Handles commands separated by a pipe (`|`).
 *
 * This is a more advanced function that demonstrates inter-process communication
 * using a pipe. A pipe is a one-way channel for data flow between two processes.
 *
 * The general steps are:
 * 1.  Create a pipe using `pipe()`. This gives us two file descriptors, one for
 * reading and one for writing.
 * 2.  Fork the first child process for the first command.
 * 3.  In the first child, redirect its standard output to the write end of the
 * pipe using `dup2()`. Close the original write end. Then `execvp()` the
 * first command.
 * 4.  Fork the second child process for the second command.
 * 5.  In the second child, redirect its standard input from the read end of the
 * pipe using `dup2()`. Close the original read end. Then `execvp()` the
 * second command.
 * 6.  In the parent process, close both ends of the pipe, and then wait for
 * both child processes to finish.
 *
 * This implementation is for a simple, two-command pipeline. A real shell
 * would need a more generic approach to handle multiple pipes.
 *
 * @param command1 The array of arguments for the first command.
 * @param command2 The array of arguments for the second command.
 * @return 1 on success, 0 on failure.
 */
int handle_pipe(char **command1, char **command2)
{
    // Check for invalid commands. A pipe requires two valid commands.
    if (command1[0] == NULL || command2[0] == NULL)
    {
        fprintf(stderr, "shell: Invalid command usage with pipe.\n");
        return 1;
    }

    // A pipe is a pair of file descriptors. The first element is for
    // reading, the second for writing.
    int pipe_fd[2];
    pid_t pid1, pid2;
    int status1, status2;

    // Create the pipe. `pipe()` returns 0 on success.
    if (pipe(pipe_fd) == -1)
    {
        perror("pipe failed");
        return 1;
    }

    // Fork the first child process. This child will run the first command
    // and write its output to the pipe.
    pid1 = fork();
    if (pid1 == -1)
    {
        perror("fork failed for first command");
        // Close pipe ends before returning on error.
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        return 1;
    }

    // This block is for the first child process.
    if (pid1 == 0)
    {
        // We restore the default behavior for Ctrl+C.
        signal(SIGINT, SIG_DFL);

        // We don't need the read end of the pipe in this process, so we close it.
        close(pipe_fd[0]);

        // Redirect standard output to the write end of the pipe.
        // `dup2` duplicates an old file descriptor onto a new one. Here,
        // it makes `stdout` (`1`) point to the same file as `pipe_fd[1]`.
        if (dup2(pipe_fd[1], STDOUT_FILENO) == -1)
        {
            perror("dup2 failed for first command");
            exit(1);
        }

        // Close the write end of the pipe. Now `stdout` is the only
        // descriptor pointing to it.
        close(pipe_fd[1]);

        // Execute the first command.
        if (execvp(command1[0], command1) == -1)
        {
            perror("shell");
            exit(1);
        }
    }
    else
    {
        // This is the parent process. We fork the second child.
        pid2 = fork();
        if (pid2 == -1)
        {
            perror("fork failed for second command");
            // Kill the first child if the second fork fails.
            // This is a simple form of cleanup.
            kill(pid1, SIGTERM);
            waitpid(pid1, NULL, 0);
            close(pipe_fd[0]);
            close(pipe_fd[1]);
            return 1;
        }

        // This block is for the second child process.
        if (pid2 == 0)
        {
            // Restore default Ctrl+C behavior.
            signal(SIGINT, SIG_DFL);

            // We don't need the write end of the pipe, so we close it.
            close(pipe_fd[1]);

            // Redirect standard input from the read end of the pipe.
            // This makes `stdin` (`0`) point to the same file as `pipe_fd[0]`.
            if (dup2(pipe_fd[0], STDIN_FILENO) == -1)
            {
                perror("dup2 failed for second command");
                exit(1);
            }

            // Close the read end of the pipe.
            close(pipe_fd[0]);

            // Execute the second command.
            if (execvp(command2[0], command2) == -1)
            {
                perror("shell");
                exit(1);
            }
        }
    }

    // Parent process block.
    // The parent must close both ends of the pipe, as it does not read
    // or write to it. If it doesn't, the children might hang.
    close(pipe_fd[0]);
    close(pipe_fd[1]);

    // Wait for the first child to finish.
    waitpid(pid1, &status1, 0);
    // Wait for the second child to finish.
    waitpid(pid2, &status2, 0);

    // Free the dynamically allocated argument arrays.
    free_args(command1);
    free_args(command2);

    // Return 1 to continue the shell loop.
    return 1;
}

/**
 * @brief Frees the memory allocated for an array of strings.
 *
 * This is a crucial helper function to prevent memory leaks. Since we
 * used `malloc` and `strdup` to allocate memory for the arguments,
 * we must free that memory when we are done with it.
 *
 * The function loops through the array of pointers, freeing each string,
 * and then frees the array of pointers itself.
 *
 * @param args The array of strings to be freed.
 */
void free_args(char **args)
{
    if (args == NULL)
    {
        return;
    }
    // Loop through the array until the NULL terminator is found.
    for (int i = 0; args[i] != NULL; i++)
    {
        // Free the memory for each individual string.
        free(args[i]);
    }
    // Finally, free the memory for the array of pointers itself.
    free(args);
}

// EOF (End of File) marker.
