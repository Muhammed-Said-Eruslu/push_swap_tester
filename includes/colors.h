#ifndef COLORS_H
#define COLORS_H

#define CLR_RESET "\033[0m"
#define CLR_BOLD "\033[1m"
#define CLR_DIM "\033[2m"
#define CLR_ITALIC "\033[3m"
#define CLR_UNDERLINE "\033[4m"

#define CLR_BLACK "\033[1;30m"
#define CLR_RED "\033[1;31m"
#define CLR_GREEN "\033[1;32m"
#define CLR_YELLOW "\033[1;33m"
#define CLR_BLUE "\033[1;34m"
#define CLR_MAGENTA "\033[1;35m"
#define CLR_CYAN "\033[1;36m"
#define CLR_WHITE "\033[1;37m"
#define CLR_GRAY "\033[0;90m"

#define CLR_PASS CLR_GREEN
#define CLR_FAIL CLR_RED
#define CLR_LEAK CLR_YELLOW
#define CLR_INFO CLR_CYAN
#define CLR_WARN CLR_YELLOW
#define CLR_MUTED CLR_GRAY

#define SYM_PASS "✔"
#define SYM_FAIL "✘"
#define SYM_PENDING "⏳"
#define SYM_LEAK "⚠"
#define SYM_ARROW "➜"

#endif
