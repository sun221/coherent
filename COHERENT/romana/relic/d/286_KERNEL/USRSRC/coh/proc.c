/* $Header: /newbits/286_KERNEL/USRSRC/coh/RCS/proc.c,v 1.1 92/01/09 13:29:01 bin Exp Locker: bin $ */
/* (lgl-
 *	The information contained herein is a trade secret of Mark Williams
 *	Company, and  is confidential information.  It is provided  under a
 *	license agreement,  and may be  copied or disclosed  only under the
 *	terms of  that agreement.  Any  reproduction or disclosure  of this
 *	material without the express written authorization of Mark Williams
 *	Company or persuant to the license agreement is unlawful.
 *
 *	COHERENT Version 2.3.37
 *	Copyright (c) 1982, 1983, 1984.
 *	An unpublished work by Mark Williams Company, Chicago.
 *	All rights reserved.
 -lgl) */
/*
 * Coherent.
 * Process handling and scheduling.
 *
 * $Log:	proc.c,v $
 * Revision 1.1  92/01/09  13:29:01  bin
 * Initial revision
 * 
 * Revision 1.2	88/08/05  15:30:01	src
 * pfork() made more rigorous, supports loadable driver forks, etc.
 * lock/unlock more efficient, since know wakeup is synchronous.
 * 
 * Revision 1.1	88/03/24  16:14:16	src
 * Initial revision
 * 
 * 88/03/10	Allan Cornish		/usr/src/sys/coh/proc.c
 * Numerous temporary fixes due to AMD 286 chip being buggy in protected mode.
 * These partial fixes will be removed once all CPU's are replaced.
 *
 * 88/01/21	Allan Cornish		/usr/src/sys/coh/proc.c
 * Race condition caused by pexit() calling sfree() on the user-area
 * when the segmentation gate is locked is now prevented.
 * Release of the user area now deferred until relproc() invoked by uwait().
 *
 * 87/11/13	Allan Cornish		/usr/src/sys/coh/proc.c
 * pexit() now sets uasa to 0 before dispatching processor.
 *
 * 87/11/05	Allan Cornish		/usr/src/sys/coh/proc.c
 * New seg struct now used to allow extended addressing.
 *
 * 87/07/08	Allan Cornish		/usr/src/sys/coh/proc.c
 * pexit() now cancels poll/alarm timed functions before terminating.
 *
 * 87/01/05	Allan Cornish		/usr/src/sys/coh/proc.c
 * pexit() now wakes the swapper before terminating.
 */
#include <sys/coherent.h>
#include <acct.h>
#include <errno.h>
#include <sys/inode.h>
#include <sys/proc.h>
#include <sys/ptrace.h>
#include <sys/sched.h>
#include <sys/seg.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/uproc.h>

/*
 * Initialisation.
 * Set up the hash table queues.
 */
pcsinit()
{
	register PROC *pp;
	register PLINK *lp;

	pp = &procq;
	SELF = pp;
	procq.p_nforw = pp;
	procq.p_nback = pp;
	procq.p_lforw = pp;
	procq.p_lback = pp;
	for (lp=&linkq[0]; lp<&linkq[NHPLINK]; lp++) {
		lp->p_lforw = lp;
		lp->p_lback = lp;
	}
}

/*
 * Initiate a process.  `f' is a kernel function that is associated with
 * the process.
 */
