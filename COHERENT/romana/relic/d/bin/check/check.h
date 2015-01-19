/*
 * Exit status bits for icheck and
 * dcheck.  This is the communication
 * path used by `check' to figure out
 * what needs to be corrected in a filesystem.
 */

/* Icheck bits */
#define	IC_MISC	01		/* Miscellaneous error - e.g out of space */
#define	IC_HARD	02		/* Human intervention needed to fix */
#define	IC_BFB	04		/* Bad free block */
#define	IC_MISS	010		/* Missing blocks */
#define	IC_DUPF	020		/* Dups in free */
#define	IC_BADF	040		/* Bad block in free list */
#define	IC_FIX	(IC_BFB|IC_MISS|IC_DUPF|IC_BADF)

/* Dcheck bits */
#define	DC_MISC	01		/* Miscellaneous error - e.g. out of space */
#define	DC_HARD	02		/* Too hard to fix without human intervention */
#define	DC_LCE	04		/* Difference in link count */
#define	DC_CLRI	010		/* Some i-node needs clri'ing */
#define	DC_FIX	(DC_LCE|DC_CLRI)
