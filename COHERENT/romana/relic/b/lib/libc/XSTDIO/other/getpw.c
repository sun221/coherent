/*
 * Coherent I/O Library
 * getpw -- get line from password file for a given `uid'.
 */
#include <stdio.h>


getpw(uid, buf)
short	uid;
char	*buf;
{
	register int	c;
	register char 	*cp;
	register FILE 	*fp;

	fp = fopen("/etc/passwd", "r");
	if (fp == NULL)
		return (1);
	while (!feof(fp)) {
		for (cp=buf; (c=getc(fp)) != EOF && c != '\n';)
			*cp++ = c;
		*cp = '\0';
		for (cp=buf; *cp != ':' && *cp != '\0'; ++cp)
			;
		if (*cp == '\0')
			continue;
		do {
			++cp;
		} while (*cp != ':' && *cp != '\0');
		if (*cp++ != '\0' && uid == atoi(cp)) {
			fclose(fp);
			return (0);
		}
	}
	fclose(fp);
	return (1);
}
