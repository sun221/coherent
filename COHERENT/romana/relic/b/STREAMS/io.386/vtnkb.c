/*
 * io.386/vtnkb.c
 *
 * Keyboard driver, virtual consoles, loadable tables.
 *
 * Revised: Fri Jul 16 08:39:12 1993 CDT
 */

#define SWANFIX 1
#define GREEKFIX 1

/*
 * User configurable AT keyboard/display driver.
 */
#include <sys/coherent.h>
#include <sys/con.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/tty.h>
#include <signal.h>
#include <sys/seg.h>
#include <sys/sched.h>
#include <sys/kb.h>
#include <sys/devices.h>
#include <sys/silo.h>
#include <stddef.h>

#include <sys/vt.h>

#define	ISVEC		1		/* Keyboard interrupt vector */
#define	DEBUG		0

#define	KBDEBUG(x)	T_CON(1,printf(x));	/* debugging output */
#define	KBDEBUG2(x,y)	T_CON(1,printf(x,y));	/* debugging output */
#define	KBDEBUG3(x,y,z)	T_CON(1,printf(x,y,z));	/* debugging output */

/*
 * values for kbstate
 */
#define	KB_IDLE		0		/* nothing going on right now */
#define	KB_SINGLE	1		/* sent a single byte cmd to the kbd */
#define	KB_DOUBLE_1	2		/* sent 1st byte of 2-byte cmd to kbd */
#define	KB_DOUBLE_2	3		/* sent 2nd byte of 2-byte cmd to kbd */

/*
 * patchable params for non-standard keyboards
 */
int	KBDATA = 0x60;			/* Keyboard data */
int	KBCTRL = 0x61;			/* Keyboard control */
int	KBSTS_CMD = 0x64;		/* Keyboard status/command */
int	KBFLAG = 0x80;			/* Keyboard reset flag */
int	KBBOOT = 1;			/* 0: disallow reboot from keyboard */
int	KBTIMEOUT = 10000;		/* shouldn't need this much */
int	KBCMDBYTE = 0x05;		/* no translation */

/*
 * KBSTATUS bits
 */
#define	STS_OBUF_FULL	0x01		/* kbd output buffer full */
#define	STS_IBUF_FULL	0x02		/* kbd input buffer full */
#define	STS_SYSTEM	0x04
#define	STS_CMD_DATA	0x08		/* 1: command or status */
#define	STS_INHIBIT	0x10		/* 0: keyboard inhibited */
#define	STS_AUX_OBUF_FULL	0x20
#define	STS_TIMEOUT	0x40		/* general timeout */
#define	STS_PAR_ERR	0x80		/* parity error */

/*
 * The following are magic commands which read from or write to the
 * controller command byte. These get output to the KBSTS_CMD port.
 */
#define	C_READ_CMD	0x20		/* read controller command byte */
#define	C_WRITE_CMD	0x60		/* write controller command byte */
#define	C_TRANSLATE	0x40		/* translate enable bit in cmd byte */

/*
 * Globals:
 * The 286 keyboard mapping table is too large to fit into kernel data space,
 * so we need to allocate a segment to it.  386 is easy.
 * The function keys tend to be small and tend to change substantially
 * more often than the mapping table, so we keep them in the kernel data space.
 */
static	unsigned shift;			/* state of all shift/lock keys */
static	unsigned char	**funkeyp = 0;	/* ptr to array of func. keys ptrs */
static	FNKEY	*fnkeys = 0;		/* pointer to structure of values */
static	unsigned fklength;		/* length of function key text */
static	unsigned prev_cmd;		/* previous command sent to KBD */
static	unsigned cmd2;			/* 2nd byte of command to KBD */
static	unsigned sh_index;		/* shift/lock state index */
#ifdef _I386
static	KBTBL	kb[MAX_KEYS];		/* keyboard table */
#else
static	SEG	*kbsegp;		/* keyboard table segment */
#endif

/*
 * State variables.
 */
int		islock;			/* Keyboard locked flag */
int		isbusy;			/* Raw input conversion busy */
static	char	table_loaded;		/* true == keyboard table resident */
static	char	fk_loaded;		/* true == function keys resident */
static	int	kbstate = KB_IDLE;	/* current keyboard state */
static  int     xlate = 1;              /* scan code translation flag */

#define	ESCAPE_CHAR	'\x1B'
#define	ESCAPE_STRING	"\x1B"

/*
 * Functions.
 */
int		isrint();
int		istime();
void		isbatch();
int		vtmmstart();
int		isopen();
int		isclose();
int		isread();
int		vtmmwrite();
int		isioctl();
void		vtmmwatch();
int		isload();
int		isuload();
int		ispoll();
int		nulldev();
int		nonedev();
int		updleds();

/*
 * Configuration table.
 */

CON vtnkbcon ={
	DFCHR|DFPOL,			/* Flags */
	KB_MAJOR,			/* Major index */
	isopen,				/* Open */
	isclose,			/* Close */
	nulldev,			/* Block */
	isread,				/* Read */
	vtmmwrite,			/* Write */
	isioctl,			/* Ioctl */
	nulldev,			/* Powerfail */
	vtmmwatch,			/* Timeout */
	isload,				/* Load */
	isuload,			/* Unload */
	ispoll				/* Poll */
};

/*
==============================================================================
==============================================================================
*/
/* constants for vtdata[] */
#define VT_VGAPORT	0x3D4
#define VT_MONOPORT	0x3B4

#ifdef	_I386
#define VT_MONOBASE	(SEG_VIDEOa|DPL_1)
#define VT_VGABASE	(SEG_VIDEOb|DPL_1)
#else
#define VT_MONOBASE	0xB000
#define VT_VGABASE	0xB800
#endif

/*
	Patchable table entries,
	we go indirect in order to produce a label which can be addressed
*/
#if SWANFIX
int VTSWAN = 0;		/* patch to 1 for epstein's fix for Swan keyboard */
#endif

#if GREEKFIX
static void ToggleGreek();
static int ToGreek();
int VTGREEK = 0;	/* patch to 1 for TECOP Greek mod */
#endif

/* Configurable variables - see ker/conf/console/Space.c */
extern int 	vga_count;
extern int 	mono_count;

HWentry	VTVGA =		{ 4, 0, VT_VGAPORT, { 0, VT_VGABASE }, { 25, 80 } };
HWentry	VTMONO =	{ 4, 0, VT_MONOPORT, { 0, VT_MONOBASE }, { 25, 80 } };

HWentry	*vtHWtable[] = {
	VTVGA,		/* VGA followed by MONO is compatible to DOS */
	VTMONO,
	0		/* MUST STAY AS LAST ELEMENT !!! */
};

extern	int	vtmminit();
static	VTDATA	const_vtdata	= {
	vtmminit, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 23, 24, 0, 0, 0, 23, 0, 0, 1
};

/* later this should be dynamic */
VTDATA	*vtconsole, **vtdata;

int	vtcount, vtmax;
extern	int	vtactive;
int	vt_verbose = { 0 };
int	vt_opened = { 0 };

