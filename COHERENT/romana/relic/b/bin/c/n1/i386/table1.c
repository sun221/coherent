/*
 * n1/i386/table1.c
 * Machine specific tables used by the code generator coder.
 * i386.
 */

#ifdef   vax
#include "INC$LIB:cc1.h"
#else
#include "cc1.h"
#endif

/*
 * Width table.
 * Indexed by type.
 * Values determine relative order of widths for iswiden(),
 * they are not widths in bytes.
 */
char	wtype[]	= {
	0,	0,		/* S8 	U8		*/
	1,	1,		/* S16	U16		*/
	2,	2,		/* S32	U32		*/
	3,	4,		/* F32	F64		*/
	5,			/* BLK			*/
	0,	1,	2,	/* FLD8 FLD16	FLD32	*/
	2,	2		/* PTR  PTRB		*/
};

/*
 * Register table.
 * Indexed by 'real' register numbers (as defined in mch.h).
 * Dword values go in EAX|EBX|ECX|EDX|EDI|ESI.
 * The code selector selects byte and word registers
 * by the corresponding dword register; thus,
 * word values go in EAX|EBX|ECX|EDX|EDI|ESI (i.e. AX|BX|CX|DX|DI|SI), and
 * byte values go in EAX|EBX|ECX|EDX (i.e. AL|BL|CL|DL; AH|BH|CH|DH are unused).
 * This allows TREG sharing of e.g. EAX/AX/AL and makes it easy to generate
 * decent code for the shorter types.
 * gen1.c/coderinit() patches the EDX:EAX and FPAC entries if -VNDP;
 * values below are for software fp, for NDP rvalue KD is FPAC, not EDX:EAX.
 */
REGDESC	reg[]	= {

/*	    KIND     KIND     int     REGNAME  REGNAME	REGNAME	PREGSET	*/
/*	    r_lvalue r_rvalue r_goal  r_enpair r_hihalf r_lohalf r_phys	*/

/* eax	*/	KBWL,  KBWL,  MRVALUE,	EDXEAX,	-1,	AX,	BEAX,
/* edx	*/	KBWL,  KBWL,  MRVALUE,	EDXEAX,	-1,	DX,	BEDX,
/* ebx	*/	KBWL,  KBWL,  MRVALUE,	-1,	-1,	BX,	BEBX,
/* ecx	*/	KBWL,  KBWL,  MRVALUE,	-1,	-1,	CX,	BECX,
/* esi	*/	KBWL,  KWL,   MRVALUE, 	-1,	-1,	SI,	BESI,
/* edi	*/	KBWL,  KWL,   MRVALUE, 	-1,	-1,	DI,	BEDI,
/* esp	*/	0,     0,     -1,      	-1,	-1,	-1,	BESP,
/* ebp	*/	KBWL,  0,     -1,      	-1,	-1,	-1,	BEBP,

/* edx:eax */	0,     KD,    MRVALUE,	-1,	EDX,	EAX,	BEDX|BEAX,

/* ax	*/	0,     0,     MRVALUE,	EAX,	AH,	AL,	BEAX,
/* dx	*/	0,     0,     MRVALUE,	EDX,	DH,	DL,	BEDX,
/* bx	*/	0,     0,     MRVALUE,	EBX,	BH,	BL,	BEBX,
/* cx	*/	0,     0,     MRVALUE,	ECX,	CH,	CL,	BECX,
/* si	*/	0,     0,     MRVALUE,	-1,	-1,	-1,	BESI,
/* di	*/	0,     0,     MRVALUE,	-1,	-1,	-1,	BEDI,
/* sp	*/	0,     0,     -1,	-1,	-1,	-1,	BESP,
/* bp	*/	0,     0,     -1,	-1,	-1,	-1,	BEBP,

/* al	*/	0,     0,     -1,	AX,	-1,	-1,	BEAX,
/* bl	*/	0,     0,     -1,	BX,	-1,	-1,	BEBX,
/* cl	*/	0,     0,     -1,	CX,	-1,	-1,	BECX,
/* dl	*/	0,     0,     -1,	DX,	-1,	-1,	BEDX,
/* ah	*/	0,     0,     -1,	AX,	-1,	-1,	BEAX,
/* bh	*/	0,     0,     -1,	BX,	-1,	-1,	BEBX,
/* ch	*/	0,     0,     -1,	CX,	-1,	-1,	BECX,
/* dh	*/	0,     0,     -1,	DX,	-1,	-1,	BEDX,

/* st	*/	0,     0,    MRVALUE,  -1,	-1,	-1,	BFPAC

/* None	*/
/* Anyr	*/
/* Anyl	*/
/* Pair	*/
/* Temp	*/
/* Lo	*/
/* Hi	*/

};

