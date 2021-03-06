/*
 * Make a process immmune to quit and hup signals.
 * Vlad 2-12-92
 */
#include	<stdio.h>
#include	<signal.h>
#include	<string.h>
#include	<ctype.h>
#include	<access.h>

char	*flatten();	/* Convert array of strings to a string */

char	*name;

main(argc, argv)
int	argc;
char	**argv;
{
	if (argc < 2) {
		fprintf(stderr, "usage: nohup command arg...\n");
		exit(1);
	}
	immune();			/* Make immunity */
	checkredirect();/* Check and redirect if necessary stdout and stderr */
	execvp(argv[1], argv + 1);	/* Execute command */
	/* We should never come here */
	fatal("%s: No such file or directory", flatten(&argv[1]));
}

/* 
 * Ignore hup and quit signals
 */
immune()
{
	signal(SIGHUP, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
}

/* 
 * If stdout and/or stderr is a tty. If it is so then redirect 
 * stdout and/or stderr to  "outfile".
 */ 
checkredirect()
{	
	char	*outname();	/* Find the name of the output file */
	char	*outfile;	/* Name of the output file */


	if (isatty(1)) {
		outfile = outname();	/* Find output file name */
		fprintf(stderr, "Sending output to %s\n", outfile);
		redirect(stdout, outfile);
		if (isatty(2))
			redirect(stderr, outfile);
	} else 
		if (isatty(2)) {
			outfile = outname();	/* Find output file name */
			redirect(stderr, outfile);
		}
}

/*
 * Redirect fp to output file.
 */
redirect(fp, name)
FILE	*fp;	/* Pointer to a file that should be redirected */
char	*name;	/* File name */
{
	if (freopen(name, "a", fp) == NULL) 
		fatal("cannot open output file");
}

/* 
 * Function flatten() concatinates an array of the strings to a string,
 * where the input strings are separated by spaces.
 */
#define	BUF_SIZE	80		/* Size of the buffer to be alloc */

char	*flatten(argv)
char	*argv[];
{
	extern char	*malloc(),
			*realloc();
	char		*buf;			/* Buffer for output string */
	unsigned	count = BUF_SIZE;	/* Size of the buffer */
	unsigned	len;			/* Current length used */
	unsigned	i;			/* Length of new addition */

	buf = malloc(BUF_SIZE);
	*buf = '\0';
	len  = 1;	/* We always need a terminator */

	for (; *argv != NULL; argv++) {
		while ((len + (i = strlen(*argv))) > count) {
			count += BUF_SIZE;
			if ((buf = realloc(buf, count)) == NULL)
				fatal("cannot allocate memory");
		}

		/* If it is not the first string, write string separator */
		if (len > 1) 
			strcpy(buf + len++ - 1, " ");

		strcpy(buf + len - 1, *argv);
		len += i;
	}
	return (realloc(buf, len));
}

/*
 * Print a message on a terminal and die. The question is we have to open
 * a terminal.
 */
fatal(msg)
char	*msg;
{
	extern char	*ttyname();
	FILE	*fp;	/* File pointer to a terminal */

	if ((fp = fopen("/dev/tty", "w", fp)) != NULL)
		fprintf(fp, "nohup: %r\n", &msg);
	exit(1);
}

/*
 * Find a name of output file. It is nohup.out or $HOME/nohup.out if
 * nohup.out is not writeable.
 */
char *outname()
{
	extern char	*getenv();
	char	*buf;				/* Output file name buffer */
	char	*dir;				/* User home directory */
	int	bufsize;			/* Size of output file name */
	char	*nohup_out = "nohup.out";	/* Name of output file */
	char	*home = "HOME";			/* HOME envir. variable */

	buf = nohup_out;
	/* Can we write into the current directory? */
	if (access(buf, AAPPND) && access(".", ACREAT)) {
		buf = nohup_out;
		dir = getenv(home);	/* Get $HOME directory */
		bufsize = strlen(dir);
		/* Allocate memory for output file $HOME/nohup.out */
		if ((buf = malloc(bufsize + strlen(nohup_out) + 1)) == NULL) 
			fatal("cannot allocate memory");
		/* Constract the name of the output file */
		strcpy(buf, dir);
		buf[bufsize] = '/';
		strcpy(buf + bufsize + 1, nohup_out);
		/* Can we write into the $HOME directory? */			
		if (access(buf, AAPPND) && access(dir, ACREAT))
			fatal("cannot open output file");
	}
	return(buf);
}





