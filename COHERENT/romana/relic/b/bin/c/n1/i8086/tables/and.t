/////////
/
/ Logical AND.
/ There is no logical AND instruction for long integers.
/ There are a number of special cases;
/ most try to eliminate AND with words of 0 or with words of all 1 bits.
/ Because of the '!' operator you must not use REL0 or REL1
/ if EQ and NE are not in the same table.
/
/////////

AND:
%	PSREL
	*		NONE	*	*	NONE
		ADR		WORD
		IMM|MMX		WORD
%	PSREL
	*		NONE	*	*	NONE
		RREG|MMX	WORD
		ADR|IMM		WORD
			[ZTEST]	[AL],[AR]
			[REL0]	[LAB]

%	PSREL
	*		NONE	*	*	NONE
		ADR		BYTE
		IMM|MMX		WORD
			[ZTESTB] [AL],[AR]
			[REL0]	[LAB]

%	PEFFECT|PRVALUE|PEREL|P_SLT
	WORD		ANYR	ANYR	*	TEMP
		TREG		WORD
		ADR|IMM		WORD
			[ZAND]	[R],[AR]
		[IFR]	[REL0]	[LAB]

%	PLT|PGE
	*		NONE	*	*	NONE
		ADR		LONG
		IMM|MMX		LONG
			[ZTEST]	[HI AL],[HI AR]
			[REL1]	[LAB]

%	PLT|PGE
	*		AX	*	*	NONE
		ADR		LONG
		ADR|IMM		LONG
			[ZMOV]	[R],[HI AL]
			[ZTEST]	[R],[HI AR]
			[REL1]	[LAB]

%	PLT|PGE
	*		*	ANYR	*	NONE
		TREG		LONG
		ADR|IMM		LONG
			[ZTEST]	[HI RL],[HI AR]
			[REL1]	[LAB]

%	PEQ|PNE
	*		NONE	*	*	NONE
		ADR|IMM		LONG
		UHC|MMX		LONG
			[ZTEST]	[LO AL],[LO AR]
			[REL0]	[LAB]

%	PEQ|PNE
	*		NONE	*	*	NONE
		ADR|IMM		LONG
		LHC|MMX		LONG
			[ZTEST]	[HI AL],[HI AR]
			[REL0]	[LAB]

%	PEQ|PNE
	*		*	ANYR	*	NONE
		TREG		LONG
		UHC|MMX		LONG
			[ZTEST]	[LO RL],[LO AR]
			[REL0]	[LAB]

%	PEQ|PNE
	*		*	ANYR	*	NONE
		TREG		LONG
		LHC|MMX		LONG
			[ZTEST]	[HI RL],[HI AR]
			[REL0]	[LAB]

%	PNE
	*		NONE	*	*	NONE
		ADR|IMM		LONG
		IMM|MMX		LONG
			[ZTEST]	[LO AL],[LO AR]
			[ZJNE]	[LAB]
			[ZTEST]	[HI AL],[HI AR]
			[ZJNE]	[LAB]

%	PNE
	*		*	ANYR	*	NONE
		TREG		LONG
		ADR|IMM		LONG
			[ZTEST]	[LO RL],[LO AR]
			[ZJNE]	[LAB]
			[ZTEST]	[HI RL],[HI AR]
			[ZJNE]	[LAB]

%	PEQ
	*		NONE	*	*	NONE
		ADR|IMM		LONG
		IMM|MMX		LONG
			[ZTEST]	[LO AL],[LO AR]
			[ZJNE]	[LAB0]
			[ZTEST]	[HI AL],[HI AR]
			[ZJE]	[LAB]
		[DLAB0]:

%	PEQ
	*		*	ANYR	*	NONE
		TREG		LONG
		ADR|IMM		LONG
			[ZTEST]	[LO RL],[LO AR]
			[ZJNE]	[LAB0]
			[ZTEST]	[HI RL],[HI AR]
			[ZJE]	[LAB]
		[DLAB0]:

%	PEFFECT|PRVALUE|P_SLT
	LONG		ANYR	ANYR	*	TEMP
		TREG		LONG
		UHS|MMX		LONG
			[ZAND]	[LO R],[LO AR]

%	PEFFECT|PRVALUE|P_SLT
	LONG		ANYR	ANYR	*	TEMP
		TREG		LONG
		UHC|MMX		LONG
			[ZAND]	[LO R],[LO AR]
			[ZSUB]	[HI R],[HI R]

%	PEFFECT|PRVALUE|P_SLT
	LONG		ANYR	ANYR	*	TEMP
		TREG		LONG
		LHS|MMX		LONG
			[ZAND]	[HI R],[HI AR]

%	PEFFECT|PRVALUE|P_SLT
	LONG		ANYR	ANYR	*	TEMP
		TREG		LONG
		LHC|MMX		LONG
			[ZSUB]	[LO R],[LO R]
			[ZAND]	[HI R],[HI AR]

%	PEFFECT|PRVALUE|P_SLT
	LONG		ANYR	ANYR	*	TEMP
		TREG		LONG
		ADR|IMM		LONG
			[ZAND]	[LO R],[LO AR]
			[ZAND]	[HI R],[HI AR]