PROC *
process(f)
int (*f)();
{
	register PROC *pp1;
	register PROC *pp;
	register SEG *sp;
	MCON mcon;

	if ((pp=kalloc(sizeof(PROC))) == NULL)
		return (NULL);

	pp->p_flags = PFCORE;
	pp->p_state = PSRUN;
	pp->p_ttdev = NODEV;

	if (f != NULL) {
		pp->p_flags |= PFKERN;
		sp = salloc((fsize_t)UPASIZE, SFSYST|SFHIGH|SFNSWP);
		if (sp == NULL) {
			kfree(pp);
			return (NULL);
		}
		pp->p_segp[SIUSERP] = sp;
		msetsys( &mcon, f, FP_SEL(sp->s_faddr) );
		kfcopy(	(char *)&mcon,
			sp->s_faddr + offset(uproc, u_syscon),
			sizeof(mcon) );
	}
	lock(pnxgate);
next:
	pp->p_pid = cpid++;
	if (cpid >= NPID)
		cpid = 2;
	pp1 = &procq;
	while ((pp1=pp1->p_nforw) != &procq) {
		if (pp1->p_pid < pp->p_pid)
			break;
		if (pp1->p_pid == pp->p_pid)
			goto next;
	}
	pp->p_nback = pp1->p_nback;
	pp1->p_nback->p_nforw = pp;
	pp->p_nforw = pp1;
	pp1->p_nback = pp;
	unlock(pnxgate);
	return (pp);
}

/*
 * Remove a process from the next queue and release and space.
 */
relproc(pp)
register PROC *pp;
{
	register SEG * sp;

	/*
	 * Child process still has a user-area.
	 */
	if ( (sp = pp->p_segp[SIUSERP]) != NULL ) {

		/*
		 * Detach user-area from child process.
		 */
		pp->p_segp[SIUSERP] = NULL;

		/*
		 * Child process is swapped out.
		 */
		if ( pp->p_flags & PFSWAP )
			sp->s_lrefc++;

		/*
		 * Release child's user-area.
		 */
		sfree( sp );
	}

	/*
	 * Remove process from doubly-linked list of all processes.
	 * Release space allocated for proc structure.
	 */
	lock(pnxgate);
	pp->p_nback->p_nforw = pp->p_nforw;
	pp->p_nforw->p_nback = pp->p_nback;
	unlock(pnxgate);
	kfree(pp);
}

/*
 * Create a clone of ourselves.
 *	N.B. - consave(&mcon) returns twice; anything not initialized
 *	in automatic storage before the call to segadup() will not be
 *	initialized when the second return from consave() commences.
 */
pfork()
{
	register PROC *cpp;
	register PROC *pp;
	register int s;
	MCON mcon;

	if ((cpp=process(NULL)) == NULL) {
		u.u_error = EAGAIN;
		return;
	}

	s = sphi();	/* Make usave a null macro if unnecessary */
	usave();	/* Put the current copy of uarea into its segment */
	spl(s);

	if (segadup(cpp) == 0) {
		u.u_error = EAGAIN;
		relproc(cpp);
		return;
	}
	if ( u.u_rdir != NULL )
		u.u_rdir->i_refc++;
	if ( u.u_cdir != NULL )
		u.u_cdir->i_refc++;
	fdadupl();
	pp = SELF;
	cpp->p_uid   = pp->p_uid;
	cpp->p_ruid  = pp->p_ruid;
	cpp->p_rgid  = pp->p_rgid;
	cpp->p_ppid  = pp->p_pid;
	cpp->p_ttdev = pp->p_ttdev;
	cpp->p_group = pp->p_group;
	cpp->p_ssig  = pp->p_ssig;
	cpp->p_isig  = pp->p_isig;
	cpp->p_cval  = CVCHILD;
	cpp->p_ival  = IVCHILD;
	cpp->p_sval  = SVCHILD;
	cpp->p_rval  = RVCHILD;

	s = sphi();
	consave(&mcon);
	spl( s );

	/*
	 * Parent process.
	 */
	if ( (pp = SELF) != cpp ) {
		segfinm(cpp->p_segp[SIUSERP]);
		kfcopy( (char *)&mcon,
			cpp->p_segp[SIUSERP]->s_faddr + offset(uproc,u_syscon),
			sizeof(mcon) );
		mfixcon(cpp);
		s = sphi();
		setrun(cpp);
		spl(s);
		return( cpp->p_pid );
	}

	/*
	 * Child process.
	 */
	else {
		u.u_btime = timer.t_time;
		u.u_flag = AFORK;
		/* for (i=0; i<NUSEG; i++) done in sproto */
			/* u.u_segl[i].sr_segp = pp->p_segp[i]; ditto */
		sproto();
		segload();
		return( 0 );
	}
}

