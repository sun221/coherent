/*
 * wc - word count
 * Counts characters, lines, and words in all
 * of the input files.
 *
 * Usage: wc [-lwc] [name ...]
 */

#include <stdio.h>
#ifdef COHERENT
#include <access.h>
#endif

char	_cmdname[] = "wc";
int	nfiles;
int	lineflg;
int	wordflg;
int	charflg;
long	chars, words, lines;
long	tchars, twords, tlines;

main(argc, argv)
char *argv[];
{
	register int i = 0;
	register char *ap;
	register FILE *fp;

	while (argv[1] != NULL && *argv[1] == '-') {
		for (ap = &argv[1][1]; *ap; ap++)
			switch (*ap) {
			case 'l':
				lineflg = 1;
				break;

			case 'w':
				wordflg = 1;
				break;

			case 'c':
				charflg = 1;
				break;

			default:
				usage();
			}
		argv++;
		argc--;
	}
	if (argc == 1) {
#ifdef	MSDOS
		_setbinary(stdin);
#endif
		wc(stdin);
		print(NULL);
	} else {
		argv++;  /* adjust args */

		while (*argv)
		{
#ifdef COHERENT
		   if (access(*argv, AREAD))
		    {  fprintf(stderr, "wc: can't open %s\n", *argv);
		       *argv[0] = 0;  /* flag arg as invalid */
		    }
#endif
		    i++;     /* count the arg */
		    argv++;  /* advance to next arg */
		}
		
		argv -= i;  /* back to start of filename args to process */
		argc--; 

		for (i=0; i<argc; i++) {
			if (*argv[i] == 0)  /* arg to skip? */ 
			   continue;        /* yes */
			if ((fp = fopen(argv[i], "rb")) == NULL) {
				fprintf(stderr,
				   "wc: can't open %s\n", argv[i]);
				continue;
			}
			nfiles++;
			wc(fp);
			print(argv[i]);
			tchars += chars;
			twords += words;
			tlines += lines;
			fclose(fp);
		}
		if (nfiles >= 2) {
			chars = tchars;
			words = twords;
			lines = tlines;
			print("Total");
		}
	}
}

/*
 * Count words, lines and characters in the input file
 * descriptor and put into global variables.
 */
wc(in)
register FILE *in;
{
	register c;
	int inw;

	inw = 0;
	chars = words = lines = 0;
	while ((c = getc(in)) != EOF) {
		chars++;
		if (c == '\n') {
			lines++;
			inw = 0;
		} else if (c==' ' || c=='\t')
			inw = 0;
		else if (!inw) {
			inw = 1;
			words++;
		}
	}
}

/*
 * Print according to option flags to totals
 */
print(name)
char *name;
{
	if (lineflg)
		printf("%8ld", lines);
	if (wordflg)
		printf("%8ld", words);
	if (charflg)
		printf("%8ld", chars);
	if (!lineflg && !wordflg && !charflg)
		printf("%8ld %8ld %8ld", lines, words, chars);
	if (name != NULL)
		printf("    %s", name);
	printf("\n");
}

usage()
{
	fprintf(stderr, "Usage: wc [-lwc] [name ...]\n");
	exit(1);
}
