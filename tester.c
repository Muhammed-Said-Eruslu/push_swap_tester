#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define CLR_GREEN "\033[1;32m"
#define CLR_RED "\033[1;31m"
#define CLR_CYAN "\033[1;36m"
#define CLR_YELLOW "\033[1;33m"
#define CLR_GRAY "\033[0;90m"
#define CLR_RESET "\033[0m"

#define OUT_INIT_CAP 4096

typedef enum e_expect
{
	EXPECT_NONE,
	EXPECT_ERROR,
	EXPECT_VALID
} t_expect;

typedef struct s_test
{
	const char	*name;
	t_expect	expect;
	const char	**args;
} t_test;

typedef struct s_exec_result
{
	char	*stdout_data;
	char	*stderr_data;
	int		exit_code;
	bool	terminated_by_signal;
	int		signal_number;
} t_exec_result;

typedef enum e_sort_status
{
	SORT_OK,
	SORT_KO,
	SORT_SKIP
} t_sort_status;

typedef enum e_leak_status
{
	LEAK_CLEAN,
	LEAK_LEAK,
	LEAK_CRASH,
	LEAK_TOOL_ERR
} t_leak_status;

typedef struct s_case_outcome
{
	bool		logic_ok;
	t_sort_status	sort_status;
	t_leak_status	leak_status;
} t_case_outcome;

typedef struct s_stats
{
	int	total;
	int	pass;
	int	fail;
	int	random_total;
	int	random_pass;
	int	random_fail;
	int	worst_total;
	int	worst_pass;
	int	worst_fail;
} t_stats;

static bool	str_contains(const char *s, const char *needle)
{
	if (!s || !needle)
		return (false);
	return (strstr(s, needle) != NULL);
}

static int	line_count(const char *s)
{
	int	count;

	if (!s || !*s)
		return (0);
	count = 0;
	while (*s)
	{
		if (*s == '\n')
			count++;
		s++;
	}
	if (*(s - 1) != '\n')
		count++;
	return (count);
}

static const char	*leak_status_text(t_leak_status st)
{
	if (st == LEAK_CLEAN)
		return ("CLEAN");
	if (st == LEAK_LEAK)
		return ("LEAK");
	if (st == LEAK_CRASH)
		return ("CRASH");
	return ("TOOL_ERR");
}

static const char	*sort_status_text(t_sort_status st)
{
	if (st == SORT_OK)
		return ("OK");
	if (st == SORT_KO)
		return ("KO");
	return ("SKIP");
}

static void	append_text(char *dst, size_t cap, const char *src)
{
	size_t	cur;
	size_t	left;

	if (!dst || cap == 0 || !src)
		return ;
	cur = strlen(dst);
	if (cur >= cap - 1)
		return ;
	left = cap - 1 - cur;
	strncat(dst, src, left);
}

static void	build_args_preview(const char **args, char *out, size_t cap)
{
	int	idx;

	if (!out || cap == 0)
		return ;
	out[0] = '\0';
	if (!args || !args[0])
	{
		append_text(out, cap, "<none>");
		return ;
	}
	idx = 0;
	while (args[idx])
	{
		if (idx > 0)
			append_text(out, cap, " ");
		append_text(out, cap, "[");
		append_text(out, cap, args[idx]);
		append_text(out, cap, "]");
		idx++;
	}
}

static void	preview_output(const char *src, char *dst, size_t cap)
{
	size_t	idx;
	size_t	w;

	if (!dst || cap == 0)
		return ;
	dst[0] = '\0';
	if (!src || src[0] == '\0')
	{
		append_text(dst, cap, "<empty>");
		return ;
	}
	idx = 0;
	w = 0;
	while (src[idx] && w + 1 < cap)
	{
		if (src[idx] == '\n' || src[idx] == '\t')
			dst[w++] = ' ';
		else
			dst[w++] = src[idx];
		idx++;
		if (w >= 180)
			break ;
	}
	dst[w] = '\0';
	if (src[idx])
		append_text(dst, cap, " ...");
}

static int	args_count(const char **args)
{
	int	count;

	count = 0;
	if (!args)
		return (0);
	while (args[count])
		count++;
	return (count);
}

static char	*dup_or_empty(const char *s)
{
	if (!s)
		return (strdup(""));
	return (strdup(s));
}

static void	append_buf(char **buf, size_t *len, size_t *cap, const char *src, size_t add)
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
	char	tmp[2048];
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
	if (WIFEXITED(status))
		res->exit_code = WEXITSTATUS(status);
	else if (WIFSIGNALED(status))
	{
		res->terminated_by_signal = true;
		res->signal_number = WTERMSIG(status);
		res->exit_code = 128 + res->signal_number;
	}
	return (0);
}

