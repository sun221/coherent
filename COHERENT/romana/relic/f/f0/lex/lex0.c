/*
 * lex/lex0.c
 * external variables
 */

#include "lex.h"

int	nxt;
int	yylval;
int	ltype;
int	inquotes;
int	indefs;
int	actn;
int	clas;
int	nfa[ARRSZ][2];
struct	def *defstart;
struct	def *ctxstart;
struct	def *scnstart;
unsigned char *classptr;
FILE	*filein = stdin;
FILE	*fileout = stdout;

char	opnerr[] = "cannot open %s";
char	outmem[] = "out of memory";
char	noactn[] = "missing action";
char	illchr[] = "illegal character";
char	illnln[] = "illegal newline";
char	illrng[] = "improper range";
char	illoct[] = "illegal octal escape";
char	unddef[] = "undefined reference";
char	undctx[] = "undefined context";
char	undstc[] = "undefined start condition";
char	illstc[] = "start condition spec in rules section";
char	regsyn[] = "regular expression syntax";
char	rulsyn[] = "rule syntax";
char	actsyn[] = "action syntax";
char	unmopr[] = "unmatched `%c' in regular expression";
char	reperr[] = "improper repetition specification";
char	eoferr[] = "unexpected EOF";

/* end of lex0.c */
