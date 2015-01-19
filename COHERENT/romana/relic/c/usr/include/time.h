/* (-lgl
 * 	COHERENT Version 3.2
 * 	Copyright (c) 1982, 1991 by Mark Williams Company.
 * 	All rights reserved. May not be copied without permission.
 -lgl) */
/*
 * time.h
 * COHERENT time function header.
 */

#ifndef	TIME_H
#define	TIME_H	TIME_H

#include <sys/types.h>

struct	tm	{
	int	tm_sec;
	int	tm_min;
	int	tm_hour;
	int	tm_mday;
	int	tm_mon;
	int	tm_year;
	int	tm_wday;
	int	tm_yday;
	int	tm_isdst;
};

extern	char		*asctime();
extern	char		*ctime();
extern	struct	tm	*gmtime();
extern	struct	tm	*localtime();
extern	time_t		time();
extern	long		timezone;
extern	char		tzname[2][32];

#endif

/* end of time.h */
