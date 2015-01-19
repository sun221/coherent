/*
 * hyphen.c
 * Nroff/Troff.
 * Hyphenation.
 */

#include <ctype.h>
#include "roff.h"

/*
 * Try to hyphenate the word found in the word buffer.
 */
hyphen(cp1, cp2)
CODE *cp1;
CODE *cp2;
{
	register CODE *cpl;
	register int n;
	int wi1, wi2, len;

	cpl = cp1;
	n = cp2 - cp1;
	while (n--)
		hyphbuf[n] = 0;
	while (cp1 < cp2) {
		n = cp2[-1].c_arg.c_code;
		if (isascii(n) && isalpha(n))
			break;
		--cp2;
	}
	len = cp2 - cp1;
	while (cp1 < cp2) {
		n = cp1->c_arg.c_code;
		if (isascii(n) && isalpha(n))
			break;
		cp1++;
	}
	if (len <= 4)
		return;
	wi1 = 0;
	wi2 = len;
	if (except(cpl, hyphbuf, wi1, wi2))
		return;
	wi2 = 1 + suffix(cpl, hyphbuf, wi2-1, wi1-1);
	wi1 = prefix(cpl, hyphbuf, wi1, wi2);
	middle(cpl, hyphbuf, wi1, wi2);
	n = len;
	hyphbuf[0] = 0;
	hyphbuf[n-3] = 0;
	hyphbuf[n-2] = 0;
	hyphbuf[n-1] = 0;
	if (wi2-wi1 <= 2) {
		hyphbuf[wi1] = 0;
		hyphbuf[wi2-1] = 0;
	}
	n = wi2;
	if (--n>=0 && cpl[n].c_arg.c_code==LEEE) {
		int m;
		m = 3;
		while (n && m--)
			hyphbuf[n--] = 0;
	}
	n = wi2;
	if (n>=2 && cpl[--n].c_arg.c_code==LDDD &&
	    cpl[--n].c_arg.c_code==LEEE) {
		if (--n<1 || cpl[n].c_arg.c_code!=LZZZ ||
		    cpl[n-1].c_arg.c_code!=LIII) {
			if (--n >= 0)
				hyphbuf[n] = 0;
			if (--n >= 0)
				hyphbuf[n] = 0;
		}
	}
}

/*
 * Look for exception words.
 */
except(wbuf, hbuf, wi1, wi2)
CODE *wbuf;
char *hbuf;
{
	unsigned ti, ti0, ti1, ti2, wih, c1, c2;
	register int wi, n;
	register char *bp;

	ti = ti1 = 0;	/* ti = 0 by c.e.f triggered by lint error */
	ti2 = EXCSIZE;
	for (;;) {
		ti0 = ti;
		if ((ti=(ti1+ti2)/2) == ti0)
			goto fail;
		wi = wi1;
		wih = wi1;
		bp = exctab[ti];
		for (;;) {
			if (*bp == LEOK) {
				if (wi == wi2)
					return 1;
				if (wi==wi2-1 && wbuf[wi].c_arg.c_code==LSSS)
					return 1;
				ti1 = ti;
				break;
			}
			if (*bp == LHYP) {
				bp++;
				wih = wi;
				hbuf[wi-1] = 1;
				continue;
			}
			if (wi >= wi2) {
				ti1 = ti;
				break;
			}
			if ((c1=wbuf[wi++].c_arg.c_code) != (c2 = *bp++)) {
				if (c1 > c2)
					ti1 = ti;
				else
					ti2 = ti;
				break;
			}
		}
		for (wi=wi1; wi<wih; wi++)
			hbuf[wi] = 0;
	}
fail:
	for (n=wi1; n<wi2; n++)
		hbuf[n] = 0;
	return 0;
}

/*
 * Look for prefixes.
 */
prefix(wbuf, hbuf, wi1, wi2)
CODE *wbuf;
char *hbuf;
register int wi2;
{
	unsigned ti, ti0, ti1, ti2, c1, c2, con;
	register int wi, wih;
	register char *bp;

	do {
		ti0 = -1;
		ti = ti1 = 0;	/* ti = 0 by c.e.f triggered by lint */
		ti2 = PRESIZE;
		for (;;) {
			ti0 = ti;
			if ((ti=(ti1+ti2)/2) == ti0)
				return wi1;
			wi = wi1;
			wih = wi1;
			bp = pretab[ti];
			for (;;) {
				if (*bp == LEOK) {
					bp++;
					goto patn;
				}
				if (*bp == LHYP) {
					bp++;
					wih = wi;
					hbuf[wi-1] = 1;
					continue;
				}
				if (wi >= wi2)
					return wi1;
				if ((c1=wbuf[wi++].c_arg.c_code) !=
				    (c2 = *bp++)) {
					if (c1 > c2)
						ti1 = ti;
					else
						ti2 = ti;
					break;
				}
			}
			for (wi=wi1; wi<wih; wi++)
				hyphbuf[wi] = 0;
		}
	patn:
		if (automate(bp, &wi1, &con, 1, wbuf, hbuf, wi, wi2) == 0) {
			while (wi > wi1)
				hbuf[--wi] = 0;
			return wi1;
		}
	} while (con != 0);
	return wi1;
}

