/*
 * request.c
 * Nroff/Troff.
 * Request handler.
 */

#include <ctype.h>
#include "roff.h"

/*
 * This is called when the user gives a request.
 * It collects the arguments and dispatches the request.
 * Store the request line into miscbuf[],
 * and also set up an arg list to pass to the request.
 */
request()
{
	REG *rp;
	int inqflag, argc, i;
	register int c;
	char *argp[ARGSIZE], *abuf, *abufend, name[2], *mpend;
	register char *ap, *mp;

	if (isascii(c = getf(0)) && isspace(c)) {
		while (c != '\n')
			c = getf(0);
		return;
	}
	oldbracelevel = bracelevel;
	argc = 0;
	for (i = 0; i < ARGSIZE; i++)
		argp[i] = "";
	ap = abuf = nalloc(ABFSIZE);
	abufend = &abuf[ABFSIZE-1];
	mp = miscbuf;
	mpend = &mp[MSCSIZE-1];
	while (c != '\n') {
		if (argc < ARGSIZE)
			argp[argc++] = ap;
		inqflag = 0;
		while ((c != '\n') && (inqflag || !isascii(c) || !isspace(c))) {
			if (c == '"' && escflag == 0)
				inqflag ^= 1;
			else {
				if (ap >= abufend)
					break;
				if (c != EEND)
					*ap++ = c;
			}
			if (mp >= mpend)
				break;
			*mp++ = c;
			c = getf(0);
		}
		while (c != '\n' && isascii(c) && isspace(c)) {
			if (ap >= abufend || mp >= mpend) {
				printe("arguments too long");
				nfree(abuf);
				while (c != '\n')
					c = getf(0);
				return;
			}
			*mp++ = c;
			c = getf(0);
		}
		*ap++ = '\0';
	}
	*mp++ = '\0';
#if	(DDEBUG != 0)
	{
		char **dap;
		dap = &argp[1];
		printd(DBGREQX, "request %s", *dap++);
		while (*dap != NULL)
			printd(DBGREQX, " %s", *dap++);
		printd(DBGREQX, "\n");
	}
#endif
	argname(argp[0], name);
	if (dflag)
		fprintf(stderr, "%s\n", miscbuf);
	if ((rp = findreg(name, RTEXT)) == NULL)
		printe("request '%s' not found", argp[0]);
	else {
		if (rp->t_reg.r_macd.r_div.m_type == MREQS)
			(*rp->t_reg.r_macd.r_div.m_func)(argc, argp);
		else {
			if (adstreg(rp) != 0) {
				strp->x1.s_argc = argc;
				for (i = 0; i < ARGSIZE; i++)
					strp->x1.s_argp[i] = argp[i];
				strp->x1.s_abuf = abuf;
			}
			return;
		}
	}
	nfree(abuf);
}

/*
 * Store the next argument from line 'lp' in argument buffer 'ap'.
 * 'n' is the length of the buffer.
 * Skip trailing spaces and return the new line pointer.
 */
char *
nextarg(lp, ap, n) register char *lp, *ap; int n;
{
	register char *apend;

	apend = (ap == NULL) ? ap : &ap[n-1];
	while (!isascii(*lp) || !isspace(*lp)) {
		if (*lp == '\0')
			break;
		if (ap < apend)
			*ap++ = *lp;
		lp++;
	}
	if (ap != NULL)
		*ap = '\0';
	while (isascii(*lp) && isspace(*lp))
		lp++;
	return lp;
}

/* end of request.c */
