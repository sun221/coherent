/////////
/
/ Addition.
/ Check for +1 (inc) and +2 (inc, inc; thanks to Mac).
/ Handle LARGE model addition of integers and pointers,
/ which are OFFSET adjustments only.
/
/////////

ADD:
%	PEFFECT|PRVALUE|PSREL|P_SLT
	WORD		ANYR	ANYR	*	TEMP
		TREG		WORD
		1|MMX		*
%	PLVALUE|P_SLT
	WORD		ANYL	ANYL	*	TEMP
		TREG		WORD
		1|MMX		*
			[ZINC]	[R]
		[IFR]	[REL0]	[LAB]

#ifndef ONLYSMALL
%	PEFFECT|PRVALUE|P_SLT
	LPTX		ANYR	ANYR	*	TEMP
		TREG		LPTX
		1|MMX		*
%	PLVALUE|P_SLT
	LPTX		ANYL	ANYL	*	TEMP
		TREG		LPTX
		1|MMX		*
			[ZINC]	[LO R]
#endif

%	PEFFECT|PRVALUE|PSREL|P_SLT
	WORD		ANYR	ANYR	*	TEMP
		TREG		WORD
		2|MMX		*
%	PLVALUE|P_SLT
	WORD		ANYL	ANYL	*	TEMP
		TREG		WORD
		2|MMX		*
			[ZINC]	[R]
			[ZINC]	[R]
		[IFR]	[REL0]	[LAB]

#ifndef ONLYSMALL
%	PEFFECT|PRVALUE|P_SLT
	LPTX		ANYR	ANYR	*	TEMP
		TREG		LPTX
		2|MMX		*
%	PLVALUE|P_SLT
	LPTX		ANYL	ANYL	*	TEMP
		TREG		LPTX
		2|MMX		*
			[ZINC]	[LO R]
			[ZINC]	[LO R]
#endif

%	PEFFECT|PRVALUE|PSREL|P_SLT
	WORD		ANYR	ANYR	*	TEMP
		TREG		WORD
		ADR|IMM		WORD
%	PLVALUE|P_SLT
	WORD		ANYL	ANYL	*	TEMP
		TREG		WORD
		ADR|IMM		WORD
			[ZADD]	[R],[AR]
		[IFR]	[REL0]	[LAB]

#ifndef ONLYSMALL
%	PEFFECT|PRVALUE|P_SLT
	LPTX		ANYR	ANYR	*	TEMP
		TREG		LPTX
		ADR|IMM		WORD
%	PLVALUE|P_SLT
	LPTX		ANYL	ANYL	*	TEMP
		TREG		LPTX
		ADR|IMM		WORD
			[ZADD]	[LO R],[AR]
#endif

/////////
/
/ Index like contexts.
/ The hard tree is an integer and the
/ result is a pointer. There are a lot of
/ cases here, mainly to make sure the
/ correct segment base gets moved over to
/ the result.
/
/////////

#ifndef ONLYSMALL
%	PEFFECT|PRVALUE
	LPTX		DXAX	AX	*	TEMP
		TREG		WORD
		ICN|MMX		LPTX
			[ZADD]	[LO R],[AR]
			[ZSUB]	[HI R],[HI R]

%	PLVALUE
	LPTX		ANYL	LOTEMP	AX	TEMP
		TREG		WORD
		ICN|MMX		LPTX
			[ZADD]	[LO R],[AR]
			[ZSUB]	[REGNO AX],[REGNO AX]
			[ZMOV]	[HI R],[REGNO AX]

%	PEFFECT|PRVALUE
	LPTX		DXAX	AX	*	TEMP
		TREG		WORD
		LCN|MMX		LPTX
			[ZADD]	[LO R],[LO AR]
			[ZSUB]	[HI R],[HI R]

%	PLVALUE
	LPTX		ANYL	LOTEMP	AX	TEMP
		TREG		WORD
		LCN|MMX		LPTX
			[ZADD]	[LO R],[LO AR]
			[ZMOV]	[REGNO AX],[HI AR]
			[ZMOV]	[HI R],[REGNO AX]

%	PEFFECT|PRVALUE
	LPTX		DXAX	AX	*	TEMP
		TREG		WORD
		ACS|MMX		LPTX
			[ZADD]	[LO R],[AR]
			[ZMOV]	[HI R],[REGNO CS]