/* Terminal structure. */
TTY	**vttty;

/*
==============================================================================
==============================================================================
*/

static silo_t in_silo;

/*
 * Given hw pointer for one of four types of adapters, see if
 * device is present by write/readback of video memory.
 *
 * return 1 if present, else 0
 */
int
hwpresent( hw )
HWentry *hw;
{
	int	save, present = 1;

	PRINTV( "hwpresent: %x:%x",
		hw->vidmemory.seg, hw->vidmemory.off );
	save = ffword( hw->vidmemory.off, hw->vidmemory.seg );

	sfword(  hw->vidmemory.off, hw->vidmemory.seg, 0xAA55 );
	if( ffword( hw->vidmemory.off, hw->vidmemory.seg ) != 0xAA55 )
		present = 0;

	sfword(  hw->vidmemory.off, hw->vidmemory.seg, 0x55AA );
	if( ffword( hw->vidmemory.off, hw->vidmemory.seg ) != 0x55AA )
		present = 0;

	sfword(  hw->vidmemory.off, hw->vidmemory.seg, save );
	PRINTV( "%s present\n", present ? "" : " NOT" );
	return present;
}

/*
 * Load entry point.
 */
isload()
{
	register 	int i;
	register	HWentry **hw;
	register	VTDATA *vp;

	PRINTV("vtload:\n");
	fk_loaded = 0;
	table_loaded = 0;
	kbstate = KB_IDLE;

	/* Sugar for idtune and kpatch. */
	VTVGA.count = vga_count;
	VTMONO.count = mono_count;

	/* figure out what our current max is */
	for( vtmax = 0, hw = vtHWtable; *hw; ++hw ) {
		vtmax += (*hw)->count;
		(*hw)->found = 0;	/* assume non-exist */
	}
	PRINTV( "vtload: %d screens possible\n", vtmax );

	vtdata = (VTDATA **) kalloc( vtmax * sizeof( *vtdata ) );
	if( vtdata == NULL ) {
		printf( "vtload: unable to obtain vtdata[%d]\n", vtmax );
		u.u_error = -1;
		return;
	}
	PRINTV( "vtload: obtained vtdata[%d] @%x\n", vtmax, vtdata );

	vttty = (TTY **) kalloc( vtmax * sizeof( *vttty ) );
	if( vttty == NULL ) {
		printf( "vtload: unable to obtain vttty[%d]\n", vtmax );
		u.u_error = -1;
		return;
	}
	PRINTV( "vtload: obtained vttty[%d] @%x\n", vtmax, vttty );

	/* determine which video adaptors are present */
	for( vtcount = 0, hw = vtHWtable; *hw; ++hw ) {
/* suppress board sensing since it seems to confuse some equipment */
#if 0
		if( !hwpresent(*hw) )
			continue;
#endif

		/* remember our logical start */
		(*hw)->start = vtcount;
		PRINTV( ", start %d\n", vtcount );

		/* allocate the necessary memory */
		for ( i = 0; i < (*hw)->count; ++i ) {
			vp = vtdata[vtcount] = kalloc( sizeof(VTDATA) );
			PRINTV( "     vtdata[%d] = @%x\n", vtcount, vp );
			if( vp == NULL || !VTttyinit(vtcount) ) {
				printf("not enough memory for VTDATA\n" );
				break;
			}

			/* fill in appropriately */
			*vp = const_vtdata;
			vp->vmm_port = (*hw)->port;
			vp->vmm_vseg = (*hw)->vidmemory.seg;
			vp->vmm_voff = (*hw)->vidmemory.off;

			vp->vt_ind = vtcount;
			vtdatainit(vp);
			if (i == 0 ) {
				vp->vmm_visible = VNKB_TRUE;
				vp->vmm_seg = vp->vmm_vseg;
				vp->vmm_off = vp->vmm_voff;
				vtupdscreen(vtcount);
			}
			(*hw)->found++;
			vtcount++;
		}
	}

	/*
	 * initialize vtconsole
	 */
	vtconsole = vtdata[vtactive = 0];
	vtconsole->vmm_invis = 0;		/* vtconsole cursor visible */

	/*
	 * Seize keyboard interrupt.
	 */
#ifdef	_I386
	setivec(ISVEC, isrint);
#else
#if	VT_MAJOR == KB_MAJOR
	setivec(1, isrint);
#else
	/*
	 * Map table and vector to us
	 */
	i = sphi();
	PRINTV( "VTload: unload old vector\n" );
	kcall( Kclrivec, 1 );
	setivec(1, isrint);
	spl( i );
#endif
#endif	/* _I386 */

	/*
	 * Enable mmwatch() invocation every second.
	 */
	drvl[VT_MAJOR].d_time = 1;

	/*
	 * Initialize video display.
	 */
	for ( i = 0; i < vtcount; ++i )
		vtmmstart( vttty[i] );


#ifndef	_I386
	/*
	 * Allocate a segment to store the in-core keyboard table.
	 * This would be a lot more convenient in kernel data space,
	 * but small model COHERENT doesn't have that luxury.
	 */
	kbsegp = salloc((fsize_t)MAX_TABLE_SIZE, SFSYST|SFNSWP|SFHIGH);
	if (kbsegp == (SEG *)0)
		printf("kb: unable to allocate keyboard table segment\n");
	KBDEBUG("Exiting kbload()\n");
#endif
	fklength = 0;
}

/*
 * Unload entry point.
 */
isuload()
{
	register int i;
	register level = sphi();

	clrivec(ISVEC);
#ifndef	_I386
#if	VT_MAJOR != KB_MAJOR
	kcall( Ksetivec, ISVEC, &Kisrint );
#endif
#endif
	spl( level );

	/* Restore pointers to original state. */
	vtconsole = vtdata[0];
	vtconsole->vmm_invis = 0;
	vtconsole->vmm_visible = VNKB_TRUE;

	if( vt_opened )
		printf( "VTclose with %d open screens\n", vt_opened );
	if( kbstate != KB_IDLE )
		printf("kb: keyboard busy during unload\n");
#ifndef	_I386
	if (kbsegp != (SEG *)0) {
		table_loaded = 0;
		sfree(kbsegp);
	}
#endif

#ifndef	_I386
	for( i = 0; i < vtcount; ++i ) {
		PRINTV( "VTuload: free far %x:%x, tty %x\n",
			vttty[i]->t_buffer->s_faddr, vttty[i] );
		sfree( vttty[i]->t_buffer );
		kfree( vttty[i] );
		sfree( vtdata[i].vt_buffer );
	}
#endif
}

/*
 * Open routine.
 */
isopen(dev, mode)
dev_t dev;
unsigned int mode;
{
	register int s;
	register TTY *tp;
	int	index = vtindex(dev);

	PRINTV("isopen: %x\n", dev);
	if (index < 0 || index >= vtcount) {
		u.u_error = ENXIO;
		return;
	}

	tp = vttty[index];
	if ((tp->t_flags&T_EXCL) != 0 && !super()) {
		u.u_error = ENODEV;
		return;
	}
	ttsetgrp(tp, dev, mode);

	s = sphi();
	if (tp->t_open++ == 0) {
		tp->t_flags = T_CARR;	/* indicate "carrier" */
		ttopen(tp);
	}
	spl(s);
#if 0
	updleds();			/* update keyboard status LEDS */
#endif
}


