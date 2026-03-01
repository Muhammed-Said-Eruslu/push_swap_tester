#include "tester.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int	generate_unique_random_set(int *dst, int count, int min, int max)
{
	int	range;
	int	i;
	int	j;
	int	candidate;
	int	dup;

	if (!dst || count <= 0 || min > max)
		return (-1);
	range = max - min + 1;
	if (count > range)
		return (-1);
	i = 0;
	while (i < count)
	{
		candidate = min + (rand() % range);
		dup = 0;
		j = 0;
		while (j < i)
		{
			if (dst[j] == candidate)
			{
				dup = 1;
				break ;
			}
			j++;
		}
		if (!dup)
			dst[i++] = candidate;
	}
	return (0);
}

char	*join_ints_as_args(const int *arr, int count)
{
	char	*buf;
	size_t	cap;
	size_t	len;
	int		i;
	int		w;

	if (!arr || count <= 0)
		return (strdup(""));
	cap = (size_t)count * 14 + 2;
	buf = malloc(cap);
	if (!buf)
		return (NULL);
	len = 0;
	i = 0;
	while (i < count)
	{
		w = snprintf(buf + len, cap - len, "%d", arr[i]);
		if (w < 0)
			return (free(buf), NULL);
		len += (size_t)w;
		if (i != count - 1)
			buf[len++] = ' ';
		i++;
	}
	buf[len] = '\0';
	return (buf);
}