/*
 * Look for suffixes.
 */
suffix(wbuf, hbuf, wi1, wi2)
CODE *wbuf;
char *hbuf;
register int wi2;
{
	unsigned ti, ti0, ti1, ti2, c1, c2, con;
	register int wi, wih;
	register char *bp;

	do {
		ti0 = -1;
		ti = ti1 = 0;	/* ti = 0 by cef triggered by lint */
		ti2 = SUFSIZE;
		for (;;) {
			ti0 = ti;
			if ((ti=(ti1+ti2)/2) == ti0)
				return wi1;
			wi = wi1;
			wih = wi1;
			bp = suftab[ti];
			for (;;) {
				if (*bp == LEOK) {
					bp++;
					goto patn;
				}
				if (*bp == LHYP) {
					bp++;
					wih = wi;
					hbuf[wi] = 1;
					continue;
				}
				if (wi <= wi2)
					return wi1;
				if ((c1=wbuf[wi--].c_arg.c_code) !=
				    (c2 = *bp++)) {
					if (c1 > c2)
						ti1 = ti;
					else
						ti2 = ti;
					break;
				}
			}
			for (wi=wi1; wi>wih; wi--)
				hyphbuf[wi] = 0;
		}
	patn:
		if (automate(bp, &wi1, &con, -1, wbuf, hbuf, wi, wi2) == 0) {
			while (wi < wi1)
				hbuf[++wi] = 0;
			return wi1;
		}
	} while (con != 0);
	return wi1;
}

/*
 * Try to hyphenate the middle of a word.
 */
middle(wbuf, hbuf, wi1, wi2)
CODE *wbuf;
char *hbuf;
{
	int new, bil, c2, c3, n;
	unsigned con;
	register int wi, bi, c1;

	wi = wi1;
	bi = 0;
	while (wi < wi2) {
		c1 = wbuf[wi++].c_arg.c_code;
		if (wi<wi2 && wbuf[wi].c_arg.c_code==LHHH) {
			wi++;
			switch (c1) {
			case LCCC:
				c1 = LDCH;
				break;
			case LGGG:
				c1 = LDGH;
				break;
			case LPPP:
				c1 = LDPH;
				break;
			case LSSS:
				c1 = LDSH;
				break;
			case LTTT:
				c1 = LDTH;
				break;
			default:
				--wi;
				break;
			}
		}
		hletbuf[bi] = c1;
		hindbuf[bi++] = wi-1;
	}
	bil = bi-2;
	for (bi=0; bi<bil; bi++) {
		if (!vowel(hletbuf[bi]))
			continue;
		c1 = hletbuf[bi+1];
		c2 = hletbuf[bi+2];
		if (c1==c2 && consn(c1)) {
			if (c1 == LLLL)
				continue;
			if (c1 == LSSS) {
				if (bi>=bil-1 || !vowel(hletbuf[bi+3]))
					continue;
				if (automate(mm0code, &new, &con, 1,
					wbuf, hbuf, hindbuf[bi+3], wi2)==0)
					continue;
			}
			hbuf[hindbuf[++bi]] = 1;
			continue;
		}
		if (c1==LCCC && c2==LKKK) {
			hbuf[hindbuf[bi+=2]] = 1;
			continue;
		}
		if (c1==LQQQ && c2==LUUU) {
			hbuf[hindbuf[bi]] = 1;
			continue;
		}
		if (bi < bil-1) {
			c3 = hletbuf[bi+3];
			if (!consn(c1) || !consn(c2) || !vowel(c3))
				continue;
			if ((n=matpair(c1, c2)) == 2)
				continue;
			if (n==1 && automate(mm1code, &new, &con, 1,
				wbuf, hbuf, hindbuf[bi+3], wi2)==0)
				continue;
			hbuf[hindbuf[++bi]] = 1;
			continue;
		}
	}
}

/*
 * See if we match a set of double consonants.  If we do,
 * return the associated number in the table.
 */
matpair(c1, c2)
register int c1;
{
	register int c;
	register char *cp;

	cp = dbctab;
	while ((c = *cp++) != LNUL) {
		if (c1 < c)
			return 0;
		if (c1 > c) {
			cp += 2;
			continue;
		}
		if (c2 != *cp++) {
			cp++;
			continue;
		}
		return *cp;
	}
	return 0;
}

/*
 * Given a pattern string, execute it on the given word buffer.
 */