void isvtswitch();

/*
 * Close a tty.
 */
isclose(dev)
{
	register int s;
	int	index = vtindex(dev);
	register TTY *tp = vttty[index];

#if 0
	s = sphi();
	if (--tp->t_open == 0) {
		ttclose(tp);
		spl(s);
		if( index == vtactive )
			isvtswitch( VTKEY_HOME );
	} else
		spl(s);
#else
	if (--tp->t_open == 0)
		ttclose(tp);
#endif
}

/*
 * Read routine.
 */
isread(dev, iop)
dev_t dev;
IO *iop;
{
	int	index = vtindex(dev);
	register TTY *tp = vttty[index];

	ttread(tp, iop, 0);
	if (tp->t_oq.cq_cc)
		vtmmtime(tp);
}

/*
 * special constants/struct for the XWindow/KDMAPDISP calls
 */

#define KDMAPDISP       (('K' << 8) | 2)      /* map display into user space */
#define KDSKBMODE       (('K' << 8) | 6)      /* turn scan code xlate on/off */
#define KDMEMDISP       (('K' << 8) | 7)      /* dump byte of virt/phys mem  */
#define KDENABIO        (('K' << 8) | 60)     /* enable IO                   */
#define KIOCSOUND       (('K' << 8) | 63)     /* start sound generation      */ 
#define KDSETLED        (('K' << 8) | 66)     /* set leds                   */

#define TIMER_CTL    0x43                     /* Timer control */
#define TIMER_CNT    0x42                     /* Timer counter */
#define SPEAKER_CTL  0x61                     /* Speaker control */

struct kd_memloc {
        char    *vaddr;         /* virtual address to map to */
        char    *physaddr;      /* physical address to map to */
        long    length;         /* size in bytes to map */
        long    ioflg;          /* enable I/O addresses if non-zero */
};

static TIM tp;
int
resetkb(action)
   int action;
{
   int i;
   if (action == 1) {
      timeout(&tp,20,resetkb,2);
      outb(KBCTRL, 0xCC);             /* Clock high */
   }
   if (action == 2) {
      i = inb(KBDATA);
      outb(KBCTRL, 0xCC);                     /* Clear keyboard */
      outb(KBCTRL, 0x4D);                     /* Enable keyboard */
   }
}

static int X11led;

/*
 * Ioctl routine.
 * nb: archaic TIOCSHIFT and TIOCCSHIFT no longer needed/supported.
 */
isioctl(dev, com, vec)
dev_t dev;
struct sgttyb *vec;
{
	register int s;

	switch (com) {
#define KDDEBUG 0
#if KDDEBUG
       case KDMEMDISP:
       {
               struct kd_memloc* mem;
               unsigned char ub, pb;
               mem = vec;
                pxcopy( mem->physaddr, &pb, 1, SEG_386_KD );
               ub = getubd( mem->vaddr );
               printf( "User's byte %x(%x), Physical byte %x, Addresses %x %x\n",
                       mem->ioflg, ub, pb, mem->vaddr, mem->physaddr );
                break;;             
       }
#endif
       case KDMAPDISP:
       {
               struct kd_memloc* mem;
               mem = vec;
#if KDDEBUG
               printf( "mapPhysUser(%x, %x, %x) = %d\n",
                        mem->vaddr, mem->physaddr, mem->length,  
#endif
               mapPhysUser(mem->vaddr, mem->physaddr, mem->length)
#if KDDEBUG
               )
#endif
;
       }
       case KDENABIO:
       {
               int i;
               for (i = 0 ; i < 64 ; i++ )
                   iomapAnd(0,i);
                break;;             
       }
       case KIOCSOUND:
       {
               if (vec) {
                 outb(TIMER_CTL, 0xB6); 
                 outb(TIMER_CNT, (int)vec&0xFF);
                 outb(TIMER_CNT, (int)vec>>8);
                 outb(SPEAKER_CTL, inb(SPEAKER_CTL) | 03); /* Turn speaker on */
               }
               else 
                 outb(SPEAKER_CTL, inb(SPEAKER_CTL) & ~03 ); /* speaker off */
                break;;             
       }
       case KDSKBMODE:
       {
	       static int vtB4X11;
               /* outb(KBCTRL, 0x0C);             /* Clock low */
               /* timeout(&tp,3,resetkb,1);       /* wait about 20-30ms */
	       if (xlate > vec) {	/* Turning translation off */
		   kb_cmd2(K_SCANCODE_CMD, 1);      /* set 1 for X */

#if 0
		   /* deactivate virtual terminal */
	           vtB4X11 = vtactive;
        	   vtdeactivate(vtdata[vtactive], vtdata[vtcount]);
		   vtactivate(vtdata[vtcount]);
		   vtactive = vtcount;
#endif
	       }
	       else if (xlate < vec) {		   /* turning translation on */
		   kb_cmd2(K_SCANCODE_CMD, 3);      /* set 3 for COH */
#if 0
		   /* reactivate virtual terminal */
	           vtactivate(vtdata[vtB4X11]);
		   vtactive = vtB4X11;
#endif
	       }
               xlate = (int)vec;
	       /* kb_cmd(K_ALL_TMB_CMD);	/* default: TMB for all keys */
               break;;             
       }
       case KDSETLED:
       {
               X11led = (int)vec;
               updleds();
               break;;             
        }
	case TIOCSETF:
	case TIOCGETF:
		isfunction(com, (char *)vec);
		break;
	case TIOCSETKBT:
		issettable(vec);
		break;
	case TIOCGETKBT:
		isgettable(vec);
		break;
	default:				/* pass to TTY driver */
		s = sphi();
		ttioctl(vttty[vtindex(dev)], com, vec);
		spl(s);
		break;
	}
}

/*
 * Set the in-core keyboard mapping table.
 * The table is sorted by scan code prior to calling ioctl().
 * All unused table entries (holes in the scan code map) have
 * a zero for the k_key field.
 * This makes key lookup at interrupt time fast by using the scan code
 * as an index into the table.
 */
