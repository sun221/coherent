/*
 * Standard I/O Library
 * Unget char for next getc
 */

#include <stdio.h>

int
ungetc(c, fp)
int	c;
register FILE	*fp;
{
	extern	int	_fginit();
	extern	int	_fgetb();
	extern	int	_fgetc();
	extern	int	_fgeteof();
	int	_fungoti();
	int	_fungotb();
	int	_fungotc();
	int	_fungote();
	int	_funungc();

	if (c == EOF)			/* ANSI 4.9.7.11 (22) */
		return(EOF);		/* Leave input stream unchanged */
	if (fp->_gt==&_fginit)
		fp->_gt = &_fungoti;
	else if (fp->_gt==&_fgetb)
		fp->_gt = &_fungotb;
	else if (fp->_gt==&_fgetc)
		fp->_gt = &_fungotc;
	else if (fp->_gt==&_fgeteof)
		fp->_gt = &_fungote;
	else
		return (EOF);
	fp->_pt = &_funungc;
	fp->_cc = 0;
	fp->_ff |= _FUNGOT;
	fp->_ff &= ~_FEOF;		/* ANSI 4.9.7.11 (24) */
	fp->_uc = c;
	return (c);
}


/*
 * uninitialized fp unget
 */

static
int
_fungoti(fp)
register FILE	*fp;
{
	extern	int	_fginit();
	extern	int	_fpinit();

	fp->_gt = &_fginit;
	fp->_pt = &_fpinit;
	fp->_cc = 0;
	fp->_ff &= ~_FUNGOT;
	return (fp->_uc);
}


/*
 * buffered unget
 */

static
int
_fungotb(fp)
register FILE	*fp;
{
	extern	int	_fgetb();
	extern	int	_fputb();

	fp->_gt = &_fgetb;
	fp->_pt = &_fputb;
	if ((fp->_cc = fp->_cp - fp->_dp) > 0)
		fp->_cc = _ep(fp) - fp->_cp;
	fp->_ff &= ~_FUNGOT;
	return (fp->_uc);
}


/*
 * unbuffered unget
 */

static
int
_fungotc(fp)
register FILE	*fp;
{
	extern	int	_fgetc();
	extern	int	_fputc();

	fp->_gt = &_fgetc;
	fp->_pt = &_fputc;
	fp->_cc = 0;
	fp->_ff &= ~_FUNGOT;
	return (fp->_uc);
}


/*
 * string unget (for sscanf)
 */

static
int
_fungote(fp)
register FILE	*fp;
{
	extern int	_fgeteof();

	fp->_gt = &_fgeteof;
	fp->_cc = -strlen(fp->_cp);
	return (fp->_uc);
}


/*
 * un unget; occurs if put done before get after unget
 */

static
int
_funungc(c, fp)
register char	c;
register FILE	*fp;
{
	(*fp->_gt)(fp);
	return (putc(c, fp));
}
