/* $Header: /src386/bin/db/RCS/i8086b.c,v 1.3 93/03/11 07:45:40 bin Exp Locker: bin $
 *
 *	The information contained herein is a trade secret of Mark Williams
 *	Company, and  is confidential information.  It is provided  under a
 *	license agreement,  and may be  copied or disclosed  only under the
 *	terms of  that agreement.  Any  reproduction or disclosure  of this
 *	material without the express written authorization of Mark Williams
 *	Company or persuant to the license agreement is unlawful.
 *
 *	COHERENT Version 2.3.35
 *	Copyright (c) 1982, 1983, 1984.
 *	An unpublished work by Mark Williams Company, Chicago.
 *	All rights reserved.
 */
/*
 * A debugger.
 * Intel 8086.
 *
 * $Log:	i8086b.c,v $
 * Revision 1.3  93/03/11  07:45:40  bin
 * Hal: post NDP update that at least
 * can st breakpoints.
 * 
 * Revision 1.1  92/06/10  14:36:31  bin
 * Initial revision
 * 
 * Revision 1.2	89/06/19  16:46:29 	src
 * Bug:	80286 virtual mode instructions not supported [ie: lgdt].
 * Fix:	Metakey 'V' added to support 3 byte virtual mode instructions. (ABC)
 * 
 * Revision 1.1	88/10/17  04:03:49 	src
 * Initial revision
 *
 * Revision: 386 version 92/05/01 
 * Bernard Wald, Wald Software Consulting, Germany
 */

#include <stdio.h>
#include <sys/param.h>
#include <l.out.h>
#include "trace.h"
#include "i8086.h"

/*
 * Disassemble an Intel 80386 instruction.
 */

#define	MAXINSTLEN	50	/* max. expanded opcode string length */
#define	MAXDISPLCTSIZE	6	/* max. displacement size in bytes */
#define	ESC_2_OPMAP2	0x0F	/* escape to second byte of the opcode */

/* define instruction prefix codes */
#define REP	0xF3
#define REPNE	0xF2
#define LOCK	0xF0

/* define address size prefix codes */
#define ADDRPRE	0x67

/* define operand size prefix codes */
#define OPDPRE	0x66

/* define segment override prefix codes */
#define CS_PFX	0x2E
#define DS_PFX	0x3E
#define ES_PFX	0x26
#define FS_PFX	0x64
#define GS_PFX	0x65
#define SS_PFX	0x36

#define EBP_SIB	5

#define UNSIGNED	0
#define SIGNED		1

#define ADDZERO		0
#define ADDADDR		1

#define mod(x)		(((x)>>6)&3)
#define modR(x)		(((x)>>3)&7)
#define modRM(x)	((x)&7)
#define sibss(x)	(((x)>>6)&3)
#define sibx(x)		(((x)>>3)&7)
#define sibb(x)		((x)&7)

#define ADDRINDEX(x)	(x == RM32 ? 1 : 0)

#define TOLSYM		0x100		/* print symbol if greater */
#define TOLHEX		0x10		/* print hexidecimal if greater */
					/* print decimal if less */
static	int		addrflag=0;	/* address size prefix flag */
static	int		opdflag=0;	/* operand size prefix flag */
static	int		segflag=0;	/* segment override prefix flag */
static	char		*segPreStrp;	/* segment prefix str., e.g. "cs:" */
static	unsigned char	m;		/* "Mod" field of ModR/M byte */
static	unsigned char	mRM;		/* "R/M" field of ModR/M byte */
static	unsigned char	mReg;		/* "REG" field of ModR/M byte */
static	unsigned char	modRMloaded;	/* ModR/M byte part of instruction
					   has already been loaded, if set */
static	unsigned char	sb;		/* "base" field of SIB byte */
static	unsigned char	sss;		/* "ss" field of SIB byte */
static	unsigned char	sx;		/* "index" field of SIB byte */
static	unsigned char	sibLoaded;	/* SIB byte part of instruction
					   has already been loaded, if set */