issettable(vec)
char	*vec;
{
	register unsigned i;
	register int s;
	int timeout;
	static	KBTBL	this_key;	/* current key from kbd table */
	unsigned int cmd_byte;
#ifndef _I386
	register faddr_t faddr;		/* address of keyboard table */
#endif

	PRINTV(" TIOCSETKBT");
	kb_cmd2(K_SCANCODE_CMD, 3);		/* select set 3 */
	kb_cmd(K_ALL_TMB_CMD);			/* default: TMB for all keys */
#ifndef _I386
	faddr = kbsegp->s_faddr;
#endif
	for (i = 0; i < MAX_KEYS; ++i) {
		ukcopy(vec, &this_key, sizeof(this_key));
#ifdef _I386
		kb[i] = this_key;		/* store away */
#else
		kfcopy(&this_key, faddr, sizeof(this_key));
		faddr += sizeof(this_key);
#endif
		vec += sizeof(this_key);
		if (this_key.k_key != i && this_key.k_key != 0) {
			printf("kb: incorrect or unsorted table entry %d\n", i);
#ifdef _I386
			u.u_error = EINVAL;
#else
			u.u_error = EBADFMT;
#endif
			return;
		}
		if (this_key.k_key != i)
			continue;		/* no key */
		switch (this_key.k_flags&TMODE) {
		case T:				/* typematic */
			kb_cmd2(K_KEY_T_CMD, i);
			break;
		case M:				/* make only */
			kb_cmd2(K_KEY_M_CMD, i);
			break;
		case MB:			/* make/break */
			kb_cmd2(K_KEY_MB_CMD, i);
			break;
		case TMB:			/* typematic make/break */
			break;			/* this is the default */
		default:
			printf("kb: bad key mode\n");
		}
	}
	updleds();
	kb_cmd2(K_SCANCODE_CMD, 3);		/* select set 3 */
	kb_cmd(K_ENABLE_CMD);			/* start scanning */
	/*
	 * The following code disables translation from the on-board
	 * keyboard/aux controller. Without disabling translation, the
	 * received scan codes still look like code set 1 codes even
	 * though we put the keyboard controller in scan code set 3.
	 * Yes, this is progress....
	 */
#if 0
	while (inb(KBSTS_CMD) & STS_IBUF_FULL)
		;
	outb(KBSTS_CMD, C_READ_CMD);		/* read controller cmd byte */
	while (!(inb(KBSTS_CMD) & STS_OBUF_FULL))
		;
	cmd_byte = inb(KBDATA);
	KBDEBUG2(" cmd_byte=%x", cmd_byte);
#endif
	timeout = KBTIMEOUT;
	s = sphi();
	while ((inb(KBSTS_CMD) & STS_IBUF_FULL) && --timeout > 0)
		;
	outb(KBSTS_CMD, C_WRITE_CMD);		/* write controller cmd byte */
	for (timeout = 50; --timeout > 0;)
		;
	timeout = KBTIMEOUT;
	while ((inb(KBSTS_CMD) & STS_IBUF_FULL) && --timeout > 0)
		;
	outb(KBDATA, KBCMDBYTE);		 /* turn off translation */	
	timeout = KBTIMEOUT;
	while ((inb(KBSTS_CMD) & STS_IBUF_FULL) && --timeout > 0)
		;
	spl(s);
#if DEBUG || 1
	kb_cmd2(K_SCANCODE_CMD, 0);		/* query s.c. mode */
#endif
	++table_loaded;
	PRINTV("... TIOCSETKBT\n");
}

/*
 * Get the in-core keyboard mapping table and pass it to the user.
 */
isgettable(vec)
char	*vec;
{
#ifdef _I386
	KBDEBUG(" TIOCGETKBT");
	kucopy(kb, vec, sizeof(kb));
#else
	register unsigned i;
	register faddr_t faddr;		/* address of keyboard table */
	static	KBTBL	this_key;	/* current key from kbd table */

	KBDEBUG(" TIOCGETKBT");
	faddr = kbsegp->s_faddr;
	for (i = 0; i < MAX_KEYS; ++i) {
		fkcopy(faddr, &this_key, sizeof(this_key));
		kucopy(&this_key, vec, sizeof(this_key));
		faddr += sizeof(this_key);
		vec += sizeof(this_key);
	}
#endif
}


/*
 * Set and receive the function keys.
 */
isfunction(c, v)
int c;
FNKEY *v;
{
	register unsigned char *cp;
	register unsigned i;
	unsigned char	numkeys = 0;

	if (c == TIOCGETF) {
		KBDEBUG(" TIOCGETF");
		if (!fk_loaded)
			u.u_error = EINVAL;
		else
			kucopy(fnkeys, v, fklength);	/* copy ours to user */
	} else { /* TIOCSETF */
		/*
		 * If we had a previous function key arena, free it up.
		 * Since we don't know how large the function key arena will
		 * be, we must size it in the user data space prior to
		 * (re)kalloc()'ing it. This is ugly, but a helluva lot better
		 * than the old driver which used a hard coded limit of 150!
		 */
		KBDEBUG(" TIOCSETF");
		fk_loaded = 0;
		if (fnkeys != (FNKEY *)0)
			kfree(fnkeys);		/* free old arena */
		if (funkeyp != NULL)
			kfree(funkeyp);		/* free old ptr array */
		ukcopy(&v->k_nfkeys, &numkeys, sizeof(numkeys));
		/*
		 * I'd use offsetof (), but right now it is broken due to a
		 * compiler bug.
		 */
		fklength = sizeof (FNKEY);
		cp = (char *) (v + 1);
		for (i = 0; i < numkeys; i++) {
			do {
				++fklength;
			} while (getubd(cp++) != DELIM);
		}
		fnkeys = (FNKEY *)kalloc(fklength);
		funkeyp = (unsigned char **)kalloc(numkeys * sizeof(char *));
		if (fnkeys == (FNKEY *)0 || funkeyp == NULL) {
			if (fnkeys != (FNKEY *)0) {
				kfree(fnkeys);
				fnkeys = 0;
			}
			if (funkeyp != NULL) {
				kfree(funkeyp);
				funkeyp = 0;
			}
			u.u_error = ENOMEM;
			return;
		}
		cp = (char *) (fnkeys + 1);		/* point to Fn ... */
		v = (char *) (v + 1);			/* ... key arena */
		for (i = 0; i < numkeys; i++) {
			funkeyp[i] = cp;	           /* save pointer */
			while ((*cp++ = getubd(v++)) != DELIM)  /* copy key */
				;
		}
		fnkeys->k_nfkeys = numkeys;
		fk_loaded = 1;
	}
}


/*
 * Poll routine.
 */
ispoll(dev, ev, msec)
dev_t dev;
int ev;
int msec;
{
	register TTY *tp = vttty[vtindex(dev)];

	return ttpoll(tp, ev, msec);
}

/*
 * Receive interrupt.
 */
#define	K_E0ESC	0xE0		/* Swan Keyboard, Strange Escape Byte	*/

