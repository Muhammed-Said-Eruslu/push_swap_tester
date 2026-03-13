#ifndef TESTER_H
#define TESTER_H

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

#include "colors.h"
#include "config.h"

typedef enum e_expect
{
	EXPECT_NONE,
	EXPECT_ERROR,
	EXPECT_VALID
} t_expect;

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
	LEAK_TOOL_ERR,
	LEAK_SKIP
} t_leak_status;

typedef struct s_test_case
{
	const char *name;
	t_expect expect;
	const char **args;
} t_test_case;

typedef struct s_exec_result
{
	char *stdout_data;
	char *stderr_data;
	int exit_code;
	bool terminated_by_signal;
	int signal_number;
	bool timeout;
} t_exec_result;

typedef struct s_score_rule
{
	int size;
	int max_moves;
	int grade;
} t_score_rule;

typedef struct s_judge_result
{
	bool logic_ok;
	t_sort_status sort_status;
	t_leak_status leak_status;
	int moves;
	int score;
	char checker_msg[64];
} t_judge_result;

typedef struct s_stats
{
	int total;
	int pass;
	int fail;
	int leak;
	int timeout;
} t_stats;

typedef struct s_progress
{
	int current;
	int total;
	int width;
	int pass;
	int fail;
} t_progress;

int exec_capture_with_timeout(const char *path,
							  const char *const *args,
							  int timeout_sec,
							  t_exec_result *out);
void exec_result_destroy(t_exec_result *result);

int count_moves(const char *ops_output);
int judge_with_checker(const char *checker_path,
					   const char *const *args,
					   const char *ops_output,
					   t_sort_status *out_status,
					   char *msg_buf,
					   size_t msg_cap);
int judge_with_valgrind(const char *target_path,
						const char *const *argv,
						int timeout_sec,
						t_leak_status *out_status);
int compute_score(int size, int move_count);
const char *sort_status_text(t_sort_status st);
const char *leak_status_text(t_leak_status st);

void ui_print_header(void);
void ui_print_row(const char *name,
				  bool pass,
				  t_sort_status sort_status,
				  t_leak_status leak_status,
				  int input_chars,
				  int moves,
				  int score);
void ui_print_footer(const t_stats *stats);
void ui_progress_draw(const t_progress *progress, const char *label);
void ui_print_final_status(bool success, const t_stats *stats);
void ui_success_animation(void);

int generate_unique_random_set(int *dst, int count, int min, int max);
char *join_ints_as_args(const int *arr, int count);
char *str_trim_ws(const char *src);
bool str_contains(const char *s, const char *needle);
int args_count(const char *const *args);

size_t manual_cases_get(const t_test_case **out_cases);

#endif