/* Function declatations */
char		*getmodREGstr();
char		*getmodRM16str();
char		*getmodRM32str();
char		*getsibstr();
char		*getmodRMstr();
unsigned long	getVal();
unsigned short	getValData();
unsigned char	check4prefix();
unsigned long	evalValData();
char		*getSegValStr();
char		*getASCIIimmediateStr();
char		*getASCIIdisplcmtStr();
char		*getSegPreStr();
unsigned char	c2opdsizeAdj();
unsigned char	getnbytes();
unsigned char	getgenRegIndex();

char *
newconinst(sp, s)
char	*sp;		/* pointer to assembler nmeumonic string, which will 
			   contain the expanded string to be output (to screen). */
int	s;		/* segment number */
{
	unsigned char	ibp[MAXDISPLCTSIZE];	/* buffer to store read-in instruction/
						   displacement machine code */
	char	 	is[MAXINSTLEN], *isp;	/* buffer to store formatted 
						   instructon string. */
	char		*tsp;			/* temporary string pointer */
	int		loop, useMap2;

	/*
	 * Initialize
	 */
	addrflag = 0;
	opdflag = 0;	
	segflag = 0;	
	modRMloaded = 0;
	sibLoaded = 0;	

	isp = is;

	/*
	 * Check whether any prefixes are present
	 */
	for (loop=1, useMap2=0; loop; ) {
		if (getb(s, ibp, sizeof(char)) == 0)
			return (NULL);
		switch (ibp[0]) {
		case REP:		/* rep instruction prefix */
		case REPNE:		/* repne instruction prefix */
		case LOCK:		/* lock instruction prefix */
			loop = 0;	/* treat instruction prefixe like a */
			break;		/* separate instruction */		
		case ADDRPRE:		/* address size prefix */
			addrflag = 1;
			continue;
		case OPDPRE:		/* operand size prefix */
			opdflag = 1;
			continue;
		case CS_PFX:		/* "cs:" segment override prefix */
		case DS_PFX:		/* "ds:" segment override prefix */
		case ES_PFX:		/* "es:" segment override prefix */
		case FS_PFX:		/* "fs:" segment override prefix */
		case GS_PFX:		/* "gs:" segment override prefix */
		case SS_PFX:		/* "ss:" segment override prefix */
			segPreStrp = opStrMap1[ ibp[0] ]; /* store segment override */ 
			segflag = 1;
			break;
		case ESC_2_OPMAP2:	/* opcode info is in the next byte */
			useMap2 = 1;
			if (getb(s, ibp, sizeof(char)) == 0)
				return (NULL);
			loop = 0;
			break;
		default:
			loop = 0;
		}
	}

	/*
	 * get and evaluate the formated opcode string obtained from the 
	 * opStrMap1&2[]
	 */

	/* get formatted opcode string and store in is[] */
	if ( (tsp = useMap2 ? opStrMap2[ibp[0]] : opStrMap1[ibp[0]]) == NULL ) {
		sprintf(sp, "Invalid instruction byte = %02x (opStrMap%d)\n"
			, ibp[0], useMap2 ? 2 : 1);
		return(	sp = (char *)strchr(sp, '\0') );
	}
	strcpy(isp, tsp);

	/* Expand the formatted opcode string pointed at by isp to produce an 
	   assembler nmeumonic string. Make sure sp is pointing to the current
	   end-of-string after each loop. */
	for ( ; isp[0]; isp++) {
		switch (isp[0]) {
		case '%':
			if ( (isp = fmatPercent(isp, sp, s, ibp)) == NULL) {
				printf("Invalid opcode string.\n");
				return(NULL);
			}
			/* point to end of string */
			sp = (char *)strchr(sp, '\0');
			break;
		case ' ':
			*sp++ = '\t';
			*sp = '\0';
			break;
		default:
			*sp++ = isp[0];
			*sp = '\0';

		}
	}
	return(sp);
}