isrint()
{
	register unsigned c;
	register unsigned r;
	static	char keyup;
#if SWANFIX
	static	char e0esc;
#endif

	/*
	 * Schedule raw input handler if not already active.
	 */
	if (! isbusy) {
		defer (isbatch, vttty [vtactive]);
		isbusy = 1;
	}

	/*
	 * Pull character from the data
	 * port. Pulse the KBFLAG in the control
	 * port to reset the data buffer.
	 */

	r = inb (KBDATA) & 0xFF;
	c = inb (KBCTRL);
	outb (KBCTRL, c | KBFLAG);
	outb (KBCTRL, c);

	/*
	 * check returned value from keyboard to see if it's a command
	 * or status back to us. If not, it we assume that it's a key code.
	 */
	KBDEBUG2 ("\nintr(%x) ", r);
	if (!xlate) switch (r) {
	case K_BAT_BAD:
		printf("kb: keyboard BAT failed\n");
		break;
	case K_RESEND:
		KBDEBUG("\nkb: request to resend command\n");
		outb(KBDATA, prev_cmd);
		break;
	case K_OVERRUN_23:
		printf("kb: keyboard buffer overrun\n");
		break;
	case K_ACK:
		/*
		 * we received an ACKnowledgement from the keyboard.
		 * advance the state machine and continue.
		 */
		KBDEBUG(" ACK ");
		switch (kbstate) {
		case KB_IDLE:			/* shouldn't happen */
			printf("vtnkb: ACK while idle ");
			break;
		case KB_SINGLE:			/* done with 1-byte command */
		case KB_DOUBLE_2:		/* done w/ 2nd of 2-byte cmd */
			kbstate = KB_IDLE;
			wakeup(&kbstate);
			break;
		case KB_DOUBLE_1:
			kbstate = KB_DOUBLE_2;
			outb(KBDATA, cmd2);
			break;
		default:
			printf("kb: bad kbstate %d\n", kbstate);
			break;
		}
		break;
	default:
         	isin(r);
             	break;
	} else switch (r) {

	case K_BREAK:
		keyup = 1;			/* key going up */
		break;

	case K_ECHO_R:
	case K_BAT_OK:
		break;				/* very nice, but ignored */

	case K_BAT_BAD:
		printf ("kb: keyboard BAT failed\n");
		break;

	case K_RESEND:
		KBDEBUG ("\nkb: request to resend command\n");
		outb (KBDATA, prev_cmd);
		break;

	case K_OVERRUN_23:
		printf ("kb: keyboard buffer overrun\n");
		break;

	case K_ACK:
		/*
		 * we received an ACKnowledgement from the keyboard.
		 * advance the state machine and continue.
		 */
		KBDEBUG (" ACK ");
		switch (kbstate) {
		case KB_IDLE:			/* shouldn't happen */
			printf ("vtnkb: ACK while idle ");
			break;

		case KB_SINGLE:			/* done with 1-byte command */
		case KB_DOUBLE_2:		/* done w/ 2nd of 2-byte cmd */
			kbstate = KB_IDLE;
			wakeup (& kbstate);
			break;

		case KB_DOUBLE_1:
			kbstate = KB_DOUBLE_2;
			outb (KBDATA, cmd2);
			break;

		default:
			printf ("kb: bad kbstate %d\n", kbstate);
			break;
		}
		break;
#if SWANFIX
	case K_E0ESC:
		if (VTSWAN) {
			e0esc = 1;
			break;
		}
#endif
	default:
#if SWANFIX
		process_key (r, keyup, e0esc);
		e0esc = 0;
#else
		process_key (r, keyup);
#endif
		keyup = 0;
	}
}

/*
 * Process a key given its scan code and direction.
 * 
 * In this table driven version of the keyboard driver, we trade off the
 * code complexity associated with all the black magic that used to be
 * performed on a per-key basis with the increased memory requirements
 * associated with the table driven approach.
 */
#if SWANFIX 1
process_key(key, up, e0esc)
unsigned key;
char	 up, e0esc;
#else
process_key(key, up)
unsigned key;
int	 up;
#endif
{
	register unsigned char *cp;
	KBTBL	key_vals;			/* table values for this key */
	unsigned val;
	unsigned char flags;
	register TTY *tp = vttty[vtactive];
	VTDATA *vp = vtdata[vtactive];

	KBDEBUG3(" proc(%x %s)", key, (up ? "up" : "down"));
	if (!table_loaded)
		return;				/* throw away key */
#ifdef _I386
	/*
	 *  It's ugly but, if e0esc, then we use the ALT_GR field to point
	 *  at the actual table entry we want.  We weren't really using the
	 *  ALT_GR field anyway.  Trouble remapping shift keys because
	 *  loader requires all entries to be identical, thus ALT_GR is
	 *  by default being used.
	 */

#if SWANFIX
	if (VTSWAN && e0esc && (kb [key].k_flags & S) == 0)
		key = kb [key].k_val [ALT_GR];   /* Ugly kludge */
#endif
	key_vals = kb [key];
#else
	fkcopy (kbsegp->s_faddr + (key * sizeof(KBTBL)),
		& key_vals, sizeof (key_vals));
#endif
	if (key_vals.k_key != key)		/* empty entry */
		return;
	flags = key_vals.k_flags;

	if (flags & S) {			/* some shift/lock key ? */
		switch (key_vals.k_val [BASE]) {
		case caps:
		case num:
			if (!up) {
				shift ^= (1 << key_vals.k_val[BASE]);
				updleds2 ();
			}
			break;

		case scroll:
			if (! up) {
				shift ^= (1 << key_vals.k_val[BASE]);
				updleds2 ();
				if (_IS_RAW_INPUT_MODE (tp)) {
					if (tp->t_flags & T_STOP)
						isin (tp->t_tchars.t_startc);
					else
						isin (tp->t_tchars.t_stopc);
				}
			}
			break;
		default:
			if (up)
				shift &= ~ (1 << key_vals.k_val [BASE]);
			else
				shift |= (1 << key_vals.k_val [BASE]);
			break;
		}

		/*
		 * Calculate the shift index based upon the state of
		 * the shift and lock keys.
		 */

		sh_index = BASE;		/* default condition */
		if (shift & (1 << altgr))
			sh_index = ALT_GR;
		else {
			if (shift & ((1 << lalt) | (1 << ralt)))
				sh_index |= ALT;
			if (shift & ((1 << lctrl) | (1 << rctrl)))
				sh_index |= CTRL;
			if (shift & ((1 << lshift) | (1 << rshift)))
				sh_index |= SHIFT;
		}
		T_CON(2, printf("shift=%x sh_index=%d\n", shift, sh_index));
		return;
	} /* if (flags & S) */

	/*
	 * If the key has no value in the current
	 * shift state, the key is just tossed away.
	 */
	if (up || key_vals.k_val [sh_index] == none)
		return;

	if (((flags & C) != 0 && (shift & (1 << caps)) != 0) ||
	    ((flags & N) != 0 && (shift & (1 << num))) != 0)
		val = key_vals.k_val [sh_index ^ SHIFT];
	else
		val = key_vals.k_val [sh_index];

	/*
	 * Check for function key or special key implemented as
	 * a function key (reboot == f0, tab and back-tab, etc).
	 */
	if (flags & F) {
		PRINTV ("<{F%d}>", val);
		if (VTKEY (val)) {
			T_CON(4,
			  printf( "<{F%d !!}>\b\b\b\b\b\b\b\b\b\b", val));
			defer (isvtswitch, val);
			return;
		}
		/* If the tty is not open, ignore it */
		if (! tp->t_open)
			return;
#if GREEKFIX
		if (VTGREEK && val == fgk) {
			ToggleGreek ();
			return;
		}
#endif /* GREEKFIX */
		if (val == 0 && ! up && KBBOOT)
			boot ();
		if (! fk_loaded || val >= fnkeys->k_nfkeys)
			return;
		if ((cp = funkeyp [val]) == NULL) /* has a value? */
			return;
		while (* cp != DELIM)
			isin (* cp ++);		/* queue up Fn key value */
		return;
	}

	/*
	 * Normal key processing.
	 */
	/* If the tty is not open, ignore it */
#if GREEKFIX
	if (tp->t_open)
		if (VTGREEK) {
			if (ToGreek (& val))
				isin (val);
		} else
			isin (val);
#else
	if (tp->t_open)
		isin (val);		 /* send the char */
#endif /* GREEKFIX */
}