static int	run_with_capture(char *const argv[], t_exec_result *res)
{
	int		out_pipe[2];
	int		err_pipe[2];
	pid_t	pid;

	memset(res, 0, sizeof(*res));
	if (pipe(out_pipe) < 0 || pipe(err_pipe) < 0)
		return (-1);
	pid = fork();
	if (pid < 0)
		return (-1);
	if (pid == 0)
	{
		dup2(out_pipe[1], STDOUT_FILENO);
		dup2(err_pipe[1], STDERR_FILENO);
		close(out_pipe[0]);
		close(out_pipe[1]);
		close(err_pipe[0]);
		close(err_pipe[1]);
		execvp(argv[0], argv);
		perror("execvp");
		exit(127);
	}
	close(out_pipe[1]);
	close(err_pipe[1]);
	res->stdout_data = read_fd_all(out_pipe[0]);
	res->stderr_data = read_fd_all(err_pipe[0]);
	close(out_pipe[0]);
	close(err_pipe[0]);
	if (!res->stdout_data || !res->stderr_data)
		return (-1);
	if (wait_child_status(pid, res) < 0)
		return (-1);
	return (0);
}

static int	run_checker_with_ops(const char *checker, const char **args,
		const char *ops, t_exec_result *res)
{
	int		in_pipe[2];
	int		out_pipe[2];
	int		err_pipe[2];
	pid_t	pid;
	int		argc;
	char	**argv;
	int		i;

	memset(res, 0, sizeof(*res));
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
		execvp(argv[0], argv);
		perror("execvp checker");
		exit(127);
	}
	close(in_pipe[0]);
	close(out_pipe[1]);
	close(err_pipe[1]);
	if (ops && *ops)
		write(in_pipe[1], ops, strlen(ops));
	close(in_pipe[1]);
	res->stdout_data = read_fd_all(out_pipe[0]);
	res->stderr_data = read_fd_all(err_pipe[0]);
	close(out_pipe[0]);
	close(err_pipe[0]);
	free(argv);
	if (!res->stdout_data || !res->stderr_data)
		return (-1);
	if (wait_child_status(pid, res) < 0)
		return (-1);
	return (0);
}

static const char	*detect_checker_path(void)
{
	const char	*candidates[] = {
		"./checker",
		"./checker_linux",
		"./checker_Mac",
		NULL
	};
	int			idx;

	idx = 0;
	while (candidates[idx])
	{
		if (access(candidates[idx], X_OK) == 0)
			return (candidates[idx]);
		idx++;
	}
	return (NULL);
}

static int	run_push_swap(const char *bin, const char **args, t_exec_result *res)
{
	int		argc;
	char	**argv;
	int		i;

	argc = args_count(args);
	argv = calloc((size_t)argc + 2, sizeof(char *));
	if (!argv)
		return (-1);
	argv[0] = (char *)bin;
	i = 0;
	while (i < argc)
	{
		argv[i + 1] = (char *)args[i];
		i++;
	}
	argv[argc + 1] = NULL;
	i = run_with_capture(argv, res);
	free(argv);
	return (i);
}

static t_sort_status	judge_sort_status(const char *checker, const char **args,
		const char *ops)
{
	t_exec_result	cr;
	char			*trim;
	char			*newline;
	t_sort_status	status;
	char			**filtered;
	int			argc;
	int			i;

	if (!checker)
		return (SORT_SKIP);
	argc = args_count(args);
	filtered = calloc((size_t)argc + 1, sizeof(char *));
	if (!filtered)
		return (SORT_SKIP);
	i = 0;
	while (i < argc)
	{
		if (!(args[i][0] == '-' && args[i][1] == '-'))
			filtered[i] = (char *)args[i];
		else
			filtered[i] = NULL;
		i++;
	}
	/* compact filtered list into a contiguous argv-style array */
	i = 0;
	{
		int j = 0;
		while (i < argc)
		{
			if (filtered[i])
				filtered[j++] = filtered[i];
			i++;
		}
		filtered[j] = NULL;
	}

	if (run_checker_with_ops(checker, (const char **)filtered, ops, &cr) < 0)
	{
		free(filtered);
		return (SORT_SKIP);
	}
	free(filtered);
	trim = dup_or_empty(cr.stdout_data);
	free(cr.stdout_data);
	free(cr.stderr_data);
	if (!trim)
		return (SORT_SKIP);
	newline = strchr(trim, '\n');
	if (newline)
		*newline = '\0';
	status = SORT_KO;
	if (strcmp(trim, "OK") == 0)
		status = SORT_OK;
	else if (strcmp(trim, "KO") == 0)
		status = SORT_KO;
	free(trim);
	return (status);
}