/*
 * Evalute "%c1c2" and store the expanded results in wb[]. Then, copy wb[] into
 * sp[] and return isp pointing to the next character of the formatted 
 * instruction string. 
 *
 *	CAREFULL! - when doing string manipulations, make sure that wbp
 *		    is pointing at '\0' before breaking from switch statement.
 */
char *
fmatPercent(isp, sp, s, ibp)
char		*isp, *sp;
int		s;
unsigned char	*ibp;
{
	char		wb[MAXINSTLEN], *wbp;	/* work buffer (pointer) */
	unsigned char	c1, c2;			/* 1st & 2nd char after '%' */
	long		deltaVal;
	unsigned long	val;
	char		*tsp1, *tsp2;		/* temporary string pointer */

	c1 = *++isp;
	c2 = *++isp;
	wbp = wb;
	wbp[0] = '\0';
	
	switch (c1) {
	case 'A':
		c2 = c2opdsizeAdj(c2);
		val = getVal(s, ibp, c2, UNSIGNED, ADDZERO, &deltaVal);
		wbp = getSegValStr(ibp, wbp, c2);
		wbp = getASCIIdisplcmtStr(wbp, 'a', UNSIGNED, s, deltaVal, val);
		break;
	case 'C':
		getmodRM(s, ibp, sizeof(char));
		wbp = (char *)strcpy(wbp, ctrlReg[mReg]);
		wbp = (char *)strchr(wbp, '\0');
		break;
	case 'D':
		getmodRM(s, ibp, sizeof(char));
		wbp = (char *)strcpy(wbp, dbgReg[mReg]);
		wbp = (char *)strchr(wbp, '\0');
		break;
	case 'E':
	case 'M':
	case 'R':
		if (getaddrsize() == RM16)
			wbp = getmodRM16str(s, ibp, c2, wbp);
		else
			wbp = getmodRM32str(s, ibp, c2, wbp);
		break;
	case 'G':
		wbp = getmodREGstr(s, ibp, c2, wbp);
		break;
	case 'I':
		*wbp++ = '$';
		c2 = c2opdsizeAdj(c2);
		val = getVal(s, ibp, c2, SIGNED, ADDZERO, &deltaVal);
		wbp = getSegValStr(ibp, wbp, c2);
		wbp = getASCIIimmediateStr(wbp, 'x',
			SIGNED, s, deltaVal, val, c2);
		break;
	case 'J':
		wbp = getSegPreStr(wbp);
		c2 = c2opdsizeAdj(c2);
		val = getVal(s, ibp, c2, SIGNED, ADDADDR, &deltaVal);
		wbp = getSegValStr(ibp, wbp, c2);
		wbp = getASCIIdisplcmtStr(wbp, 'a', SIGNED, s, deltaVal, val);
		break;
	case 'K':
		wbp = getSegPreStr(wbp);
		c2 = c2opdsizeAdj(c2);
		val = getVal(s, ibp, c2, SIGNED, ADDADDR, &deltaVal);
		wbp = getSegValStr(ibp, wbp, c2);
		wbp = getASCIIdisplcmtStr(wbp, 's', SIGNED, s, deltaVal, val);
		break;
/*	case 'M':	 see case 'E' 
		break;			*/
	case 'O':
		wbp = getSegPreStr(wbp);
		c2 = getaddrsize() == RM16 ? 'w' : 'd';
		val = getVal(s, ibp, c2, SIGNED, ADDZERO, &deltaVal);
		wbp = getSegValStr(ibp, wbp, c2);
		wbp = getASCIIdisplcmtStr(wbp, 's', SIGNED, 
			(hdrinfo.defsegatt == RM16 ? 2 : 1), deltaVal, val);
		break;
/*	case 'R':	 see case 'E' 
		break;			*/
	case 'S':
		getmodRM(s, ibp, sizeof(char));
		wbp = (char *)strcpy(wbp, segReg[mReg]);
		wbp = (char *)strchr(wbp, '\0');
		break;
	case 'T':
		getmodRM(s, ibp, sizeof(char));
		wbp = (char *)strcpy(wbp, tstReg[mReg]);
		wbp = (char *)strchr(wbp, '\0');
		break;
	case 'X':
/*		if (getaddrsize()==RM16) {
			sprintf(wbp, "ds:(%%si)");
		}
		else {
			sprintf(wbp, "ds:(%%esi)");
		}
		wbp = (char *)strchr(wbp, '\0');
		break;
*/	case 'Y':
/*		if (getaddrsize()==RM16) {
			sprintf(wbp, "es:(%%di)");
		}
		else {
			sprintf(wbp, "es:(%%edi)");
		}
		wbp = (char *)strchr(wbp, '\0');
*/		break;
	case 'Z':
		c2 = c2opdsizeAdj(c2);
		switch(c2) {
		case 'b':
			*wbp++ = 'b';
			break;
		case 'd':
			*wbp++ = 'l';
			break;
		case 'w':
			*wbp++ = 'w';
			break;
		case 'z':
			*wbp++ = (getopdsize()==RM16 ? 'w': 'l');
			break;
		}
		wbp[0] = '\0';
		break;
	case 'e':
		if (getopdsize()==OPD16) {
			*wbp++ = '%';
			*wbp++ = c2;
			*wbp = '\0';
		}
		else {
			*wbp++ = '%';
			*wbp++ = 'e';
			*wbp++ = c2;
			*wbp = '\0';
		}
		break;
	case 'g':	/* just inserting the group string into isp */
		getmodRM(s, ibp, sizeof(char));
		if (grpStrMap[c2-'0'][mReg] == NULL) {
			sprintf(wbp, "\nInvalid instruction byte = %02x (grpStrMap[%d][%d])\n"
				, ibp[0], c2-'0', mReg);
			return(	wbp = (char *)strchr(wbp, '\0') );
		}
		tsp1 = malloc(MAXINSTLEN);
		tsp2 = isp;
		strcpy(tsp1, isp+1);
		strcpy(isp+1, grpStrMap[c2-'0'][mReg]);
		isp = (char *)strchr(isp+1, '\0');
		strcpy(isp, tsp1);
		isp = tsp2;
		free(tsp1);
		*wbp = '\0';
		break;
	case 'r':
		if (getb(s, ibp, sizeof(char)) == 0)
			return (NULL);
		break;
	default:
		*wbp++ = c1;
		*wbp++ = c2;
		*wbp = '\0';
	}

	sp = (char *)strcpy(sp, wb);

	return(isp);
}

