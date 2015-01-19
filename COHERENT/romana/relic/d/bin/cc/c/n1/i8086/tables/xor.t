/////////
/
/ Logical exclusive or.
/ There is no long XOR instruction.
/ An attempt is made to optimize long operations.
/ An XOR with a word of 0 bits is a nop.
/ An XOR with a word of 1 bits is a ones complement, and is done with a 'not'.
/
/////////

XOR:
%	PEFFECT|PRVALUE|PREL|P_SLT
	WORD		ANYR	ANYR	*	TEMP
		TREG		WORD
		ADR|IMM		WORD
			[ZXOR]	[R],[AR]
		[IFR]	[REL0]	[LAB]

%	PEFFECT|PRVALUE|P_SLT
	LONG		ANYR	ANYR	*	TEMP
		TREG		LONG
		UHC|MMX		LONG
			[ZXOR]	[LO R],[LO AR]

%	PEFFECT|PRVALUE|P_SLT
	LONG		ANYR	ANYR	*	TEMP
		TREG		LONG
		UHS|MMX		LONG
			[ZXOR]	[LO R],[LO AR]
			[ZNOT]	[HI R]

%	PEFFECT|PRVALUE|PGE|PLT|P_SLT
	LONG		ANYR	ANYR	*	TEMP
		TREG		LONG
		LHC|MMX		LONG
			[ZXOR]	[HI R],[HI AR]
		[IFR]	[REL1]	[LAB]

%	PEFFECT|PRVALUE|PGE|PLT|P_SLT
	LONG		ANYR	ANYR	*	TEMP
		TREG		LONG
		LHS|MMX		LONG
			[ZNOT]	[LO R]
			[ZXOR]	[HI R],[HI AR]
		[IFR]	[REL1]	[LAB]

%	PEFFECT|PRVALUE|PGE|PLT|P_SLT
	LONG		ANYR	ANYR	*	TEMP
		TREG		LONG
		ADR|IMM		LONG
			[ZXOR]	[LO R],[LO AR]
			[ZXOR]	[HI R],[HI AR]
		[IFR]	[REL1]	[LAB]
