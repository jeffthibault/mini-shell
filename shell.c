#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#define BUFFER_SIZE 80
#define NUM_OF_CMDS 4

// Declare built-in command functions
void cd(char* args[]);
void help(char* args[]);
void exit_shell();
void guessing_game();

// Global array declaration of built-in commands
char* cmd_arr[4] = {"cd", "help", "exit", "guess"};

// Global array declaration that contains summary of each built-in cmd
char* help_msgs[4] = {
	"-usage: cd <dir>\n-synopsis: Change the shell working directory to the specified directory.\n\n",
	"-usage: help\n-synopsis: Display a summary of each built-in shell function.\n\n",
	"-usage: exit\n-synopsis: Terminate the shell.\n\n",
	"-usage: guess\n-synopsis: Plays a guessing game with the user.\n\n" 
};

// Global array of function pointers to built-in commands
void (*fun_ptr_arr[])(char**) = {cd, help, exit_shell, guessing_game};

// Signal handler to handle shell termination
void sigint_handler(int sig) {
	write(1, "mini shell terminated\n", 22);
	exit(0);
}

// Function to identify a built-in command
// str is an input argument
// Returns index of cmd in the global array or -1 if not found
int get_builtin_command(char* str) {
	int i = 0;
	for (i = 0; i < NUM_OF_CMDS; i++) {
		if (strcmp(str, cmd_arr[i]) == 0)
			return i;
	}
	return -1;
}

// Function to change the current working directory
// args are the input arguments
void cd(char* args[]) {
	int ret;
	ret = chdir(args[1]);
	if (ret != 0)
		printf("Error: unable to change to specified path.\n");
}

// Function to print a summary of one of the built-in commands
// args are the input arguments
void help(char* args[]) {
	int i = 0;
	for (i = 0; i < NUM_OF_CMDS; i++) {
		printf("%s\n", cmd_arr[i]);
		printf("%s", help_msgs[i]);
	}
}

// Function to exit the shell gracefully
void exit_shell() {
	exit(0);
}

// Function that checks if a string is empty (all whitespace)
// s is the string to check
// Returns 1 if true, 0 if false
int is_empty(char* s) {
	while (*s != '\0') {
		if (!isspace(*s))
			return 0;
		s++;
	}
	return 1;
}

// Function that parses the input into the shell
// str is the input, args are the arguments, n is size of input buffer
// Returns the index of a pipe if one is found
// If the input is empty, it returns 0
// Default return is -1 if the input is not empty and does not have a pipe
int parse_input(char* str, char* args[], int n) {
	if (is_empty(str)) //string is all white space
		return 0; 
	char* token = strtok(str, " \n");
	int status = -1;
	int i = 0;
	for (i = 0; i < n; i++) {
		if (token == NULL) //no more arguments to extract
			break;
		else {
			if (strcmp(token, "|") == 0) //there is a pipe
				status = i;
			args[i] = token; //store the argument
			token = strtok(NULL, " \n");
		}
	}
	return status;
}

// Function that plays a guessing game with the user in the shell
void guessing_game() {
	printf("Welcome to the Guessing Game!\n");
	printf("You have 6 chances to guess the magic number"
		" between 1 and 100.\n");
	srand(time(0));
	unsigned int magic_num = rand() % 100 + 1;
	char str[20];
	int usr_guess;
	int count = 0;
	do {
		count++;
		if (count > 6) { //user out of guesses, game ends
			printf("You ran out of guesses!\n");
			printf("The magic number was %d\n", magic_num);
			break;
		}
		printf("Guess #%d: ", count);
		scanf("%s", str);
		usr_guess = atoi(str); //convert the user input to an int
		while (usr_guess <= 0) { //force the user to input an int
			printf("Invalid Input! Enter an int between 1 and 100.\n");
			printf("Guess #%d: ", count);
			scanf("%s", str);
			usr_guess = atoi(str);
		}
		getchar(); //clear in the input buffer
		if (usr_guess > magic_num)
			printf("Your guess is too high.\n");
		else if (usr_guess < magic_num)
			printf("Your guess is too low.\n");
		else
			printf("Your guess is correct!\n");
	} while(usr_guess != magic_num);
}