char	*
getmodREGstr(s, ibp, c2, wbp)
int		s;
unsigned char	*ibp;
char		c2, *wbp;
{
	getmodRM(s, ibp, sizeof(char));
	strcpy(wbp, genReg[getgenRegIndex(c2)][mReg]);
	wbp = (char *)strchr(wbp, '\0');
	return(wbp);
}

char	*
getmodRM16str(s, ibp, c2, wbp)
int		s;
unsigned char	*ibp;
char		c2, *wbp;
{
	unsigned char	genRegIndex;
	long		deltaVal;
	unsigned long	val;

	getmodRM(s, ibp, sizeof(char));
	c2 = c2opdsizeAdj(c2);

	switch(m) {
	case 0:
		if (mRM==6) {
			wbp = getSegPreStr(wbp);
			val = getVal(s, ibp, 'w', SIGNED, ADDZERO, &deltaVal);
			wbp = getSegValStr(ibp, wbp, 'w');
			wbp = getASCIIdisplcmtStr(wbp, 's', SIGNED, 
			    (hdrinfo.defsegatt == RM16 ? 2 : 1), deltaVal, val);
			break;
		}
		wbp = getSegPreStr(wbp);
		wbp = (char *)strcpy(wbp, modRMtab16[mRM] );
		wbp = (char *)strchr(wbp, '\0');
		break;
	case 1:
		wbp = getmodRMstr(s, ibp, wbp, 'b', 'd', RM16, 1);
		break;
	case 2:
		wbp = getmodRMstr(s, ibp, wbp, 'w', 'd', RM16, 1);
		break;
	case 3:
		genRegIndex = getgenRegIndex(c2);

		wbp = (char *)strcpy(wbp, genReg[genRegIndex][mRM]);
		wbp = (char *)strchr(wbp, '\0');
		break;
	}
	return(wbp);
}

