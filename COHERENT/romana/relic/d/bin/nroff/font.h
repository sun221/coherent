/*
 * Nroff/Troff.
 * Font header.
 * All fonts are numbered so as to have a standard
 * output code sequence.
 */

/* Common fonts to both printers */
#define	TRMED		0		/* Times Roman standard */
#define	TRITL		1		/* Times Roman italic */
#define	TRBLD		2		/* Times Roman bold */
#define	TRSML		3		/* Times Roman small */
#define	HELV		4		/* Helv Bold for troff */
#define LPTR		5		/* Line Printer standard */
#define COURMED		6		/* Courier standard */
/* HP Laser-jet II only fonts	*/
#define	COURBOLD	7		/* Courier bold */
#define	CENROMAN	8		/* Century Schoolbook 10pt */
#define	CENITALIC	9		/* Century Schoolbook 10pt Italic */
#define	CENBOLD		10		/* Century Schoolbook 10pt Bold */
#define	SCHROMAN	11		/* Century Schoolbook 12pt */
#define	SCHITALIC	12		/* Century Schoolbook 12pt Italic */
#define	SCHBOLD		13		/* Century Schoolbook 12pt Bold */
#define	CENLARGE	14		/* Century Schoolbook 14pt Bold */
#define	CENHUGE		15		/* Century Schoolbook 18pt Bold */
#define ZAPBOLD		16		/* Zapf Humanist 12pt Bold */
#define	ZAPLARGE	17		/* Zapf Humanist 14pt Bold */
#define	LPROMAN		18		/* prestige elite roman */
#define	LPITALIC	19		/* prestige elite italic */
#define	LPBOLD		20		/* prestige elite bold */
#define	IBMGRAPH	21		/* IBM Screen graphics */
#define	MWCLOGO		22		/* MWC logo		*/
#define	LPTINY		23		/* Tiny font		*/
#define	LPMATH		24		/* Math font		*/
#define	PIFONT		25		/* PI font		*/
#ifdef	OLDHP
#define	CART
#endif
