#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define BUFFSIZE 4096

char isIORedirected = 0;
char isChangeDirectory = 0;
int fdIn = -1;
int fdOut = -1;
char* inFile;
char* outFile;
int saveOut;
int saveIn;


/* Retrieve the hostname and make sure that this program is not being run on the main odin server.
 * It must be run on one of the vcf cluster nodes (vcf0 - vcf3).
 */
void check()
{
	char hostname[10];
	gethostname(hostname, 9);
	hostname[9] = '\0';
	if (strcmp(hostname, "csci-odin") == 0) {
		fprintf(stderr, "WARNING: TO MINIMIZE THE RISK OF FORK BOMBING THE ODIN SERVER,\nYOU MUST RUN THIS PROGRAM ON ONE OF THE VCF CLUSTER NODES!!!\n");
		exit(EXIT_FAILURE);
	} // if
} // check

/*
  * Retrieve and print the current working directory (cwd), similar to pwd(1). If the cwd contains
  * the home directory (/home/myid/abc12345 format), replace it with ~.
  */
void pwd() {
	char cwd[BUFFSIZE];
	// set cwd to the current working directory
	if (getcwd(cwd, sizeof(cwd)) == NULL) { 
		perror("getcwd");
		exit(EXIT_FAILURE);
	} // if

	// if cwd contains the user's home directory, replace it with ~
	int homeDirLength = strlen(getenv("HOME"));
	char newCwd[BUFFSIZE] = "~";
	char *tmp = &cwd[homeDirLength];
	if (!strncmp(cwd, getenv("HOME"), homeDirLength)) {
		strcat(newCwd, tmp);
		printf("%s", newCwd);
	} else {
		printf("%s", cwd);
	} // else
} // pwd