char	*
getmodRM32str(s, ibp, c2, wbp)
int		s;
unsigned char	*ibp;
char		c2, *wbp;
{
	unsigned char	genRegIndex;
	long		deltaVal;
	unsigned long	val;

	getmodRM(s, ibp, sizeof(char));
	c2 = c2opdsizeAdj(c2);

	switch(m) {
	case 0:
		if (mRM==4) {
			wbp = getsibstr(s, ibp, wbp, '\0', m);
			break;
		}
		if (mRM==5) {
			wbp = getSegPreStr(wbp);
			val = getVal(s, ibp, 'd', SIGNED, ADDZERO, &deltaVal);
			wbp = getSegValStr(ibp, wbp, 'd');
			wbp = getASCIIdisplcmtStr(wbp, 's', SIGNED, 
			    (hdrinfo.defsegatt == RM16 ? 2 : 1), deltaVal, val);
			break;
		}
		wbp = getSegPreStr(wbp);
		wbp = (char *)strcpy(wbp, modRMtab32[mRM] );
		wbp = (char *)strchr(wbp, '\0');
		break;
	case 1:
		if (mRM==4) {
			wbp = getsibstr(s, ibp, wbp, 'b', m);
			break;
		}
		wbp = getmodRMstr(s, ibp, wbp, 'b', 'd', RM32, 1);
		break;
	case 2:
		if (mRM==4) {
			wbp = getsibstr(s, ibp, wbp, 'd', m);
			break;
		}
		wbp = getmodRMstr(s, ibp, wbp, 'd', 'd', RM32, 1);
		break;
	case 3:
		genRegIndex = getgenRegIndex(c2);

		wbp = (char *)strcpy(wbp, genReg[genRegIndex][mRM]);
		wbp = (char *)strchr(wbp, '\0');
		break;
	}
	return(wbp);
}

char	*
getmodRMstr(s, ibp, wbp, displctsize, form, addrsize, signedVal)
int		s;
unsigned char	*ibp;
char		*wbp;
unsigned char	displctsize, form, addrsize, signedVal;
{
	long		deltaVal;
	unsigned long	val;

	wbp = getSegPreStr(wbp);

	val = getVal(s, ibp, displctsize, signedVal, ADDZERO, &deltaVal);
	wbp = getSegValStr(ibp, wbp, displctsize);
	wbp = getASCIIdisplcmtStr(wbp, form, signedVal, s, deltaVal, val);

	wbp = (char *)strcpy(wbp, modRMtab[ADDRINDEX(addrsize)][mRM] );
	wbp = (char *)strchr(wbp, '\0');

	return(wbp);
}