/**
 *
 * void
 * ismmfunc(c)	-- process keyboard related output escape sequences
 * char c;
 */
void
ismmfunc(c)
register int c;
{

	switch (c) {
	case 't':	/* Enter numlock */
		shift |= (1 << num);
		updleds ();			/* update LED status */
		break;

	case 'u':	/* Leave numlock */
		shift &= ~ (1 << num);
		updleds ();			/* update LED status */
		break;

	case '=':			/* Enter alternate keypad -- ignored */
	case '>':			/* Exit alternate keypad -- ignored */
		break;

	case 'c':	/* Reset terminal */
		islock = 0;
		break;
	}
}

/**
 *
 * void
 * isin(c)	-- append character to raw input silo
 * char c;
 */
static
isin(c)
register int c;
{
	int cache_it = 1;
	TTY * tp = vttty[vtactive];
	void ttstart();

	/*
	 * If using software incoming flow control, process and
	 * discard t_stopc and t_startc.
	 */
	if (_IS_IXON_MODE (tp)) {
#if _I386
		if (_IS_START_CHAR (tp, c) ||
		    (_IS_IXANY_MODE (tp) && (tp->t_flags & T_STOP) != 0)) {
			tp->t_flags &= ~ (T_STOP | T_XSTOP);
			ttstart (tp);
			cache_it = 0;
		} else if (_IS_STOP_CHAR (tp, c)) {
			if ((tp->t_flags & T_STOP) == 0)
				tp->t_flags |= (T_STOP | T_XSTOP);
			cache_it = 0;
		}
#else
		if (_IS_STOP_CHAR (tp, c)) {
			if ((tp->t_flags & T_STOP) == 0)
				tp->t_flags |= T_STOP;
			cache_it = 0;
		}
		if (_IS_START_CHAR (tp, c)) {
			tp->t_flags &= ~ T_STOP;
			ttstart (tp);
			cache_it = 0;
		}
#endif
	}

	/*
	 * Cache received character.
	 */
	if (cache_it) {
		in_silo.si_buf [in_silo.si_ix] = c;

		if (++ in_silo.si_ix >= sizeof (in_silo.si_buf))
			in_silo.si_ix = 0;
	}
}

/**
 *
 * void
 * isbatch()	-- raw input conversion routine
 *
 *	Action:	Enable the video display.
 *		Canonize the raw input silo.
 *
 *	Notes:	isbatch() was scheduled as a deferred process by isrint().
 */
static void
isbatch(tp)
register TTY * tp;
{
	register int c;
	static int lastc;
	VTDATA *vp = tp->t_ddp;

	/*
	 * Ensure video display is enabled.
	 */
	if( vp->vmm_visible ) {
		vtmm_von(vp);
	}
	isbusy = 0;

	/*
	 * Process all cached characters.
	 */
	while (in_silo.si_ix != in_silo.si_ox) {
		/*
		 * Get next cached char.
		 */
		c = in_silo.si_buf [in_silo.si_ox];

		if (in_silo.si_ox >= sizeof (in_silo.si_buf) - 1)
			in_silo.si_ox = 0;
		else
			in_silo.si_ox ++;

		if (islock == 0 || _IS_INTERRUPT_CHAR (tp, c) ||
		    _IS_QUIT_CHAR (tp, c)) {
			ttin (tp, c);
		} else if ((c == 'b') && lastc == ESCAPE_CHAR) {
			islock = 0;
			ttin (tp, lastc);
			ttin (tp, c);
		} else if (c == 'c' && lastc == ESCAPE_CHAR) {
			ttin (tp, lastc);
			ttin (tp, c);
		} else
			putchar ('\a');
		lastc = c;
	}
}

/*
 * update the keyboard status LEDS.
 * we chose the shift/lock key positions so this would be easy.
 * this flavor of routine is called while processing a system call on
 * behalf of the user.
 */

updleds()
{
	if (!xlate)
		kb_cmd2(K_LED_CMD, X11led);
	else
		kb_cmd2(K_LED_CMD, (shift >> 1) & 0x7);
}

/*
 * same as above, but callable from interrupt routines and other places
 * which cannot sleep() waiting for the state machine to go idle.
 */
updleds2()
{
	register timeout;
	register int s;

	timeout = KBTIMEOUT;
	s = sphi();
	while (--timeout > 0 && (inb(KBSTS_CMD) & STS_IBUF_FULL))
		;
	kbstate = KB_DOUBLE_1;
	if (!xlate)
		cmd2 = X11led;
	else
		cmd2 = (shift >> 1) & 0x7;
	prev_cmd = K_LED_CMD;
	outb (KBDATA, K_LED_CMD);
	spl (s);
}

/*
 * unlock the scroll in case an interrupt character is received
 */
kbunscroll()
{
	shift &= ~ (1 << scroll);
	updleds ();
}

/*
 * ship a single byte command to the keyboard
 */
kb_cmd(cmd)
unsigned cmd;
{
	register int timeout;
	register int s;

	s = sphi();
	KBDEBUG2(" kb_cmd(%x)", cmd);
	while (kbstate != KB_IDLE)
#ifdef _I386
		x_sleep(&kbstate, pritty, slpriSigCatch, "kb a");
#else
		v_sleep(&kbstate, CVTTIN, IVTTIN, SVTTIN, "kb a");
#endif
	kbstate = KB_SINGLE;
	timeout = KBTIMEOUT;
	while (--timeout > 0 && (inb(KBSTS_CMD) & STS_IBUF_FULL))
		;
	if (!timeout)
		printf("kb: command timeout\n");
	else {
		outb(KBDATA, cmd);
		while (kbstate != KB_IDLE)
#ifdef _I386
			x_sleep(&kbstate, pritty, slpriSigCatch, "kb b");
#else
			v_sleep(&kbstate, CVTTIN, IVTTIN, SVTTIN, "kb b");
#endif
	}
	spl(s);
}

/*
 * ship a two byte command to the keyboard
 */
