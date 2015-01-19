/* (lgl-
 *	The information contained herein is a trade secret of Mark Williams
 *	Company, and  is confidential information.  It is provided  under a
 *	license agreement,  and may be  copied or disclosed  only under the
 *	terms of  that agreement.  Any  reproduction or disclosure  of this
 *	material without the express written authorization of Mark Williams
 *	Company or persuant to the license agreement is unlawful.
 *
 *	COHERENT Version 5.0
 *	Copyright (c) 1982, 1993.
 *	An unpublished work by Mark Williams Company, Chicago.
 *	All rights reserved.
 -lgl) */
/*
 * File:	coh.386/sig.c
 *
 * Purpose:	signal handling
 *
 * Revised: Tue May  4 11:59:15 1993 CDT
 */

/*
 * ----------------------------------------------------------------------
 * Includes.
 */

#include <common/_limits.h>
#include <common/_wait.h>
#include <common/_gregset.h>
#include <kernel/sigproc.h>

#include <sys/coherent.h>
#include <sys/errno.h>
#include <sys/ino.h>
#include <sys/inode.h>
#include <sys/io.h>
#include <sys/proc.h>
#include <sys/ptrace.h>
#include <sys/sched.h>
#include <sys/seg.h>
#include <sys/file.h>
#include <sys/core.h>


/*
 * ----------------------------------------------------------------------
 * Definitions.
 *	Constants.
 *	Macros with argument lists.
 *	Typedefs.
 *	Enums.
 */

/*
 * ----------------------------------------------------------------------
 * Functions.
 *	Import Functions.
 *	Export Functions.
 *	Local Functions.
 */
void	curr_check_signals();
int	ptset();
void	sendsig();
int	sigdump();
__sigfunc_t	usigsys();

static struct _fpstate * empack();
static	int		ptret();

/*
 * ----------------------------------------------------------------------
 * Global Data.
 *	Import Variables.
 *	Export Variables.
 *	Local Variables.
 */
/*
 * Patchable variables.
 *
 * Patch DUMP_TEXT nonzero to make text segment show up in core files.
 * Patch DUMP_LIM set the upper limit in bytes of how much of a
 * segment is written to a core file.
 *
 * CATCH_SEGV can be patched to allow signal () to be used to catch the
 * SIGSEGV signal (). This is off by default because:
 *   i)	Experience has shown that kernel printf () messages being on by
 *	default is very useful for catching bugs.
 *  ii)	Certain extremely ill-behaved applications apparently catch SIGSEGV
 *	blindly as part of some catch-all behaviour, and when such faults
 *	happen have been known to loop generating kernel printf () output
 *	which bogs down the system unacceptably.
 * iii)	Turning the signal off to avoid ii) causes less actual disruption than
 *	leaving it on.
 * This only applies to the signal () interface; we assume that apps which are
 * smart enough to use sigset () or sigaction () also know what to do with
 * SIGSEGV.
 */

int	DUMP_TEXT = 0;
int	DUMP_LIM=512*1024;
int	CATCH_SEGV = 0;

/*
 * ----------------------------------------------------------------------
 * Code.
 */

/*
 * Or an entire signal mask into the "hold" mask.
 */

static void
sigMask (mask)
__sigset_t * mask;
{
	int		i;
	__sigmask_t   *	loop = mask->_sigbits;
	__sigset_t	signal_mask;

	curr_signal_mask (NULL, & signal_mask);

	for (i = 0 ; i < __ARRAY_LENGTH (mask->_sigbits) ; i ++)
		signal_mask._sigbits [i] |= * loop ++;

	curr_signal_mask (& signal_mask, NULL);
}


/*
 * Set up the action to be taken on a signal.
 */