char	*
getsibstr(s, ibp, wbp, displctsize, m)
int		s;
unsigned char	*ibp;
char		*wbp;
unsigned char	displctsize, m;
{
	long		deltaVal;
	unsigned long	val;

	getsib(s, ibp, sizeof(char));

	switch(m) {
	case 0:
		wbp = getSegPreStr(wbp);
		if (sb == EBP_SIB) {
			val = getVal(s, ibp, 'd', SIGNED, ADDZERO, &deltaVal);
			wbp = getSegValStr(ibp, wbp, 'd');
			wbp = getASCIIdisplcmtStr(wbp, 'd', SIGNED, s, deltaVal, val);
		}
		else {
			wbp = (char *)strcpy(wbp, modRMtab[ADDRINDEX(RM32)][sb]);
			wbp = (char *)strchr(wbp, '\0');
		}
		break;
	case 1:
		wbp = getSegPreStr(wbp);

		val = getVal(s, ibp, displctsize, SIGNED, ADDZERO, &deltaVal);
		wbp = getSegValStr(ibp, wbp, displctsize);
		wbp = getASCIIdisplcmtStr(wbp, 'd', SIGNED, s, deltaVal, val);

		wbp = (char *)strcpy(wbp, modRMtab[ADDRINDEX(RM32)][sb]);
		wbp = (char *)strchr(wbp, '\0');
		break;
	case 2:
		wbp = getSegPreStr(wbp);

		val = getVal(s, ibp, displctsize, SIGNED, ADDZERO, &deltaVal);
		wbp = getSegValStr(ibp, wbp, displctsize);
		wbp = getASCIIdisplcmtStr(wbp, 'd', SIGNED, s, deltaVal, val);

		wbp = (char *)strcpy(wbp, modRMtab[ADDRINDEX(RM32)][sb]);
		wbp = (char *)strchr(wbp, '\0');
		break;
	}
	if (sibtab[sss][sx] != NULL) {
		wbp = (char *)strcpy(wbp, sibtab[sss][sx]);
		wbp = (char *)strchr(wbp, '\0');
	}
	return(wbp);
}

unsigned long
getVal(s, ibp, mysizeof, signedVal, addVal, deltaValp)
int		s;
unsigned char	*ibp;
unsigned char	mysizeof, signedVal;
unsigned char	addVal;
long		*deltaValp;
{
	unsigned char	nbytes;
	unsigned long	val;

	if ( (nbytes = getValData(s, ibp, signedVal, mysizeof)) == 0)
		return(ADDZERO);
	nbytes = check4prefix(mysizeof, nbytes);
	val = evalValData(ibp, signedVal, nbytes, addVal, deltaValp);
	return(val);
}

unsigned short
getValData(s, ibp, signedVal, mysizeof)
int		s;
unsigned char	*ibp, signedVal;
unsigned char	mysizeof;
{
	unsigned char	nbytes, i;

	if ( (nbytes = getnbytes(mysizeof)) == 0 )
		return(0);

	if ( (getb(s, ibp, nbytes)) == 0 ) {
		printf("Unable to read opcode displacement or immediate data\n");
		return (0);
	}

	for (i=nbytes; i<MAXDISPLCTSIZE; i++) {
		if (signedVal)
			ibp[i] = (ibp[i-1] & 0x80) ? 0xFF : 0;
		else
			ibp[i] = 0;
	}
	return(nbytes);
}

unsigned char
check4prefix(mysizeof, nbytes)
{
	switch(mysizeof) {
	case 'f':		/* 32 bit pointer */
	case 'l':		/* 48 bit pointer */
		nbytes -= 2;
		break;
	}
	return(nbytes);
}

unsigned long
evalValData(ibp, signedVal, nbytes, addVal, deltaValp)
unsigned char	*ibp;
unsigned char	signedVal, nbytes;
unsigned char	addVal;
long		*deltaValp;
{
	unsigned long	baseVal, val;

	if (addVal)
		baseVal = add;
	else	baseVal = 0L;

	switch (nbytes) {
		case DBBYTE:
			if (signedVal) {
				*deltaValp = *(signed char *)ibp;
				if ((signed long)*deltaValp >= 0)
					val = baseVal + *deltaValp;
				else
					val = baseVal	
					    - (unsigned long)
						(-(signed long)*deltaValp);
			}
			else {
				*deltaValp = *(unsigned char *)ibp;
				val = baseVal + *deltaValp;
			}
			break;		
		case DBWORD:
			if (signedVal) {
				*deltaValp = *(signed short *)ibp;
				if ((signed long)*deltaValp >= 0)
					val = baseVal + *deltaValp;
				else
					val = baseVal	
					    - (unsigned long)
						(-(signed long)*deltaValp);
			}
			else {
				*deltaValp = *(unsigned short *)ibp;
				val = baseVal + *deltaValp;
			}
			break;		
		case DBLONG:
			*deltaValp = *(long *)ibp;
			if (signedVal) {
				if ((signed long)*deltaValp >= 0)
					val = baseVal + *deltaValp;
				else
					val = baseVal	
					    - (unsigned long)
						(-(signed long)*deltaValp);
			}
			else {
				val = baseVal + *deltaValp;
			}
			break;		
	}	
	return(val);
}

