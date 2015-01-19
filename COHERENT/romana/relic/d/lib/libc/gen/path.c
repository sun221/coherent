#include <path.h>
#define NULL	((char *)0)
char *path(p, f, m) char *p, *f; int m;
{
	static char pathname[MAXPATH];
	register char *p1, *p2, *p3;
	register int c, d;

	if ((p1 = p) == NULL)
		return NULL;
	do {
		p2 = pathname;
		while ((c = *p1++) != 0 && c != LISTSEP)
			if (p2 < pathname+MAXPATH)
				*p2++ = c;
		if (p2 > pathname && p2[-1] != PATHSEP)
			if (p2 < pathname+MAXPATH)
				*p2++ = PATHSEP;
		if ((p3 = f) == NULL)
			break;
		while ((d = *p3++) != 0)
			if (p2 < pathname+MAXPATH)
				*p2++ = d;
		if (p2 < pathname+MAXPATH) {
			*p2 = 0;
			if (access(pathname, m) >= 0)
				return pathname;
		}
	} while (c != 0);
	return NULL;
}