/*
 * The opcode table.
 * Indexed by operation.
 * The 0th and 1st entries of the relations are used by REL0 and by REL1.
 * The 2nd entry is the special supress EQ/NE entry used by LREL1 and LREL0.
 */
unsigned char	optab[][3] = {

	{	ZADD,	ZINC,	ZADC	},		/* ADD */
	{	ZSUB,	ZDEC,	ZSBB	},		/* SUB */
	{	ZMUL,	0,	0	},		/* MUL */
	{	ZDIV,	0,	0	},		/* DIV */
	{	ZDIV,	0,	0	},		/* REM */
	{	ZAND,	ZTEST,	ZSUB	},		/* AND */
	{	ZOR,	0,	0	},		/* OR  */
	{	ZXOR,	ZNOT,	0	},		/* XOR */
	{	ZSAL,	ZSHL,	ZRCL	},		/* SHL */
	{	ZSAR,	ZSHR,	ZRCR	},		/* SHR */

	{	ZADD,	ZINC,	ZADC	},		/* AADD */
	{	ZSUB,	ZDEC,	ZSBB	},		/* ASUB */
	{	ZMUL,	ZMOV,	ZMOVB	},		/* AMUL */
	{	ZDIV,	ZMOV,	ZMOVB	},		/* ADIV */
	{	ZDIV,	ZMOV,	ZMOVB	},		/* AREM */
	{	ZAND,	ZSUB,	0	},		/* AAND */
	{	ZOR,	0,	0	},		/* AOR  */
	{	ZXOR,	ZNOT,	0	},		/* AXOR */
	{	ZSAL,	ZSHL,	ZRCL	},		/* ASHL */
	{	ZSAR,	ZSHR,	ZRCR	},		/* ASHR */

	{	ZJE,	ZJE,	0	},		/* EQ  */
	{	ZJNE,	ZJNE,	0	},		/* NE  */
	{	ZJG,	0,	ZJG	},		/* GT  */
	{	ZJGE,	ZJNS,	ZJG	},		/* GE  */
	{	ZJLE,	0,	ZJL	},		/* LE  */
	{	ZJL,	ZJS,	ZJL	},		/* LT  */
	{	ZJA,	0,	ZJA	},		/* UGT */
	{	ZJAE,	0,	ZJA	},		/* UGE */
	{	ZJBE,	0,	ZJB	},		/* ULE */
	{	ZJB,	0,	ZJB	},		/* ULT */

	{	0,	0,	0	},		/* STAR    */
	{	0,	0,	0	},		/* ADDR    */
	{	ZNEG,	ZSBB,	0	},		/* NEG     */
	{	ZNOT,	0,	0	},		/* COM     */
	{	0,	0,	0	},		/* NOT     */
	{	0,	0,	0	},		/* QUEST   */
	{	0,	0,	0	},		/* COLON   */
	{	ZADD,	ZINC,	0	},		/* INCBEF  */
	{	ZSUB,	ZDEC,	0	},		/* DECBEF  */
	{	ZADD,	ZINC,	0	},		/* INCAFT  */
	{	ZSUB,	ZDEC,	0	},		/* DECAFT  */
	{	0,	0,	0	},		/* COMMA   */
	{	0,	0,	0	},		/* CALL    */
	{	0,	0,	0	},		/* ANDAND  */
	{	0,	0,	0	},		/* OROR    */
	{	0,	0,	0	},		/* CAST    */
	{	0,	0,	0	},		/* CONVERT */
	{	0,	0,	0	},		/* FIELD   */
	{	0,	0,	0	},		/* SIZEOF  */
	{	ZMOV,	ZSUB,	0	},		/* ASSIGN  */
	{	0,	0,	0	},		/* NOP     */
	{	0,	0,	0	},		/* INIT    */
	{	0,	0,	0	},		/* ARGLST  */
	{	0,	0,	0	},		/* LEAF    */
	{	0,	0,	0	},		/* FIXUP   */
	{	0,	0,	0	}		/* BLKMOVE */
};

