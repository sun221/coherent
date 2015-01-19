/////////
/
/ Shift right. This is longer than the code table for shift left because
/ signed and unsigned shift right generate different code.
/ The table must be different from the left shift code table
/ in any case (even if a conditional were added for signed and unsigned)
/ because the halves of long integers get swapped around.
/ Short shifts (by 1 or 2 for 8086, by byte for 80286) are done inline.
/ Longer shifts load CL with the shift count and use a variable bit shift.
/ Longs are more difficult; a short loop is compiled to do the shift.
/
/////////

SHR:
%	PEFFECT|PRVALUE|PREL|P_SLT|P80186
	WORD		ANYR	ANYR	*	TEMP
		TREG		FS16
		BYTE|MMX	WORD
			[ZSAR]	[R],[AR]
		[IFR]	[REL0]	[LAB]

%	PEFFECT|PRVALUE|PREL|P_SLT|P80186
	WORD		ANYR	ANYR	*	TEMP
		TREG		UWORD
		BYTE|MMX	WORD
			[ZSHR]	[R],[AR]
		[IFR]	[REL0]	[LAB]

%	PEFFECT|PRVALUE|PREL|P_SLT
	WORD		ANYR	ANYR	*	TEMP
		TREG		FS16
		1|MMX		*
			[ZSAR]	[R],[CONST 1]
		[IFR]	[REL0]	[LAB]

%	PEFFECT|PRVALUE|PREL|P_SLT
	WORD		ANYR	ANYR	*	TEMP
		TREG		FS16
		2|MMX		*
			[ZSAR]	[R],[CONST 1]
			[ZSAR]	[R],[CONST 1]
		[IFR]	[REL0]	[LAB]

%	PEFFECT|PRVALUE|PREL|P_SLT
	WORD		ANYR	ANYR	*	TEMP
		TREG		UWORD
		1|MMX		*
			[ZSHR]	[R],[CONST 1]
		[IFR]	[REL0]	[LAB]

%	PEFFECT|PRVALUE|PREL|P_SLT
	WORD		ANYR	ANYR	*	TEMP
		TREG		UWORD
		2|MMX		*
			[ZSHR]	[R],[CONST 1]
			[ZSHR]	[R],[CONST 1]
		[IFR]	[REL0]	[LAB]

/////////
/
/ Non trivial word shifts.
/ Load CL with count and use variable format shift instruction.
/ If right is a constant, use a MOVB to load CL; saves a byte.
/
/////////
%	PEFFECT|PRVALUE|PREL|P_SLT
	WORD		AX	AX	CX	TEMP
		TREG		FS16
		IMM|MMX		WORD
			[ZMOVB]	[REGNO CL],[AR]
			[ZSAR]	[R],[REGNO CL]
		[IFR]	[REL0]	[LAB]

%	PEFFECT|PRVALUE|PREL|P_SLT
	WORD		AX	AX	CX	TEMP
		TREG		UWORD
		IMM|MMX		WORD
			[ZMOVB]	[REGNO CL],[AR]
			[ZSHR]	[R],[REGNO CL]
		[IFR]	[REL0]	[LAB]

%	PEFFECT|PRVALUE|P_SLT
	WORD		ANYR	ANYR	CX	TEMP
		TREG		FS16
		ADR		WORD
			[ZMOV]	[REGNO CX],[AR]
			[ZSAR]	[R],[REGNO CL]

%	PEFFECT|PRVALUE|P_SLT
	WORD		ANYR	ANYR	CX	TEMP
		TREG		UWORD
		ADR		WORD
			[ZMOV]	[REGNO CX],[AR]
			[ZSHR]	[R],[REGNO CL]

/////////
/
/ Non trivial long shifts.
/ Load count to CX and use a loop.
/ Shifts of 0 have been deleted by the optimizer;
/ this means the JCXZ instruction is not needed if the right
/ operand is a constant.
/
/////////

%	PEFFECT|PRVALUE|P_SLT
	LONG		DXAX	DXAX	CX	DXAX
		TREG		FS32
		IMM|MMX		WORD
			[ZMOV]	[REGNO CX],[AR]
		[DLAB0]:[ZSAR]	[HI R],[CONST 1]
			[ZRCR]	[LO R],[CONST 1]
			[ZLOOP]	[LAB0]

%	PEFFECT|PRVALUE|P_SLT
	LONG		DXAX	DXAX	CX	DXAX
		TREG		FU32
		IMM|MMX		WORD
			[ZMOV]	[REGNO CX],[AR]
		[DLAB0]:[ZSHR]	[HI R],[CONST 1]
			[ZRCR]	[LO R],[CONST 1]
			[ZLOOP]	[LAB0]

%	PEFFECT|PRVALUE|P_SLT
	LONG		ANYR	ANYR	CX	TEMP
		TREG		FS32
		ADR		WORD
			[ZMOV]	[REGNO CX],[AR]
			[ZJCXZ]	[LAB0]
		[DLAB1]:[ZSAR]	[HI R],[CONST 1]
			[ZRCR]	[LO R],[CONST 1]
			[ZLOOP]	[LAB1]
		[DLAB0]:

%	PEFFECT|PRVALUE|P_SLT
	LONG		ANYR	ANYR	CX	TEMP
		TREG		FU32
		ADR		WORD
			[ZMOV]	[REGNO CX],[AR]
			[ZJCXZ]	[LAB0]
		[DLAB1]:[ZSHR]	[HI R],[CONST 1]
			[ZRCR]	[LO R],[CONST 1]
			[ZLOOP]	[LAB1]
		[DLAB0]:
