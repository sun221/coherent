/*
 *  mdial.c
 *
 *  Implement modem dialing.
 */

#include <stdio.h>
#include <signal.h>
#include "modemcap.h"
#include "dial.h"
#include "alarm.h"

char	modembuf[BUFSIZ];

mdial(telno, fd)
char *telno;
int fd;
{
	register char *cp, *mp;
	char ch;
	int len;

	if ( !DI || !AS )
		return(merrno = M_A_PROB);

	len = strlen(CS)+strlen(DS)+strlen(telno)+strlen(DE)+strlen(CE);
	if ( len > (BUFSIZ-2) ) {
		merrno = M_DEV_TEL;
		goto exitfn;
	}
	sprintf(modembuf, "%s%s%s%s%s", CS, DS, telno, DE, CE);
	SETALRM(30);
	if ( (write(fd, modembuf, len=strlen(modembuf)) != len) || timedout ) {
		merrno = M_D_HUNG;
		goto exitfn;
	}

	if ( CO == NULL )
		goto exitfn;

	SETALRM( TT ? CONNECTTIME: (CONNECTTIME + strlen(telno)/2) );

 	mp = modembuf;
	for (cp=CO; *cp; ) {
		if ( (read(fd, &ch, 1) != 1) || timedout ||
		     (mp >= &modembuf[BUFSIZ-1]) ) {
			*mp = '\0';
			merrno = (mp!=modembuf) ? M_A_PROB: M_NO_ANS;
			goto exitfn;
		}
		*mp++ = ch;
		cp = (*cp == ch) ? cp+1: CO;
	}
	*mp = '\0';

exitfn:
	CLRALRM();
	return( merrno );
}
