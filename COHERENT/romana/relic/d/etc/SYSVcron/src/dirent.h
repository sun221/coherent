/*
	<dirent.h> -- definitions for SVR3 directory access routines

	last edit:	25-Apr-1987	D A Gwyn
			12-Jul-1990	D A Fenyes
					Coherent conversion

	Prerequisite:	<sys/types.h>
*/


#include	"sys.dirent.h"

#define	DIRBUF		2048		/* buffer size for fs-indep. dirs */
	/* must in general be larger than the filesystem buffer size */

typedef struct
	{
	int	dd_fd;			/* file descriptor */
	int	dd_loc;			/* offset in block */
	int	dd_size;		/* amount of valid data */
	char	*dd_buf;		/* -> directory block */
	}	DIR;			/* stream data from opendir() */


extern DIR		*opendir();
extern struct dirent	*readdir();
extern void		rewinddir();
extern int		closedir();
extern daddr_t		telldir();
extern void		seekdir();


#ifndef NULL
#define	NULL	0			/* DAG -- added for convenience */
#endif

