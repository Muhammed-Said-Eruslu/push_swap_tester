#include "tester.h"

#include <stdio.h>
#include <unistd.h>

void	ui_print_header(void)
{
	printf("\n%s+----+------------------------------+--------+--------+--------+-------+--------+%s\n",
		CLR_INFO, CLR_RESET);
	printf("%s| #  | TEST                         | RESULT | SORT   | LEAK   | CHARS | SCORE  |%s\n",
		CLR_INFO, CLR_RESET);
	printf("%s+----+------------------------------+--------+--------+--------+-------+--------+%s\n",
		CLR_INFO, CLR_RESET);
}

void	ui_print_row(const char *name, bool pass, t_sort_status sort_status,
		t_leak_status leak_status, int input_chars, int moves, int score)
{
	const char	*symbol;
	const char	*res_clr;
	const char	*res_txt;
	const char	*sort_clr;
	const char	*sort_txt;
	const char	*leak_clr;
	const char	*leak_txt;
	char		score_buf[16];
	char		char_buf[16];

	( void )moves;
	symbol = pass ? SYM_PASS : SYM_FAIL;

	res_clr = pass ? CLR_PASS : CLR_FAIL;
	res_txt = pass ? "PASS" : "FAIL";

	if (sort_status == SORT_OK) { sort_clr = CLR_PASS; sort_txt = "OK"; }
	else if (sort_status == SORT_KO) { sort_clr = CLR_FAIL; sort_txt = "KO"; }
	else { sort_clr = CLR_MUTED; sort_txt = "SKIP"; }

	if (leak_status == LEAK_CLEAN) { leak_clr = CLR_PASS; leak_txt = "CLEAN"; }
	else if (leak_status == LEAK_LEAK) { leak_clr = CLR_LEAK; leak_txt = "LEAK"; }
	else if (leak_status == LEAK_CRASH) { leak_clr = CLR_FAIL; leak_txt = "CRASH"; }
	else if (leak_status == LEAK_SKIP) { leak_clr = CLR_MUTED; leak_txt = "SKIP"; }
	else { leak_clr = CLR_WARN; leak_txt = "TOOL"; }

	if (score > 0)
		snprintf(score_buf, sizeof(score_buf), "%d/5", score);
	else
		snprintf(score_buf, sizeof(score_buf), "-");

	if (input_chars >= 0)
		snprintf(char_buf, sizeof(char_buf), "%d", input_chars);
	else
		snprintf(char_buf, sizeof(char_buf), "-");

	printf("\033[2K\r| %-2s | %-28.28s | %s%-6s%s | %s%-6s%s | %s%-6s%s | %-5s | %-6s |\n",
		symbol, name,
		res_clr, res_txt, CLR_RESET,
		sort_clr, sort_txt, CLR_RESET,
		leak_clr, leak_txt, CLR_RESET,
		char_buf, score_buf);
}

void	ui_print_footer(const t_stats *stats)
{
	printf("%s+----+------------------------------+--------+--------+--------+-------+--------+%s\n",
		CLR_INFO, CLR_RESET);
	printf("%sSummary%s total:%d pass:%d fail:%d leak:%d timeout:%d\n",
		CLR_BOLD, CLR_RESET,
		stats->total, stats->pass, stats->fail, stats->leak, stats->timeout);
}

void	ui_progress_draw(const t_progress *progress, const char *label)
{
	int	filled;
	int	i;
	int	percent;
	char	spinner;
	const char	*frames;

	if (!progress || progress->total <= 0)
		return ;
	frames = "|/-\\";
	spinner = frames[progress->current % 4];
	percent = (progress->current * 100) / progress->total;
	filled = (progress->current * progress->width) / progress->total;
	printf("\r%s%c %s%s [", CLR_CYAN, spinner,
		label ? label : "Running", CLR_RESET);
	i = 0;
	while (i < progress->width)
	{
		if (i < filled)
			printf("█");
		else
			printf("░");
		i++;
	}
	printf("] %3d%% (%d/%d) %sPASS:%d%s %sFAIL:%d%s",
		percent, progress->current, progress->total,
		CLR_PASS, progress->pass, CLR_RESET,
		CLR_FAIL, progress->fail, CLR_RESET);
	fflush(stdout);
	if (progress->current >= progress->total)
		printf("\n");
}

void	ui_success_animation(void)
{
	int	i;

	i = 0;
	while (i < 3)
	{
		printf("\r%s✨ Tests completed successfully ✨%s", CLR_PASS, CLR_RESET);
		fflush(stdout);
		usleep(140000);
		printf("\r%s🚀 Push Swap is ready!            %s", CLR_PASS, CLR_RESET);
		fflush(stdout);
		usleep(140000);
		i++;
	}
	printf("\n%s"
		"  ____   _    ____  ____\n"
		" |  _ \\ / \\  / ___|/ ___|\n"
		" | |_) / _ \\ \\___ \\___ \\\n"
		" |  __/ ___ \\ ___) |__) |\n"
		" |_| /_/   \\_\\____/____/\n"
		"%s",
		CLR_PASS, CLR_RESET);
}

void	ui_print_final_status(bool success, const t_stats *stats)
{
	if (success)
	{
		printf("%s[%s]%s All tests passed (%d/%d).\n",
			CLR_PASS, SYM_PASS, CLR_RESET, stats->pass, stats->total);
		ui_success_animation();
		return ;
	}
	printf("%s[%s]%s Errors found in tests. Passed: %d | Failed: %d\n",
		CLR_FAIL, SYM_FAIL, CLR_RESET,
		stats->pass, stats->fail);
}
