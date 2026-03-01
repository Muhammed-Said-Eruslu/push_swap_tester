#include "tester.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define READ_CHUNK 2048
#define OUT_INIT_CAP 4096

static void	append_buf(char **buf, size_t *len, size_t *cap,
		const char *src, size_t add)
{
	char	*newbuf;
	size_t	need;

	need = *len + add + 1;
	if (need > *cap)
	{
		while (*cap < need)
			*cap *= 2;
		newbuf = realloc(*buf, *cap);
		if (!newbuf)
		{
			free(*buf);
			*buf = NULL;
			return ;
		}
		*buf = newbuf;
	}
	memcpy(*buf + *len, src, add);
	*len += add;
	(*buf)[*len] = '\0';
}

static char	*read_fd_all(int fd)
{
	char	*buf;
	char	tmp[READ_CHUNK];
	ssize_t	rd;
	size_t	len;
	size_t	cap;

	cap = OUT_INIT_CAP;
	len = 0;
	buf = malloc(cap);
	if (!buf)
		return (NULL);
	buf[0] = '\0';
	while (1)
	{
		rd = read(fd, tmp, sizeof(tmp));
		if (rd == 0)
			break ;
		if (rd < 0)
		{
			if (errno == EINTR)
				continue ;
			free(buf);
			return (NULL);
		}
		append_buf(&buf, &len, &cap, tmp, (size_t)rd);
		if (!buf)
			return (NULL);
	}
	return (buf);
}

static int	wait_child_status(pid_t pid, t_exec_result *res)
{
	int	status;

	if (waitpid(pid, &status, 0) < 0)
		return (-1);
	res->terminated_by_signal = false;
	res->signal_number = 0;
	res->exit_code = 0;
	res->timeout = false;
	if (WIFEXITED(status))
		res->exit_code = WEXITSTATUS(status);
	else if (WIFSIGNALED(status))
	{
		res->terminated_by_signal = true;
		res->signal_number = WTERMSIG(status);
		res->exit_code = 128 + res->signal_number;
		if (res->signal_number == SIGALRM)
			res->timeout = true;
	}
	return (0);
}

static char	**build_argv(const char *path, const char *const *args)
{
	char	**argv;
	int		argc;
	int		i;

	argc = args_count(args);
	argv = calloc((size_t)argc + 2, sizeof(char *));
	if (!argv)
		return (NULL);
	argv[0] = (char *)path;
	i = 0;
	while (i < argc)
	{
		argv[i + 1] = (char *)args[i];
		i++;
	}
	argv[argc + 1] = NULL;
	return (argv);
}

int	exec_capture_with_timeout(const char *path, const char *const *args,
		int timeout_sec, t_exec_result *out)
{
	int		out_pipe[2];
	int		err_pipe[2];
	pid_t	pid;
	char	**argv;

	if (!path || !out)
		return (-1);
	memset(out, 0, sizeof(*out));
	argv = build_argv(path, args);
	if (!argv)
		return (-1);
	if (pipe(out_pipe) < 0 || pipe(err_pipe) < 0)
		return (free(argv), -1);
	pid = fork();
	if (pid < 0)
		return (free(argv), -1);
	if (pid == 0)
	{
		dup2(out_pipe[1], STDOUT_FILENO);
		dup2(err_pipe[1], STDERR_FILENO);
		close(out_pipe[0]);
		close(out_pipe[1]);
		close(err_pipe[0]);
		close(err_pipe[1]);
		if (timeout_sec > 0)
			alarm((unsigned int)timeout_sec);
		execvp(path, argv);
		perror("execvp");
		exit(127);
	}
	close(out_pipe[1]);
	close(err_pipe[1]);
	out->stdout_data = read_fd_all(out_pipe[0]);
	out->stderr_data = read_fd_all(err_pipe[0]);
	close(out_pipe[0]);
	close(err_pipe[0]);
	free(argv);
	if (!out->stdout_data || !out->stderr_data)
		return (exec_result_destroy(out), -1);
	if (wait_child_status(pid, out) < 0)
		return (exec_result_destroy(out), -1);
	return (0);
}

void	exec_result_destroy(t_exec_result *result)
{
	if (!result)
		return ;
	free(result->stdout_data);
	free(result->stderr_data);
	result->stdout_data = NULL;
	result->stderr_data = NULL;
	result->exit_code = 0;
	result->terminated_by_signal = false;
	result->signal_number = 0;
	result->timeout = false;
}