/*
 * Die.
 */
pexit(s)
{
	register PROC *pp1;
	register PROC *pp;
	register SEG  *sp;
	register int n;

	pp = SELF;

	/*
	 * Cancel alarm and poll timers [if any].
	 */
	timeout( &pp->p_alrmtim, 0, NULL, 0 );
	timeout( &pp->p_polltim, 0, NULL, 0 );

	/*
	 * Write out accounting directory and close all files associated with
	 * this process.
	 */
	setacct();
	if ( u.u_rdir )
		ldetach(u.u_rdir);
	if ( u.u_cdir )
		ldetach(u.u_cdir);
	fdaclose();

	/*
	 * Free all segments in reverse order, except for user-area.
	 */
	for ( n = NUSEG; --n > 0; ) {
		if ( (sp = pp->p_segp[n]) != NULL ) {
			pp->p_segp[n] = NULL;
			sfree( sp );
		}
	}

	/*
	 * Wakeup our parent.  If we have any children, init will become the
	 * new parent.  If there are any children we are tracing who are
	 * waiting for us, we wake them up.
	 */
	pp1 = &procq;
	while ((pp1=pp1->p_nforw) != &procq) {
		if (pp1->p_pid == pp->p_ppid) {
			if (pp1->p_state==PSSLEEP && pp1->p_event==(char *)pp1)
				wakeup((char *)pp1);
		}
		if (pp1->p_ppid == pp->p_pid) {
			pp1->p_ppid = 1;
			if (pp1->p_state == PSDEAD)
				wakeup((char *)eprocp);
			if ((pp1->p_flags&PFTRAC) != 0)
				wakeup((char *)&pts.pt_req);
		}
	}

	/*
	 * Wake up swapper if swap timer is active.
	 */
	if ( stimer.t_last != 0 )
		wakeup( (char *) &stimer );

	/*
	 * And finally mark us as dead and give up the processor forever.
	 */
	pp->p_exit = s;
	pp->p_state = PSDEAD;
	uasa = 0;
	dispatch();
}

/*
 * Sleep on the event `e'.  This gives up the processor until someone
 * wakes us up.  Since it is possible for many people to sleep on the
 * same event, the caller when awakened should make sure that what he
 * was waiting for has completed and if not, go to sleep again.  `cl'
 * is the cpu value we get to get the cpu as soon as we are woken up.
 * `sl' is the swap value we get to keep us in memory for the duration
 * of the sleep.  `sr' is the swap value that allows us to get swapped
 * in if we have been swapped out.
 */
sleep(e, cl, sl, sr)
char *e;
{
	register PROC *bp;
	register PROC *fp;
	register PROC *pp;
	register int s;

	pp = SELF;

	/*
	 * See if we have a signal awaiting.
	 */
	if (cl<CVNOSIG && pp->p_ssig && nondsig()) {
		sphi();
		envrest(&u.u_sigenv);
	}

	/*
	 * Get ready to go to sleep and do so.
	 */
	s = sphi();
	pp->p_state = PSSLEEP;
	pp->p_event = e;
	pp->p_lctim = utimer;
	addu(pp->p_cval, cl);
	pp->p_ival = sl;
	pp->p_rval = sr;
	fp = &linkq[hash(e)];
	bp = fp->p_lback;
	pp->p_lforw = fp;
	fp->p_lback = pp;
	pp->p_lback = bp;
	bp->p_lforw = pp;
	spl(s);
	dispatch();

	/*
	 * We have just woken up.  Get ready to return.
	 */
	subu(pp->p_cval, cl);
	pp->p_ival = 0;
	pp->p_rval = 0;

	/*
	 * Check for an interrupted system call.
	 */
	if (cl<CVNOSIG && pp->p_ssig && nondsig()) {
		sphi();
		envrest(&u.u_sigenv);
	}
}

/*
 * Defer function to wake up all processes sleeping on the event `e'.
 */
