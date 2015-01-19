////////
/
/ Code table preface.
/ Must be the first file in the input list.
/
////////

#define	WORD	(FS16|FU16|FSPTR|FSPTB)
#define	UWORD	(FU16|FSPTR|FSPTB)
#define	LONG	(FS32|FU32)
#define	INT	(FS16|FU16|FS32|FU32)
#define	BYTE	(FS8|FU8)
#define	NFLT	(FS8|FU8|FS16|FU16|FSPTR|FSPTB|FLPTR|FLPTB|FS32|FU32)
#define	PEREL	(PEQ|PNE)
#define	PSREL	(PEQ|PNE|PGT|PGE|PLT|PLE)
#define	PUREL	(PEQ|PNE|PUGT|PULE|PUGE|PULT)
#define	PREL	(PEQ|PNE|PGT|PGE|PLT|PLE|PUGT|PUGE|PULT|PULE)
#define	PNEREL	(PGT|PGE|PLT|PLE|PUGT|PUGE|PULT|PULE)
#define	LPTX	(FLPTR|FLPTB)
