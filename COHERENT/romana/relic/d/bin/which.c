/*
 * locate a program file along the user's PATH.
 * try to mimic output of the BSD version.
 */
#include <stdio.h>
#include <sys/stat.h>
#include <access.h>

char	*getenv();
char	*index();
char	**parsepath();		/* parse PATH into argv-like structure */
void	fatal();		/* display error message and die */
void	usage();		/* display usage message and die */
char	*calloc();
char	*index();

#define	TRUE		(0 == 0)
#define	FALSE		(0 != 0)
#define	EXEC_BITS	(S_IEXEC | (S_IEXEC >> 3) | (S_IEXEC >> 6))

main(argc, argv)
int	argc;
char	*argv[];
{
	char	*pathp;				/* pointer to PATH string */
	char	**pathpp;			/* pointer to path entries */
	char	**pathlist;			/* pointer to path list */
	static	char	pathbuf[BUFSIZ];	/* expanded file names */
	static	char	path[BUFSIZ];		/* saved copy for error msgs */

	if ((pathp = getenv("PATH")) == NULL)
		fatal("PATH not found in environment");
	strcpy(path, pathp);
	if (--argc < 1)
		usage();
	pathlist = parsepath(pathp);

	for (++argv; argc-- > 0; ++argv) {
		pathpp = pathlist;
		for ( ; *pathpp != NULL; ++pathpp) {
			if (**pathpp != '\0') {
				strcpy(pathbuf, *pathpp);
				strcat(pathbuf, "/");
				strcat(pathbuf, *argv);
			} else
				strcpy(pathbuf, *argv);
			if (is_exec(pathbuf)) {
				puts(pathbuf);
				break;
			}
		}
		if (*pathpp == NULL)
			if (index(*argv, '/') != NULL)
				printf("%s not found\n", *argv);
			else
				printf("no %s in %s\n", *argv, path);
	}
	exit(0);
}

/*
 * check to see if the given name is executable.
 * this isn't as trivial as it may seem since access() says
 * that root (UID 0) can execute anything, even if the
 * execute mode bits are not set.
 */
is_exec(name)
char	*name;
{
	static	struct	stat	statbuf;

	if (access(name, AEXEC) != 0)
		return FALSE;
	if (stat(name, &statbuf) == -1) {
		fprintf(stderr, "%s: Unable to stat\n", name);
		return FALSE;
	}
	if ((statbuf.st_mode & S_IFMT) != S_IFREG)
		return FALSE;
	return (getuid() != 0 || (statbuf.st_mode & EXEC_BITS));
}

/*
 * parse a path of the form dir1:dir2:dir3 into an array of pointers to
 * the individual directory entries. Note that this null terminates each
 * directory entry by replacing all ':' characters in "path" with NUL's. 
 * take special care of leading and trailing ':' which imply the current
 * directory.
 */
char	**
parsepath(path)
char	*path;
{
	register char	*cp;
	register int	i = 1;
	char	**patharray, **pp;

	for (cp = path; *cp != '\0'; )	/* calculate number of entries */
		if (*cp++ == ':')
			++i;
	if ((patharray = (char **)calloc(i + 1, sizeof (char *))) == NULL)
		fatal("Out of memory for PATH list");
	for (cp = path, pp = patharray; *cp != '\0' || pp == patharray; ) {
		*pp++ = cp;
		while (*cp != '\0') {
			if (*cp == ':') {
				*cp++ = '\0';
				break;
			}
			++cp;
		}
		if ((pp - patharray) < i)
			*pp = cp;
	}
	return (patharray);
}

void
usage()
{
	fatal("usage: which name ....");
}

void
fatal(info)
char	*info;
{
	fprintf(stderr, "which: %r\n", &info);
	exit(1);
}
