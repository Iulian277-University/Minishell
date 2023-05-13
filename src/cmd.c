// SPDX-License-Identifier: BSD-3-Clause

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <unistd.h>

#include "cmd.h"
#include "utils.h"

/**
 * Internal change-directory command.
 */
static bool shell_cd(word_t *dir)
{
	/* Execute cd. */
	if (chdir(dir->string) == -1)
		return false;
	return true;
}

/**
 * Internal exit/quit command.
 */
static int shell_exit(void)
{
	/* Execute exit/quit. */
	exit(0);
}

/**
 * Parse a simple command (internal, environment variable assignment,
 * external command).
 */
static int parse_simple(simple_command_t *s, int level, command_t *father)
{
	/* Sanity checks. */
	if (s->verb == NULL)
		return 0;

	/* If builtin command, execute the command. */
	if (strcmp(s->verb->string, "exit") == 0 || strcmp(s->verb->string, "quit") == 0) {
		DIE(s->params, "exit: Too many arguments\n");
		return shell_exit();
	}

	if (strcmp(s->verb->string, "cd") == 0) {
		if (!s->params)
			return 0;

		/* Write output to `s->out` */
		if (s->out != NULL) {
			int stdout_cpy = dup(STDOUT_FILENO);
			int fd = open(s->out->string, O_WRONLY | O_CREAT | O_TRUNC, 0644);

			DIE(fd == -1, "open");
			dup2(fd, STDOUT_FILENO);
			close(fd);

			/* Restore `stdout` */
			dup2(stdout_cpy, STDOUT_FILENO);
			close(stdout_cpy);
		}

		/* Perform the `cd` command */
		if (shell_cd(s->params))
			return 0;
		return 1;
	}

	/* If variable assignment, execute the assignment and return
	 * the exit status.
	 */
	if (s->verb != NULL && s->params == NULL && strchr(get_word(s->verb), '=') != NULL) {
		/* Get the variable name and value */
		const char *var_name = s->verb->string;
		char *var_value = NULL;

		/* Check if there is a value assigned to the variable */
		if (s->verb->next_part != NULL && s->verb->next_part->next_part != NULL)
			var_value = get_word(s->verb->next_part->next_part);

		/* Set the variable `var_name` to the `var_value` value */
		setenv(var_name, var_value, 1);

		return 0;
	}

	/* If external command:
	 *   1. Fork new process
	 *     2c. Perform redirections in child
	 *     3c. Load executable in child
	 *   2. Wait for child
	 *   3. Return exit status
	 */
	/* Extract command and arguments */
	int argc;
	char *command = get_word(s->verb);
	char **argv = get_argv(s, &argc);

	/* Extract redirections */
	char *out_redir = NULL, *err_redir = NULL;

	if (s->out != NULL)
		out_redir = get_word(s->out);
	if (s->err != NULL)
		err_redir = get_word(s->err);

	int fd, status;
	int out_flags = 0, err_flags = 0;

	pid_t pid = fork();

	switch (pid) {
	case -1:
		/* Error */
		DIE(1, "fork");
		break;
	case 0:
		/* Child process */
		if (s->in != NULL) {
			fd = open(s->in->string, O_RDONLY);
			DIE(fd == -1, "open");
			dup2(fd, STDIN_FILENO);
			close(fd);
		}

		/* If `s->out` and `s->err` are the same: "command &> file" */
		if (s->out != NULL && s->err != NULL && strcmp(out_redir, err_redir) == 0) {
			out_flags = O_WRONLY | O_CREAT;
			if (s->io_flags & IO_OUT_APPEND)
				out_flags |= O_APPEND;
			else
				out_flags |= O_TRUNC;
			fd = open(out_redir, out_flags, 0644);
			DIE(fd == -1, "open");
			dup2(fd, STDOUT_FILENO);
			dup2(fd, STDERR_FILENO);
			close(fd);
		} else { /* Different redirections for `stdout` and `stderr` */
			/* Set `stdout` */
			out_flags = O_WRONLY | O_CREAT;
			if (s->io_flags & IO_OUT_APPEND)
				out_flags |= O_APPEND;
			else
				out_flags |= O_TRUNC;
			if (s->out != NULL) {
				fd = open(out_redir, out_flags, 0644);
				DIE(fd == -1, "open");
				dup2(fd, STDOUT_FILENO);
				close(fd);
			}

			/* Set `stderr` */
			err_flags = O_WRONLY | O_CREAT;
			if (s->io_flags & IO_ERR_APPEND)
				err_flags |= O_APPEND;
			else
				err_flags |= O_TRUNC;
			if (s->err != NULL) {
				fd = open(err_redir, err_flags, 0644);
				DIE(fd == -1, "open");
				dup2(fd, STDERR_FILENO);
				close(fd);
			}
		}

		/* Execute the `command` with `argv` */
		execvp(command, argv);
		fprintf(stderr, "Execution failed for '%s'\n", command);
		exit(EXIT_FAILURE);
		break;
	default:
		/* Parent process */
		waitpid(pid, &status, 0);
		if (WIFEXITED(status))
			return WEXITSTATUS(status);
		return 1;
	}

	return 0;
}