wakeup(e)
char *e;
{
	extern void dwakeup();

	defer( dwakeup, e );
}

/*
 * Wake up all processes sleeping on the event `e'.
 */
static void
dwakeup( e )
char *e;
{
	register PROC *pp;
	register PROC *pp1;
	register int s;

	/*
	 * Identify event queue to check.
	 * Disable interrupts.
	 */
	pp1 = &linkq[hash(e)];
	pp = pp1;
	s = sphi();

	/*
	 * Traverse doubly-linked circular event-queue.
	 */
	while ( (pp = pp->p_lforw) != pp1 ) {

		/*
		 * Process is waiting on event 'e'.
		 */
		if ( pp->p_event == e ) {
			/*
			 * Remove process from event queue.
			 * Update process priority.
			 * Insert process into run queue.
			 */
			pp->p_lback->p_lforw = pp->p_lforw;
			pp->p_lforw->p_lback = pp->p_lback;
			addu( pp->p_cval, (utimer-pp->p_lctim)*CVCLOCK );
			setrun( pp );

			/*
			 * Enable interrupts.
			 * Restart search at start of event queue.
			 * Disable interrupts.
			 */
			spl( s );
			pp = pp1;
			s = sphi();
		}
	}
	spl(s);
}

/*
 * Reschedule the processor.
 */
dispatch()
{
	register PROC *pp1;
	register PROC *pp2;
	register unsigned v;
	register int s;

	s = sphi();
	pp1 = iprocp;
	pp2 = &procq;
	v = 0;
	while ((pp2=pp2->p_lforw) != &procq) {
		v -= pp2->p_cval;
		if ((pp2->p_flags&PFCORE) == 0)
			continue;
		pp1 = pp2->p_lforw;
		pp1->p_cval += pp2->p_cval;
		pp2->p_cval = v;
		pp1->p_lback = pp2->p_lback;
		pp1->p_lback->p_lforw = pp1;
		pp1 = pp2;
		break;
	}
	spl(s);

	quantum = NCRTICK;
	disflag = 0;
	if ( pp1 != SELF ) {
		/*
		 * Consave() returns twice.
		 * 1st time is after our context is saved in u.u_syscon,
		 *	whereupon we should restore other proc's context.
		 * 2nd time is after our context is restored by another proc.
		 * Conrest() forces a context switch to a new process.
		 */
		s = sphi();
		SELF = pp1;
		if (consave(&u.u_syscon) == 0)
			conrest( FP_SEL(pp1->p_u->s_faddr),
				 offset(uproc,u_syscon) );
		if ( SELF->p_pid != 0 )
			segload();
		spl(s);
	}
}

/*
 * Add a process to the run queue.
 * This routine must be called at high priority.
 */
setrun(pp1)
register PROC *pp1;
{
	register PROC *pp2;
	register unsigned v;

	v = 0;
	pp2 = &procq;
	for (;;) {
		pp2 = pp2->p_lback;
		if ((v+=pp2->p_lforw->p_cval) >= pp1->p_cval)
			break;
		if (pp2 == &procq)
			break;
	}
	pp2->p_lforw->p_lback = pp1;
	pp1->p_lforw = pp2->p_lforw;
	pp2->p_lforw = pp1;
	pp1->p_lback = pp2;
	v -= pp1->p_cval;
	pp1->p_cval = v;
	pp1->p_lforw->p_cval -= v;
	pp1->p_state = PSRUN;
}

/*
 * Wait for the gate `g' to unlock, and then lock it.
 */
lock(g)
register GATE g;
{
	register int s;

	s = sphi();
	while (g[0]) {
		g[1] = 1;
		sleep((char *)g, CVGATE, IVGATE, SVGATE);
	}
	g[0] = 1;
	spl(s);
}

/*
 * Unlock the gate `g'.
 */
unlock(g)
register GATE g;
{
	g[0] = 0;
	if (g[1]) {
		g[1] = 0;
		disflag = 1;
		wakeup((char *)g);
	}
}
