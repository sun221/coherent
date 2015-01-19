#include <stdio.h>
#include <sys/stat.h>
#include <access.h>
#include "mail.h"
#include <time.h>
#include <sys/types.h>

char	*parent();
char	keyname[64];		/* Destination public key file name */

/*
 *	Miscellaneous Routines
 */

char *
dupstr(str)
{
	return( strcpy(myalloc(strlen(str)+1), str) );
}

char *
myalloc(size)
unsigned int size;
{
	register char *cp;
	extern char *malloc();

	if ( (cp=malloc(size)) == NULL )
		merr("Out of memory");
	return(cp);
}

fatal(x)
{
	fprintf(stderr, "Fatal Error: %r\n", &x);
	fflush(stderr);
	rmexit(1);
}

/*
 * Check access on a file.
 */
maccess(name)
char *name;
{
	struct stat sb;

	if (stat(name, &sb) < 0) {
		if (access(parent(name), ACREAT) < 0)
			return (-1);
	} else if (access(name, AWRITE) < 0)
		return (-1);
	return (0);
}

/*
 * Check enrollment for xmail.
 */
xaccess(name) char *name;
{
	struct stat sb;
	sprintf(keyname, "%s%s", PUBKEYDIR, name);
	return stat(keyname, &sb) >= 0;
}
/*
 * Find the parent directory for access permissions.
 */
char *
parent(name)
char *name;
{
	register char *cp, *xp;
	static char p[256];

	xp = rindex(name, '/');
	if (xp == NULL)
		return (".");
	if (xp == name)
		return ("/");
	if (xp - name >= 256)
		return ("");
	cp = p;
	while (name < xp)
		*cp++ = *name++;
	*cp = 0;
	return (p);
}

/*
 * Copy from the file stream `ifp' (starting at
 * position `start' and ending at `end' or EOF)
 * to the file stream `ofp' which is assumed
 * to be already correctly positioned.
 * Returns non-zero on errors.
 * intstop == 1 means stop on interrupt
 * intstop == 0 means ignore interrupt
 */
mcopy(ifp, ofp, start, end, intstop)
register FILE *ifp, *ofp;
fsize_t start, end;
{
	register int c;

	fseek(ifp, start, 0);
	end -= start;
	if (intstop)
		while (!intcheck() && end-- > 0  &&  (c = getc(ifp))!=EOF){
			putc(c, ofp);
			fflush(ofp);
		}
	else
		while (end-- > 0  &&  (c = getc(ifp))!=EOF){
			putc(c, ofp);
			fflush(ofp);
		}

	if (ferror(ofp))
		return (1);
	return (0);
}

#if XMAIL

char helpmessage[] = "\
\
mail -- computer mail\n\
xmail -- secret computer mail\n\
Usage:	mail [ options ] [ user ... ]\n\
or:	xmail [ options ] user [ ... ]\n\
Options:\n\
	-f file		Print mail from 'file' instead of the default\n\
	-p		Print mail non-interactively\n\
	-q		Exit on interrupt, leaving mail unchanged\n\
	-r		Print mail in reverse order, latest first\n\
	-v		Verbose commentary on alias expansion\n\
If 'user' is present, send each a mail message read from standard input.\n\
If 'xmail' is the command, use xencode to encrypt the mail messages.\n\
Mail message ends with EOF of a line containing only '.'.  Otherwise, read\n\
and print the invoking user's mailbox.\n\
\
";

#else

char helpmessage[] = "\
\
mail -- computer mail\n\
Usage:	mail [ options ] [ user ... ]\n\
Options:\n\
	-f file		Print mail from 'file' instead of the default\n\
	-p		Print mail non-interactively\n\
	-q		Exit on interrupt, leaving mail unchanged\n\
	-r		Print mail in reverse order, latest first\n\
	-v		Verbose commentary on alias expansion\n\
If 'user' is present, send each a mail message read from standard input.\n\
Mail message ends with EOF of a line containing only '.'.  Otherwise, read\n\
and print the invoking user's mailbox.\n\
\
";
#endif

usage()
{
	mmsg(helpmessage);
	rmexit(1);
}

#define	LOGFILE "/usr/spool/uucp/.Log/mail/lmail"

static char *logfile = LOGFILE;
static FILE *logfp = NULL;

logopen()
{
	time_t t;

	/* Try to open the log file.  If this fails, no big deal; we
	   just won't log anything.  */
	time(&t);
	if ( (logfp=fopen(logfile, "a")) == NULL ) {
#if 0
		fprintf(stderr, "Can't open Log File: \"%s\"", logfile);
#endif
	} /* If open of logfile failed. */
	logdump("------------------\n");
	logdump(ctime(&t));
}

logclose()
{
	if ( logfp != NULL )  
		fclose(logfp);
}

logdump(x)
{
	if ( logfp != NULL ){
		fprintf(logfp, "%r", &x);
		fflush(logfp);
	}
}