automate(patp, newp, conp, dirn, wbuf, hbuf, wi1, wi2)
char *patp;
int *newp;
unsigned *conp;
CODE *wbuf;
char *hbuf;
{
	int wi, wis;
	register int c, n;
	register char *bp;

	bp = patp;
	*conp = 0;
	wi = wi1;
	wis = wi1;
	for (;;) {
		switch (*bp++) {
		case LNUL:
			goto succ;
		case LHYP:
			wis = wi;
			*conp = 0;
			hbuf[wi-(dirn>0?1:0)] = 1;
			continue;
		case LRHP:
			hbuf[wi-(dirn>0?1:0)] = 0;
			continue;
		case LCON:
			*conp = 1;
			continue;
		case LNEW:
			n = *bp++;
			if (wi != wi2) {
				c = wbuf[wi].c_arg.c_code;
				wi += dirn;
				continue;
			}
			if (n == 1)
				goto fail;
			if (n == 2)
				goto succ;
			bp += n-3;
			continue;
		case LOLD:
			wi -= dirn;
			c = wbuf[wi-dirn].c_arg.c_code;
			continue;
		case LBRF:
			goto fail;
		case LBRS:
			goto succ;
		case LCBT:
			if (*bp++ != c) {
				bp++;
				continue;
			}
			if ((n = *bp++) == 1)
				goto fail;
			if (n == 2)
				goto succ;
			bp += n-3;
			continue;
		case LCBF:
			if (*bp++ == c) {
				bp++;
				continue;
			}
			if ((n = *bp++) == 1)
				goto fail;
			if (n == 2)
				goto succ;
			bp += n-3;
			continue;
		default:
			panic("bad pattern");
		}
	}
succ:
	*newp = wis;
	return 1;
fail:
	return 0;
}

/*
 * See if the given code is a vowel.
 */
vowel(c)
register int c;
{
	if (c>=LAAA && c<=LYYY && contab[c-LAAA]==0)
		return 1;
	return 0;
}

/*
 * See if the given code is a consonant.
 */
consn(c)
register int c;
{
	if (c>=LAAA && c<=LDTH && contab[c-LAAA]==1)
		return 1;
	return 0;
}

/*
 * Code to fail if we match ((er|ers)$).
 */
char mm0code[] ={
	 0005, 0002, 0012, 0051, 0002, 0005, 0002, 0012,
	 0066, 0002, 0005, 0001, 0012, 0067, 0002, 0005,
	 0001, 0010, 0000
};

/*
 * Code to fail if we match ((er|ers|age|ages|est)$).
 */
char mm1code[] ={
	 0005, 0002, 0011, 0051, 0030, 0012, 0045, 0002,
	 0005, 0002, 0012, 0053, 0002, 0005, 0002, 0012,
	 0051, 0002, 0005, 0001, 0012, 0067, 0002, 0005,
	 0001, 0010, 0005, 0002, 0011, 0066, 0016, 0012,
	 0067, 0002, 0005, 0002, 0012, 0070, 0002, 0005,
	 0001, 0010, 0005, 0001, 0012, 0067, 0002, 0005,
	 0001, 0010, 0000
};

/*
 * Table to determine whether a letter is a constant or a vowel.
 */
char contab[] ={
	0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1,
	1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1
};

/*
 * Pairs of consonants which aren't allowed somewhere.
 */
char dbctab[] ={
	LBBB, LLLL, 2,
	LBBB, LRRR, 2,
	LCCC, LLLL, 2,
	LCCC, LRRR, 2,
	LFFF, LLLL, 2,
	LFFF, LRRR, 2,
	LFFF, LTTT, 1,
	LGGG, LLLL, 2,
	LGGG, LRRR, 2,
	LKKK, LNNN, 2,
	LLLL, LDDD, 1,
	LLLL, LKKK, 2,
	LLLL, LQQQ, 2,
	LMMM, LPPP, 1,
	LNNN, LDDD, 1,
	LNNN, LGGG, 1,
	LNNN, LKKK, 2,
	LNNN, LSSS, 1,
	LNNN, LTTT, 1,
	LNNN, LXXX, 2,
	LNNN, LDCH, 2,
	LPPP, LLLL, 2,
	LPPP, LRRR, 2,
	LRRR, LGGG, 1,
	LRRR, LKKK, 2,
	LRRR, LMMM, 1,
	LRRR, LNNN, 1,
	LRRR, LTTT, 1,
	LSSS, LPPP, 2,
	LSSS, LQQQ, 2,
	LSSS, LTTT, 1,
	LTTT, LRRR, 2,
	LTTT, LDCH, 2,
	LWWW, LHHH, 2,
	LWWW, LLLL, 2,
	LWWW, LNNN, 2,
	LWWW, LRRR, 2,
	LDCH, LLLL, 2,
	LDCH, LRRR, 2,
	LDDD, LGGG, 2,
	LDDD, LRRR, 2,
	LDGH, LTTT, 2,
	LDPH, LRRR, 2,
	LDTH, LRRR, 2,
	LNUL
};

/* end of hyphen.c */