static t_leak_status	run_leak_check(const char *bin, const char **args)
{
	t_exec_result	vr;
	int			argc;
	char			**argv;
	int			i;

	argc = args_count(args);
	argv = calloc((size_t)argc + 12, sizeof(char *));
	if (!argv)
		return (LEAK_TOOL_ERR);
	argv[0] = "valgrind";
	argv[1] = "--leak-check=full";
	argv[2] = "--show-leak-kinds=all";
	argv[3] = "--track-origins=yes";
	argv[4] = "--errors-for-leak-kinds=all";
	argv[5] = "--error-exitcode=101";
	argv[6] = "--";
	argv[7] = (char *)bin;
	i = 0;
	while (i < argc)
	{
		argv[8 + i] = (char *)args[i];
		i++;
	}
	argv[8 + argc] = NULL;
	if (run_with_capture(argv, &vr) < 0)
		return (free(argv), LEAK_TOOL_ERR);
	free(argv);
	if (vr.terminated_by_signal)
		return (free(vr.stdout_data), free(vr.stderr_data), LEAK_CRASH);
	if (vr.exit_code == 101)
		return (free(vr.stdout_data), free(vr.stderr_data), LEAK_LEAK);
	if (!str_contains(vr.stderr_data, "ERROR SUMMARY: 0 errors"))
		return (free(vr.stdout_data), free(vr.stderr_data), LEAK_TOOL_ERR);
	if (!str_contains(vr.stderr_data,
			"All heap blocks were freed -- no leaks are possible")
		&& !str_contains(vr.stderr_data, "definitely lost: 0 bytes in 0 blocks"))
		return (free(vr.stdout_data), free(vr.stderr_data), LEAK_LEAK);
	free(vr.stdout_data);
	free(vr.stderr_data);
	return (LEAK_CLEAN);
}

static bool	judge_logic(const t_test *tc, const t_exec_result *pr)
{
	if (pr->terminated_by_signal)
		return (false);
	if (tc->expect == EXPECT_NONE)
		return (pr->exit_code == 0
			&& pr->stdout_data[0] == '\0'
			&& pr->stderr_data[0] == '\0');
	if (tc->expect == EXPECT_ERROR)
		return (pr->exit_code != 0 && str_contains(pr->stderr_data, "Error"));
	return (pr->exit_code == 0 && !str_contains(pr->stderr_data, "Error"));
}

static const char	*logic_label(bool logic_ok)
{
	if (logic_ok)
		return (CLR_GREEN "OK" CLR_RESET);
	return (CLR_RED "KO" CLR_RESET);
}

static const char	*sort_label(t_sort_status st)
{
	if (st == SORT_OK)
		return (CLR_GREEN "OK" CLR_RESET);
	if (st == SORT_KO)
		return (CLR_RED "KO" CLR_RESET);
	return (CLR_GRAY "SKIP" CLR_RESET);
}

static const char	*leak_label(t_leak_status st)
{
	if (st == LEAK_CLEAN)
		return (CLR_GREEN "CLEAN" CLR_RESET);
	if (st == LEAK_LEAK)
		return (CLR_RED "LEAK" CLR_RESET);
	if (st == LEAK_CRASH)
		return (CLR_RED "CRASH" CLR_RESET);
	return (CLR_YELLOW "TOOL_ERR" CLR_RESET);
}

static const char	*detect_strategy(const char **args)
{
	int			idx;
	const char	*strategy;

	strategy = "adaptive";
	if (!args)
		return (strategy);
	idx = 0;
	while (args[idx])
	{
		if (strcmp(args[idx], "--simple") == 0)
			strategy = "simple";
		else if (strcmp(args[idx], "--medium") == 0)
			strategy = "medium";
		else if (strcmp(args[idx], "--complex") == 0)
			strategy = "complex";
		else if (strcmp(args[idx], "--adaptive") == 0)
			strategy = "adaptive";
		idx++;
	}
	return (strategy);
}