__sigfunc_t
usigsys (signal, func, regsetp)
unsigned	signal;
__sigfunc_t	func;
gregset_t     *	regsetp;
{
	int		sigtype;
	__sigmask_t	mask;
	__sigset_t	signal_mask;
	__sigaction_t	signal_action;

	sigtype = signal & ~ 0xFF;
	signal &= 0xFF;

#if 0
	T_HAL(8, printf("[%d]sig(%d, %x) ", SELF->p_pid, signal, func));
#endif

	/*
	 * Check the passed signal number. SIGKILL and SIGSTOP are not allowed
	 * to be caught.
	 */

	/* Range check on 1-based signal number. */
	if (signal <= 0 || signal > NSIG || signal == SIGSTOP ||
	    signal == SIGKILL) {
		u.u_error = EINVAL;
		return;
	}

	signal_action.sa_handler = func;
	signal_action.sa_flags = 0;
	__SIGSET_EMPTY (signal_action.sa_mask);
	func = 0;

	curr_signal_mask (NULL, & signal_mask);

	mask = __SIGSET_MASK (signal);

	switch (sigtype) {
	case SIGHOLD:
		__SIGSET_ADDMASK (signal_mask, signal, mask);
		break;

	case SIGRELSE:
		__SIGSET_CLRMASK (signal_mask, signal, mask);
		break;

	case SIGIGNORE:
		__SIGSET_CLRMASK (signal_mask, signal, mask);
		signal_action.sa_handler = SIG_IGN;
		curr_signal_action (signal, & signal_action, NULL);
		break;

	case 0:				/* signal () */
		if (signal == SIGSEGV && CATCH_SEGV == 0) {
			u.u_error = EINVAL;
			return 0;
		}
		u.u_sigreturn = (__sigfunc_t) regsetp->_i386._edx;

		if (signal == SIGCHLD) {
			/*
			 * With signal (), the argument implicitly mangles the
			 * SA_NOCLDWAIT flag. I have no idea whether the
			 * SA_NOCLDSTOP flag is also implicitly affected, but
			 * for now we leave it alone.
			 */

			curr_signal_misc (__SF_NOCLDWAIT,
					  signal_action.sa_handler ==
						SIG_IGN ? __SF_NOCLDWAIT : 0);
		}
		signal_action.sa_flags |= __SA_RESETHAND;
		curr_signal_action (signal, & signal_action, & signal_action);

		/*
		 * Using the signal () interface automatically causes a
		 * pending signal to be discarded.
		 */

		proc_unkill (SELF, signal);
		return signal_action.sa_handler;		

	case SIGSET:
		u.u_sigreturn = (__sigfunc_t) regsetp->_i386._edx;

		if (__SIGSET_TSTMASK (signal_mask, signal, mask))
			func = SIG_HOLD;

		if (signal_action.sa_handler == SIG_HOLD) {

			__SIGSET_ADDMASK (signal_mask, signal, mask);

			if (func != SIG_HOLD) {
				curr_signal_action (signal, NULL,
						    & signal_action);
				func = signal_action.sa_handler;
			}
			break;
		}

		if (signal == SIGCHLD) {
			/*
			 * With sigset (), the argument implicitly mangles the
			 * SA_NOCLDWAIT flag. I have no idea whether the
			 * SA_NOCLDSTOP flag is also implicitly affected, but
			 * for now we leave it alone.
			 */

			curr_signal_misc (__SF_NOCLDWAIT,
					  signal_action.sa_handler ==
						SIG_IGN ? __SF_NOCLDWAIT : 0);
		}
		__SIGSET_CLRMASK (signal_mask, signal, mask);
		__SIGSET_ADDMASK (signal_action.sa_mask, signal, mask);
		curr_signal_action (signal, & signal_action, & signal_action);

		if (func != SIG_HOLD)
			func = signal_action.sa_handler;
		break;

	case SIGPAUSE:
		/*
		 * Like upause(), do a sleep on an event which never gets a
		 * wakeup. The sleep returns immediately if a signal was
		 * already holding.
		 */

		__SIGSET_CLRMASK (signal_mask, signal, mask);
		curr_signal_mask (& signal_mask, NULL);
		(void) x_sleep ((char *) & u, prilo, slpriSigCatch,
				"sigpause");
		return 0;

	default:
		u.u_error = SIGSYS;
		return 0;
	}

	curr_signal_mask (& signal_mask, NULL);
	return func;
}

/*
 * Send a signal to the process `pp'. This function is only valid for use with
 * signals generated from user level with kill (), sigsend () or
 * sigsendset ().
 */

void
sendsig(sig, pp)
register unsigned sig;
register PROC *pp;
{
	__siginfo_t	siginfo;

#if 0
	T_HAL(8, if (sig == SIGINT) printf("[%d]gets int ", pp->p_pid));
#else
	T_HAL(8, printf ("[%d] sig=%d ", pp->p_pid, sig));
#endif

	siginfo.__si_signo = sig;
	siginfo.__si_code = 0;
	siginfo.__si_errno = 0;
	siginfo.__si_pid = SELF->p_pid;
	siginfo.__si_uid = SELF->p_uid;

	proc_send_signal (pp, & siginfo);
}


/*
 * Return signal number if we have a non ignored or delayed signal, else zero.
 */

int
nondsig()
{
	return curr_signal_pending ();
}


/*
 * If we have a signal that isn't ignored, activate it. The register set is
 * not "const" because the low-level trap code that uses it wants to modify
 * it.
 */

#if	__USE_PROTO__
void curr_check_signals (gregset_t * regsetp)
#else
void
curr_check_signals (regsetp)
gregset_t     *	regsetp;
#endif
{
	register int signum;
	int ptval;

	/*
	 * Fetch an unprocessed signal.
	 * Return if there are none.
	 * The while() structure is only for traced processes.
	 */
	while ((signum = curr_signal_pending ()) != 0) {
		__sigaction_t	signal_action;

		/*
		 * Reset the signal to indicate that it has been processed,
		 * and fetch the signal disposition.
		 */

got_signal:
		proc_unkill (SELF, signum);
		curr_signal_action (signum, NULL, & signal_action);

		/*
		 * Store the signal number in the u area.
		 * This is how a core dump records the death signal.
		 */
		u.u_signo = signum;

		/*
		 * If the signal is not defaulted, go run the requested
		 * function.
		 */

		if (signal_action.sa_handler != SIG_DFL) {
			if (__xmode_286 (regsetp))
				oldsigstart (signum, signal_action.sa_handler,
					     regsetp);
			else
				msigstart (signum, signal_action.sa_handler,
					   regsetp);

	/*
	 * If the signal needs to be reset after delivery, do so. Note that
	 * all signal-related activity goes though the defined function
	 * interface; many subtle things may need to happen, so we let the
	 * layering take care of it.
	 */

			if ((signal_action.sa_flags & __SA_RESETHAND) != 0) {
				signal_action.sa_handler = SIG_DFL;
				curr_signal_action (signum, & signal_action,
						    NULL);
			} else
				sigMask (& signal_action.sa_mask);

			return;
		}

		/*
		 * msysgen() is a nop for COHERENT 4.0.  The comment in the
		 * assembly code is "Nothing useful to save".
		 */

		msysgen (u.u_sysgen);

		/*
		 * When a traced process is signaled, it may need to exchange
		 * data with its parent (via ptret).
		 */

		if ((SELF->p_flags & PFTRAC) != 0) {
			SELF->p_flags |= PFWAIT;
			ptval = ptret (regsetp);

			T_HAL(0x10000, printf("ptret()=%x ", ptval));

			SELF->p_flags &= ~ (PFWAIT | PFSTOP);

			while ((signum = curr_signal_pending ()) != 0)
				proc_unkill (SELF, signum);

			if (ptval == 0)
				/* see if another signal came in */
				continue;

			if ((signum = ptval) != SIGKILL)
				goto got_signal;
		}

		/*
		 * Some signals cause a core file to be written.
		 */
		switch (signum) {
		case SIGQUIT:
		case SIGILL:
		case SIGTRAP:
		case SIGABRT:
		case SIGFPE:
		case SIGSEGV:
		case SIGSYS:
			if (sigdump ())
				signum |= __WCOREFLG;
			break;
		}
		pexit (signum);
	}
}

/*
 * Create a dump of ourselves onto the file `core'.
 */

int
sigdump ()
{
	register INODE *ip;
	register SR *srp;
	register SEG * sp;
	register int n;
	register paddr_t ssize;
	extern	int	DUMP_LIM;
	struct ch_info chInfo;
	IO		io;
	struct direct	dir;

	if (SELF->p_flags & PFNDMP)
		return 0;

	/* Make the core with the real owners */
	schizo ();

	io.io_seg = IOSYS;
	io.io_flag = 0;
	if (ftoi ("core", 'c', & io, & dir)) {
		schizo ();
		return 0;
	}

	if ((ip = u.u_cdiri) == NULL) {
		if ((ip = imake (IFREG | 0644, 0, & io, & dir)) == NULL) {
			schizo ();
			return 0;
		}
	} else {
		if ((ip->i_mode & IFMT) != IFREG || iaccess (ip, IPW) == 0 ||
		    getment (ip->i_dev, 1) == NULL) {
			idetach (ip);
			schizo ();
			return 0;
		}
		iclear (ip);
	}
	schizo ();
	u.u_error = 0;

	/* Write core file header */
	chInfo.ch_magic = CORE_MAGIC;
	chInfo.ch_info_len = sizeof (chInfo);
	chInfo.ch_uproc_offset = U_OFFSET;

	io.io_seek = 0;
	io.io_seg = IOSYS;
	io.io.vbase = & chInfo;
	io.io_ioc = sizeof (chInfo);
	io.io_flag = 0;

	iwrite (ip, & io);

	/*
	 * Added to aid in kernel debugging - if DUMP_TEXT is nonzero,
	 * dump the text segment (to see if it was corrupted) and set
	 * the dump flag so that postmortem utilities will know that
	 * text is present in the core file.
	 */

	if (DUMP_TEXT)
		u.u_segl [SISTEXT].sr_flag |= SRFDUMP;

	for (srp = u.u_segl + 1 ; u.u_error == 0 && srp < u.u_segl + NUSEG ;
	     srp ++) {

		if ((srp->sr_flag & SRFDUMP) == 0)
			continue;

		/* Don't try to dump empty segments. */
		if ((sp = srp->sr_segp) == NULL) {
			srp->sr_flag &= ~SRFDUMP;
			continue;
		}

		/* Don't dump segments too big to dump. */
		if (sp->s_size > DUMP_LIM)
			srp->sr_flag &= ~SRFDUMP;
	}

	/* Always dump the U segment. */
	u.u_segl [SIUSERP].sr_flag |= SRFDUMP;

	for (srp = u.u_segl ; u.u_error == 0 && srp < u.u_segl + NUSEG ;
	     srp ++) {

		/* Only dump segments flagged for dumping. */
		if ((srp->sr_flag & SRFDUMP) == 0)
			continue;

		sp = srp->sr_segp;

		ssize = sp->s_size;
		io.io_seg = IOPHY;
		io.io.pbase = MAPIO (sp->s_vmem, 0);
		io.io_flag = 0;
		sp->s_lrefc ++;
		while (u.u_error == 0 && ssize != 0) {
			n = ssize > SCHUNK ? SCHUNK : ssize;
			io.io_ioc = n;
			iwrite (ip, & io);
			io.io.pbase += n;
			ssize -= (paddr_t) n;
		}
		sp->s_lrefc --;
	}
	idetach (ip);
	return u.u_error == 0;
}

/*
 * Send a ptrace command to the child.
 *
 * "pid" is child pid.
 */

int
ptset(req, pid, addr, data)
unsigned req;
int *addr;
{
	register PROC *pp;

	lock (pnxgate);
	for (pp = procq.p_nforw ; pp != & procq ; pp = pp->p_nforw)
		if (pp->p_pid == pid)
			break;
	unlock (pnxgate);
	if (pp == & procq || (pp->p_flags & PFSTOP) == 0 ||
	    pp->p_ppid != SELF->p_pid) {
		u.u_error = ESRCH;
		return;
	}

	lock (pts.pt_gate);
	pts.pt_req = req;
	pts.pt_pid = pid;
	pts.pt_addr = addr;
	pts.pt_data = data;
	pts.pt_errs = 0;
	pts.pt_rval = 0;
	pts.pt_busy = 1;

	wakeup ((char *) & pts.pt_req);

	while (pts.pt_busy) {
		x_sleep ((char *) & pts.pt_busy, primed, slpriSigCatch,
			 "ptrace");
		/* Send a ptrace command to the child.  */
	}

	u.u_error = pts.pt_errs;
	unlock (pts.pt_gate);
	return pts.pt_rval;
}

/*
 * This routine is called when a child that is being traced receives a signal
 * that is not caught or ignored.  It follows up on any requests by the parent
 * and returns when done.
 *
 * After ptrace handling done in this routine, a real or simulated signal
 * may need to be sent to the traced process.
 * Return a signal number to be sent to the child process, or 0 if none.
 */

static int
ptret (regsetp)
gregset_t     *	regsetp;
{
	extern void (*ndpKfrstor)();
	register PROC *pp;
	register PROC *pp1;
	register int sign;
	unsigned off;
	int doEmUnpack = 0;
	struct _fpstate * fstp = empack ();

	pp = SELF;
next:
	u.u_error = 0;
	if (pp->p_ppid == 1)
		return SIGKILL;
	sign = -1;

	/* wake up parent if it is sleeping */
	lock (pnxgate);
	pp1 = & procq;
	for (;;) {
		if ((pp1 = pp1->p_nforw) == & procq) {
			sign = SIGKILL;
			break;
		}
		if (pp1->p_pid != pp->p_ppid)
			continue;
		if (ASLEEP (pp1))
			wakeup ((char *) pp1);
		break;
	}
	unlock (pnxgate);

	while (sign < 0) {
		/* If no pending ptrace transaction for this process, sleep. */
		if (pts.pt_busy == 0 || pp->p_pid != pts.pt_pid) {
			/*
			 * If a signal bit is set now, just exit - let
			 * actvsig() handle it next time through.
			 * Doing sleep and goto next will stick us in a loop
			 */

			if (nondsig ())
				return 0;
			x_sleep ((char *) & pts.pt_req, primed,
				 slpriSigCatch, "ptret");
			goto next;
		}

		switch (pts.pt_req) {
		case PTRACE_RD_TXT:
			if (__xmode_286 (regsetp)) {
				pts.pt_rval = getuwd (NBPS + pts.pt_addr);
				break;
			}
			/* Fall through for 386 mode processes. */

		case PTRACE_RD_DAT:
			pts.pt_rval = getuwd (pts.pt_addr);
			break;

		case PTRACE_RD_USR:
			/* See ptrace.h for valid offsets. */
			off = (unsigned) pts.pt_addr;
			if (off & 3)
				u.u_error = EINVAL;
			else if (off < PTRACE_FP_CW) {
				/* Reading CPU general register state */
				if (off == PTRACE_SIG)
					pts.pt_rval = u.u_signo;
				else
					pts.pt_rval =
						((int *) regsetp) [off >> 2];
			} else if (off < PTRACE_DR0) {
				/*
				 * Reading NDP state.
				 * If NDP state not already saved, save it.
				 * Fetch desired info.
				 * Restore NDP state in case we will resume.
				 */
				if (rdNdpUser ()) {
					/* if using coprocessor */
					if (! rdNdpSaved ()) {
						ndpSave (& u.u_ndpCon);
						wrNdpSaved (1);
					}
pts.pt_rval = ((int *) & u.u_ndpCon) [(off - PTRACE_FP_CW) >> 2];
					ndpRestore (& u.u_ndpCon);
					wrNdpSaved (0);
				} else if (fstp) {
pts.pt_rval = getuwd(((int *) fstp) + ((off - PTRACE_FP_CW) >> 2));
					/* if emulating */
				} else { /* no ndp state to display */
					pts.pt_rval = 0;
					u.u_error = EFAULT;
				}
			} else /* Bad pseudo offset. */
				u.u_error = EINVAL;
			break;

		case PTRACE_WR_TXT:
			if (__xmode_286 (regsetp)) {
				putuwd (NBPS + pts.pt_addr, pts.pt_data);
				break;
			}
			/* Fall through for 386 mode processes. */

		case PTRACE_WR_DAT:
			putuwd (pts.pt_addr, pts.pt_data);
			break;

		case PTRACE_WR_USR:
			/* See ptrace.h for valid offsets. */
			off = (unsigned) pts.pt_addr;

			if (off & 3)
				u.u_error = EINVAL;
			else if (off < PTRACE_FP_CW) {
				/* Writing CPU general register state */
				if (off == PTRACE_SIG)
					u.u_error = EINVAL;
				else
					((int *) regsetp) [off >> 2] =
						pts.pt_data;
			} else if (off < PTRACE_DR0) {
				if (rdNdpUser ()) {
					/*
					 * Writing NDP state.
					 * If NDP state not already saved, save it.
					 * Store desired info.
					 * Restore NDP state in case we will resume.
					 */
					if (! rdNdpSaved ()) {
						ndpSave (& u.u_ndpCon);
						wrNdpSaved (1);
					}
((int *)&u.u_ndpCon)[(off - PTRACE_FP_CW)>>2] = pts.pt_data;
					ndpRestore (& u.u_ndpCon);
					wrNdpSaved (0);
				} else if (fstp && ndpKfrstor) {
putuwd(((int *)fstp) + ((off - PTRACE_FP_CW)>>2), pts.pt_data);
					doEmUnpack = 1;
				} else { /* No NDP state to modify. */
					u.u_error = EFAULT;
				}
			} else /* Bad pseudo offset. */
				u.u_error = EINVAL;
			break;

		case PTRACE_RESUME:
			regsetp->_i386._eflags &= ~ MFTTB;
			goto sig;

		case PTRACE_TERM:
			sign = SIGKILL;
			break;

		case PTRACE_SSTEP:
			regsetp->_i386._eflags |= MFTTB;
		sig:
			if (pts.pt_data < 0 || pts.pt_data > NSIG) {
				u.u_error = EINVAL;
				break;
			}
			sign = pts.pt_data;
			break;

		default:
			u.u_error = EINVAL;
		}

		if ((pts.pt_errs = u.u_error) == EFAULT)
			pts.pt_errs = EINVAL;

		pts.pt_busy = 0;
		wakeup((char *) & pts.pt_busy);
	}
	if (doEmUnpack)
		(* ndpKfrstor) (fstp, & u.u_ndpCon);
	return sign;
}

