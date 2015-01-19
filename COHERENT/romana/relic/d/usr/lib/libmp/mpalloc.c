#include "mprec.h"

char	*malloc();
#ifndef NULL
#define NULL	((char *)0)
#endif


/*
 *	Mpalc allocates space for either a multi-precision value or
 *	a mint.  It does not return unless the space is available.
 *	"Size" is the number of bytes that the value requires.
 */

char *
mpalc(size)
unsigned size;
{
	register char *result;

	result = (char *) malloc(size);
	if (result == NULL)
		mperr("No space for value");
	return (result);
}


/*
 *	Mpfree is used to free any space allocated by malloc.
 *	Note that if "blkp" is NULL, then nothing should be done.
 */

void
mpfree(blkp)
register char	*blkp;
{
	if (blkp != NULL)
		free(blkp);
}
