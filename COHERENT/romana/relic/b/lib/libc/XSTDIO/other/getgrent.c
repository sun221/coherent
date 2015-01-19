/*
 * Coherent I/O Library.
 * Routines to get the group file entry.
 * (searches by next entry, name or numerical id).
 */

#include <stdio.h>
#include <grp.h>

#define field(x)	{ x=cp; while (*cp++); }
#define	NGRPLINE	256
#define	NGRMEM		64		/* Maximum no. of members in a group */
#define	GRFILE	"/etc/group"

static	char	grline[NGRPLINE];
static	char	*grmems[NGRMEM+1];
static	struct	group gr;
static	FILE	*grfile	= { NULL };

struct group *
getgrnam(name)
char *name;
{
	register struct group *grp;

	setgrent();
	while ((grp = getgrent()) != NULL)
		if (streq(name, grp->gr_name))
			return (grp);
	return (NULL);
}

struct	group *
getgrgid(gid)
{
	register struct group *grp;

	setgrent();
	while ((grp = getgrent()) != NULL)
		if (gid == grp->gr_gid)
			return (grp);
	return (NULL);
}

struct group *
getgrent()
{
	register char *cp, *xp;

	if (grfile == NULL)
		if ((grfile = fopen(GRFILE, "r")) == NULL)
			return (NULL);
	cp = grline;
	{
		register int c;

		while ((c = getc(grfile))!=EOF && c!='\n') {
			if (c == ':')
				c = '\0';
			if (cp < &grline[NGRPLINE-1])
				*cp++ = c;
		}
		if (c == EOF)
			return (NULL);
	}
	*cp = '\0';
	cp = grline;
	field(gr.gr_name);
	field(gr.gr_passwd);
	field(xp);
	gr.gr_gid = atoi(xp);
	{
		register char **mp;

		gr.gr_mem = mp = grmems;
		for (;;) {
			if (*cp == '\0')
				break;
			*mp++ = cp;
			while (*cp!=',' && *cp!='\0')
				cp++;
			if (*cp == ',')
				*cp++ = '\0';
		}
		*mp = NULL;
	}
	return (&gr);
}

setgrent()
{
	if (grfile != NULL)
		rewind(grfile);
}

endgrent()
{
	if (grfile != NULL) {
		fclose(grfile);
		grfile = NULL;
	}
}
