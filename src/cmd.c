// SPDX-License-Identifier: BSD-3-Clause

#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <unistd.h>

#include "cmd.h"
#include "utils.h"

#define READ		0
#define WRITE		1

/**
 * Internal change-directory command.
 */
static bool shell_cd(word_t *dir)
{
	/* TODO: Execute cd. */
	if (chdir(dir->string) == -1) {
		fprintf(stderr, "[cd]: %s: No such file or directory\n", dir->string);
		return false;
	}

	return true;
}

/**
 * Internal exit/quit command.
 */
static int shell_exit(void)
{
	/* TODO: Execute exit/quit. */
	exit(0);
}

/**
 * Parse a simple command (internal, environment variable assignment,
 * external command).
 */
static int parse_simple(simple_command_t *s, int level, command_t *father)
{
	/* TODO: Sanity checks. */
	if (s->verb == NULL) {
		return 0;
	}

	/* TODO: If builtin command, execute the command. */
	if (strcmp(s->verb->string, "exit") == 0 || strcmp(s->verb->string, "quit") == 0) {
		if (s->params != NULL) {
			fprintf(stderr, "[exit/quit]: Too many arguments\n");
			return 1;
		}

		return shell_exit();
	}

	if (strcmp(s->verb->string, "cd") == 0) {
		if (s->params == NULL) {
			fprintf(stderr, "[cd]: Too few arguments\n");
			return 1;
		}

		return shell_cd(s->params);
	}

	/* TODO: If variable assignment, execute the assignment and return
	 * the exit status.
	 */

	/* TODO: If external command:
	 *   1. Fork new process
	 *     2c. Perform redirections in child
	 *     3c. Load executable in child
	 *   2. Wait for child
	 *   3. Return exit status
	 */
	
	int fd;
	int status;

	pid_t pid = fork();
	switch (pid) {
		case -1:
			// Error
			DIE(1, "fork");
			break;
		case 0:
			// Child process
			if (s->in != NULL) {
				fd = open(s->in->string, O_RDONLY);
				if (fd == -1) {
					fprintf(stderr, "[open]: %s: No such file or directory\n", s->in->string);
					exit(1);
				}
				dup2(fd, STDIN_FILENO);
				close(fd);
			}

			if (s->out != NULL) {
				fd = open(s->out->string, O_WRONLY | O_CREAT | O_TRUNC, 0644);
				if (fd == -1) {
					fprintf(stderr, "[open]: %s: No such file or directory\n", s->out->string);
					exit(1);
				}
				dup2(fd, STDOUT_FILENO);
				close(fd);
			}

			if (s->err != NULL) {
				fd = open(s->err->string, O_WRONLY | O_CREAT | O_TRUNC, 0644);
				if (fd == -1) {
					fprintf(stderr, "[open]: %s: No such file or directory\n", s->err->string);
					exit(1);
				}
				dup2(fd, STDERR_FILENO);
				close(fd);
			}

			execvp(s->verb->string, (char *const *)s->params);
			fprintf(stderr, "[execvp]: %s: No such file or directory\n", s->verb->string);
			exit(1);
			break;
		default:
			// Parent process
			waitpid(pid, &status, 0);
			if (WIFEXITED(status)) {
				return WEXITSTATUS(status);
			}
			return 1;
			break;
	}

	return 0;
}

/**
 * Process two commands in parallel, by creating two children.
 */
static bool run_in_parallel(command_t *cmd1, command_t *cmd2, int level,
		command_t *father)
{
	/* TODO: Execute cmd1 and cmd2 simultaneously. */

	return true; /* TODO: Replace with actual exit status. */
}

/**
 * Run commands by creating an anonymous pipe (cmd1 | cmd2).
 */
static bool run_on_pipe(command_t *cmd1, command_t *cmd2, int level,
		command_t *father)
{
	/* TODO: Redirect the output of cmd1 to the input of cmd2. */

	return true; /* TODO: Replace with actual exit status. */
}

/**
 * Parse and execute a command.
 */
int parse_command(command_t *c, int level, command_t *father)
{
	/* TODO: sanity checks */
	if (c == NULL) {
		return 0;
	}

	if (c->op == OP_NONE) {
		/* TODO: Execute a simple command. */
		return parse_simple(c->scmd, level, father);
	}

	switch (c->op) {
		case OP_SEQUENTIAL:
			/* TODO: Execute the commands one after the other. */
			break;

		case OP_PARALLEL:
			/* TODO: Execute the commands simultaneously. */
			break;

		case OP_CONDITIONAL_NZERO:
			/* TODO: Execute the second command only if the first one
			* returns non zero.
			*/
			break;

		case OP_CONDITIONAL_ZERO:
			/* TODO: Execute the second command only if the first one
			* returns zero.
			*/
			break;

		case OP_PIPE:
			/* TODO: Redirect the output of the first command to the
			* input of the second.
			*/
			break;

		default:
			return SHELL_EXIT;
	}

	return 0; /* TODO: Replace with actual exit code of command. */
}