static void	print_failure_detail(const t_test *tc, const t_exec_result *pr,
		const t_case_outcome *oc, const char *strategy)
{
	char	args_preview[512];
	char	stdout_preview[256];
	char	stderr_preview[256];

	build_args_preview(tc->args, args_preview, sizeof(args_preview));
	preview_output(pr->stdout_data, stdout_preview, sizeof(stdout_preview));
	preview_output(pr->stderr_data, stderr_preview, sizeof(stderr_preview));
	printf("    %sReason:%s ", CLR_YELLOW, CLR_RESET);
	if (!oc->logic_ok)
	{
		if (tc->expect == EXPECT_NONE)
			printf("EXPECT_NONE ihlali (çıkış/çıktı beklenmiyordu). ");
		else if (tc->expect == EXPECT_ERROR)
			printf("EXPECT_ERROR ihlali (Error veya non-zero exit yok). ");
		else
			printf("EXPECT_VALID ihlali (non-zero exit veya Error görüldü). ");
	}
	if (tc->expect == EXPECT_VALID && oc->sort_status == SORT_KO)
		printf("Checker sonucu KO (üretilen hamleler sıralamıyor). ");
	if (oc->leak_status != LEAK_CLEAN)
		printf("Leak durumu %s. ", leak_status_text(oc->leak_status));
	printf("\n");
	printf("    %sDetails:%s algo=%s expected=%s exit=%d signal=%s sort=%s leak=%s\n",
		CLR_YELLOW, CLR_RESET,
		strategy,
		tc->expect == EXPECT_NONE ? "NONE" :
		(tc->expect == EXPECT_ERROR ? "ERROR" : "VALID"),
		pr->exit_code,
		pr->terminated_by_signal ? "yes" : "no",
		sort_status_text(oc->sort_status),
		leak_status_text(oc->leak_status));
	printf("    %sInput:%s %s\n", CLR_YELLOW, CLR_RESET, args_preview);
	printf("    %sStdout:%s %s\n", CLR_YELLOW, CLR_RESET, stdout_preview);
	printf("    %sStderr:%s %s\n", CLR_YELLOW, CLR_RESET, stderr_preview);
}

static bool	final_case_pass(const t_test *tc, const t_case_outcome *oc)
{
	if (!oc->logic_ok)
		return (false);
	if (tc->expect == EXPECT_VALID && oc->sort_status == SORT_KO)
		return (false);
	if (oc->leak_status != LEAK_CLEAN)
		return (false);
	return (true);
}

static void	print_banner(const char *checker)
{
	printf("\n%s===============================================%s\n",
		CLR_YELLOW, CLR_RESET);
	printf("%s      PUSH_SWAP PROFESSIONAL C TESTER         %s\n",
		CLR_YELLOW, CLR_RESET);
	printf("%s===============================================%s\n",
		CLR_YELLOW, CLR_RESET);
	printf("%sBinary:%s ./push_swap\n", CLR_CYAN, CLR_RESET);
	if (checker)
		printf("%sChecker:%s %s\n", CLR_CYAN, CLR_RESET, checker);
	else
		printf("%sChecker:%s bulunamadı (sort sonucu SKIP)\n", CLR_CYAN, CLR_RESET);
	printf("%sLeak:%s valgrind --leak-check=full\n\n", CLR_CYAN, CLR_RESET);
}

static void	run_single_test(const char *bin, const char *checker, const t_test *tc,
		t_stats *stats)
{
	t_exec_result	pr;
	t_case_outcome	oc;
	bool			pass;
	int			moves;
	const char	*strategy;

	stats->total++;
	if (run_push_swap(bin, tc->args, &pr) < 0)
	{
		printf("[%sFAIL%s] %-30s | çalıştırma hatası\n", CLR_RED, CLR_RESET,
			tc->name);
		stats->fail++;
		return ;
	}
	oc.logic_ok = judge_logic(tc, &pr);
	oc.sort_status = SORT_SKIP;
	if (tc->expect == EXPECT_VALID)
		oc.sort_status = judge_sort_status(checker, tc->args, pr.stdout_data);
	oc.leak_status = run_leak_check(bin, tc->args);
	pass = final_case_pass(tc, &oc);
	moves = line_count(pr.stdout_data);
	strategy = detect_strategy(tc->args);
	printf("[%s%s%s] %-30s | algo:%s logic:%s sort:%s leak:%s moves:%d\n",
		pass ? CLR_GREEN : CLR_RED,
		pass ? "PASS" : "FAIL",
		CLR_RESET,
		tc->name,
		strategy,
		logic_label(oc.logic_ok),
		sort_label(oc.sort_status),
		leak_label(oc.leak_status),
		moves);
	if (pass)
		stats->pass++;
	else
	{
		stats->fail++;
		print_failure_detail(tc, &pr, &oc, strategy);
	}
	free(pr.stdout_data);
	free(pr.stderr_data);
}

