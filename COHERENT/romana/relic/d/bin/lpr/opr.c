/*
 * Submit an off-line print request
 * Select the appropriate spooler for
 * the specified printer location.
 */

#include <stdio.h>

struct	dests	{
	char	*d_name;
	char	*d_comm;
}	dests[] = {
	"-lp", "/bin/lpr",
	"-vp", "/bin/vpr",
};

#define	NDEST	(sizeof(dests)/sizeof(dests[0]))

main(argc, argv)
char *argv[];
{
	register struct dests *dp, *xdp;

	dp = dests;
	if (argc>1 && *argv[1]=='-')
		for (xdp = dests; xdp < &dests[NDEST]; xdp++)
			if (strcmp(xdp->d_name, argv[1]) == 0) {
				argc--;
				argv++;
				dp = xdp;
			}
	argv[0] = dp->d_comm;
	execv(dp->d_comm, argv);
	fprintf(stderr, "opr: %s unavailable\n", dp->d_name);
	exit(1);
}
