#include "tester.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

bool	str_contains(const char *s, const char *needle)
{
	if (!s || !needle)
		return (false);
	return (strstr(s, needle) != NULL);
}

char	*str_trim_ws(const char *src)
{
	const char	*start;
	const char	*end;
	size_t		len;
	char		*out;

	if (!src)
		return (strdup(""));
	start = src;
	while (*start && isspace((unsigned char)*start))
		start++;
	end = start + strlen(start);
	while (end > start && isspace((unsigned char)*(end - 1)))
		end--;
	len = (size_t)(end - start);
	out = malloc(len + 1);
	if (!out)
		return (NULL);
	memcpy(out, start, len);
	out[len] = '\0';
	return (out);
}

int	args_count(const char *const *args)
{
	int	count;

	count = 0;
	if (!args)
		return (0);
	while (args[count])
		count++;
	return (count);
}