static void	shuffle_ints(int *arr, int n)
{
	int	i;
	int	j;
	int	tmp;

	i = n - 1;
	while (i > 0)
	{
		j = rand() % (i + 1);
		tmp = arr[i];
		arr[i] = arr[j];
		arr[j] = tmp;
		i--;
	}
}

static char	**build_random_args(int n)
{
	int		*pool;
	char	**args;
	int		i;
	char	buf[32];

	pool = malloc(sizeof(int) * n);
	args = calloc((size_t)n + 1, sizeof(char *));
	if (!pool || !args)
		return (free(pool), free(args), NULL);
	i = 0;
	while (i < n)
	{
		pool[i] = i - (n / 2);
		i++;
	}
	shuffle_ints(pool, n);
	i = 0;
	while (i < n)
	{
		snprintf(buf, sizeof(buf), "%d", pool[i]);
		args[i] = strdup(buf);
		if (!args[i])
		{
			while (i-- > 0)
				free(args[i]);
			free(args);
			free(pool);
			return (NULL);
		}
		i++;
	}
	args[n] = NULL;
	free(pool);
	return (args);
}

static void	free_args(char **args)
{
	int	i;

	if (!args)
		return ;
	i = 0;
	while (args[i])
	{
		free(args[i]);
		i++;
	}
	free(args);
}

static char	*itoa_dup(int value)
{
	char	buf[32];

	snprintf(buf, sizeof(buf), "%d", value);
	return (strdup(buf));
}

static char	**build_reverse_args(int n)
{
	char	**args;
	int		i;

	args = calloc((size_t)n + 1, sizeof(char *));
	if (!args)
		return (NULL);
	i = 0;
	while (i < n)
	{
		args[i] = itoa_dup(n - i);
		if (!args[i])
			return (free_args(args), NULL);
		i++;
	}
	return (args);
}

static char	**build_rotated_args(int n)
{
	char	**args;
	int		i;
	int		shift;
	int		value;

	args = calloc((size_t)n + 1, sizeof(char *));
	if (!args)
		return (NULL);
	shift = n / 3;
	i = 0;
	while (i < n)
	{
		value = ((i + shift) % n) + 1;
		args[i] = itoa_dup(value);
		if (!args[i])
			return (free_args(args), NULL);
		i++;
	}
	return (args);
}

static char	**build_nearly_sorted_args(int n)
{
	char	**args;
	int		i;
	int		mid;

	args = calloc((size_t)n + 1, sizeof(char *));
	if (!args)
		return (NULL);
	i = 0;
	while (i < n)
	{
		args[i] = itoa_dup(i + 1);
		if (!args[i])
			return (free_args(args), NULL);
		i++;
	}
	if (n > 3)
	{
		mid = n / 2;
		free(args[mid]);
		free(args[mid + 1]);
		args[mid] = itoa_dup(mid + 2);
		args[mid + 1] = itoa_dup(mid + 1);
		if (!args[mid] || !args[mid + 1])
			return (free_args(args), NULL);
	}
	return (args);
}

static char	**build_sawtooth_args(int n)
{
	char	**args;
	int		i;
	int		lo;
	int		hi;
	int		value;

	args = calloc((size_t)n + 1, sizeof(char *));
	if (!args)
		return (NULL);
	lo = 1;
	hi = n;
	i = 0;
	while (i < n)
	{
		if (i % 2 == 0)
			value = hi--;
		else
			value = lo++;
		args[i] = itoa_dup(value);
		if (!args[i])
			return (free_args(args), NULL);
		i++;
	}
	return (args);
}

static char	**build_flagged_args_view(const char **base_args, const char *flag)
{
	char	**view;
	int		argc;
	int		i;

	argc = args_count(base_args);
	view = calloc((size_t)argc + (flag ? 2 : 1), sizeof(char *));
	if (!view)
		return (NULL);
	i = 0;
	if (flag)
		view[i++] = (char *)flag;
	while (base_args && *base_args)
		view[i++] = (char *)*base_args++;
	view[i] = NULL;
	return (view);
}