/*
 * Per type info.
 * Column 0 is function return register (-1 if never a return type).
 * Column 1 is function return context, for select.
 * Column 2 is size of a temp.
 * Column 3 is the actual size in memory, for autoinc/autodec.
 * Column 4 is bit set for searching pattern tables.
 * Column 5 is register kind.
 * Column 6 is register kind for a pair.
 * gen1.c/coderinit() patches the F64 entry if -VNDP;
 * its return register name below is for software fp but is FPAC if -VNDP.
 */
PERTYPE	pertype[] = {
/*		REGNAME	char		char	char	TYPESET	KIND	KIND	*/
/*		p_frreg	p_frcat		p_size	p_incr	p_type	p_kind	p_pair	*/
/* S8    */  {	EAX,	MRVALUE,	4,	1,	FS8,	KB,	KL	},
/* U8    */  {	EAX,	MRVALUE,	4,	1,	FU8,	KB,	KL	},
/* S16   */  {	EAX,	MRVALUE,	4,	2,	FS16,	KW,	KL	},
/* U16   */  {	EAX,	MRVALUE,	4,	2,	FU16,	KW,	KL	},
/* S32   */  {	EAX,	MRVALUE,	4,	4,	FS32,	KL,	0 	},
/* U32   */  {	EAX,	MRVALUE,	4,	4,	FU32,	KL,	0 	},
/* F32   */  {	-1,	MRVALUE,	8,	4,	FF32,	0,	0 	},
/* F64   */  {	EDXEAX,	MRVALUE,	8,	8,	FF64,	KD,	0 	},
/* BLK   */  {	-1,	MRVALUE,	0,	0,	FBLK,	0,	0 	},
/* FLD8  */  {	-1,	MRVALUE,	0,	0,	FFLD8,	0,	0 	},
/* FLD16 */  {	-1,	MRVALUE,	0,	0,	FFLD16,	0,	0 	},
/* FLD32 */  {	-1,	MRVALUE,	0,	0,	FFLD32,	0,	0 	},
/* PTR  */   {	EAX,	MRVALUE,	4,	4,	FPTR,	KP,	0 	},
/* PTB  */   {	EAX,	MRVALUE,	4,	4,	FPTB,	KP,	0 	}
};

/*
 * These tables adjust relational ops when the
 * sense is reversed or when the subtrees are swapped.
 */
char	fliprel[] = {
	EQ, NE, LT, LE, GE, GT, ULT, ULE, UGE, UGT
};

char	otherel[] = {
	NE, EQ, LE, LT, GT, GE, ULE, ULT, UGT, UGE
};

/*
 * Type table.
 * Indexed by machine type.
 * Used by type testing macros defined in cc1mch.h.
 */
char	tinfo[]	= {
	0060,	0064,		/* S8	U8	*/
	0022,	0026,		/* S16	U16	*/
	0021,	0025,		/* S32	U32	*/
	0010,	0010,		/* F32	F64	*/
	0100,			/* BLK		*/
	0060,			/* FLD8		*/
	0022,			/* FLD16	*/
	0021,			/* FLD32	*/
	0205,	0305		/* PTR	PTB	*/
};

/* end of n1/i386/table1.c */
