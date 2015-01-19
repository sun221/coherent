/*********************************************************************
*                         COPYRIGHT NOTICE                           *
**********************************************************************
*        This software is copyright (C) 1982 by Pavel Curtis         *
*                                                                    *
*        Permission is granted to reproduce and distribute           *
*        this file by any means so long as no fee is charged         *
*        above a nominal handling fee and so long as this            *
*        notice is always included in the copies.                    *
*                                                                    *
*        Other rights are reserved except as explicitly granted      *
*        by written permission of the author.                        *
*                Pavel Curtis                                        *
*                Computer Science Dept.                              *
*                405 Upson Hall                                      *
*                Cornell University                                  *
*                Ithaca, NY 14853                                    *
*                                                                    *
*                Ph- (607) 256-4934                                  *
*                                                                    *
*                Pavel.Cornell@Udel-Relay   (ARPAnet)                *
*                decvax!cornell!pavel       (UUCPnet)                *
*********************************************************************/

/*
 *      curses.h - Main header file for the curses package
 *
 *  $Header: /src386/usr/bin/infocmp/RCS/curses.h,v 1.1 92/03/13 10:21:14 bin Exp $
 *
 *  $Log:	curses.h,v $
 * Revision 1.1  92/03/13  10:21:14  bin
 * Initial revision
 * 
Revision 2.1  82/10/25  14:46:08  pavel
Added Copyright Notice

Revision 2.0  82/10/24  15:17:22  pavel
Beta-one Test Release

Revision 1.4  82/08/23  22:30:13  pavel
The REAL Alpha-one Release Version

Revision 1.3  82/08/20  16:52:46  pavel
Fixed <terminfo.h> bug

Revision 1.2  82/08/19  19:10:13  pavel
Alpha Test Release One

Revision 1.1  82/08/12  18:38:57  pavel
Initial revision

 *
 */

#ifndef WINDOW

#include "terminfo.h"


#define bool    char


#ifndef MINICURSES
    typedef unsigned short  chtype;
#else
    typedef unsigned char   chtype;
#endif  MINICURSES


#ifndef TRUE
#  define TRUE    (1)
#  define FALSE   (0)
#endif

#define ERR     (0)
#define OK      (1)

#define _SUBWIN         01
#define _ENDLINE        02
#define _FULLWIN        04
#define _SCROLLWIN      010

#define _NOCHANGE       -1

struct _win_st {
	short   _cury, _curx;
	short   _maxy, _maxx;
	short   _begy, _begx;
	short   _flags;
	chtype  _attrs;
	bool    _clear;
	bool    _leave;
	bool    _scroll;
	bool    _idlok;
	bool    _use_keypad;    /* 0=no, 1=yes, 2=yes/timeout */
	bool    _use_meta;      /* T=use the meta key */
	bool    _nodelay;       /* T=don't wait for tty input */
	chtype  **_line;
	short   *_firstchar;    /* First changed character in the line */
	short   *_lastchar;     /* Last changed character in the line */
	short   *_numchngd;     /* Number of changes made in the line */
	short	_regtop;	/* Top and bottom of scrolling region */
	short	_regbottom;
};

#define WINDOW  struct _win_st

WINDOW	*stdscr, *curscr;

int	LINES, COLS;

WINDOW  *initscr(), *newwin(), *subwin();
char    *longname();
struct  screen  *newterm(), *set_term();


/*
 * psuedo functions
 */

#define getyx(win,y,x)   (y = (win)->_cury, x = (win)->_curx)

#define inch()           	winch(stdscr)
#define standout()      	wstandout(stdscr)
#define standend()      	wstandend(stdscr)
#define attron(at)      	wattron(stdscr,at)
#define attroff(at)     	wattroff(stdscr,at)
#define attrset(at)     	wattrset(stdscr,at)

#define winch(win)       	((win)->_line[(win)->_cury][(win)->_curx])
#define wstandout(win)          (wattrset(win,A_STANDOUT))
#define wstandend(win)          (wattrset(win,A_NORMAL))
#define wattron(win,at)         ((win)->_attrs |= (at))
#define wattroff(win,at)        ((win)->_attrs &= ~(at))
#define wattrset(win,at)        ((win)->_attrs = (at))


#ifndef MINICURSES
	/*
	 * psuedo functions for standard screen
	 */
#  define addch(ch)       waddch(stdscr, ch)
#  define getch()         wgetch(stdscr)
#  define addstr(str)     waddstr(stdscr, str)
#  define getstr(str)     wgetstr(stdscr, str)
#  define move(y, x)      wmove(stdscr, y, x)
#  define clear()         wclear(stdscr)
#  define erase()         werase(stdscr)
#  define clrtobot()      wclrtobot(stdscr)
#  define clrtoeol()      wclrtoeol(stdscr)
#  define insertln()      winsertln(stdscr)
#  define deleteln()      wdeleteln(stdscr)
#  define refresh()       wrefresh(stdscr)
#  define insch(c)        winsch(stdscr,c)
#  define delch()         wdelch(stdscr)
#  define setscrreg(t,b)  wsetscrreg(stdscr,t,b)

	    /*
	     * mv functions
	     */
#  define mvwaddch(win,y,x,ch)    (wmove(win,y,x) == ERR ? ERR : waddch(win,ch))
#  define mvwgetch(win,y,x)       (wmove(win,y,x) == ERR ? ERR : wgetch(win))
#  define mvwaddstr(win,y,x,str)  (wmove(win,y,x) == ERR ? ERR \
							 : waddstr(win,str))