static void	run_worst_case(const char *bin, const char *checker, t_stats *stats,
		const char *label, int n, char **base_args, const char *flag)
{
	t_test			tc;
	t_exec_result	pr;
	t_case_outcome	oc;
	char			**flagged_args;
	bool			pass;
	bool			captured;
	char			name[128];
	const char	*strategy;

	if (!base_args)
	{
		stats->worst_total++;
		stats->worst_fail++;
		return ;
	}
	flagged_args = build_flagged_args_view((const char **)base_args, flag);
	if (!flagged_args)
	{
		free_args(base_args);
		stats->worst_total++;
		stats->worst_fail++;
		return ;
	}
	if (flag)
		snprintf(name, sizeof(name), "Worst %s n=%d [%s]", label, n, flag + 2);
	else
		snprintf(name, sizeof(name), "Worst %s n=%d [adaptive]", label, n);
	tc = (t_test){name, EXPECT_VALID, (const char **)flagged_args};
	strategy = detect_strategy(tc.args);
	captured = false;
	if (run_push_swap(bin, tc.args, &pr) < 0)
	{
		pass = false;
		printf("[%sFAIL%s] %-30s | algo:%s çalıştırma hatası\n",
			CLR_RED, CLR_RESET, name, strategy);
	}
	else
	{
		captured = true;
		oc.logic_ok = judge_logic(&tc, &pr);
		oc.sort_status = judge_sort_status(checker, tc.args, pr.stdout_data);
		oc.leak_status = run_leak_check(bin, tc.args);
		pass = final_case_pass(&tc, &oc);
		printf("[%s%s%s] %-30s | algo:%s logic:%s sort:%s leak:%s moves:%d\n",
			pass ? CLR_GREEN : CLR_RED,
			pass ? "PASS" : "FAIL",
			CLR_RESET,
			name,
			strategy,
			logic_label(oc.logic_ok),
			sort_label(oc.sort_status),
			leak_label(oc.leak_status),
			line_count(pr.stdout_data));
	}
	stats->worst_total++;
	if (pass)
		stats->worst_pass++;
	else
	{
		stats->worst_fail++;
		if (captured)
			print_failure_detail(&tc, &pr, &oc, strategy);
	}
	if (captured)
	{
		free(pr.stdout_data);
		free(pr.stderr_data);
	}
	free(flagged_args);
	free_args(base_args);
}

static void	run_random_suite(const char *bin, const char *checker, t_stats *stats)
{
	const char	*flags[] = {NULL, "--simple", "--medium", "--complex"};
	t_test		tc;
	t_exec_result	pr;
	t_case_outcome	oc;
	char		**dyn_args;
	char		**flagged_args;
	int			idx;
	int			f;
	bool			pass;
	const char	*strategy;

	printf("\n%s-- Random valid suite (checker + leak) --%s\n", CLR_CYAN, CLR_RESET);
	f = 0;
	while (f < 4)
	{
		printf("%sStrategy:%s %s\n", CLR_CYAN, CLR_RESET,
			flags[f] ? flags[f] + 2 : "adaptive");
		idx = 1;
		while (idx <= 10)
		{
			dyn_args = build_random_args(20);
			if (!dyn_args)
				break ;
			flagged_args = build_flagged_args_view((const char **)dyn_args,
					flags[f]);
			if (!flagged_args)
			{
				free_args(dyn_args);
				stats->random_total++;
				stats->random_fail++;
				idx++;
				continue ;
			}
			tc.name = "Random20";
			tc.expect = EXPECT_VALID;
			tc.args = (const char **)flagged_args;
			if (run_push_swap(bin, tc.args, &pr) < 0)
			{
				printf("[%sFAIL%s] Random20 #%d | algo:%s çalıştırma hatası\n",
					CLR_RED, CLR_RESET, idx,
					flags[f] ? flags[f] + 2 : "adaptive");
				stats->random_total++;
				stats->random_fail++;
				free(flagged_args);
				free_args(dyn_args);
				idx++;
				continue ;
			}
			oc.logic_ok = judge_logic(&tc, &pr);
			oc.sort_status = judge_sort_status(checker, tc.args, pr.stdout_data);
			oc.leak_status = run_leak_check(bin, tc.args);
			pass = final_case_pass(&tc, &oc);
			strategy = detect_strategy(tc.args);
			printf("[%s%s%s] Random20 #%d | algo:%s logic:%s sort:%s leak:%s moves:%d\n",
				pass ? CLR_GREEN : CLR_RED,
				pass ? "PASS" : "FAIL",
				CLR_RESET,
				idx,
				strategy,
				logic_label(oc.logic_ok),
				sort_label(oc.sort_status),
				leak_label(oc.leak_status),
				line_count(pr.stdout_data));
			stats->random_total++;
			if (pass)
				stats->random_pass++;
			else
			{
				stats->random_fail++;
				print_failure_detail(&tc, &pr, &oc, strategy);
			}
			free(pr.stdout_data);
			free(pr.stderr_data);
			free(flagged_args);
			free_args(dyn_args);
			idx++;
		}
		f++;
	}
}