char	*
getSegValStr(ibp, wbp, mysizeof)
unsigned char	*ibp;
char		*wbp;
unsigned char	mysizeof;
{
	unsigned char	nbytes;

	nbytes = getnbytes(mysizeof);

	switch(mysizeof) {
	case 'f':		/* 32 bit pointer */
	case 'l':		/* 48 bit pointer */
		sprintf(wbp, "0x%02x%02x:", ibp[nbytes-1], ibp[nbytes-2]);
		wbp = (char *)strchr(wbp, '\0');
		break;
	}
	return(wbp);
}

char	*
getASCIIimmediateStr(wbp, form, signedVal, segn, deltaVal, val, mysizeof)
char		*wbp;
unsigned char	form, signedVal;
int		segn;
signed long	deltaVal;
unsigned long	val;
unsigned char	mysizeof;
{
	switch (getnbytes(mysizeof)) {
	case DBBYTE:
		val &= 0xFF;
		break;
	case DBWORD:
		val &= 0xFFFF;
		break;
	}
	getASCIIdisplcmtStr(wbp, form, signedVal, segn, deltaVal, val);
}

char	*
getASCIIdisplcmtStr(wbp, form, signedVal, segn, deltaVal, val)
char		*wbp;
unsigned char	form, signedVal;
int		segn;
signed long	deltaVal;
unsigned long	val;
{
	if (signedVal && deltaVal<0)
		deltaVal = -deltaVal;
	switch(form) {
	case 'a':	/* print address */
		if ( deltaVal > TOLSYM)		/* print out symbol, if possible */
			conaddr(wbp, segn, val, I);
		else {			/* Print out address in hex format */
			*wbp = '(';
			printaddr(wbp, val);
			wbp = (char *)strchr(wbp, '\0');
			*wbp = ')';
			*wbp = '\0';
		}
		break;
	case 'd':					/* print decimal */
		if (!val)
			break;
/*		if (deltaVal > TOLSYM) {
			conaddr(wbp, segn, val, I);
		}
		else {
*/			if (getopdsize()==OPD16) {
				sprintf(wbp, "%d", val);
			}
			else {
				sprintf(wbp, "%ld", val);
			}
/*		}
*/		break;
	case 's':	/* if available, print symbol; otherwise print address */
		conaddr(wbp, segn, val, I);
		break;
	case 'x':					/* print hex */
/*		if (deltaVal > TOLSYM) {
			conaddr(wbp, segn, val, I);
		}
		else {
*/			if (getopdsize()==OPD16) {
				if (deltaVal > TOLHEX) {
					sprintf(wbp, "0x%x", val);
				}
				else {
					sprintf(wbp, "%d", val);
				}
			}
			else {
				if (deltaVal > TOLHEX) {
					sprintf(wbp, "0x%lx", val);
				}
				else {
					sprintf(wbp, "%ld", val);
				}
			}
/*		}
*/		break;
	default:
		if(1) printf("invalid form = %c;  ", form);
	}

	wbp = (char *)strchr(wbp, '\0');
	return(wbp);
}

printaddr(wbp, v)
char		*wbp;
unsigned long	v;
{
	if (getopdsize()==OPD16) {
		sprintf(wbp, "%04x", v);
	}
	else {
		sprintf(wbp, "%04X", v);
	}
}