#  define mvwgetstr(win,y,x)      (wmove(win,y,x) == ERR ? ERR : wgetstr(win))
#  define mvwinch(win,y,x)        (wmove(win,y,x) == ERR ? ERR : winch(win))
#  define mvwdelch(win,y,x)       (wmove(win,y,x) == ERR ? ERR : wdelch(win))
#  define mvwinsch(win,y,x,c)     (wmove(win,y,x) == ERR ? ERR : winsch(win,c))
#  define mvaddch(y,x,ch)         mvwaddch(stdscr,y,x,ch)
#  define mvgetch(y,x)            mvwgetch(stdscr,y,x)
#  define mvaddstr(y,x,str)       mvwaddstr(stdscr,y,x,str)
#  define mvgetstr(y,x)           mvwgetstr(stdscr,y,x)
#  define mvinch(y,x)             mvwinch(stdscr,y,x)
#  define mvdelch(y,x)            mvwdelch(stdscr,y,x)
#  define mvinsch(y,x,c)          mvwinsch(stdscr,y,x,c)
  
#else MINICURSES

#  define addch			  m_addch
#  define addstr                  m_addstr
#  define erase                   m_erase
#  define clear                   m_clear
#  define refresh                 m_refresh
#  define initscr                 m_initscr
#  define newterm                 m_newterm
#  define mvaddch(y,x,ch)         (move(y,x) == ERR ? ERR : addch(ch))
#  define mvaddstr(y,x,str)       (move(y,x) == ERR ? ERR : addstr(str))

/*
 * These functions don't exist in minicurses, so we define them
 * to nonexistent functions to help the user catch the error.
 */
#  define box             no_box
#  define clrtobot        no_clrtobot
#  define clrtoeol        no_clrtoeol
#  define delch           no_delch
#  define deleteln        no_deleteln
#  define delwin          no_delwin
#  define getch           no_getch
#  define getstr          no_getstr
#  define insch           no_insch
#  define insertln        no_insertln
#  define longname        no_longname
#  define mvprintw        no_mvprintw
#  define mvscanw         no_mvscanw
#  define mvwin           no_mvwin
#  define mvwprintw       no_mvwprintw
#  define mvwscanw        no_mvwscanw
#  define newwin          no_newwin
#  define overlay         no_overlay
#  define overwrite       no_overwrite
#  define printw          no_printw
#  define putp            no_putp
#  define scanw           no_scanw
#  define scroll          no_scroll
#  define setscrreg       no_setscrreg
#  define subwin          no_subwin
#  define touchwin        no_touchwin
#  define tstp            no_tstp
#  define vidattr         no_vidattr
#  define waddch          no_waddch
#  define waddstr         no_waddstr
#  define wclear          no_wclear
#  define wclrtobot       no_wclrtobot
#  define wclrtoeol       no_wclrtoeol
#  define wdelch          no_wdelch
#  define wdeleteln       no_wdeleteln
#  define werase          no_werase
#  define wgetch          no_wgetch
#  define wgetstr         no_wgetstr
#  define winsch          no_winsch
#  define winsertln       no_winsertln
#  define wmove           no_wmove
#  define wprintw         no_wprintw
#  define wrefresh        no_wrefresh
#  define wscanw          no_wscanw
#  define wsetscrreg      no_wsetscrreg

/* mv functions that aren't valid */
#  define mvdelch         no_mvwdelch
#  define mvgetch         no_mvwgetch
#  define mvgetstr        no_mvwgetstr
#  define mvinch          no_mvwinch
#  define mvinsch         no_mvwinsch
#  define mvwaddch        no_mvwaddch
#  define mvwaddstr       no_mvaddstr
#  define mvwdelch        no_mvwdelch
#  define mvwgetch        no_mvwgetch
#  define mvwgetstr       no_mvwgetstr
#  define mvwinch         no_mvwinch
#  define mvwinsch        no_mvwinsch

#endif MINICURSES


#ifndef MINICURSES
/* Funny "characters" enabled for various special function keys for input */
#define KEY_BREAK       0401            /* break key (unreliable) */
#define KEY_DOWN        0402            /* The four arrow keys ... */
#define KEY_UP          0403
#define KEY_LEFT        0404
#define KEY_RIGHT       0405            /* ... */
#define KEY_HOME        0406            /* Home key (upward+left arrow) */
#define KEY_BACKSPACE   0407            /* backspace (unreliable) */
#define KEY_F0          0410            /* Function keys.  Space for 64 */
#define KEY_F(n)        (KEY_F0+(n))    /* keys is reserved. */
#define KEY_DL          0510            /* Delete line */
#define KEY_IL          0511            /* Insert line */
#define KEY_DC          0512            /* Delete character */
#define KEY_IC          0513            /* Insert char or enter insert mode */
#define KEY_EIC         0514            /* Exit insert char mode */
#define KEY_CLEAR       0515            /* Clear screen */
#define KEY_EOS         0516            /* Clear to end of screen */
#define KEY_EOL         0517            /* Clear to end of line */
#define KEY_SF          0520            /* Scroll 1 line forward */
#define KEY_SR          0521            /* Scroll 1 line backwards (reverse) */
#define KEY_NPAGE       0522            /* Next page */
#define KEY_PPAGE       0523            /* Previous page */
#define KEY_STAB        0524            /* Set tab */
#define KEY_CTAB        0525            /* Clear tab */
#define KEY_CATAB       0526            /* Clear all tabs */
#define KEY_ENTER       0527            /* Enter or send (unreliable) */
#define KEY_SRESET      0530            /* soft (partial) reset (unreliable) */
#define KEY_RESET       0531            /* reset or hard reset (unreliable) */
#define KEY_PRINT       0532            /* print or copy */
#define KEY_LL          0533            /* home down or bottom (lower left) */

#endif MINICURSES

#endif WINDOW