%	PEFFECT|PRVALUE
	LPTX		DXAX	AX	*	TEMP
		TREG		WORD
		ADS|MMX		LPTX
			[ZADD]	[LO R],[AR]
			[ZMOV]	[HI R],[REGNO DS]

%	PLVALUE
	LPTX		ANYL	LOTEMP	*	TEMP
		TREG		WORD
		ACS|MMX		LPTX
			[ZADD]	[LO R],[AR]
			[ZPUSH]	[REGNO CS]
			[ZPOP]	[HI R]

%	PLVALUE
	LPTX		ANYL	LOTEMP	*	TEMP
		TREG		WORD
		ADS|MMX		LPTX
			[ZADD]	[LO R],[AR]
			[ZPUSH]	[REGNO DS]
			[ZPOP]	[HI R]

%	PEFFECT|PRVALUE
	LPTX		DXAX	AX	*	DXAX
		TREG		WORD
		REG|MMX		LPTX
			[ZADD]	[LO R],[LO AR]
			[ZMOV]	[HI R],[HI AR]

%	PLVALUE
	LPTX		ANYL	LOTEMP	*	TEMP
		TREG		WORD
		REG|MMX		LPTX
			[ZADD]	[LO R],[LO AR]
			[ZPUSH]	[HI AR]
			[ZPOP]	[HI R]

%	PEFFECT|PRVALUE
	LPTX		DXAX	AX	*	DXAX
		TREG		WORD
		ADR		LPTX
%	PLVALUE
	LPTX		ANYL	LOTEMP	*	TEMP
		TREG		WORD
		ADR		LPTX
			[ZADD]	[LO R],[LO AR]
			[ZMOV]	[HI R],[HI AR]
#endif

%	PEFFECT|PRVALUE|PGE|PLT|P_SLT
	LONG		DXAX	DXAX	*	DXAX
		TREG		LONG
		ADR|IMM		LONG
			[ZADD]	[LO R],[LO AR]
			[ZADC]	[HI R],[HI AR]
		[IFR]	[REL1]	[LAB]

#ifndef ONLYSMALL
%	PEFFECT|PRVALUE|P_SLT
	LPTX		ANYR	ANYR	*	TEMP
		TREG		LPTX
		ADR|IMM		LONG
%	PLVALUE|P_SLT
	LPTX		ANYL	ANYL	*	TEMP
		TREG		LPTX
		ADR|IMM		LONG
			[ZADD]	[LO R],[LO AR]

%	PEFFECT|PRVALUE
	LPTX		DXAX	DXAX	*	DXAX
		TREG		LONG
		ADR|IMM		LPTX
%	PLVALUE
	LPTX		ESAX	DXAX	*	DXAX
		TREG		LONG
		ADR|IMM		LPTX
			[ZADD]	[LO R],[LO AR]
			[ZMOV]	[HI R],[HI AR]
#endif

/////////
/
/ These special table entries make pushing offsts into structures and
/ arrays more efficient. In particular, they improve the code for some of
/ the standard I/O macros, when aimed at constant streams (like the stdin,
/ stdout and stder streams).
/
/////////

#ifndef ONLYSMALL
%	PFNARG
	LPTX		AX	*	*	NONE
		ADR		LPTX
		IMM|MMX		WORD
			[ZPUSH]	[HI AL]
			[ZMOV]	[R],[LO AL]
			[ZADD]	[R],[AR]
			[ZPUSH]	[R]

%	PFNARG
	LPTX		AX	*	*	NONE
		IMM|MMX		WORD
		ADR		LPTX
			[ZPUSH]	[HI AR]
			[ZMOV]	[R],[LO AR]
			[ZADD]	[R],[AL]
			[ZPUSH]	[R]
#endif

////////
/
/ Floating point, using the numeric
/ data coprocessor (8087).
/
////////

#ifdef NDPDEF
%	PRVALUE|P_SLT
	FF32|FF64	FPAC	FPAC	*	FPAC
		TREG		FF64
		ADR		FS16
			[ZFADDI] [AR]

%	PRVALUE|P_SLT
	FF32|FF64	FPAC	FPAC	*	FPAC
		TREG		FF64
		ADR		FS32
			[ZFADDL] [AR]

%	PRVALUE|P_SLT
	FF32|FF64		FPAC	FPAC	*	FPAC
		TREG		FF64
		ADR		FF32
			[ZFADDF] [AR]

%	PRVALUE|P_SLT
	FF32|FF64	FPAC	FPAC	*	FPAC
		TREG		FF64
		ADR		FF64
			[ZFADDD] [AR]
#endif
