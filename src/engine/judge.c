#include "tester.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int	run_checker_with_ops(const char *checker, const char *const *args,
		const char *ops, t_exec_result *res)
{
	int		in_pipe[2];
	int		out_pipe[2];
	int		err_pipe[2];
	pid_t	pid;
	char	**argv;
	int		argc;
	int		i;

	argc = args_count(args);
	argv = calloc((size_t)argc + 2, sizeof(char *));
	if (!argv)
		return (-1);
	argv[0] = (char *)checker;
	i = 0;
	while (i < argc)
	{
		argv[i + 1] = (char *)args[i];
		i++;
	}
	argv[argc + 1] = NULL;
	if (pipe(in_pipe) < 0 || pipe(out_pipe) < 0 || pipe(err_pipe) < 0)
		return (free(argv), -1);
	pid = fork();
	if (pid < 0)
		return (free(argv), -1);
	if (pid == 0)
	{
		dup2(in_pipe[0], STDIN_FILENO);
		dup2(out_pipe[1], STDOUT_FILENO);
		dup2(err_pipe[1], STDERR_FILENO);
		close(in_pipe[0]);
		close(in_pipe[1]);
		close(out_pipe[0]);
		close(out_pipe[1]);
		close(err_pipe[0]);
		close(err_pipe[1]);
		execvp(checker, argv);
		perror("execvp checker");
		exit(127);
	}
	close(in_pipe[0]);
	close(out_pipe[1]);
	close(err_pipe[1]);
	if (ops && *ops)
		write(in_pipe[1], ops, strlen(ops));
	close(in_pipe[1]);
	res->stdout_data = NULL;
	res->stderr_data = NULL;
	res->exit_code = 0;
	res->signal_number = 0;
	res->terminated_by_signal = false;
	res->timeout = false;
	res->stdout_data = malloc(1);
	res->stderr_data = malloc(1);
	if (!res->stdout_data || !res->stderr_data)
		return (free(argv), -1);
	res->stdout_data[0] = '\0';
	res->stderr_data[0] = '\0';
	free(res->stdout_data);
	free(res->stderr_data);
	res->stdout_data = NULL;
	res->stderr_data = NULL;
	{
		char	buf[4096];
		ssize_t rd;
		size_t total;

		res->stdout_data = malloc(1);
		res->stderr_data = malloc(1);
		if (!res->stdout_data || !res->stderr_data)
			return (free(argv), -1);
		res->stdout_data[0] = '\0';
		res->stderr_data[0] = '\0';
		total = 0;
		while ((rd = read(out_pipe[0], buf, sizeof(buf))) > 0)
		{
			char *next = realloc(res->stdout_data, total + (size_t)rd + 1);
			if (!next)
				return (free(argv), -1);
			res->stdout_data = next;
			memcpy(res->stdout_data + total, buf, (size_t)rd);
			total += (size_t)rd;
			res->stdout_data[total] = '\0';
		}
		total = 0;
		while ((rd = read(err_pipe[0], buf, sizeof(buf))) > 0)
		{
			char *next = realloc(res->stderr_data, total + (size_t)rd + 1);
			if (!next)
				return (free(argv), -1);
			res->stderr_data = next;
			memcpy(res->stderr_data + total, buf, (size_t)rd);
			total += (size_t)rd;
			res->stderr_data[total] = '\0';
		}
	}
	close(out_pipe[0]);
	close(err_pipe[0]);
	if (waitpid(pid, &res->exit_code, 0) < 0)
		return (free(argv), -1);
	if (WIFEXITED(res->exit_code))
		res->exit_code = WEXITSTATUS(res->exit_code);
	else if (WIFSIGNALED(res->exit_code))
	{
		res->terminated_by_signal = true;
		res->signal_number = WTERMSIG(res->exit_code);
		res->exit_code = 128 + res->signal_number;
	}
	free(argv);
	return (0);
}