/*
 * If using floating point emulator, make room on user stack and save
 * floating point context there.  Code elsewhere takes care of floating
 * point context if there is a coprocessor.
 *
 * Return the virtual address in user space of the context area, or
 * return NULL if not using FP emulation.
 */

static struct _fpstate *
empack (regsetp)
gregset_t     *	regsetp;
{
	int		sphi;
	struct _fpstate * ret;
	SEG	      *	segp = u.u_segl [SISTACK].sr_segp;
	extern void (*ndpKfsave)();
	unsigned long sw_old;

	/* If not emulating, do nothing */
	if (rdNdpUser () || ! rdEmTrapped () || ! ndpKfsave)
		return NULL;

	/*
	 * Will copy at least u_sigreturn, _fpstackframe, and ndpFlags.
	 * If using ndp, need room for an _fpstate.
	 * If emulating, need room for an _fpemstate.
	 */

	ret = (struct _fpstate *)
		(__xmode_286 (regsetp) ? regsetp->_i286._usp :
					 regsetp->_i386._uesp) - 1;

	/* Add to user stack if necessary. */
	sphi = __xmode_286 (regsetp) ? ISP_286 : ISP_386;

	if (sphi - segp->s_size > (__ptr_arith_t) ret) {
		cseg_t	      *	pp;

		pp = c_extend (segp->s_vmem, btoc (segp->s_size));
		if (pp == 0) {
			printf ("Empack failed.  cmd=%s  c_extend(%x,%x)=0 ",
				u.u_comm, segp->s_vmem, btoc (segp->s_size));
			return NULL;
		}

		segp->s_vmem = pp;
		segp->s_size += NBPC;
		if (sproto (0) == 0) {
			printf ("Empack failed.  cmd=%s  sproto(0)=0 ",
				u.u_comm);
			return NULL;
		}

		segload ();
	}

	(* ndpKfsave) (& u.u_ndpCon, ret);
	sw_old = getuwd (& ret->sw);
	putuwd (& ret->status, sw_old);
	putuwd (& ret->sw, sw_old & 0x7f00);

	return ret;
}