static void	run_worst_suite(const char *bin, const char *checker, t_stats *stats)
{
	int			sizes[] = {10, 50, 100, 250, 500, 0};
	const char	*flags[] = {NULL, "--simple", "--medium", "--complex"};
	int			i;
	int			f;

	printf("\n%s-- Worst-case suite (edge stress + leak) --%s\n", CLR_CYAN,
		CLR_RESET);
	f = 0;
	while (f < 4)
	{
		printf("%sStrategy:%s %s\n", CLR_CYAN, CLR_RESET,
			flags[f] ? flags[f] + 2 : "adaptive");
		i = 0;
		while (sizes[i])
		{
			run_worst_case(bin, checker, stats, "reverse", sizes[i],
				build_reverse_args(sizes[i]), flags[f]);
			run_worst_case(bin, checker, stats, "near-sorted", sizes[i],
				build_nearly_sorted_args(sizes[i]), flags[f]);
			run_worst_case(bin, checker, stats, "rotated", sizes[i],
				build_rotated_args(sizes[i]), flags[f]);
			run_worst_case(bin, checker, stats, "sawtooth", sizes[i],
				build_sawtooth_args(sizes[i]), flags[f]);
			i++;
		}
		f++;
	}
}

static const char	*ARGS_EMPTY[] = {NULL};
static const char	*ARGS_ONE[] = {"42", NULL};
static const char	*ARGS_SORTED[] = {"1", "2", "3", "4", "5", NULL};
static const char	*ARGS_REVERSED[] = {"5", "4", "3", "2", "1", NULL};
static const char	*ARGS_MIXED_SIGN[] = {"-10", "0", "7", "-3", "2", NULL};
static const char	*ARGS_SINGLE_STR[] = {"3 2 1 0 -1", NULL};
static const char	*ARGS_PLUS[] = {"+1", "+2", "0", "-1", NULL};
static const char	*ARGS_SIMPLE_FLAG[] = {"--simple", "3", "2", "1", NULL};
static const char	*ARGS_BENCH_ONLY[] = {"--bench", NULL};

static const char	*ARGS_DUP[] = {"1", "2", "2", NULL};
static const char	*ARGS_DUP_STR[] = {"1 2 3 2", NULL};
static const char	*ARGS_ALPHA[] = {"1", "a", "3", NULL};
static const char	*ARGS_SIGN_ONLY_P[] = {"+", NULL};
static const char	*ARGS_SIGN_ONLY_M[] = {"-", NULL};
static const char	*ARGS_EMPTY_STR[] = {"", NULL};
static const char	*ARGS_SPACES[] = {"   ", NULL};
static const char	*ARGS_TAB[] = {"1\t2", NULL};
static const char	*ARGS_OVER_MAX[] = {"2147483648", NULL};
static const char	*ARGS_OVER_MIN[] = {"-2147483649", NULL};
static const char	*ARGS_UNKNOWN_FLAG[] = {"--unknown", NULL};
static const char	*ARGS_MIX_FMT[] = {"1", "2", "3 4", "5", NULL};
static const char	*ARGS_LIMITS[] = {"-2147483648", "2147483647", "0", NULL};
static const char	*ARGS_SPACED_SINGLE[] = {"   4    3   2   1   ", NULL};
static const char	*ARGS_LEADING_ZERO[] = {"0003", "0002", "0001", NULL};
static const char	*ARGS_MULTI_SIGN_P1[] = {"++1", NULL};
static const char	*ARGS_MULTI_SIGN_M1[] = {"--1", NULL};
static const char	*ARGS_MULTI_SIGN_PM[] = {"+-1", NULL};
static const char	*ARGS_MULTI_SIGN_MP[] = {"-+1", NULL};
static const char	*ARGS_DUP_ZERO_FMT[] = {"0", "-0", NULL};
static const char	*ARGS_DUP_PADDED[] = {"3", "03", NULL};
static const char	*ARGS_OVER_MAX_STR[] = {"1 2 2147483648", NULL};
static const char	*ARGS_OVER_MIN_STR[] = {"1 2 -2147483649", NULL};
static const char	*ARGS_BAD_CHUNK[] = {"1", "2", "3+4", "5", NULL};

