#include "tester.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef struct s_runtime
{
	const char	*checker;
	bool		valgrind_ok;
}t_runtime;

static const char	*detect_checker_path(void)
{
	const char	*candidates[] = {
		CHECKER_PATH_1,
		CHECKER_PATH_2,
		CHECKER_PATH_3,
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

static int	count_tokens(const char *s)
{
	int	count;
	int	in_token;

	if (!s)
		return (0);
	count = 0;
	in_token = 0;
	while (*s)
	{
		if (*s == ' ' || *s == '\t' || *s == '\n')
			in_token = 0;
		else if (!in_token)
		{
			in_token = 1;
			count++;
		}
		s++;
	}
	return (count);
}

static int	estimate_input_size(const char *const *args)
{
	int	argc;

	argc = args_count(args);
	if (argc == 1)
		return (count_tokens(args[0]));
	return (argc);
}

static bool	judge_logic(t_expect expect, const t_exec_result *res)
{
	if (!res)
		return (false);
	if (res->terminated_by_signal)
		return (false);
	if (expect == EXPECT_NONE)
		return (res->exit_code == 0
			&& res->stdout_data && res->stdout_data[0] == '\0'
			&& res->stderr_data && res->stderr_data[0] == '\0');
	if (expect == EXPECT_ERROR)
		return ((res->exit_code != 0)
			|| str_contains(res->stderr_data, "Error")
			|| str_contains(res->stdout_data, "Error"));
	return (res->exit_code == 0
		&& !str_contains(res->stderr_data, "Error")
		&& !str_contains(res->stdout_data, "Error"));
}

static void	run_one_case(const t_test_case *tc, const t_runtime *runtime,
		t_stats *stats)
{
	t_exec_result	res;
	t_sort_status	sort_st;
	t_leak_status	leak_st;
	bool			logic_ok;
	bool			pass;
	int			moves;
	int			score;
	char			msg[64];

	sort_st = SORT_SKIP;
	leak_st = LEAK_SKIP;
	moves = 0;
	score = 0;
	if (exec_capture_with_timeout(BIN_PUSH_SWAP, tc->args,
			DEFAULT_TIMEOUT_SEC, &res) < 0)
	{
		stats->total++;
		stats->fail++;
		ui_print_row(tc->name, false, SORT_SKIP, LEAK_TOOL_ERR, 0, 0);
		return ;
	}
	logic_ok = judge_logic(tc->expect, &res);
	if (tc->expect == EXPECT_VALID)
	{
		moves = count_moves(res.stdout_data);
		if (runtime->checker)
		{
			if (judge_with_checker(runtime->checker, tc->args,
					res.stdout_data, &sort_st, msg, sizeof(msg)) < 0)
				sort_st = SORT_SKIP;
		}
		score = compute_score(estimate_input_size(tc->args), moves);
	}
	if (runtime->valgrind_ok)
	{
		if (judge_with_valgrind(BIN_PUSH_SWAP, tc->args,
				DEFAULT_TIMEOUT_SEC, &leak_st) < 0)
			leak_st = LEAK_TOOL_ERR;
	}
	pass = logic_ok;
	if (tc->expect == EXPECT_VALID && sort_st == SORT_KO)
		pass = false;
	if (runtime->valgrind_ok && leak_st != LEAK_CLEAN)
		pass = false;
	stats->total++;
	if (pass)
		stats->pass++;
	else
		stats->fail++;
	if (leak_st == LEAK_LEAK)
		stats->leak++;
	if (res.timeout)
		stats->timeout++;
	ui_print_row(tc->name, pass, sort_st, leak_st, moves, score);
	exec_result_destroy(&res);
}

static void	run_random_suite(const t_runtime *runtime, t_stats *stats,
		int *progress_current, int progress_total)
{
	int			set[RANDOM_N_500];
	int			i;
	char		*joined;
	const char	*args[2];
	t_test_case	tc;
	t_progress	bar;
	char		name[64];
	int			sizes[2];
	int			s;

	sizes[0] = RANDOM_N_100;
	sizes[1] = RANDOM_N_500;
	i = 0;
	while (i < RANDOM_SUITE_CASES)
	{
		s = 0;
		while (s < 2)
		{
			if (generate_unique_random_set(set, sizes[s], -50000, 50000) == 0)
			{
				joined = join_ints_as_args(set, sizes[s]);
				if (joined)
				{
					args[0] = joined;
					args[1] = NULL;
					snprintf(name, sizeof(name), "Random n=%d #%d", sizes[s], i + 1);
					tc.name = name;
					tc.expect = EXPECT_VALID;
					tc.args = args;
					run_one_case(&tc, runtime, stats);
					free(joined);
				}
			}
			(*progress_current)++;
			bar.current = *progress_current;
			bar.total = progress_total;
			bar.width = UI_PROGRESS_WIDTH;
			ui_progress_draw(&bar, "Testler");
			s++;
		}
		i++;
	}
}

int	main(void)
{
	const t_test_case	*manual_cases;
	size_t				manual_count;
	size_t				i;
	t_runtime			runtime;
	t_stats				stats;
	t_progress			bar;
	int					progress_total;
	int					progress_current;

	if (access(BIN_PUSH_SWAP, X_OK) != 0)
		return (fprintf(stderr, "%s[ERROR]%s %s bulunamadı\n",
				CLR_FAIL, CLR_RESET, BIN_PUSH_SWAP), 1);
	runtime.checker = detect_checker_path();
	runtime.valgrind_ok = (access("/usr/bin/valgrind", X_OK) == 0
			|| access("/bin/valgrind", X_OK) == 0);
	if (!runtime.valgrind_ok)
		return (fprintf(stderr, "%s[ERROR]%s valgrind bulunamadı\n",
				CLR_FAIL, CLR_RESET), 1);
	srand((unsigned int)time(NULL));
	memset(&stats, 0, sizeof(stats));
	manual_count = manual_cases_get(&manual_cases);
	progress_total = (int)manual_count + (RANDOM_SUITE_CASES * 2);
	progress_current = 0;
	printf("\n%sPush Swap Tester%s | checker: %s | valgrind: ON\n",
		CLR_BOLD, CLR_RESET, runtime.checker ? runtime.checker : "SKIP");
	ui_print_header();
	i = 0;
	while (i < manual_count)
	{
		run_one_case(&manual_cases[i], &runtime, &stats);
		progress_current++;
		bar.current = progress_current;
		bar.total = progress_total;
		bar.width = UI_PROGRESS_WIDTH;
		ui_progress_draw(&bar, "Testler");
		i++;
	}
	run_random_suite(&runtime, &stats, &progress_current, progress_total);
	ui_print_footer(&stats);
	if (!runtime.checker)
		printf("%sNot:%s checker bulunamadı, SORT alanı SKIP görünebilir.\n",
			CLR_MUTED, CLR_RESET);
	return (stats.fail != 0);
}
