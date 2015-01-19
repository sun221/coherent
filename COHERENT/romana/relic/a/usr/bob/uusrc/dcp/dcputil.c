/*
 * dcputil.c 
 *
 * miscellaneous utility functions
 */

#include <ctype.h>
#include <pwd.h>
#include "dcp.h"
#include "perm.h"

/*
 * |myname()| returns a pointer to the local host's UUCP nodename.
 *
 * If there is a MYNAME entry in the PERMS, then return this entry
 * instead of the name in the NODENAME file.
 */

char *
myname()
{
	register char *tmp;

	if ( (tmp=perm_value(myname_e)) != NULL )
		return( tmp );
	else
		return( uucpname() );
}

/*
 *  char *visib(str)  char *str;
 *  char *visbuf(data, len)  char *data;  int len;
 *
 *  Print the string or buffer of data in "visible" format.  Printable
 *  characters are printed as themselves, special control characters
 *  (CR, LF, BS, HT) are printed in the usual escape format, and others
 *  are printed using 3-digit octal escapes.  Returns a pointer to a
 *  static buffer containing the visible data.
 */

char *
visib(str)
unsigned char *str;
{
	return( visbuf(str, strlen(str)) );
}

char *
visbuf(data, len)
unsigned char *data; 
int len;
{
	static char buf[BUFSIZ];
	register unsigned char c;
	register char *p;


	if ( len <= 0 )
		return("");
	p = buf;
	while( (p<&buf[BUFSIZ-4]) && (len--) ) {
		c = *data++;
		if ( (c!='\\') && isascii(c) && (isprint(c) || (c==' ')) ) {
			*p++ = c;
			continue;
		}
		*p++ = '\\';
		switch ( c ) {
		case '\\':  *p++ = '\\'; break;
		case '\n':  *p++ = 'n';  break;
		case '\t':  *p++ = 't';  break;
		case '\b':  *p++ = 'b';  break;
		case '\r':  *p++ = 'r';  break;
		case '\f':  *p++ = 'f';  break;
		case '\0':  *p++ = '0';  break;
		default:
			sprintf(p, "%03o", c);
			p += 3;
		}
	}
	*p = '\0';
	return( buf );
}

/*
 * |expandtilde(filename)| expands |filename| using the usual "~" convention.
 * That is, "~user" expands to the home directory of "user". "~" by itself
 * expands to the home directory of the effective userid, which in this
 * case is usually the /usr/spool/uucppublic directory. Care is taken not
 * to overflow the (static) name buffer.
 */
char *expandtilde(filename)
char *filename;
{
	struct passwd *pw, *getpwnam(), *getpwuid();
	static char namebuf[PATHLEN];
	int goodname = 1;
	char *p;
	char logline[60];

	if ('~' != *filename) {
		strcpy(namebuf, filename);
	} else {
		++filename;
		p = namebuf;
		while(*filename && '/' != *filename && (p - namebuf) < PATHLEN)
			*p++ = *filename++;
		*p = '\0';
		if (strlen(namebuf) == 0)
			pw = getpwuid(geteuid());
		else
			pw = getpwnam(namebuf);
		if (NULL == pw) {
			goodname = 0;
		} else if (strlen(pw->pw_dir)+strlen(filename)+1 > PATHLEN) {
			goodname = 0;
		} else {
			strcpy(namebuf, pw->pw_dir);
			strcat(namebuf, filename);
		}
	}
	if (goodname) {
		sprintf(logline, "tilde->%s", namebuf);
		plog(M_TRANSFER, logline);
		return namebuf;
	} else {
		plog(M_TRANSFER, "tilde->NULL");
		return NULL;
	}
}