static const t_test	g_tests[] = {
	{"No args", EXPECT_NONE, ARGS_EMPTY},
	{"Single number", EXPECT_NONE, ARGS_ONE},
	{"Already sorted", EXPECT_NONE, ARGS_SORTED},
	{"Reverse order", EXPECT_VALID, ARGS_REVERSED},
	{"Mixed sign", EXPECT_VALID, ARGS_MIXED_SIGN},
	{"Single-arg list", EXPECT_VALID, ARGS_SINGLE_STR},
	{"Plus sign numbers", EXPECT_VALID, ARGS_PLUS},
	{"Simple flag", EXPECT_VALID, ARGS_SIMPLE_FLAG},
	{"Bench only", EXPECT_NONE, ARGS_BENCH_ONLY},
	{"Mixed format", EXPECT_VALID, ARGS_MIX_FMT},
	{"Int min/max boundary", EXPECT_VALID, ARGS_LIMITS},
	{"Spaced single input", EXPECT_VALID, ARGS_SPACED_SINGLE},
	{"Leading zero valid", EXPECT_VALID, ARGS_LEADING_ZERO},

	{"Duplicate args", EXPECT_ERROR, ARGS_DUP},
	{"Duplicate string", EXPECT_ERROR, ARGS_DUP_STR},
	{"Alphabetic token", EXPECT_ERROR, ARGS_ALPHA},
	{"Invalid chunk 3+4", EXPECT_ERROR, ARGS_BAD_CHUNK},
	{"Only plus sign", EXPECT_ERROR, ARGS_SIGN_ONLY_P},
	{"Only minus sign", EXPECT_ERROR, ARGS_SIGN_ONLY_M},
	{"Multi sign ++", EXPECT_ERROR, ARGS_MULTI_SIGN_P1},
	{"Multi sign --", EXPECT_ERROR, ARGS_MULTI_SIGN_M1},
	{"Multi sign +-", EXPECT_ERROR, ARGS_MULTI_SIGN_PM},
	{"Multi sign -+", EXPECT_ERROR, ARGS_MULTI_SIGN_MP},
	{"Empty string", EXPECT_ERROR, ARGS_EMPTY_STR},
	{"Spaces only", EXPECT_ERROR, ARGS_SPACES},
	{"Tab inside token", EXPECT_ERROR, ARGS_TAB},
	{"Duplicate 0 and -0", EXPECT_ERROR, ARGS_DUP_ZERO_FMT},
	{"Duplicate padded", EXPECT_ERROR, ARGS_DUP_PADDED},
	{"Overflow max", EXPECT_ERROR, ARGS_OVER_MAX},
	{"Overflow min", EXPECT_ERROR, ARGS_OVER_MIN},
	{"Overflow max in string", EXPECT_ERROR, ARGS_OVER_MAX_STR},
	{"Overflow min in string", EXPECT_ERROR, ARGS_OVER_MIN_STR},
	{"Unknown flag", EXPECT_ERROR, ARGS_UNKNOWN_FLAG}
};

int	main(void)
{
	const char	*bin;
	const char	*checker;
	t_stats		stats;
	size_t		i;

	bin = "./push_swap";
	if (access(bin, X_OK) != 0)
	{
		fprintf(stderr, "%s[ERROR]%s %s bulunamadı veya çalıştırılamıyor\n",
			CLR_RED, CLR_RESET, bin);
		return (1);
	}
	if (access("/usr/bin/valgrind", X_OK) != 0 && access("/bin/valgrind", X_OK) != 0)
	{
		fprintf(stderr, "%s[ERROR]%s valgrind bulunamadı\n", CLR_RED, CLR_RESET);
		return (1);
	}
	checker = detect_checker_path();
	print_banner(checker);
	srand((unsigned int)time(NULL));
	memset(&stats, 0, sizeof(stats));
	i = 0;
	while (i < sizeof(g_tests) / sizeof(g_tests[0]))
	{
		run_single_test(bin, checker, &g_tests[i], &stats);
		i++;
	}
	run_random_suite(bin, checker, &stats);
	run_worst_suite(bin, checker, &stats);
	printf("\n%sSummary:%s total:%d pass:%d fail:%d\n",
		CLR_CYAN, CLR_RESET, stats.total, stats.pass, stats.fail);
	if (stats.random_total > 0)
		printf("%sRandom:%s total:%d pass:%d fail:%d\n",
			CLR_CYAN, CLR_RESET,
			stats.random_total, stats.random_pass, stats.random_fail);
	if (stats.worst_total > 0)
		printf("%sWorst:%s total:%d pass:%d fail:%d\n",
			CLR_CYAN, CLR_RESET,
			stats.worst_total, stats.worst_pass, stats.worst_fail);
	if (!checker)
		printf("%sNot:%s checker yoksa sort doğrulaması SKIP görünür.\n",
			CLR_GRAY, CLR_RESET);
	return (stats.fail != 0 || stats.random_fail != 0 || stats.worst_fail != 0);
}