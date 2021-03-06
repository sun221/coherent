/*
 * Remove trailing whitespace spaces from a line.
 */
#include <ctype.h>
#include <stdio.h>
extern char *strchr();

char *
trim(s)
char *s;
{
	register char *p;

	if (NULL == (p = strchr(s, '\0')))
		return (NULL);
	for (--p; (p > s) && isascii(*p) && isspace(*p);)
		*p-- = '\0';
	return (s);
}
#ifdef TEST
main()
{
	char buf[80];

	while (NULL != gets(buf))
		printf("'%s'\n", trim(buf));
}
#endif