kb_cmd2(cmd, arg)
unsigned cmd, arg;
{
	register int timeout;
	register int s;

	s = sphi();
	KBDEBUG3(" kb_cmd2(%x, %x)", cmd, arg);
	while (kbstate != KB_IDLE)
#ifdef _I386
		x_sleep(&kbstate, pritty, slpriSigCatch, "kb c");
#else
		v_sleep(&kbstate, CVTTIN, IVTTIN, SVTTIN, "kb c");
#endif
	kbstate = KB_DOUBLE_1;
	cmd2 = arg;
	prev_cmd = cmd;
	timeout = KBTIMEOUT;
	while (--timeout > 0 && (inb(KBSTS_CMD) & STS_IBUF_FULL))
		;
	if (!timeout)
		printf("kb: command timeout\n");
	else {
		outb(KBDATA, cmd);
		while (kbstate != KB_IDLE)
#ifdef _I386
			x_sleep(&kbstate, pritty, slpriSigCatch, "kb d");
#else
			v_sleep(&kbstate, CVTTIN, IVTTIN, SVTTIN, "kb d");
#endif
	}
	spl(s);
}

/*
==============================================================================
==============================================================================
*/

int
VTttyinit(i)
int i;
{
	TTY *tp;

	/*
	 * get pointer to TTY structure from kernal memory space
	 */
	if( (tp = vttty[i] = (TTY *)kalloc(sizeof (TTY))) == NULL )
		return(0);
	PRINTV( "     vttty[%d]: @%x, ", i, tp );

#if	FAR_TTY
	/*
	 * get pointers to the buffers pointed to by the TTY structure 
	 * from user memory space
	 */
	tp->t_buffer = salloc( (fsize_t)NCIB+2*SI_BUFSIZ, SFSYST|SFNSWP );
	tp->t_ib = 0;
	tp->t_rawin.si_buf = NCIB;
	tp->t_rawout.si_buf = NCIB+SI_BUFSIZ;
#endif
	tp->t_param = NULL;
	tp->t_start = &vtmmstart;

#ifndef	_I386
#if	VT_MAJOR == KB_MAJOR
	tp->t_cs_sel = 0;
#else
	tp->t_cs_sel = cs_sel();
#endif
#endif
	tp->t_ddp = vtdata[i];
	PRINTV( "data @%lx\n", tp->t_ddp );
	return(1);
}

vtdatainit(vp)
VTDATA	*vp;
{
#ifndef	_I386
	VT_FARSEG	vt_farseg;
#endif
	/*
	 * vtdata init - vmm part
	 */
	vp->vmm_invis = -1;			/* cursor invisible */

#ifdef	_I386
	vp->vt_buffer = kalloc( TEXTBLOCK );
	vp->vmm_seg = vp->vmm_mseg = vtds_sel();
	vp->vmm_off = vp->vmm_moff = vp->vt_buffer;
#else
	vp->vt_buffer = salloc ( (fsize_t)TEXTBLOCK, SFSYST|SFNSWP|SFHIGH );
	vp->vmm_seg = vp->vmm_mseg = FP_SEG( vp->vt_buffer->vt_faddr );
	vp->vmm_off = vp->vmm_moff = FP_OFF( vp->vt_buffer->vt_faddr );
#endif
	PRINTV( "vt@%x init index %d,%d), seg %x, off %x\n",
		vp, vp->vt_ind, vp->vmm_mseg, vp->vmm_moff );
	/*
	 * vtdata init - vnkb part
	 */
	/* Make the first memory block active, if present */ 
	vp->vnkb_lastc = 0;
	vp->vnkb_fnkeys = 0;	
	vp->vnkb_funkeyp = 0;	
	vp->vnkb_fk_loaded = 0;			/* no Fn keys yet */
}

/*
 * Given device number, return index for vtdata[], vttty[], etc.
 *
 * Major number must be VT_MAJOR for CPU to get here.
 *
 *      Minor Number	Index Value
 *	----- ------ 	----- -----  
 *	0000  0000	vtactive ... device (2,0) is the active screen
 *	0000  0001	0
 *	0000  0010	1
 *	0000  0011	2
 *	   ....
 *	0000  1111	14
 *
 *	0100  xxxx	xxxx ... color devices only
 *	0101  xxxx	xxxx - (# of color devices found) ... monochrome only
 *
 * Return value is in range 0 to vtcount-1 for valid minor numbers,
 * -1 for invalid minor numbers.
 */
int
vtindex( dev )
dev_t dev;
{
	register int	ret = -1;

	if ( dev & VT_PHYSICAL ) {
		int	hw = ( dev >> 4 ) & 3;
		int	hw_index = dev & 0x0F;

		if( hw_index < vtHWtable[hw]->found )
			ret = vtHWtable[hw]->start + hw_index;
	} else {
		int	lg_index = dev & 0x0F;

		if (lg_index == 0)
			ret = vtactive;
		if (lg_index > 0 && lg_index <= vtcount ) 
			ret = lg_index-1;
	}
	if (ret >= 0)
		ret %= vtcount;
	else
		PRINTV( "vtindex: (%x) %d. invalid !\n", dev, ret );
	return ret;
}

/*
 *
 * void
 * isvtswitch()	-- deferred virtual terminal switch
 *
 *	Action: - save current shift key status
 *		- determine new active virtual terminal
 *		- deactivate shift key status of the current virtual terminal
 *		- deactivate current virtual terminal
 *		- activate shift key status of the new virtual terminal with 
 *		  the previously saved shift key status
 *		- activate new virtual terminal 
 *
 *	Notes:	isvtswitch() was scheduled as a deferred process by 
 *	process_key() which is a function called by isrint().
 */
void
isvtswitch(key_val)
{
	register int	new_index, i;
	unsigned	lockshift, nolockshift; 
	VTDATA		*vp = vtdata[vtactive];
	VTDATA		*vp_old, *vp_new;
	static int	vtprevious;

	T_CON(2, printf("old shift=%x sh_index=%d\n", shift, sh_index));
	lockshift = shift & ((1<<scroll)|(1<<num)|(1<<caps));
	nolockshift = shift & ~((1<<scroll)|(1<<num)|(1<<caps));

	PRINTV( "F%d: %d", key_val, vtactive );
#if 0
	if( key_val == VTKEY_HOME )
		new_index = 0;
	else if( key_val == VTKEY_NEXT ) {
		new_index = vtactive;
		for( i = 0; i < vtcount; ++i ) {
			new_index = ++new_index % vtcount;
			if( vttty[new_index]->t_open )
				break;
		}
	} else {
		new_index = vtindex(vtkey_to_dev(key_val));
		if( new_index < 0) {
			putchar( '\007' );
			return;
		}
	}
#else
	switch (key_val) {
	case VTKEY_HOME:
		new_index = 0;
		break;
	case VTKEY_NEXT:
		new_index = vtactive;
		for( i = 0; i < vtcount; ++i ) {
			new_index = ++new_index % vtcount;
			if( vttty[new_index]->t_open )
				break;
		}
		break;
	case VTKEY_PREV:
		new_index = vtactive;
		for( i = 0; i < vtcount; ++i ) {
			new_index = (--new_index+vtcount) % vtcount;
			if( vttty[new_index]->t_open )
				break;
		}
		break;
	case VTKEY_TOGL:
		new_index = vtprevious;
		break;
	default:
		new_index = vtindex(vtkey_to_dev(key_val));
		if( new_index < 0) {
			putchar( '\007' );
			return;
		}
	}
#endif
	T_CON(8, printf("%d->%d ", vtactive, new_index));
	if( new_index == vtactive )
		return;

	/* Save which locking shift states are in effect. */

	vp_old = vtdata[vtactive];
	vp_new = vtdata[new_index];

	vp_old->vnkb_shift = lockshift;
	vtdeactivate(vp_new, vp_old);	/* deactivate old virtual terminal */

	/* Restore shift lock state, append current momentary shift state. */
	shift = vp_new->vnkb_shift | nolockshift;
	T_CON(2, printf("new shift=%x sh_index=%d\n", shift, sh_index));
	vtactivate(vp_new);		/* activate new virtual terminal */
	updterminal(new_index);
	vtprevious = vtactive;
	vtactive = new_index;		/* update vtactive */
}