int	count_moves(const char *ops_output)
{
	int	count;

	if (!ops_output || !*ops_output)
		return (0);
	count = 0;
	while (*ops_output)
	{
		if (*ops_output == '\n')
			count++;
		ops_output++;
	}
	return (count);
}

int	judge_with_checker(const char *checker_path, const char *const *args,
		const char *ops_output, t_sort_status *out_status,
		char *msg_buf, size_t msg_cap)
{
	t_exec_result	res;
	char			*trim;
	char			*nl;

	if (!checker_path || !out_status)
		return (-1);
	if (run_checker_with_ops(checker_path, args, ops_output, &res) < 0)
		return (-1);
	trim = str_trim_ws(res.stdout_data);
	if (!trim)
	{
		exec_result_destroy(&res);
		return (-1);
	}
	nl = strchr(trim, '\n');
	if (nl)
		*nl = '\0';
	*out_status = SORT_KO;
	if (strcmp(trim, "OK") == 0)
		*out_status = SORT_OK;
	if (msg_buf && msg_cap > 0)
		snprintf(msg_buf, msg_cap, "%s", trim[0] ? trim : "<empty>");
	free(trim);
	exec_result_destroy(&res);
	return (0);
}

int	judge_with_valgrind(const char *target_path, const char *const *args,
		int timeout_sec, t_leak_status *out_status)
{
	const char	*vg_args[512];
	int			argc;
	int			i;
	t_exec_result	res;

	if (!target_path || !out_status)
		return (-1);
	argc = args_count(args);
	if (argc > 500)
		return (-1);
	vg_args[0] = "--leak-check=full";
	vg_args[1] = "--show-leak-kinds=all";
	vg_args[2] = "--track-origins=yes";
	vg_args[3] = "--errors-for-leak-kinds=all";
	vg_args[4] = "--error-exitcode=101";
	vg_args[5] = "--";
	vg_args[6] = target_path;
	i = 0;
	while (i < argc)
	{
		vg_args[7 + i] = args[i];
		i++;
	}
	vg_args[7 + argc] = NULL;
	if (exec_capture_with_timeout("valgrind", vg_args, timeout_sec, &res) < 0)
		return (-1);
	*out_status = LEAK_CLEAN;
	if (res.terminated_by_signal)
		*out_status = LEAK_CRASH;
	else if (res.exit_code == VALGRIND_ERROR_EXIT)
		*out_status = LEAK_LEAK;
	else if (!str_contains(res.stderr_data, "ERROR SUMMARY: 0 errors")
		|| (!str_contains(res.stderr_data,
			"All heap blocks were freed -- no leaks are possible")
			&& !str_contains(res.stderr_data,
			"definitely lost: 0 bytes in 0 blocks")))
		*out_status = LEAK_LEAK;
	exec_result_destroy(&res);
	return (0);
}

int	compute_score(int size, int move_count)
{
	if (move_count < 0)
		return (0);
	if (size <= 100)
	{
		if (move_count < SCORE_100_LIMIT)
			return (5);
		if (move_count < 900)
			return (4);
		if (move_count < 1100)
			return (3);
		return (1);
	}
	if (size <= 500)
	{
		if (move_count < SCORE_500_LIMIT)
			return (5);
		if (move_count < 7000)
			return (4);
		if (move_count < 8500)
			return (3);
		return (1);
	}
	return (0);
}

const char	*sort_status_text(t_sort_status st)
{
	if (st == SORT_OK)
		return ("OK");
	if (st == SORT_KO)
		return ("KO");
	return ("SKIP");
}

const char	*leak_status_text(t_leak_status st)
{
	if (st == LEAK_CLEAN)
		return ("CLEAN");
	if (st == LEAK_LEAK)
		return ("LEAK");
	if (st == LEAK_CRASH)
		return ("CRASH");
	if (st == LEAK_SKIP)
		return ("SKIP");
	return ("TOOL_ERR");
}
