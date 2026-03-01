#include "tester.h"

#include <stdio.h>
#include <unistd.h>

static const char	*sort_cell(t_sort_status st)
{
	if (st == SORT_OK)
		return (CLR_PASS "OK" CLR_RESET);
	if (st == SORT_KO)
		return (CLR_FAIL "KO" CLR_RESET);
	return (CLR_MUTED "SKIP" CLR_RESET);
}

static const char	*leak_cell(t_leak_status st)
{
	if (st == LEAK_CLEAN)
		return (CLR_PASS "CLEAN" CLR_RESET);
	if (st == LEAK_LEAK)
		return (CLR_LEAK "LEAK" CLR_RESET);
	if (st == LEAK_CRASH)
		return (CLR_FAIL "CRASH" CLR_RESET);
	if (st == LEAK_SKIP)
		return (CLR_MUTED "SKIP" CLR_RESET);
	return (CLR_WARN "TOOL" CLR_RESET);
}

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
	const char	*result;
	char		score_buf[16];
	char		char_buf[16];

	( void )moves;
	symbol = pass ? SYM_PASS : SYM_FAIL;
	result = pass ? CLR_PASS "PASS" CLR_RESET : CLR_FAIL "FAIL" CLR_RESET;
	if (score > 0)
		snprintf(score_buf, sizeof(score_buf), "%d/5", score);
	else
		snprintf(score_buf, sizeof(score_buf), "-");
	if (input_chars >= 0)
		snprintf(char_buf, sizeof(char_buf), "%d", input_chars);
	else
		snprintf(char_buf, sizeof(char_buf), "-");
	printf("\n| %-2s | %-28.28s | %-6s | %-6s | %-6s | %-5s | %-6s |\n",
		symbol, name, result, sort_cell(sort_status), leak_cell(leak_status),
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
		printf("\r%s✨ Testler başarıyla tamamlandı ✨%s", CLR_PASS, CLR_RESET);
		fflush(stdout);
		usleep(140000);
		printf("\r%s🚀 Push Swap hazır!            %s", CLR_PASS, CLR_RESET);
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
		printf("%s[%s]%s Tüm testler geçti (%d/%d).\n",
			CLR_PASS, SYM_PASS, CLR_RESET, stats->pass, stats->total);
		ui_success_animation();
		return ;
	}
	printf("%s[%s]%s Testlerde hata var. Geçen: %d | Kalan: %d\n",
		CLR_FAIL, SYM_FAIL, CLR_RESET,
		stats->pass, stats->fail);
}
