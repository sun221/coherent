#include <stdio.h>
#include <assert.h>
#include <ctype.h>
#include "mprec.h"


/*
 *	Ibase is an int which contains the input base used by min.
 *	It should be between 2 and 16.  Regardless of ibase, the
 *	legal digits are 0 - 9 and A - F.
 */

int	ibase = 10;


/*
 *	Min reads in a number in base ibase from stdin and sets "a" 
 *	to that number.  Note that a leading minus sign is allowed as
 *	are leading blanks.
 */

void
min(a)
register mint *a;
{
	register int ch;
	register char mifl;	/* leading minus flag */
	char cval;
	mint c = {1, &cval};

	assert(2 <= ibase && ibase <= 16);
	/* throw away old value, and set to zero */
	mcopy(mzero, a);

	/* skip leading blanks */
	do {
		ch = getchar();
	} while (isascii(ch) && isspace(ch));

	/* check for leading minus */
	mifl = ch == '-';
	if (mifl)
		do {
			ch = getchar();
		} while (isascii(ch) && isspace(ch));

	/* scan thru actual number, building result in a */
	while (okdigit(ch)) {
		cval = valdigit(ch);
		smult(a, ibase, a);
		madd(a, &c, a);
		ch = getchar();
	}
	ungetc(ch, stdin);

	/* adjust sign of a */
	if (mifl)
		mneg(a, a);
}


/*
 *	Okdigit returns true iff "ch" is a valid digit.  This means
 *	iff it is a normal digit or one of the letters 'A' thru 'F'.
 */

static
okdigit(ch)
register int ch;
{
	return (isascii(ch) && isdigit(ch) || 'A'<=ch && ch<='F');
}


/*
 *	Valdigit returns the numerical value for the character "ch"
 *	when it is interpreted as a digit.  Note that valdigit assumes
 *	that "ch" has already been successfully tested by okdigit.
 */

static
valdigit(ch)
register int ch;
{
	return (isdigit(ch) ? ch+0-'0' : ch+0xA-'A');
}
