/*
 * A debugger.
 * Machine dependent header for the Intel 8086.
 */
#define LOUT	1
#define	I	077777			/* Integer infinity */
#define LI	017777777777L		/* Long infinity */
#define MLI	020000000000L		/* Minus long infinity */

/*
 * Formats.
 */
#define	INLEN	1			/* Size of smallest instruction */
#define	VAWID	8			/* Size of virtual address */
#define	DDCHR	'x'			/* Default debugger format */
#define DAFMT	"%04lx"			/* Display address format */
#define VAFMT	"%08lx"			/* Virtual address */

/*
 * Breakpoint instruction size definition.
 */
typedef char	BIN[1];