char *
getSegPreStr(wbp)
char	*wbp;
{
	if (segflag) {
		strcpy(wbp, segPreStrp);
		wbp = (char *)strchr(wbp, '\0');
	}
	return(wbp);
}

unsigned char c2opdsizeAdj(c2)
unsigned char c2;
{
	if (getopdsize()==RM16) {
		switch(c2) {			/* set to addr16 lengths */
		case 'a':
			c2 = 'w';
			break;
		case 'p':
			c2 = 'f';
			break;
		case 'v':
			c2 = 'w';
			break;
		}
	}
	else {
		switch(c2) {			/* set to addr32 lengths */
		case 'a':
			c2 = 'd';
			break;
		case 'p':
			c2 = 'l';
			break;
		case 'v':
			c2 = 'd';
			break;
		}
	}
	return(c2);
}

unsigned char
getnbytes(mysizeof)
unsigned char mysizeof;
{
	unsigned char nbytes;

	switch(mysizeof) {
	case 'b':
		nbytes = DBBYTE;
		break;
	case 'd':
		nbytes = DBLONG;
		break;
	case 'f':
		nbytes = DBPTR32;
		break;
	case 'l':
		nbytes = DBPTR48;
		break;
	case 'w':
		nbytes = DBWORD;
		break;
	case '\0':
		nbytes = 0;
		break;
	default:
		if(1) printf("invalid nbytes = %c;  "
			"a 1 byte size is chosen by default\n"
			, mysizeof);
		nbytes = DBBYTE;
	}
	return(nbytes);
}

unsigned char
getgenRegIndex(c2)
unsigned char	c2;
{
	unsigned char genRegIndex;

	switch(c2) {
	case 'a':
		if (getopdsize() == OPD16)
			genRegIndex = 1;
		else
			genRegIndex = 2;
		break;
	case 'b':
		genRegIndex = 0;
		break;
	case 'd':
		genRegIndex = 2;
		break;
	case 'v':
		if (getopdsize() == OPD16)
			genRegIndex = 1;
		else
			genRegIndex = 2;
		break;
	case 'w':
		genRegIndex = 1;
		break;
	default:
		if(1) printf("invalid c2 = %c;  "
			"a 1 byte reg is shown by default\n"
			, c2);
		genRegIndex = 0;
	}
	return(genRegIndex);
}

getaddrsize()
{
	return( addrflag ? 48 - hdrinfo.defsegatt : hdrinfo.defsegatt );
}

getopdsize()
{
	return( opdflag ? 48 - hdrinfo.defsegatt : hdrinfo.defsegatt );
}

getmodRM(s, ibp, c)
unsigned char	*ibp;
unsigned	c;
{
	if (!modRMloaded) {
		if (getb(s, ibp, c) == 0)
			return (NULL);
		m = mod(ibp[0]);
		mRM = modRM(ibp[0]);
		mReg = modR(ibp[0]);
		modRMloaded = 1;
	}
}

getsib(s, ibp, c)
unsigned char	*ibp;
unsigned	c;
{
	if (!sibLoaded) {
		if ((getb(s, ibp, c)) == 0)
			return (NULL);
		sss = sibss(ibp[0]);
		sx = sibx(ibp[0]);
		sb = sibb(ibp[0]);
		sibLoaded = 1;
	}
}

/*
 * Given a size character, `t1', and a type, `t2', return the appropriate
 * format string.
 */
char *
getform(t1, t2)
register int t1;
register int t2;
{
	register char *cp;
	register char *sp;

	if (t1=='f' || t1=='F')
		return ("%g");
	if (t1 == 'h')
		t1 = 'w';
	if ((cp=strchr(sp="bwlv", t1)) == NULL)
		return ("?");
	t1 = cp-sp;
	if ((cp=strchr(sp="duox", t2)) == NULL)
		return ("?");
	t2 = cp-sp;
	return (formtab[t1][t2]);
}

/* End of i8086b1.c */
