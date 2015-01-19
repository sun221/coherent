/* parms.c - display hard drive parameters per "atparm" in kernel */

#include <stdio.h>
#include <l.out.h>

/*
 * For easy referencing.
 */
#define	at_table		nl[0].n_value
#define	plowner		nl[1].n_value
#define NDRIVE		2

struct dparm_s {
	unsigned short	d_ncyl;		/* number of cylinders */
	unsigned char	d_nhead;	/* number of heads */
	unsigned short	d_rwcc;		/* reduced write current cyl */
	unsigned short	d_wpcc;		/* write pre-compensation cyl */
	unsigned char	d_eccl;		/* max ecc data length */
	unsigned char	d_ctrl;		/* control byte */
	unsigned char	d_fill2[3];
	unsigned short	d_landc;	/* landing zone cylinder */
	unsigned char	d_nspt;		/* number of sectors per track */
	unsigned char	d_fill3;

}	atparm[ NDRIVE ] = {
	0				/* Initialized to allow patching */
};

/*
 * Table for namelist.
 */
struct nlist nl[] ={
	"atparm_",		0,	0,
	""
};

/*
 * Symbols.
 */
char	 *kfile;			/* Kernel data memory file */
char	 *nfile;			/* Namelist file */
int	 kfd;				/* Kernel memory file descriptor */

main(argc, argv)
char *argv[];
{
	register int i;
	register char *cp;

	initialise();
	for (i=1; i<argc; i++) {
		for (cp=&argv[i][0]; *cp; cp++) {
			switch (*cp) {
			case '-':
				continue;
			case 'c':
				if (++i >= argc)
					usage();
				nfile = argv[i];
				continue;
			default:
				usage();
			}
		}
	}
	execute();
	exit(0);
}

/*
 * Initialise.
 */
initialise()
{
	nfile = "/coherent";
	kfile = "/dev/kmem";
}

/*
 * Print out usage.
 */
usage()
{
	panic("Usage: parms [-][c kernel_file]");
}

/*
 * Display parameters
 */
execute()
{
	int dr;

	nlist(nfile, nl);
	if (nl[0].n_type == 0)
		panic("Bad namelist file %s", nfile);
	if ((kfd = open(kfile, 0)) < 0)
		panic("Cannot open %s", kfile);

	kread((long)at_table, atparm, sizeof(atparm));
	printf("Hard drive parameters as stored in \"at\" driver:\n");
	for (dr = 0;  dr < NDRIVE;  dr++) {
		printf("drive %d  cyl=%4d  hd=%2d  spt=%2d  ctrl=%02x\n",
			dr, atparm[dr].d_ncyl, atparm[dr].d_nhead,
			atparm[dr].d_nspt, atparm[dr].d_ctrl);
	}
}

/*
 * Read `n' bytes into the buffer `bp' from kernel memory
 * starting at seek position `s'.
 */
kread(s, bp, n)
long s;
{
	lseek(kfd, (long)s, 0);
	if (read(kfd, bp, n) != n)
		panic("Kernel memory read error");
}

/*
 * Print out an error message and exit.
 */
panic(a1)
char *a1;
{
	fflush(stdout);
	fprintf(stderr, "%r", &a1);
	fprintf(stderr, "\n");
	exit(1);
}