/**
 * Process two commands in parallel, by creating two children.
 */
static bool run_in_parallel(command_t *cmd1, command_t *cmd2, int level, command_t *father)
{
	/* Execute cmd1 and cmd2 simultaneously. */
	pid_t pid1, pid2;
	int status1, status2;

	pid1 = fork();
	switch (pid1) {
	case -1:
		/* Error */
		DIE(1, "fork");
		break;
	case 0:
		/* Child process 1 */
		exit(parse_command(cmd1, level + 1, father));
		break;
	default:
		/* Parent process */
		pid2 = fork();
		switch (pid2) {
		case -1:
			/* Error */
			DIE(1, "fork");
			break;
		case 0:
			/* Child process 2 */
			exit(parse_command(cmd2, level + 1, father));
			break;
		default:
			/* Parent process */
			waitpid(pid1, &status1, 0);
			waitpid(pid2, &status2, 0);
			if (WIFEXITED(status1) && WIFEXITED(status2))
				return WEXITSTATUS(status1) && WEXITSTATUS(status2);
			return true;
		}
		break;
	}

	return false;
}


/**
 * Run commands by creating an anonymous pipe (cmd1 | cmd2).
 */
static bool run_on_pipe(command_t *cmd1, command_t *cmd2, int level, command_t *father)
{
	/* Redirect the output of cmd1 to the input of cmd2. */
	int pipefd[2];
	pid_t pid1, pid2;
	int status1, status2;
	bool success = false;

	if (pipe(pipefd) == -1)
		DIE(1, "pipe");

	pid1 = fork();

	switch (pid1) {
	case -1:
		/* Error */
		DIE(1, "fork");
		break;
	
	case 0:
		/* Child process 1 */
		close(pipefd[PIPE_READ]);
		dup2(pipefd[PIPE_WRITE], STDOUT_FILENO);
		close(pipefd[PIPE_WRITE]);
		success = parse_command(cmd1, level + 1, father);
		exit(success ? EXIT_SUCCESS : EXIT_FAILURE);
		break;

	default:
		/* Parent process */
		pid2 = fork();
		switch (pid2) {
		case -1:
			/* Error */
			DIE(1, "fork");
			break;

		case 0:
			/* Child process 2 */
			close(pipefd[PIPE_WRITE]);
			dup2(pipefd[PIPE_READ], STDIN_FILENO);
			close(pipefd[PIPE_READ]);
			success = parse_command(cmd2, level + 1, father);
			exit(success ? EXIT_SUCCESS : EXIT_FAILURE);
			break;

		default:
			/* Parent process */
			close(pipefd[PIPE_READ]);
			close(pipefd[PIPE_WRITE]);
			
			waitpid(pid1, &status1, 0);
			if (!WIFEXITED(status1) || !WEXITSTATUS(status1))
				success = false;

			waitpid(pid2, &status2, 0);
			if (!WIFEXITED(status2) || !WEXITSTATUS(status2))
				success = false;

			return success;
		}
		break;
	}

	return success;
}

/**
 * Parse and execute a command.
 */
int parse_command(command_t *c, int level, command_t *father)
{
	/* Sanity checks */
	if (c == NULL)
		return 0;

	if (c->op == OP_NONE) {
		/* Execute a simple command. */
		return parse_simple(c->scmd, level, father);
	}

	switch (c->op) {
	case OP_SEQUENTIAL:
		/* Execute the commands one after the other. */
		parse_command(c->cmd1, level + 1, c);
		return parse_command(c->cmd2, level + 1, c);

	case OP_PARALLEL:
		/* Execute the commands simultaneously. */
		return run_in_parallel(c->cmd1, c->cmd2, level, c);

	case OP_CONDITIONAL_NZERO:
		/* Execute the second command only if the first one returns non zero. (||) */
		if (parse_command(c->cmd1, level + 1, c) != 0)
			return parse_command(c->cmd2, level + 1, c);
		return 0;

	case OP_CONDITIONAL_ZERO:
		/* Execute the second command only if the first one returns zero. (&&) */
		if (parse_command(c->cmd1, level + 1, c) == 0)
			return parse_command(c->cmd2, level + 1, c);
		return 0;

	case OP_PIPE:
		/* Redirect the output of the first command to the input of the second. */
		return run_on_pipe(c->cmd1, c->cmd2, level, father);

	default:
		return SHELL_EXIT;
	}

	return 0;
}