int main() {
	pid_t pid;

	check();
	setbuf(stdout, NULL); // makes printf() unbuffered
	int n;
	char cmd[BUFFSIZE];

	// Project 3 TODO: set the current working directory to the user home directory upon initial launch of the shell
	// You may use getenv("HOME") to retrive the user home directory

	chdir(getenv("HOME"));

	// inifite loop that repeated prompts the user to enter a command
	while (1) {
		printf("1730sh:");
		pwd();
		// Project 3 TODO: display the current working directory as part of the prompt
		printf("$ ");
		n = read(STDIN_FILENO, cmd, BUFFSIZE);

		// if user enters a non-empty command
		if (n > 1) {
			cmd[n-1] = '\0'; // replaces the final '\n' character with '\0' to make a proper C string


			// Lab 06 TODO: parse/tokenize cmd by space to prepare the
			// command line argument array that is required by execvp().
			// For example, if cmd is "head -n 1 file.txt", then the 
			// command line argument array needs to be
			// ["head", "-n", "1", "file.txt", NULL].

			char * token = strtok(cmd, " ");
			char * commandLineArgs[BUFFSIZE];
			commandLineArgs[0] = token;

			for (int i = 1; token != NULL; i++) {
				token = strtok(NULL, " ");
				if (token == NULL) {
					commandLineArgs[i] = NULL;
					break;
				} // if
				if (strcmp(token, "<") == 0 ||
						strcmp(token, ">") == 0 ||
						strcmp(token, ">>") == 0) {
					isIORedirected = 1;
					commandLineArgs[i] = NULL;
					break;
				} // if
				commandLineArgs[i] = token;
			} // for

			// Lab 07 TODO: if the command contains input/output direction operators
			// such as "head -n 1 < input.txt > output.txt", then the command
			// line argument array required by execvp() needs to be
			// ["head", "-n", "1", NULL], while the "< input.txt > output.txt" portion
			// needs to be parsed properly to be used with dup2(2) inside the child process

			char * IO[BUFFSIZE];
			int IOLength = 0;
			IO[0] = token;
			if (isIORedirected) {
				for (int i = 1; token != NULL; i++, IOLength ++) {
					token = strtok(NULL, " ");
					IO[i] = token;
				} // for

				for (int i = 0; i < IOLength; i++) {
					if (strcmp(IO[i], "<") == 0) { // redirect INPUT to file
								       // get the filename to redirect input from (IO[i+1])
						i++;
						inFile = IO[i];

						// backup the fg for stdin
						saveIn = dup(STDIN_FILENO);
						if (saveIn == -1) {
							perror("dup stdin");
							exit(EXIT_FAILURE);
						} // if

						// open the file
						fdIn = open(inFile, O_RDONLY);
						if (fdIn == -1) {
							perror("open input");
							exit(EXIT_FAILURE);
						} // if
					} else if (strcmp(IO[i], ">") == 0) { // redirect OUTPUT to file
									      // get the filename to redirect output to (IO[i+1])
						i++;
						outFile = IO[i];

						// backup the fg for stdout
						saveOut = dup(STDOUT_FILENO);
						if (saveOut == -1) {
							perror("dup stdout");
							exit(EXIT_FAILURE);
						} // if

						// open the file
						fdOut = creat(outFile, 0644);
						if (fdOut == -1) {
							perror("creat");
							exit(EXIT_FAILURE);
						} // if
					} else if (strcmp(IO[i], ">>") == 0) { // redirect OUTPUT to file in APPEND mode
									       // get the filename to redirect output to (IO[i+1]
						i++;
						outFile = IO[i];

						// backup the fg for stdout
						saveOut = dup(STDOUT_FILENO);
						if (saveOut == -1) {
							perror("dup stdout");
							exit(EXIT_FAILURE);
						} // if

						// open the file
						fdOut = open(outFile, O_WRONLY | O_CREAT | O_APPEND, 0644);
						if (fdOut == -1) {
							perror("open");
							exit(EXIT_FAILURE);
						} // if
					} // if
				} // for

			} // if
			  // Lab 06 TODO: if the command is "exit", quit the program

			if (!strcmp(commandLineArgs[0], "exit")) exit(EXIT_SUCCESS);


			// Project 3 TODO: else if the command is "cd", then use chdir(2) to
			// to support change directory functionalities

			else if (!strcmp(commandLineArgs[0], "cd")) {
				if (commandLineArgs[1] == NULL) chdir(getenv("HOME"));
				else if (!strcmp(commandLineArgs[1], "~")) chdir(getenv("HOME"));
				else chdir(commandLineArgs[1]);
				isChangeDirectory = 1;
			} // if

			// Lab 06 TODO: for all other commands, fork a child process and let
			// the child process execute user-specified command with its options/arguments.
			// NOTE: fork() only needs to be called once. DO NOT CALL fork() more than one time.

			if (!isChangeDirectory) {
				if ((pid = fork()) < 0) {
					perror("fork");
					exit(EXIT_FAILURE);
				} else if (pid == 0) { // child process

					// Lab 07 TODO: inside the child process, use dup2(2) to redirect
					// standard input and output as specified by the user command
					if (isIORedirected) {
						if (fdIn != -1) {
							if (dup2(fdIn, STDIN_FILENO) == -1) perror("dup2 fdIn");
						} // if

						if (fdOut != -1)  {
							if (dup2(fdOut, STDOUT_FILENO) == -1) perror("dup2 fdOut");
						} // if
					} // if

					// Lab 06 TODO: inside the child process, invoke execvp().
					// if execvp() returns -1, be sure to use exit(EXIT_FAILURE);
					// to terminate the child process

					if (execvp(commandLineArgs[0], commandLineArgs) == -1) {
						perror("execvp");
						exit(EXIT_FAILURE);
					} // if

					exit(EXIT_SUCCESS);

				} else { // parent process

					// Lab 06 TODO: inside the parent process, wait for the child process
					// You are not required to do anything special with the child's 
					// termination status

					if (fdOut == -1) {

					} // if
					  // close all files
					if (fdIn != -1) {
						if (close(fdIn) == -1) {
							perror("close fdIn");
							exit(EXIT_FAILURE);
						}
						fdIn = -1;
					} else { // if stdin was changed, set it back to its default value of STDIN_FILENO
						dup2(STDIN_FILENO, fdIn);
					} // if

					if (fdOut != -1) {
						if (close(fdOut) == -1) {
							perror("close fdOut");
							exit(EXIT_FAILURE);
						}
						fdOut = -1;
					} else { // if stdout was changed, set it back to its default value of STDOUT_FILENO
						dup2(STDOUT_FILENO, fdOut);
					} // if

					int status;
					wait(&status);
				} // if
			} else {
				isChangeDirectory = 0;
			} // if
		} // if
	} // while
} // main