vtdeactivate(vp_new, vp_old)
register VTDATA	*vp_new, *vp_old;
{
	register i;
	VTDATA	*vpi;

	/* store old screen contents in memory segment */
	ffcopy (vp_old->vmm_voff, vp_old->vmm_vseg,
		vp_old->vmm_moff, vp_old->vmm_mseg, TEXTBLOCK);

	/*
	 * if changing to another screen on same video board
	 *	for all screens on same board as new screen
	 *		deactivate, but don't update
	 * else - changing to a screen on different board
	 *	for all screens NOT on same board as new screen
	 *		deactivate, but don't update
	 */
	if ( vp_old->vmm_port == vp_new->vmm_port ) {
		T_CON(8, printf("deactivate on %x ", vp_new->vmm_port));
		for (i = 0; i < vtcount; ++i) {
			vpi = vtdata[i];
			if ( vpi->vmm_port == vp_new->vmm_port ) {
				/* deactivate, but don't update */
				vpi->vmm_invis = ~0; 
	 			vpi->vmm_visible = VNKB_FALSE;
				vpi->vmm_seg = vpi->vmm_mseg;
				vpi->vmm_off = vpi->vmm_moff;
				if( vpi->vmm_seg == 0 )
					printf( "[1]vpi->vmm_seg = 0\n" );
				PRINTV( "vt.back %d. seg %x off %x\n", i,
					vpi->vmm_seg, vpi->vmm_off );
			}
		}
	} else {
		T_CON(8, printf("deactivate %x->%x ",
		  vp_old->vmm_port, vp_new->vmm_port));
		for (i = 0; i < vtcount; ++i) {
			vpi = vtdata[i];
			if ( (vpi->vmm_port != vp_new->vmm_port) 
			  && (vpi->vmm_invis == 0) ) {
				/* update, but don't deactivate */
				vpi->vmm_invis = ~0; 
				vtupdscreen(i);
			}
		}
	}
}

vtactivate(vp)
VTDATA *vp;
{
	register VTDATA	*vpi;
	register i;

	/* 
	 * copy from screen contents from heap segment to video memory 
	 * only if necessary
	 */
	if (vp->vmm_visible == VNKB_FALSE)
		ffcopy (vp->vmm_moff, vp->vmm_mseg,
			vp->vmm_voff, vp->vmm_vseg, TEXTBLOCK);

	for (i = 0 ; i < vtcount ; ++ i) {
		vpi = vtdata [i];
		if (vpi->vmm_port == vp->vmm_port) {
			vpi->vmm_invis = -1;
			vpi->vmm_visible = VNKB_FALSE;
			vpi->vmm_seg = vpi->vmm_mseg;
			vpi->vmm_off = vpi->vmm_moff;
			if (vpi->vmm_seg == 0)
				printf ("[2]vpi->vmm_seg = 0\n");
			PRINTV ("vt.back seg %x off %x\n",
				vpi->vmm_seg, vpi->vmm_off);
		}		
	}
	/*
	 * Set new active terminal
	 */
	vp->vmm_invis = 0;	
	vp->vmm_visible = VNKB_TRUE;
	vp->vmm_seg = vp->vmm_vseg;
	vp->vmm_off = vp->vmm_voff;
	if (vp->vmm_seg == 0)
		printf ("vp->vmm_seg = 0\n");
}

/*
 * update the terminal to match vtactive
 */
updterminal(index)
int index;
{
	vtupdscreen(index);
	updleds2();
}

/*
 * Given a function key number (e.g. vt0),
 * return the corresponding minor device number.
 *
 * Assume valid key number (VTKEY(fnum) is true) by the time we get here.
 */
int
vtkey_to_dev(fnum)
int fnum;
{
	if (fnum >=vt0 && fnum <= vt15)
		return fnum-vt0+1;
	if (fnum >=color0 && fnum <= color15)
		return (fnum-color0)|(VT_PHYSICAL|VT_HW_COLOR);
	if (fnum >=mono0 && fnum <= mono15)
		return (fnum-mono0)|(VT_PHYSICAL|VT_HW_MONO);
	printf("vtkey_to_dev(%d)! ", fnum);
	return 0;
}

#if GREEKFIX
/*
 * ToggleGreek() must be called every time Alt+Enter is pressed.
 * It toggles the "InGreek" flag and resets all others.
 *
 * ToGreek(unsigned *) returns FALSE if val is a dead key (Greek
 * accent key) that must NOT be processed, TRUE otherwise.
*/	

static int	InGreek=0;
static int	Tonos=0;
static int	Dialytika=0;

static int	UpperG[26] = {
  128,129,150,131,132,148,130,134,136,141,137,138,139,
  140,142,143,58,144,145,146,135,151,145,149,147,133};

static int	LowerG[26] = {
  152,153,175,155,156,173,154,158,160,165,161,162,163,
  164,166,167,59,168,169,171,159,224,170,174,172,157};

static int	VoyelG[7] = {152,156,158,160,166,172,224};
static int	TonedG[7] = {225,226,227,229,230,231,233};

void
ToggleGreek()
{
	InGreek 	^= 1;
	Tonos		 = 0;
	Dialytika	 = 0;
	return;
}

int
ToGreek(ip)
unsigned	*ip;
{
	unsigned	i,j;

	i = *ip;

	/*	If Not Greek exit			*/
	if(!InGreek)
		return 1;

	/*	Capture dead keys			*/
	if(i == ';') {
		Tonos ^= 1;
		return 0;
	}	

	if(i == ':') {
		Dialytika ^= 1;
		return 0;
	}	

	/*	Check if character translation needed	*/
	if ((i >= 'A') && (i <= 'Z'))
		i = UpperG[i - 'A'];
	else if	((i >= 'a') && (i <= 'z'))
		i = LowerG[i - 'a'];
	else
		return 1;

	/*	Check if any accent has to be added	*/
	if (Tonos) {
		Tonos = 0;
		for(j = 0;j < 7;j++)
		if (i == VoyelG[j]) {
			i = TonedG[j];
			break;
		}
	} else if (Dialytika) {
		Dialytika=0;
		if (i == 160)
			i = 228;
		else if (i == 172)
			i = 232;
	}

	/*	Exit point for translated characters	*/
	*(ip) = i;
	return 1;
}

#endif /* GREEKFIX */
/* End of nkb.c */