// Function that executes a piped commands
// cmd1 is the first half of the pipe
// cmd2 is the second half of the pipe
// builtin is the index of a builtin command
// returns 0 success, 1 if fail
int execute_piped_command(char* cmd1[], char* cmd2[], int builtin) {
	pid_t pid1, pid2;
	int pipefd[2];
	int status1, status2;

	pipe(pipefd); //start a pipe
	pid1 = fork(); //start a child process for the first cmd
	if (pid1 == 0) {
		dup2(pipefd[1], STDOUT_FILENO);
		close(pipefd[0]);
		if (builtin == 1) { //handle a pipe from the builtin cmd help
			(*fun_ptr_arr[builtin])(cmd1);
			exit(EXIT_SUCCESS);
		}
		else if (execvp(cmd1[0], cmd1) == -1) { //handle a pipe from a bin/ cmd
			fprintf(stderr, "%s: command not found\n", cmd1[0]);
			exit(EXIT_FAILURE);
		}
	}
	pid2 = fork(); //start a child processes for the second cmd
	if (pid2 == 0) {
		dup2(pipefd[0], STDIN_FILENO);
		close(pipefd[1]);
		if (execvp(cmd2[0], cmd2) == -1) { //handle the second half of the pipe
			fprintf(stderr, "%s: command not found\n", cmd2[0]);
			exit(EXIT_FAILURE);
		}
	}
	//close the pipe and wait for the children to finish
	close(pipefd[0]);
	close(pipefd[1]);
	waitpid(pid1, &status1, 0);
	waitpid(pid2, &status2, 0);
	
	if (WEXITSTATUS(status1) == 0 && WEXITSTATUS(status2) == 0)
		return 0;
	return 1;
}

// Function that executes non-piped commands from bin/
// args is the input arguments, pipe_index is the index of '|'
// returns 0 if successful, 1 if failure
int execute_command(char* args[], int pipe_index) {
	int status;
	int cmd_index = get_builtin_command(args[0]);
	
	// execute a non-piped built in command
	if (cmd_index != -1 && pipe_index == -1) {
		(*fun_ptr_arr[cmd_index])(args);
		return 0;	
	}
	
	// execute a piped command
	if (pipe_index > 0) {
		char* cmd1[BUFFER_SIZE] = { NULL };
		char* cmd2[BUFFER_SIZE] = { NULL };
		int i = 0;
		while (i < pipe_index) { //get first half of pipe
			cmd1[i] = args[i];
			i++;
		}
		i++;
		int j = 0;
		while (args[i] != NULL) { //get second half of pipe
			cmd2[j] = args[i];
			j++;
			i++;
		}
		//execute the pipe
		return execute_piped_command(cmd1, cmd2, cmd_index); 
	}
	else if (fork() == 0) { //execute a non-piped bin/ cmd
		if (execvp(args[0], args) == -1) {
			fprintf(stderr, "%s: command not found\n", args[0]);
			exit(EXIT_FAILURE);
		}
		exit(EXIT_SUCCESS);
	}
	else {
		wait(&status); //wait for the child process to finish
		if (WEXITSTATUS(status) == 0)
			return 0;
		return 1;
	}
}

// Main entry into the program
// Keeps the shell running in an infinite loop
int main() {
	signal(SIGINT, sigint_handler);
	char input[BUFFER_SIZE];
	char* inputArgs[BUFFER_SIZE] = { NULL };
	int status;

	while(1) {
		memset(inputArgs, 0, BUFFER_SIZE); //reset the input args
		printf("mini-shell>");
		fgets(input, BUFFER_SIZE, stdin);
		status = parse_input(input, inputArgs, BUFFER_SIZE);
		if (status == 0) //if input is white space then reset the loop
			continue;
		execute_command(inputArgs, status);
	}

	return 0;
}
