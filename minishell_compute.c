/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

#include "utils/utils.h"

#define MAX_LINE_SIZE		256
#define MAX_ARGS		8

#define ERROR			0
#define SIMPLE			1
#define REDIRECT		2
#define PIPE			3
#define EXIT_CMD		5

static char *verb;
static char **args;
static char *stdin_file;
static char *stdout_file;
static char *stderr_file;
static char *redirect_file = "redirect.txt";

static int type;


static void alloc_mem(void)
{
	args = malloc(MAX_ARGS * sizeof(char *));
}

static int parse_line(char *line)
{
	int ret = SIMPLE;
	int idx = 0;
	char *token;
	char *delim = "=\n";
	char *saveptr;

	stdin_file = NULL;
	stdout_file = NULL;
	stderr_file = NULL;

	/* Check for exit. */
	if (strncmp("exit", line, strlen("exit")) == 0)
		return EXIT_CMD;

	/* Normal command. */
	delim = " \t\n";
	token = strtok_r(line, delim, &saveptr);

	if (token == NULL)
		return ERROR;

	verb = strdup(token);

	/* Copy args. */
	idx = 0;
	while (token != NULL) {
		if (token == NULL) {
			printf(" Expansion failed\n");
			return ERROR;
		}

		args[idx++] = strdup(token);
		token = strtok_r(NULL, delim, &saveptr);
	}

	args[idx++] = NULL;
	return ret;
}

/**
 *  @args - array that contains a simple command with parameters
 */
static void simple_cmd(char **args)
{
	(void) args;
	pid_t pid, wait_ret;
	int status;
	int fd;

	pid = fork();
	switch (pid) {
		case -1:
			// Error
			DIE(1, "fork");
			break;
		case 0:
			// Child process

			// Redirect the output from stdout to `redirect_file`
			// Open a new file descriptor for `redirect_file`
			fd = open(redirect_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
			DIE(fd < 0, "open");

			// Duplicate the file descriptor for stdout
			int dup_ret = dup2(fd, STDOUT_FILENO);
			DIE(dup_ret < 0, "dup2");

			// Close the file descriptor for `redirect_file`
			int close_ret = close(fd);
			DIE(close_ret < 0, "close");

			// Execute the command
			execvp(args[0], args);
			DIE(1, "execvp");
			break;
		default:
			// Parent process
			wait_ret = waitpid(pid, &status, 0);
			DIE(wait_ret < 0, "waitpid");

			if (WIFEXITED(status))
				printf("Child process (pid %d) terminated normally with exit code=%d\n", pid, WEXITSTATUS(status));
	}
}

int main(void)
{
	char line[MAX_LINE_SIZE];

	alloc_mem();

	while (1) {
		printf("> ");
		fflush(stdout);

		memset(line, 0, MAX_LINE_SIZE);

		if (fgets(line, sizeof(line), stdin) == NULL)
			exit(EXIT_SUCCESS);

		type = parse_line(line);

		switch (type) {
			case EXIT_CMD:
				exit(EXIT_SUCCESS);

			case SIMPLE:
				simple_cmd(args);
				break;
		}
	}

	return 0;
}
