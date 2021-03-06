#*********************************************************************
#                         COPYRIGHT NOTICE                           *
#*********************************************************************
#        This software is copyright (C) 1982 by Pavel Curtis         *
#                                                                    *
#        Permission is granted to reproduce and distribute           *
#        this file by any means so long as no fee is charged         *
#        above a nominal handling fee and so long as this            *
#        notice is always included in the copies.                    *
#                                                                    *
#        Other rights are reserved except as explicitly granted      *
#        by written permission of the author.                        *
#                Pavel Curtis                                        *
#                Computer Science Dept.                              *
#                405 Upson Hall                                      *
#                Cornell University                                  *
#                Ithaca, NY 14853                                    *
#                                                                    *
#                Ph- (607) 256-4934                                  *
#                                                                    *
#                Pavel.Cornell@Udel-Relay   (ARPAnet)                *
#                decvax!cornell!pavel       (UUCPnet)                *
#********************************************************************/

#
# $Header: /src386/usr/lib/ncurses/RCS/MKterm.h.awk,v 1.2 92/06/10 13:40:14 bin Exp Locker: bin $
#

BEGIN		{
		    print "/*"
		    print "**	term.h -- Definition of struct term"
		    print "*/"
		    print ""
		    print "#ifndef SGTTY"
		    print "#    include \"curses.h\""
		    print "#endif"
		    print ""
		    print "#ifdef SINGLE"
		    print "#	define CUR _first_term."
		    print "#else"
		    print "#	define CUR cur_term->"
		    print "#endif"
		    print ""
		    print ""
		}


$4 == "bool"	{
	    printf "#define %-30s CUR Booleans[%d]\n", $1, BoolCount++
		}

$4 == "number"	{
		    printf "#define %-30s CUR Numbers[%d]\n", $1, NumberCount++
		}

$4 == "str"	{
		    printf "#define %-30s CUR Strings[%d]\n", $1, StringCount++
		}


END		{
			print  ""
			print  ""
			print  "struct term"
			print  "{"
			print  "   char	 *term_names;	/* offset in str_table of terminal names */"
			print  "   char	 *str_table;	/* pointer to string table */"
			print  "   short Filedes;	/* file description being written to */"
			print  "#ifdef USE_TERMIO"
			print  "   struct termio Otermio,"
			print  "                 Ntermio;"
			print  "#else"
			print  "   SGTTY Ottyb,		/* original state of the terminal */"
			print  "	 Nttyb;		/* current state of the terminal */"
			print  "#endif"
			print  ""
			printf "   char		 Booleans[%d];\n", BoolCount
			printf "   short	 Numbers[%d];\n", NumberCount
			printf "   char		 *Strings[%d];\n", StringCount
			print  "};"
			print  ""
			print  "extern struct term _first_term;"
			print  "struct term	*cur_term;"
			print  ""
			printf "#define BOOLCOUNT %d\n", BoolCount
			printf "#define NUMCOUNT  %d\n", NumberCount
			printf "#define STRCOUNT  %d\n", StringCount
		}
