/*
 * This file contains all of
 * the machine specific tables used by
 * the tree modification routines of the
 * Intel iAPX-86 code generator.
 */
#ifdef   vax
#include "INC$LIB:cc1.h"
#else
#include "cc1.h"
#endif

/*
 * This table holds information
 * about the subgoals of the various operators
 * in C. It is used by the leaf insert code
 * in modify3.c
 */
int	ldtab[]	= {

	ndown(MRVALUE,	MRADDR),
	ndown(MRVALUE,	MRADDR),
	ndown(MRVALUE,	MRADDR),
	ndown(MRVALUE,	MRADDR),
	ndown(MRVALUE,	MRADDR),
	ndown(MHARD,    MRADDR),	/* & */
	ndown(MRVALUE,	MRADDR),
	ndown(MRVALUE,	MRADDR),
	ndown(MRVALUE,	MHARD),		/* << */
	ndown(MRVALUE,	MHARD),		/* >> */

	ndown(MLADDR,	MRVALUE),
	ndown(MLADDR,	MRVALUE),
	ndown(MLADDR,	MRVALUE),
	ndown(MLADDR,	MRVALUE),
	ndown(MLADDR,	MRVALUE),
	ndown(MLADDR,	MRVALUE),
	ndown(MLADDR,	MRVALUE),
	ndown(MLADDR,	MRVALUE),
	ndown(MLADDR,	MHARD),		/* <<= */
	ndown(MLADDR,	MHARD),		/* >>= */

	ndown(MRVALUE,	MRADDR),
	ndown(MRVALUE,	MRADDR),
	ndown(MRVALUE,	MRADDR),
	ndown(MRVALUE,	MRADDR),
	ndown(MRVALUE,	MRADDR),
	ndown(MRVALUE,	MRADDR),
	ndown(MRVALUE,	MRADDR),
	ndown(MRVALUE,	MRADDR),
	ndown(MRVALUE,	MRADDR),
	ndown(MRVALUE,	MRADDR),

	ndown(MRADDR,	MJUNK),		/* * */
	ndown(MLADDR,	MJUNK),		/* & */
	ndown(MRVALUE,	MJUNK),		/* - */
	ndown(MRVALUE,	MJUNK),		/* ~ */
	ndown(MFLOW,	MJUNK),		/* ! */
	ndown(MFLOW,	MPASSED),	/* ? */
	ndown(MPASSED,	MPASSED),	/* : */
	ndown(MLADDR,	MRADDR),	/* ++ prefix */
	ndown(MLADDR,	MRADDR),	/* -- prefix */
	ndown(MLADDR,	MRADDR),	/* ++ postfix */
	ndown(MLADDR,	MRADDR),	/* -- postfix */
	ndown(MEFFECT,	MPASSED),	/* , */
	ndown(MLADDR,	MFNARG),	/* Call */
	ndown(MFLOW,	MFLOW),		/* && */
	ndown(MFLOW,	MFLOW),		/* || */
	ndown(MRADDR,	MRADDR),	/* Cast of types */
	ndown(MRADDR,	MRADDR),	/* Convert */
	ndown(MLADDR,	MRADDR),	/* Field select */
	ndown(MJUNK,	MJUNK),		/* Sizeof */
	ndown(MLADDR,	MRVALUE),	/* Simple assignment */
	ndown(MJUNK,	MJUNK),		/* Nop */
	ndown(MJUNK,	MJUNK),		/* Init type */
	ndown(MFNARG,	MFNARG),	/* Argument list */
	ndown(MRVALUE,	MJUNK),		/* Leaf */
	ndown(MRVALUE,	MJUNK),		/* Fix up */
	ndown(MRVALUE,	MRVALUE)	/* Block move */
};
