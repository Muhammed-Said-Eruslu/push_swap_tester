#include "tester.h"

static const char	*ARGS_EMPTY[] = {NULL};
static const char	*ARGS_ONE[] = {"42", NULL};
static const char	*ARGS_TWO_SORTED[] = {"1", "2", NULL};
static const char	*ARGS_TWO_REVERSED[] = {"2", "1", NULL};
static const char	*ARGS_SORTED[] = {"1", "2", "3", "4", "5", NULL};
static const char	*ARGS_REVERSED[] = {"5", "4", "3", "2", "1", NULL};
static const char	*ARGS_NEG_ONLY[] = {"-5", "-4", "-3", "-2", "-1", NULL};
static const char	*ARGS_MIXED_SIGN[] = {"-10", "0", "7", "-3", "2", NULL};
static const char	*ARGS_LIMITS[] = {"-2147483648", "2147483647", "0", NULL};
static const char	*ARGS_DUP[] = {"1", "2", "2", NULL};
static const char	*ARGS_DUP_NEG[] = {"-1", "0", "-1", NULL};
static const char	*ARGS_DUP_ZERO_FMT[] = {"0", "-0", NULL};
static const char	*ARGS_DUP_PADDED[] = {"3", "03", NULL};
static const char	*ARGS_ALPHA[] = {"1", "a", "3", NULL};
static const char	*ARGS_ALPHA_STR[] = {"1 a 3", NULL};
static const char	*ARGS_BAD_CHUNK[] = {"1", "2", "3+4", "5", NULL};
static const char	*ARGS_TAB_TOKEN[] = {"1\t2", NULL};
static const char	*ARGS_SPACE_ONLY[] = {"   ", NULL};
static const char	*ARGS_EMPTY_STR[] = {"", NULL};
static const char	*ARGS_SIGN_PLUS_ONLY[] = {"+", NULL};
static const char	*ARGS_SIGN_MINUS_ONLY[] = {"-", NULL};
static const char	*ARGS_MULTI_SIGN_PP[] = {"++1", NULL};
static const char	*ARGS_MULTI_SIGN_MM[] = {"--1", NULL};
static const char	*ARGS_MULTI_SIGN_PM[] = {"+-1", NULL};
static const char	*ARGS_MULTI_SIGN_MP[] = {"-+1", NULL};
static const char	*ARGS_PLUS[] = {"+1", "+2", "0", "-1", NULL};
static const char	*ARGS_PLUS_STR[] = {"+3 +2 +1 0 -1", NULL};
static const char	*ARGS_SINGLE_STR[] = {"3 2 1 0 -1", NULL};
static const char	*ARGS_SINGLE_STR_SORTED[] = {"1 2 3 4 5", NULL};
static const char	*ARGS_SINGLE_STR_SPACED[] = {"   4    3   2   1   ", NULL};
static const char	*ARGS_MIX_FMT[] = {"1", "2", "3 4", "5", NULL};
static const char	*ARGS_LEADING_ZERO[] = {"0003", "0002", "0001", NULL};
static const char	*ARGS_OVER_MAX[] = {"2147483648", NULL};
static const char	*ARGS_OVER_MIN[] = {"-2147483649", NULL};
static const char	*ARGS_OVER_MAX_STR[] = {"1 2 2147483648", NULL};
static const char	*ARGS_OVER_MIN_STR[] = {"1 2 -2147483649", NULL};
static const char	*ARGS_UNKNOWN_FLAG[] = {"--unknown", NULL};

static const t_test_case	g_cases[] = {
	/* no-op / trivial */
	{"No args", EXPECT_NONE, ARGS_EMPTY},
	{"Single number", EXPECT_NONE, ARGS_ONE},
	{"Two sorted", EXPECT_NONE, ARGS_TWO_SORTED},
	{"Sorted 1..5", EXPECT_NONE, ARGS_SORTED},
	{"Single-string sorted", EXPECT_NONE, ARGS_SINGLE_STR_SORTED},

	/* valid sorting */
	{"Two reversed", EXPECT_VALID, ARGS_TWO_REVERSED},
	{"Already sorted", EXPECT_NONE, ARGS_SORTED},
	{"Reverse order", EXPECT_VALID, ARGS_REVERSED},
	{"Negative only", EXPECT_VALID, ARGS_NEG_ONLY},
	{"Mixed sign", EXPECT_VALID, ARGS_MIXED_SIGN},
	{"Int min/max boundary", EXPECT_VALID, ARGS_LIMITS},
	{"Plus sign numbers", EXPECT_VALID, ARGS_PLUS},
	{"Plus in single string", EXPECT_VALID, ARGS_PLUS_STR},
	{"Single-arg list", EXPECT_VALID, ARGS_SINGLE_STR},
	{"Spaced single input", EXPECT_VALID, ARGS_SINGLE_STR_SPACED},
	{"Mixed format args", EXPECT_VALID, ARGS_MIX_FMT},
	{"Leading zero valid", EXPECT_VALID, ARGS_LEADING_ZERO},

	/* parser / error cases */
	{"Duplicate args", EXPECT_ERROR, ARGS_DUP},
	{"Duplicate negative", EXPECT_ERROR, ARGS_DUP_NEG},
	{"Duplicate 0 and -0", EXPECT_ERROR, ARGS_DUP_ZERO_FMT},
	{"Duplicate padded", EXPECT_ERROR, ARGS_DUP_PADDED},
	{"Alphabetic token", EXPECT_ERROR, ARGS_ALPHA},
	{"Alphabetic in string", EXPECT_ERROR, ARGS_ALPHA_STR},
	{"Invalid chunk 3+4", EXPECT_ERROR, ARGS_BAD_CHUNK},
	{"Tab inside token", EXPECT_ERROR, ARGS_TAB_TOKEN},
	{"Spaces only", EXPECT_ERROR, ARGS_SPACE_ONLY},
	{"Empty string", EXPECT_ERROR, ARGS_EMPTY_STR},
	{"Only plus sign", EXPECT_ERROR, ARGS_SIGN_PLUS_ONLY},
	{"Only minus sign", EXPECT_ERROR, ARGS_SIGN_MINUS_ONLY},
	{"Multi sign ++", EXPECT_ERROR, ARGS_MULTI_SIGN_PP},
	{"Multi sign --", EXPECT_ERROR, ARGS_MULTI_SIGN_MM},
	{"Multi sign +-", EXPECT_ERROR, ARGS_MULTI_SIGN_PM},
	{"Multi sign -+", EXPECT_ERROR, ARGS_MULTI_SIGN_MP},
	{"Overflow max", EXPECT_ERROR, ARGS_OVER_MAX},
	{"Overflow min", EXPECT_ERROR, ARGS_OVER_MIN},
	{"Overflow max in string", EXPECT_ERROR, ARGS_OVER_MAX_STR},
	{"Overflow min in string", EXPECT_ERROR, ARGS_OVER_MIN_STR},
	{"Unknown flag", EXPECT_ERROR, ARGS_UNKNOWN_FLAG}
};

size_t	manual_cases_get(const t_test_case **out_cases)
{
	if (out_cases)
		*out_cases = g_cases;
	return (sizeof(g_cases) / sizeof(g_cases[0]));
}
