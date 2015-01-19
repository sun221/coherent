/*
 * General Loader-Binder
 *
 * Knows about FILE struct to the extent it is revealed in putc
 * if BREADBOX is non-zero
 *
 */

#include <stdio.h>
#include <canon.h>
#include <l.out.h>
#include <ar.h>
#include <sys/stat.h>

typedef	unsigned long	uaddr_t;	/* universal address type */

/*
 * Macro to test for symbol equality.
 */
#define	eq( s1, s2 )	( strncmp( s1, s2, NCPLN ) == 0 )

/*
 * Segment descriptor
 */
typedef	struct seg_s {
	uaddr_t	vbase;		/* virtual address base */
	fsize_t	daddr,		/* seek addr of segment */
		size;		/* size */
} seg_t;

/*
 * Symbol descriptor
 */
typedef	struct	sym_t	{
	struct	sym_t	*next;	/* chained together */
	struct	ldsym	s;	/* id and value */
	struct	mod_t	*mod;	/* pass 1; defining module */
	unsigned int	symno;	/* pass 2; symbol number */
} sym_t;			/* above 2 items could be united */

/*
 * Descriptor for each input module
 */
typedef	struct	mod_t	{
	struct	mod_t	*next;	/* chained together */
	char	*fname;		/* file containing module */
	char	mname[DIRSIZ];	/* module name if in archive */
	seg_t	seg[NLSEG];	/* descriptor for each segment */
	unsigned int	nsym;	/* #symbols for this module */
	sym_t	*sym[];		/* array of nsym symbol ptrs */
} mod_t;

/*
 * Command line flag option
 */
typedef	int	flag_t;

typedef	struct	ar_hdr	arh_t;
typedef	struct	ar_sym	ars_t;
typedef	struct	ldheader ldh_t;
typedef	struct	ldsym	lds_t;

/*
 * Structures built in pass 1, used in pass 2
 */
#define	NHASH	64

/*
 * Seconds between ranlib update and archive modify times
 */
#define	SLOPTIME 150

/*
 * Values for worder
 */
#define	LOHI	0
#define	HILO	1

/*
 * For pass 2; these will change if format of relocation changed
 */
#define	getaddr	getlohi
#define	putaddr(addr, fp, sgp)	putlohi((short)(addr), fp, sgp)
#define	getsymno getlohi
#define	putsymno putlohi

/*
 * C requires this...
 */
void	baseall(), endbind(), undef();
uaddr_t	setbase(), newpage(), lentry();
fsize_t	segoffs();
void	symredef(), rdsystem();
sym_t	*addsym(), *symref(), *newsym();
fsize_t	symoff();
void	loadmod(), putstruc(), putword(), putlohi(), puthilo(), putbyte();
unsigned short	getword(), getlohi(), gethilo();
void	message(), fatal(), usage(), filemsg(), modmsg(), mpmsg(), spmsg();

/*
 * Start variables & constants
 */

extern	sym_t *	symtable[NHASH];	/* hashed symbol table */
extern	mod_t * modhead, *modtail;	/* module list head and tail */
extern	sym_t * etext_s,*edata_s,*end_s;/* special loader generated symbols */
extern	char	etext_id[NCPLN],	/* and their names */
		edata_id[NCPLN],	
		end_id[NCPLN];
extern	ldh_t	oldh;			/* output load file header */
extern	char	*mchname[];		/* names of known machines */
extern	uaddr_t	segsize[],		/* size of segment on target machine */
		drvbase[],		/* base of loadable driver */
		drvtop[];		/* address limit of loadable driver */

extern	flag_t	noilcl,			/* discard internal symbols `L...' */
		nolcl,			/* discard local symbols */
		watch,			/* watch everything happen */
		worder;			/* byte order in word; depends on machine */
	
/*
 * Structures associated with storage economy
 */
extern	char	pbuf1[BUFSIZ],		/* permanent i/o buffers */
		pbuf2[BUFSIZ];
extern	mod_t	*mtemp;			/* only one module in core at a time */
extern	FILE	*mfp;			/* temp file for module structures */
extern	int	mdisk,			/* flag <>0 means module struct to disk */
		nmod,			/* module count */
		mxmsym;			/* max # of symbols ref'd by 1 module */

extern	sym_t	*symtable[NHASH];	/* hashed symbol table */
extern	mod_t	*modhead, *modtail;	/* module list head and tail */
extern	seg_t	oseg[NLSEG];		/* output segment descriptors */
extern	int	nundef;			/* number of undefined symbols */
extern	uaddr_t	commons;		/* accumulate size of commons */

extern	char	*outbuf;		/* buffer for in-memory load */
extern	FILE	*outputf[NLSEG];	/* output ptrs (for each segment) */

/*
 *	Kludge for maximum size of a loadable data segment
 *	for the 8086, used in both fixstack and ld
 */

#define	MAXSEG86	0x10000L	/* Maximum Segment Size		*/
#define	DEFSTACK	0x1000		/* Initial Stack Size by Kernel	*/
#define	WARNSIZE	0x1000		/* Warning Tolerance Distance	*/
