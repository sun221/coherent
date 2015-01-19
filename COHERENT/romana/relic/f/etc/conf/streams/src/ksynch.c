#define	_DDI_DKI	1
#define	_SYSV4		1


/*
 * System V DDI/DKI compatible synchronisation functions
 *
 * This implements the synchronisation functions introduced in the System V
 * DDI/DKI multiprocessor edition for a simple uniprocessor. The locking
 * implementations given here are totally unsuitable for multiprocessor use.
 *
 * Some good multiprocessor lock algorithms can be found in:
 *	"Synchronisation Without Contention"
 *	John M. Mellor-Crummey & Michael L. Scott,
 *	Proceedings 4th International Conference on Architectural Support for
 *		Programming Languages and Operating Systems (ASPLOS 4)
 *	1991, ACM
 */

/*
 *-IMPORTS:
 *	<common/ccompat.h>
 *		__CONST__
 *		__USE_PROTO__
 *		__ARGS ()
 *	<common/xdebug.h>
 *		__LOCAL__
 *	<kernel/x86lock.h>
 *		atomic_uchar_t
 *		atomic_ushort_t
 *		__atomic_uchar_t
 *		ATOMIC_TEST_AND_SET_UCHAR ()
 *		ATOMIC_FETCH_AND_STORE_USHORT ();
 *		ATOMIC_FETCH_UCHAR ();
 *		ATOMIC_FETCH_USHORT ();
 *		ATOMIC_CLEAR_UCHAR ();
 *		ATOMIC_CLEAR_USHORT ();
 *	<kernel/ddi_cpu.h>
 *		ASSERT_BASE_LEVEL ();
 *		ddi_cpu_data ()
 *	<kernel/v_proc.h>
 *		plist_t
 *		PROCESS_WOKEN
 *		PROCESS_SIGNALLED
 *		MAKE_SLEEPING ()
 *		WAKE_ONE ()
 *		WAKE_ALL ()
 *		PROC_HANDLE ()
 *	<sys/debug.h>
 *		ASSERT ()
 *	<sys/types.h>
 *		pl_t
 *		uchar_t
 *		uint_t
 *	<sys/inline.h>
 *		splhi ()
 *		splx ()
 *		splcmp ()
 *	<sys/cmn_err.h>
 *		cmn_err ()
 *	<sys/kmem.h>
 *		KM_SLEEP
 *		KM_NOSLEEP
 */

#include <common/ccompat.h>
#include <kernel/ddi_cpu.h>
#include <kernel/ddi_lock.h>
#include <kernel/v_proc.h>
#include <sys/debug.h>
#include <sys/types.h>
#include <sys/inline.h>
#include <sys/cmn_err.h>
#include <sys/kmem.h>

#include <sys/ksynch.h>


/*
 * The actual content definitions of the following data structures has been
 * kept totally private to this file in order to reduce the temptation of
 * allocating or accessing the definitions in other subsystems.
 *
 * Note that for efficiency considerations it might eventually become
 * necessary to export these definitions elsewhere to allow locks to be
 * statically defined (to break dependencies) or incorporated directly as
 * members of other data structures (to reduce allocation overheads).
 * (Of course, that means we have to define ...._INIT () routines).
 *
 * For now, we'll try very very hard to resist the temptation to optimize
 * too early and violate our encapsulation.
 */

/*
 * Configuration issue: DDI/DKI sleep locks don't really map well onto any
 * old-style Unix kernel functionality, nor does the "wake one" feature of
 * DDI/DKI synchronization variables. Sleep locks and synchronization
 * variables need to interface intimately with the kernel scheduling services
 * (in ways that were usually not anticipated by the original authors of those
 * services). This requires us to confront some key aspects of the difference
 * between the old and new styles to discover why we really *need* to
 * introduce a different way of doing things.
 *
 * Some features of the old sleep ()/wakeup () model:
 *	a) These functions correspond exactly to the SV_WAIT[_SIG] () and
 *	   SV_BROADCAST () functions in the DDI/DKI, except that they do not
 *	   incorporate the basic-lock functionality of SV_WAIT[_SIG] ().
 *
 *	b) Whether sleep was interruptible or not was related to the
 *	   "priority" indication passed to SV_WAIT[_SIG] () rather than being
 *	   orthogonal issue. This need not be vital, except that the
 *	   "priority" for sleep ()/wakeup () calls was expressed in magic,
 *	   highly implementation-dependent numbers. [*]
 *
 *	c) On many systems, if sleep () was interrupted by a signal the result
 *	   was that the sleep was aborted by a non-local return. This is not
 *	   necessarily a problem *if* the caller was allowed to form a chain
 *	   of handlers to provide unwind-protection facilities. Sadly, this
 *	   is usually not the case.
 *
 *	d) The ability to awaken only *one* process at a time was not
 *	   considered an issue; firstly, because contention was normally only
 *	   associated with actions involving long delay, such as I/O, and
 *	   because processes could only be run sequentially anyway. In other
 *	   words, even if contention happened, the fact that after a wakeup ()
 *	   processes were run one at a time meant that often by the time a
 *	   process was run after a wakeup () the lock had been acquired *and*
 *	   released by any process with higher priority.
 *
 * [*] For instance, I have no idea what the three (!) different numbers that
 * have to be passed to a sleep () call in Coherent really do. There is no
 * internal documentation in the kernel source that clearly explicates the
 * roles that each number plays, nor what effect it has on the overall picture
 * of process scheduling.
 *
 * Each of features requires modification to the sleep ()/wakeup () model, as
 * outlined below:
 *	a) In a uniprocessor kernel, sleep ()/wakeup () could function without
 *	   basic locks since the caller could gain equivalent protection by
 *	   simply manipulating the interrupt priority level, which would be
 *	   reset during the course of the dispatch process after the caller
 *	   had been safely put to sleep. In a multiprocessor, an unlock call
 *	   occuring *during* a call to sleep () might occur before the
 *	   process status had been changed, thus causing the caller to never
 *	   see the unlock...
 *
 *	   This suggests the following basic model for sleep locks:
 *
 *		for (;;) {
 *			acquire_basic_lock ();
 *			if (resource_available ())
 *				break;
 *			if (MAKE_SLEEPING () == PROCESS_SLEPT) {
 *				release_basic_lock ();
 *				RUN_NEXT ();	// does not return
 *			}
 *		}
 *
 *	   where MAKE_SLEEPING () and RUN_NEXT () together form the same kind
 *	   of actions as sleep (), but broken into a pair that makes the
 *	   setjmp ()-style behaviour of rescheduling apparent and useful. In
 *	   particular, this keeps the desirable property that the low-level
 *	   scheduling system leaves the locking system in the hands of the
 *	   client code.
 *
 *	   Of course, there are various ways that this can be achieved. The
 *	   preferred style would be for MAKE_SLEEPING () to (optionally) test
 *	   for signals and to return some discriminating value. The values
 *	   returned from MAKE_SLEEPING () could be
 *
 *		PROCESS_SLEPT		slept
 *		PROCESS_WOKEN		woken normally
 *		PROCESS_SIGNALLED	woken by signal
 *
 *	   but for practical reasons this is difficult. While it is desirable
 *	   to expose at least part of the context-saving behaviour of the
 *	   inner scheduling layer, most of these facilities have the same
 *	   limitations as the setjmp () facility in that if the context which
 *	   directly called the context-save routine exits, all bets are off.
 *	   See (c) below for further discussion, and the final form will be
 *	   introduced at the end.
 *
 *	b) This can be generally dealt with as-is by introducing appropriate
 *	   magic to map the abstract priorities into numbers for passing to
 *	   MAKE_SLEEPING ().
 *
 *	c) The old style of signal handling is forbidden in the DDI/DKI
 *	   envinronment due to the locking style in use. In fact, the only
 *	   way that a driver can detect that a signal has been sent to a
 *	   process is by performing SV_WAIT_SIG () or SLEEP_LOCK_SIG () and
 *	   testing the return value.
 *
 *	   Moreover, signalling introduces one deficiency in the model
 *	   presented in a) above, namely that if a process is signalled, the
 *	   wakeup done in the signalling process knows nothing about the
 *	   locking system being used above (and in fact, if we layer on top of
 *	   old sleep ()/wakeup () code, our entire queueing system is ignored,
 *	   which would be easily fixable but for that locking problem).
 *
 *	   The simple solution would be to introduce an extra lock on part of
 *	   the process table than can ameliorate some of the problems, but
 *	   too many basic locks leads to hierarchy/ordering problems, and is
 *	   especially wasteful given that approriate row-level locking on
 *	   list headers should be completely sufficient without having to
 *	   introduce node-level locks.
 *
 *	   Essentially, we define the basic lock as being part of the list
 *	   head that sleeping processes are queued on and put a back-pointer
 *	   to the list head in the process table so that signal wakeup. This
 *	   complicates the generic sleep-lock and synchronization variable
 *	   functions a little, but we should be able to impose the semantics
 *	   we want (namely being able to guarantee deterministic ordering
 *	   on locks, which requires that the generic functions be accurately
 *	   able to determine the status of processes threaded on the sleep
 *	   queues).
 *
 *	   This definition of responsibilty for the queue head w.r.t. locking
 *	   behaviour allows us to fold the release_basic_lock () and
 *	   RUN_NEXT () behaviour into MAKE_SLEEPING (), which also helps to
 *	   solve the problem of dealing with the scope of context-save
 *	   introduced in (a). Note that we have to be careful about using the
 *	   process-table pointer to the list head since in order to find the
 *	   list head to lock it we have to read this information which is
 *	   potentially being modified as we access it.
 *
 *	   [ Note that it is possible to write a macro for MAKE_SLEEPING ()
 *	     that does the context save in the caller's scope, but since it is
 *	     possible for a routine to *not* sleep at all (due to a pending
 *	     signal), that would mean it had to take the form:
 *		FINALIZE_SLEEP (SAVE_CONTEXT (BEGIN_SLEEP (...)), ...)
 *	     in order to avoid introducing auxiliaries. ]
 *
 *	d) The assumptions on which the old-style behaviour is based are no
 *	   longer valid on multiprocessing systems. Firstly, the more con-
 *	   current nature of multiprocessor systems increases the likelihood
 *	   of contention for locks. Secondly, broadcast wakeup is likely to
 *	   cause several of the woken processes to be run simultaneously on
 *	   different processors, resulting in timelines like this (measured
 *	   from a broadcast wakeup of processes waiting for a resource):
 *
 *		CPU A : |-<acquire----------release>...............
 *		CPU n : .|--*|--*|--*|--*...........|--<acquire ---
 *
 *	   where '|' indicates selection of a process, '-' indicates CPU time
 *	   consumed, and '*' indicates a process sleeping due to resource
 *	   unavailability, and '.' represents CPU time spent on unrelated
 *	   activity. Clearly, broadcast wakeup is not an optimal way of
 *	   implementing one-at-a-time lock functionality on a multiprocessor.
 *
 *	   Of course, implementing one-at-a-time wakeup facilities is not a
 *	   trivial matter. While awakening a single waiting process is fairly
 *	   simple, the matter of selecting *which* process to awaken is a
 *	   complex one that can have considerable impact on other kernel
 *	   systems such as the virtual memory system. In addition, there is
 *	   extra information available in the DDI/DKI system whose relation
 *	   to scheduling has not been studied (such as the number of locks
 *	   held by a context). Clearly, the internal layering of the DDI/DKI
 *	   implementation should be designed to make it easy to export this
 *	   information to the low-level kernel scheduling system.
 *
 *	   Of course, the worst case is that normal kernel scheduling policy
 *	   is completely supplanted in the DDI/DKI case. There looks to be
 *	   little alternative for the multiprocessor case, but it seems to
 *	   be a good idea to design the DDI/DKI implementation to support the
 *	   old-style broadcast functionality as a fallback.
 */


/*
 * Common inital part of a lock that is used to queue locks and hold the
 * statistics information. We use this definition to simplify the process of
 * keeping lists of all the allocated locks and to permit simple generic
 * operations on such lists.
 *
 * Downcast operators should be provided for the lock structures so that the
 * mapping from the node to the lock can be done in a fully portable fashion.
 */

typedef struct lock_node lnode_t;

struct lock_node {
	lnode_t	      *	ln_next;	/* pointer to successor */
	lnode_t	      *	ln_prev;	/* pointer to predecessor */
	lkinfo_t      *	ln_lkinfo;	/* statistics information */
};


/*
 * Internal structure of a basic lock
 */

struct __basic_lock {
	lnode_t		bl_node;	/* generic information */

#ifdef	__TICKET_LOCK__
	atomic_ushort_t	bl_next_ticket;	/* next ticket number to be granted */
	atomic_ushort_t	bl_lock_holder;	/* ticket number of lock holder */
#endif

	atomic_uchar_t	bl_locked;	/* is the basic lock acquired? */

	pl_t		bl_min_pl;	/* minimum pl to be used in LOCK () */

	uchar_t		bl_hierarchy;	/* acquisition order constraint */
};


/*
 * Internal structure of a read/write lock.
 *
 * Due to the shared nature of read locks, they are naturally expressed by
 * a count of outstanding readers maintained by atomic increment and
 * decrement operations. The exclusive (write) mode of such locks are best
 * expressed via the "ticket" mechanism if FIFO ordering is desired and/or
 * contention is to be minimized.
 *
 * The following implementation consists of a basic (exclusive) lock simply
 * augmented by a count; the use of a "ticket gate" test-and-set lock to
 * control access to the lock internal data reduces dependency on atomic
 * facilities such as fetch_and_increment and compare_and_swap that are not
 * widely available. If such facilities are available, consider using the
 * lock algorithm outlined in the paper referenced in the file header comment
 * above.
 */

struct readwrite_lock {
	lnode_t		rw_node;	/* generic information */

	atomic_ushort_t	rw_next_ticket;	/* next ticket number to be granted */
	atomic_ushort_t	rw_lock_holder;	/* ticket number of lock holder */

	atomic_ushort_t	rw_readers;

	atomic_uchar_t	rw_locked;

	pl_t		rw_min_pl;	/* min pl to acquire the lock with */

	uchar_t		rw_hierarchy;	/* acquisition order constraint */
};


/*
 * Internal structure of a synchronisation variable
 */

struct synch_var {
	lnode_t		sv_node;	/* generic information */

	plist_t		sv_plist [1];	/* list of blocked processes */
};


/*
 * Internal structure of a sleep lock.
 *
 * Note that the "sl_holder" member exists purely to support the debugging
 * SLEEP_LOCKOWNED () function, nothing else. What the System V documentation
 * fails to discuss is that sleep locks don't have to be owned by *any*
 * process. Sleep locks can be acquired at interrupt level via
 * SLEEP_TRYLOCK (), and released by interrupts as well. There doesn't appear
 * to be any mechanism to prevent locks being passed between processes by
 * various methods, and so on and so forth.
 *
 * Therefore, the flag members are the only members actually used by the
 * implementations of the sleep-lock functions, and the "sl_holder" member
 * may not be correct if the lock was acquired by an interrupt context.
 *
 * As discussed elsewhere, the main focus of activity for sleep locks is the
 * list of waiting processes, which contains a test-and-set lock that should
 * be used to control most aspects of lock state.
 */

struct sleep_lock {
	lnode_t		sl_node;	/* generic information */

	atomic_uchar_t	sl_locked;	/* is sleep lock held? */

	plist_t		sl_plist [1];	/* process list */

	_VOID	      *	sl_holder;	/* ID of process holding lock */
};


/*
 * Downcasting operators. Hopefully we won't need these.
 */

#define	lnode_to_basic(n)	((lock_t *) ((char *) (n) - \
					     offsetof (lock_t, bl_node)))

#define	lnode_to_rw(n)		((rwlock_t *) ((char *) (n) - \
					       offsetof (rwlock_t, rw_node)))

#define	lnode_to_sv(n)		((sv_t *) ((char *) (n) - \
					   offsetof (sv_t, sv_node)))

#define	lnode_to_sleep(n)	((sleep_t *) ((char *) (n) - \
					      offsetof (sleep_t, sl_node)))


/*
 * For debugging purposes, we want to keep lists of all the allocated locks.
 *
 * In the absence of atomic compare-and-swap operations, it's hard to define
 * good list-manipulation code, so we'll protect our list operations with
 * simple test-and-set locks. As usual on a uniprocessor these are still
 * useful for detecting potential inconsistency.
 *
 * There are no init or destroy methods/macros for this structure since there
 * are only the following static instances defined.
 */

__LOCAL__ struct lock_list {
	__CONST__ char * ll_name;	/* name of list */

	atomic_uchar_t	ll_locked;	/* single-thread list operations */
	lnode_t	      *	ll_head;	/* head of list */
} basic_locks = { "basic lock" },
  rw_locks = { "read/write lock" },
  synch_vars = { "synchronisation variable" },
  sleep_locks = { "sleep lock" };

#define	LOCKLIST_LOCK(l,n)	(TEST_AND_SET_LOCK ((l)->ll_locked, plhi, n))
#define	LOCKLIST_UNLOCK(l,p)	(ATOMIC_CLEAR_UCHAR ((l)->ll_locked),\
				 (void) splx (p))

/*
 * We must abstract the linkage between locks and memory allocation; the lock
 * system requires memory allocation services which require lock services...
 * certain definitions below (and presumably similar definitions in the memory
 * management system) can be used to resolve this dependency. In addition,
 * the abstraction provided can be used to decouple the provided systems (and
 * this aids portability) or to increase their coupling (to increase
 * performance).
 *
 * The definitions (which may be macros):
 *	_lock_malloc ()
 *	_lock_free ()
 * define an internal interface to the memory management system. If the memory
 * allocation system uses LOCK_ALLOC () to allocate the locks used to
 * coordinate access to the memory pool(s), the functions above will work as
 * expected even during the memory system's call to LOCK_ALLOC (), and will
 * coordinate access as necessary with other memory allocation interfaces
 * thereafter.
 */

__EXTERN_C_BEGIN__

_VOID	      *	_lock_malloc	__PROTO ((size_t size, int flag));
void		_lock_free 	__PROTO ((_VOID * mem, size_t size));

__EXTERN_C_END__


/*
 * For now, we just map these functions onto the kmem_ interface and deal
 * with startup issues there.
 */

#define	_lock_malloc(s,f)	kmem_alloc (s, f)
#define	_lock_free(s,f)		kmem_free (s, f)


/*
 * This file-local function encapsulates the hierarchy-recording function that
 * deals with the bookkeeping necessary to ensure that locks are acquired
 * strictly in the order defined (to avoid deadlock).
 *
 * Due to the way in which the DDI/DKI locking functions are defined, it might
 * be possible on some architectures for a basic or read/write lock acquired
 * on one CPU to eventually be released on another.
 *
 * In a multi-processor system where this might be possible, it might be more
 * sensible to record the hierarchy information in some per-priority-level
 * fashion (as the DDI/DKI alludes to). Since this is currently not
 * necessary, we'll elect to design that ability later.
 */

#if	__USE_PROTO__
__LOCAL__ void (LOCK_COUNT_HIERARCHY) (__lkhier_t hierarchy)
#else
__LOCAL__ void
LOCK_COUNT_HIERARCHY __ARGS ((hierarchy))
__lkhier_t	hierarchy;
#endif
{
	dcdata_t      *	dcdata = ddi_cpu_data ();
	pl_t		prev_pl = splhi ();

	/*
	 * We'll skip the usual paranoid ASSERT () tests since this is purely
	 * a local function.
	 */

	dcdata->dc_hierarchy_cnt [hierarchy - __MIN_HIERARCHY__] ++;

	if (dcdata->dc_max_hierarchy < hierarchy)
		dcdata->dc_max_hierarchy = hierarchy;

	(void) splx (prev_pl);
}


/*
 * This file-local function is the dual to the above, for adjusting the
 * hierarchy information for a lock release.
 */

#define	ARRAY_MAX(array)	(sizeof (array) / sizeof ((array) [0])

#if	__USE_PROTO__
__LOCAL__ void (LOCK_FREE_HIERARCHY) (__lkhier_t hierarchy)
#else
__LOCAL__ void
LOCK_FREE_HIERARCHY __ARGS ((hierarchy))
__lkhier_t	hierarchy;
#endif
{
	dcdata_t      *	dcdata = ddi_cpu_data ();
	pl_t		prev_pl = splhi ();

	/*
	 * We'll skip the usual paranoid ASSERT () tests since this is purely
	 * a local function.
	 */

	dcdata->dc_hierarchy_cnt [hierarchy - __MIN_HIERARCHY__] --;

	/*
	 * Work out the lowest occupied priority level.
	 */

	do {
		if (dcdata->dc_hierarchy_cnt [dcdata->dc_max_hierarchy -
					      __MIN_HIERARCHY__] > 0)
			break;
	} while (-- dcdata->dc_max_hierarchy >= __MIN_HIERARCHY__);

	(void) splx (prev_pl);
}


#if 0
/*
 * This file-local function takes care of recording debugging information and
 * statistics relating to lock acquisition.
 */

#if	__USE_PROTO__
__LOCAL__ void (TRACE_BASIC_LOCK) (lock_t * lockp)
#else
__LOCAL__ void
TRACE_BASIC_LOCK __ARGS ((lockp))
lock_t	      *	lockp;
#endif
{
	/*
	 * We'll skip the usual paranoid ASSERT () tests since this is purely
	 * a local function.
	 */
}



/*
 * This file-local function takes care of undoing any data-structure changes
 * made by the above and recording additional statistics when a lock is
 * released
 */

#if	__USE_PROTO__
__LOCAL__ void (UNTRACE_BASIC_LOCK) (lock_t * lockp)
#else
__LOCAL__ void
UNTRACE_BASIC_LOCK __ARGS ((lockp))
lock_t	      *	lockp;
#endif
{
	/*
	 * We'll skip the usual paranoid ASSERT () tests since this is purely
	 * a local function.
	 */
}


/*
 * This file-local function takes care of recording debugging information and
 * statistics relating to lock acquisition.
 */

#if	__USE_PROTO__
__LOCAL__ void (TRACE_RW_LOCK) (rwlock_t * lockp)
#else
__LOCAL__ void
TRACE_RW_LOCK __ARGS ((lockp))
rwlock_t      *	lockp;
#endif
{
	/*
	 * We'll skip the usual paranoid ASSERT () tests since this is purely
	 * a local function.
	 */
}



/*
 * This file-local function takes care of undoing any data-structure changes
 * made by the above and recording additional statistics when a lock is
 * released.
 */

#if	__USE_PROTO__
__LOCAL__ void (UNTRACE_RW_LOCK) (rwlock_t * lockp)
#else
__LOCAL__ void
UNTRACE_RW_LOCK __ARGS ((lockp))
rwlock_t      *	lockp;
#endif
{
	/*
	 * We'll skip the usual paranoid ASSERT () tests since this is purely
	 * a local function.
	 */
}

#else

/*
 * Since all the above functions have empty bodies, we'll define empty macros
 * to get around the warnings.
 */

#define	TRACE_BASIC_LOCK(l)
#define	UNTRACE_BASIC_LOCK(l)

#define	TRACE_RW_LOCK(l)
#define	UNTRACE_RW_LOCK(l)

#endif



/*
 * Simple common function to acquire a simple test-and-set lock.
 *
 * TEST_AND_SET_LOCK () uses the splraise () private function defined in
 * <sys/inline.h> to raise the processor priority level to "pl". This ensures
 * maximum safety in the use of this function, and certain DDI/DKI facilities
 * such as freezestr () require this behaviour.
 */

#if	__USE_PROTO__
pl_t (TEST_AND_SET_LOCK) (__atomic_uchar_t locked, pl_t pl,
			  __CONST__ char * name)
#else
pl_t
TEST_AND_SET_LOCK __ARGS ((locked, pl, name))
__atomic_uchar_t	locked;
pl_t			pl;
__CONST__ char	      * name;
#endif
{
	for (;;) {
		pl_t		prev_pl = splraise (pl);

		if (ATOMIC_TEST_AND_SET_UCHAR (locked) == 0)
			return prev_pl;		/* all OK */


		/*
		 * While we spin for the lock, we can allow interrupts that
		 * were permissible at entry to this routine.
		 */

		(void) splx (prev_pl);

#ifdef	__UNIPROCESSOR__
		cmn_err (CE_PANIC, "%s : test-and-set deadlock", name);
#elif	defined (__TEST_AND_TEST__)
		/*
		 * To reduce contention, we wait until the test-and-set lock
		 * is free before attempting to re-acquire it. Of course, more
		 * sophisticated backoff schemes might also help, even for
		 * this approach.
		 */

		while (ATOMIC_FETCH_UCHAR (locked) != 0)
			/* DO NOTHING */;
#endif
	}
}



/*
 * File-local function to initialise the generic part of a lock; this normally
 * enqueues the lock on a list. This function also has the responsibility of
 * allocating any needed statistics buffers, which requires that it be passed
 * the flag which indicates whether it is allowed to block or not.
 */

#if	__USE_PROTO__
__LOCAL__ void (INIT_LNODE) (lnode_t * lnode, lkinfo_t * lkinfop,
			     struct lock_list * list, int __NOTUSED (flag))
#else
__LOCAL__ void
INIT_LNODE __ARGS ((lnode, lkinfop, list, flag))
lnode_t	      *	lnode;
lkinfo_t      *	lkinfop;
struct lock_list
	      *	list;
int		flag;
#endif
{
	pl_t		prev_pl;

	ASSERT (lnode != NULL && list != NULL);

	/*
	 * Here we allocate and initialise any needed statistics information,
	 * currently nothing.
	 */

	lnode->ln_lkinfo = lkinfop;
	lnode->ln_prev = NULL;


	/*
	 * Lock the list, enqueue the node, and unlock the list.
	 */

	prev_pl = LOCKLIST_LOCK (list, list->ll_name);

	if ((lnode->ln_next = list->ll_head) != NULL)
		lnode->ln_next->ln_prev = lnode;
	list->ll_head = lnode;

	LOCKLIST_UNLOCK (list, prev_pl);
}



/*
 * The dual to the above, this file-local function dequeues the node from the
 * given list of locks and frees any statistics buffer memory.
 */

#if	__USE_PROTO__
__LOCAL__ void (FREE_LNODE) (lnode_t * lnode, struct lock_list * list)
#else
__LOCAL__ void
FREE_LNODE __ARGS ((lnode, list))
lnode_t	      *	lnode;
struct lock_list
	      *	list;
#endif
{
	pl_t		prev_pl;

	/*
	 * We'll skip the paranoid assertions on this one, since the function
	 * is file-local.
	 *
	 * Lock the list, dequeue the node, unlock the list. After that we
	 * can free any statistics buffers.
	 */

	prev_pl = LOCKLIST_LOCK (list, list->ll_name);

	if (lnode->ln_prev == NULL)
		list->ll_head = lnode->ln_next;
	else
		lnode->ln_prev->ln_next = lnode->ln_next;

	if (lnode->ln_next != NULL)
		lnode->ln_next->ln_prev = lnode->ln_prev;

	LOCKLIST_UNLOCK (list, prev_pl);
}


/*
 *-STATUS:
 *	DDI/DKI
 *
 *-NAME:
 *	LOCK_ALLOC ()	Allocate a basic lock
 *
 *-SYNOPSIS:
 *	#include <sys/types.h>
 *	#include <sys/kmem.h>
 *	#include <sys/ksynch.h>
 *
 *	lock_t * LOCK_ALLOC (uchar_t hierarchy, pl_t min_pl,
 *			     lkinfo_t * lkinfop, int flag);
 *
 *-ARGUMENTS:
 *	hierarchy	Hierarchy value which asserts the order in which this
 *			lock will be acquired relative to other basic and
 *			read/write locks. "hierarchy" must be within the range
 *			of 1 through 32 inclusive and must be chosen such that
 *			locks are normally acquired in order of increasing
 *			"hierarchy" number. In other words, when acquiring a
 *			basic lock using any function other than TRYLOCK (),
 *			the lock being acquired must have a "hierarchy" value
 *			that is strictly greater than the "hierarchy" values
 *			associated with all locks currently held by the
 *			calling context.
 *
 *			Implementations of lock testing may differ in whether
 *			they assume a separate range of "hierarchy" values for
 *			each interrupt priority level or a single range that
 *			spans all interrupt priority levels. In order to be
 *			portable across different implementations, drivers
 *			which may acquire locks at more than one interrupt
 *			priority level should define the "hierarchy" among
 *			those locks such that the "hierarchy" is strictly
 *			increasing with increasing priority level (eg. if M
 *			is the maximum "hierarchy" value defined for any lock
 *			that may be acquired at priority level N, then M + 1
 *			should be the minimum hierarchy value for any lock
 *			that may be acquired at any priority level greater
 *			than N).
 *
 *	min_pl		Minimum priority level argument which asserts the
 *			minimum priority leel that will be passed in with any
 *			attempt to acquire this lock [see LOCK ()].
 *			Implementations which do not require that the
 *			interrupt priority level be raised during lock
 *			acquisition may choose not to enforce the "min_pl"
 *			assertion. The valid values for this argument are as
 *			follows:
 *
 *			  plbase	Block no interrupts
 *			  pltimeout	Block functions scheduled by itimeout
 *					and dtimeout
 *			  pldisk	Block disk device interrupts
 *			  plstr		Block STREAMS interrupts
 *			  plhi		Block all interrupts
 *
 *			The notion of a "min_pl" assumes a defined order of
 *			priority levels. The following partial order is
 *			defined:
 *			  plbase < pltimeout <= pldisk, plstr <= plhi
 *
 *			The ordering of pldisk and plstr relative to each
 *			other is not defined.
 *
 *			Setting a given priority level will block interrupts
 *			associated with that level as well as all levels that
 *			are defined to be less than or equal to the specified
 *			level. In order to be portable a driver should not
 *			acquire locks at different priority levels where the
 *			relative order of those priority levels is not defined
 *			above.
 *
 *			The "min_pl" argument should specify a priority level
 *			that would be sufficient to block out any interrupt
 *			handler that might attempt to acquire this lock. In
 *			addition, potential deadlock problems involving
 *			multiple locks should be considered when defining the
 *			"min_pl" value. For example, if the normal order of
 *			acquisition of locks A and B (as defined by the lock
 *			hierarchy) is to acquire A first and then B, lock B
 *			should never be acquired at a priority level less
 *			than the "min_pl" for lock A. Therefore, the "min_pl"
 *			for lock B should be greater than or equal to the
 *			"min_pl" for lock A.
 *
 *			Note that the specification of a "min_pl" with a
 *			LOCK_ALLOC () call does not actually cause any
 *			interrupts to be blocked upon lock acquisition, it
 *			simply asserts that subsequent LOCK () calls to
 *			acquire this lock will pass in a priority level at
 *			least as great as "min_pl".
 *
 *	lkinfop		Pointer to a lkinfo structure. The "lk_name" member of
 *			the "lkinfo" structure points to a character string
 *			defining a name that will be associated with the lock
 *			for the purposes of statistics gathering. The name
 *			should begin with the driver prefix and should be
 *			unique to the lock or group of locks for which the
 *			driver wishes to collect a uniquely identifiable set
 *			of statistics (ie, if a given name is shared by a
 *			group of locks, the statistics of the individual locks
 *			within the group will not be uniquely identifiable).
 *			There are no flags within the lk_flags member of the
 *			lkinfo structure defined for use with LOCK_ALLOC ().
 *
 *			A given lkinfo structure may be shared among multiple
 *			basic locks and read/write locks but a lkinfo
 *			structure may not be shared between a basic lock and
 *			a sleep lock. The called must ensure that the lk_flags
 *			and lk_pad members of the lkinfo structure are zeroed
 *			out before passing it to LOCK_ALLOC ().
 *
 *	flag		Specifies whether the caller is willing to sleep
 *			waiting for memory. If "flag" is set to KM_SLEEP, the
 *			caller will sleep if necessary until sufficient memory
 *			is available. If "flag" is set to KM_NOSLEEP, the
 *			caller will not sleep, but LOCK_ALLOC () will return
 *			NULL if sufficient memory is not immediately
 *			available.
 *
 *-DESCRIPTION:
 *	LOCK_ALLOC () dynamically allocates and initialises an instance of a
 *	basic lock. The lock is initialised to the unlocked state.
 *
 *-RETURN VALUE:
 *	Upon successful completion, LOCK_ALLOC () returns a pointer to the
 *	newly allocated lock. If KM_NOSLEEP is specified and sufficient
 *	memory is not immediately available, LOCK_ALLOC () returns a NULL
 *	pointer.
 *
 *-LEVEL:
 *	Base only if "flag" is set to KM_SLEEP. Base or Interrupt if "flag" is
 *	set to KM_NOSLEEP.
 *
 *-NOTES:
 *	May sleep if "flag" is set to KM_NOSLEEP.
 *
 *	Driver-defined basic locks and read/write locks may be held across
 *	calls to this function if "flag" is set to KM_NOSLEEP but may not be
 *	held if "flag" is KM_SLEEP.
 *
 *	Driver-defined sleep locks may be held across calls to this function
 *	regardless of the value of "flag".
 *
 *-SEE ALSO:
 *	LOCK (), LOCK_DEALLOC (), TRYLOCK (), UNLOCK (), lkinfo
 */

#define	ASSERT_HIERARCHY_OK(h,lk) \
		ASSERT ((lk) != NULL), \
		ASSERT ((h) >= __MIN_HIERARCHY__ && \
			(h) <= (((lk)->lk_flags & INTERNAL_LOCK) != 0 ? \
				  __MAX_HIERARCHY__ : __MAX_DDI_HIERARCHY__))

#if	__USE_PROTO__
lock_t * (LOCK_ALLOC) (__lkhier_t hierarchy, pl_t min_pl, lkinfo_t * lkinfop,
		       int flag)
#else
lock_t *
LOCK_ALLOC __ARGS ((hierarchy, min_pl, lkinfop, flag))
__lkhier_t	hierarchy;
pl_t		min_pl;
lkinfo_t      *	lkinfop;
int		flag;
#endif
{
	lock_t	      *	lockp;

	ASSERT_HIERARCHY_OK (hierarchy, lkinfop);
	ASSERT (splcmp (plbase, min_pl) <= 0 && splcmp (min_pl, plhi) <= 0);
	ASSERT (flag == KM_SLEEP || flag == KM_NOSLEEP);

	/*
	 * Allocate and initialise the data, possibly waiting for enough
	 * memory to become available.
	 */

	if ((lockp = (lock_t *) _lock_malloc (sizeof (* lockp),
					      flag)) != NULL) {
		INIT_LNODE (& lockp->bl_node, lkinfop, & basic_locks, flag);

		lockp->bl_min_pl = min_pl;
		lockp->bl_hierarchy = hierarchy;

		ATOMIC_CLEAR_UCHAR (lockp->bl_locked);

#ifdef	__TICKET_LOCK__
		ATOMIC_CLEAR_USHORT (lockp->bl_next_ticket);
		ATOMIC_CLEAR_USHORT (lockp->bl_lock_holder);
#endif
	}

	return lockp;
}


/*
 *-STATUS:
 *	DDI/DKI
 *
 *-NAME:
 *	LOCK_DEALLOC ()	Deallocate an instance of a basic lock.
 *
 *-SYNOPSIS:
 *	#include <sys/ksynch.h>
 *
 *	void LOCK_DEALLOC (lock_t * lockp);
 *
 *-ARGUMENTS:
 *	lockp		Pointer to the basic lock to be deallocated.
 *
 *-DESCRIPTION:
 *	LOCK_DEALLOC () deallocates the basic lock specified by "lockp".
 *
 *-RETURN VALUE:
 *	None.
 *
 *-LEVEL:
 *	Base or Interrupt.
 *
 *-NOTES:
 *	Does not sleep.
 *
 *	Attempting to deallocate a lock that is currently locked or is being
 *	waited for is an error and will result in undefined behavior.
 *
 *	Driver-defined basic locks (other than the one being deallocated),
 *	read/write locks and sleep locks may be held across calls to this
 *	function.
 *
 *-SEE ALSO:
 *	LOCK (), LOCK_ALLOC (), TRYLOCK (), UNLOCK ()
 */

#if	__USE_PROTO__
void (LOCK_DEALLOC) (lock_t * lockp)
#else
void
LOCK_DEALLOC __ARGS ((lockp))
lock_t	      *	lockp;
#endif
{
	ASSERT (lockp != NULL);
	ASSERT (ATOMIC_FETCH_UCHAR (lockp->bl_locked) == 0);

#ifdef	__TICKET_LOCK__
	ASSERT (ATOMIC_FETCH_USHORT (lockp->bl_next_ticket) ==
		ATOMIC_FETCH_USHORT (lockp->bl_lock_holder));
#endif

	/*
	 * Remove from the list of all basic locks and free any statistics
	 * buffer space before freeing the lock itself.
	 */

	FREE_LNODE (& lockp->bl_node, & basic_locks);

	_lock_free (lockp, sizeof (* lockp));
}


/*
 *-STATUS:
 *	DDI/DKI
 *
 *-NAME:
 *	LOCK ()		Acquire a basic lock
 *
 *-SYNOPSIS:
 *	#include <sys/types.h>
 *	#include <sys/ksynch.h>
 *
 *	pl_t LOCK (lock_t * lockp, pl_t pl);
 *
 *-ARGUMENTS:
 *	lockp		Pointer to the basic lock to be acquired.
 *
 *	pl		The interrupt priority level to be set while the lock
 *			is held by the caller. Because some implementations
 *			require that interrupts that might attempt to acquire
 *			the lock be blocked on which the lock is held,
 *			portable drivers must specify a "pl" value that is
 *			sufficient to block out any interrupt handler that
 *			might attempt to acquire this lock. See the
 *			description of the "min_pl" argument to LOCK_ALLOC ()
 *			for additional discussion. Implementations that do
 *			not require that the interrupt priority level be
 *			raised during lock acquisition may choose to ignore
 *			this argument.
 *
 *-DESCRIPTION:
 * 	LOCK () sets the interrupt priority level in accordance with the
 *	value specified by "pl" (if required by the implementation) and
 * 	acquires the lock specified by "lockp". If the lock is not currently
 * 	available, the caller will wait until the lock is available. It is
 *	implementation-defined whether the caller will block during the wait.
 *	Some implementations may cause the caller to spin for the duration of
 *	the wait, while on others the caller may block at some point.
 *
 *-RETURN VALUE:
 *	Upon acquiring the lock, LOCK () returns the previous interrupt
 *	priority level.
 *
 *-LEVEL:
 *	Base or Interrupt.
 *
 *-NOTES:
 * 	Basic locks are not recursive. A call to LOCK () attempting to
 *	acquire a lock that is already held by the calling context will
 *	result in deadlock.
 *
 * 	Calls to LOCK () should honor the ordering defined by the lock
 *	hierarchy [see LOCK_ALLOC ()] in order to avoid deadlock.
 *
 * 	Driver-defined sleep locks may be held across calls to this function.
 *
 * 	Driver-defined basic locks and read/write locks may be held across
 *	calls to this function subject to the hierarchy and recursion
 *	restrictions described above.
 *
 * 	When called from interrupt level, the "pl" argument must not specify
 *	a priority below the level at which the interrupt handler is running.
 *
 *-SEE ALSO:
 *	LOCK_ALLOC (), LOCK_DEALLOC (), TRYLOCK (), UNLOCK ()
 */

#if	__USE_PROTO__
pl_t (LOCK) (lock_t * lockp, pl_t pl)
#else
pl_t
LOCK __ARGS ((lockp, pl))
lock_t	      *	lockp;
pl_t		pl;
#endif
{
	pl_t		prev_pl;
#ifdef	__TICKET_LOCK__
	ushort_t	ticket_no;
#endif

	ASSERT (lockp != NULL);
	ASSERT (splcmp (pl, plhi) <= 0);


	/*
	 * Enforce minimum-priority assertion, pl >= lockp->bl_min_pl. Note
	 * that splcmp () abstracts subtraction-for-comparison of priority
	 * levels, which explains the form of the assertion.
	 */

	ASSERT (splcmp (pl, lockp->bl_min_pl) >= 0);


	/*
	 * On a uniprocessor, encountering a basic lock that is already
	 * locked is *always* an error, even on machines with many different
	 * interrupt priority levels. The hierarchy-assertion mechanism
	 * cannot always deal with this, since TRYLOCK () can acquire locks
	 * in a different order.
	 *
	 * On a multiprocessor, we can just spin here. Note that since LOCK ()
	 * is defined as requiring values of "pl" greater than the current
	 * interrupt priority level, the use of splraise () by
	 * TEST_AND_SET_LOCK () is legitimate.
	 */

	prev_pl = TEST_AND_SET_LOCK (lockp->bl_locked, pl, "LOCK");

#ifdef	__TICKET_LOCK__
	/*
	 * If we are working with a ticket-lock scheme, we now take a ticket
	 * and release the test-and-set lock. After that, we can just wait for
	 * our number to come up...
	 */

	ticket_no = ATOMIC_FETCH_USHORT (lockp->bl_next_ticket);

	if (ATOMIC_FETCH_AND_STORE_USHORT (lockp->bl_next_ticket,
					   ticket_no + 1) != ticket_no) {
		/*
		 * If we didn't read the same number twice, then the basic
		 * lock protection isn't doing its job.
		 */

		cmn_err (CE_PANIC, "LOCK : Ticket-lock gate failure");
	}

	ATOMIC_CLEAR_UCHAR (lockp->bl_locked);

	while (ATOMIC_FETCH_USHORT (lockp->bl_lock_holder) != ticket_no) {
		/*
		 * At this point we might want to implement some form of
		 * proportional backoff to reduce memory traffic. Later.
		 *
		 * Another hot contender for this spot is detecting failures
		 * on other CPUs...
		 */

#ifdef	__UNIPROCESSOR__
		cmn_err (CE_PANIC, "LOCK : deadlock on ticket");
#endif
	}

#endif	/* defined (__TICKET_LOCK__) */


	/*
	 * Now would be an appropriate time to check that the requested level
	 * is >= the current level on entry. The architectures that we are
	 * likely to target require this to be true, although some novel
	 * schemes which reduce interrupt latency do not.
	 *
	 * Since the TEST_AND_SET_LOCK () function which set our priority
	 * level uses splraise (), we are safe making this test at this late
	 * stage.
	 */

	ASSERT (splcmp (pl, prev_pl) >= 0);


	/*
	 * Test the lock-acquisition-hierarchy assertions.
	 */

	ASSERT (ddi_cpu_data ()->dc_max_hierarchy < lockp->bl_hierarchy);

	LOCK_COUNT_HIERARCHY (lockp->bl_hierarchy);


	/*
	 * Since it may be useful for post-mortem debugging to record some
	 * information about the context which acquired the lock, we defer
	 * to some generic recorder function or macro.
	 *
	 * Note that in general it does not appear to be possible to make
	 * detailed assertions about the relation between the contexts in
	 * which a lock is acquired and/or released. In particular, it
	 * might be possible for a basic lock to be tied to some device
	 * hardware-related operation where a lock might be acquired on one
	 * CPU and released on another.
	 *
	 * In addition, any lock statistics are kept by this operation.
	 */

	TRACE_BASIC_LOCK (lockp);

	return prev_pl;
}


/*
 *-STATUS:
 *	DDI/DKI
 *
 *-NAME:
 *	TRYLOCK ()	Try to acquire a basic lock
 *
 *-SYNOPSIS:
 *	#include <sys/types.h>
 *	#include <sys/ksynch.h>
 *
 *	pl_t TRYLOCK (lock_t * lockp, pl_t pl);
 *
 *-ARGUMENTS:
 *	lockp		Pointer to the basic lock to be acquired.
 *
 *	pl		The interrupt priority level to be set while the lock
 *			is held by the caller. Because some implementations
 *			require that interrupts that might attempt to acquire
 *			the lock be blocked on the processor on which the
 *			lock is held, portable drivers must specify a "pl"
 *			value that is sufficient to block out and interrupt
 *			handler that might attempt to acquire this lock. See
 *			the description of LOCK_ALLOC () for additional
 *			discussion and a list of the valid values for "pl".
 *			Implementations that do not require that the interrupt
 *			priority level be raised during lock acquisition may
 *			choose to ignore this argument.
 *
 *-DESCRIPTION:
 *	If the lock specified by "lockp" is immediately available (can be
 *	acquired without waiting) TRYLOCK () sets the interrupt priority level
 *	in accordance with the value specified by "pl" (if required by the
 *	implementation) and acquires the lock. If the lock is not immediately
 *	available, the function returns without acquiring the lock.
 *
 *-RETURN VALUE:
 *	If the lock is acquired, TRYLOCK () returns the previous interrupt
 *	priority level ("plbase" - "plhi"). If the lock is not acquired the
 *	value "invpl" is returned.
 *
 *-LEVEL:
 *	Base or Interrupt.
 *
 *-NOTES:
 *	Does not sleep.
 *
 *	TRYLOCK () may be used to acquire a lock in a different order from
 *	the order defined by the lock hierarchy.
 *
 *	Driver-defined basic locks, read/write locks, and sleep locks may be
 *	held across calls to this function.
 *
 *	When called from interrupt level, the "pl" argument must not specify
 *	a priority level below the level at which the interrupt handler is
 *	running.
 *
 *-SEE ALSO:
 *	LOCK (), LOCK_ALLOC (), LOCK_DEALLOC (), UNLOCK ()
 */

#if	__USE_PROTO__
pl_t (TRYLOCK) (lock_t * lockp, pl_t pl)
#else
pl_t
TRYLOCK __ARGS ((lockp, pl))
lock_t	      *	lockp;
pl_t		pl;
#endif
{
	pl_t		prev_pl;
#ifdef	__TICKET_LOCK__
	ushort_t	ticket_no;
#endif

	ASSERT (lockp != NULL);
	ASSERT (splcmp (pl, plhi) <= 0);


	/*
	 * Enforce minimum-priority assertion, pl >= lockp->bl_min_pl. Note
	 * that splcmp () abstracts subtraction-for-comparison of priority
	 * levels, which explains the form of the assertion.
	 */

	ASSERT (splcmp (pl, lockp->bl_min_pl) >= 0);


#ifdef	__TICKET_LOCK__
	/*
	 * If this is a ticket-lock, we test the ticket numbers to see
	 * whether there is any reason to even try acquiring the test-and-set
	 * lock that forms the ticket gate.
	 *
	 * We read the "lock holder" and "next ticket" entries in that
	 * sequence to be pessimistic, since we can assume that other CPUs
	 * might be looking at this...
	 */

	ticket_no = ATOMIC_FETCH_USHORT (lockp->bl_lock_holder);

	if (ticket_no != ATOMIC_FETCH_USHORT (lockp->bl_next_ticket))
		return invpl;
#endif


	/*
	 * We block out interrupts at this point to allow the following
	 * operations room to do their stuff in, since interrupts at an
	 * inappropriate moment can cause deadlock... of course, if the
	 * definition of the lock is such that interrupts can proceed, then
	 * that's OK, since the lock acquisition is atomic.
	 */

	prev_pl = splx (pl);


	/*
	 * Now would be an appropriate time to check that the
	 * requested level is >= the current level on entry. Strictly
	 * speaking, this does not have to be true if the processor is
	 * currently at base level, but for now we'll discourage that
	 * behaviour.
	 */

	ASSERT (splcmp (pl, prev_pl) >= 0);


	/*
	 * Test to see whether the lock in question is already taken, and if
	 * not, we take it. We don't spin if this is a ticket lock, since if
	 * the basic lock is taken there is *no way* that the lock will be
	 * free for us immediately.
	 */

	if (ATOMIC_TEST_AND_SET_UCHAR (lockp->bl_locked) == 0) {
#ifdef	__TICKET_LOCK__
		/*
		 * If this is a ticket lock, now would be a good time to
		 * check the ticket numbers to ensure that the lock *really*
		 * is free.
		 */

		ticket_no = ATOMIC_FETCH_USHORT (lockp->bl_next_ticket);

		if (ticket_no != ATOMIC_FETCH_USHORT (lockp->bl_lock_holder)) {

			ATOMIC_CLEAR_UCHAR (lockp->bl_locked);
			goto try_failed;
		}

		if (ATOMIC_FETCH_AND_STORE_USHORT (lockp->bl_next_ticket,
						   ticket_no + 1) != ticket_no) {
			/*
			 * If we didn't read the same number twice, then the
			 * test-and-set lock protection isn't doing its job.
			 */

			cmn_err (CE_PANIC, "TRYLOCK : Ticket-lock gate failure");
		}


		/*
		 * Now we have the lock, we can release the ticket-gate lock.
		 */

		ATOMIC_CLEAR_UCHAR (lockp->bl_locked);

#endif	/* defined (__TICKET_LOCK__) */


		/*
		 * TRYLOCK () bypasses the hierarchy-assertion mechanism,
		 * although we record the maximum acquired hierarchy level for
		 * maximum strictness checking in inner LOCK () attempts.
		 *
		 * We also record debugging and statistics information here
		 * with TRACE_BASIC_LOCK ().
		 */

		LOCK_COUNT_HIERARCHY (lockp->bl_hierarchy);

		TRACE_BASIC_LOCK (lockp);

		return prev_pl;
	}

try_failed:
	/*
	 * We cannot acquire the lock, so reset the priority and exit with
	 * the flag value.
	 */

	(void) splx (prev_pl);

	return invpl;
}


/*
 *-STATUS:
 *	DDI/DKI
 *
 *-NAME:
 *	UNLOCK ()	Release a basic lock.
 *
 *-SYNOPSIS:
 *	#include <sys/types.h>
 *	#include <sys/ksynch.h>
 *
 *	void UNLOCK (lock_t * lockp, pl_t pl);
 *
 *-ARGUMENTS:
 *	lockp		Pointer to the basic lock to be released.
 *
 *	pl		The interrupt priority level to be set after releasing
 *			the lock. See the description of the "min_pl" argument
 *			to LOCK_ALLOC () for a list of the valid values for
 *			"pl". If lock calls are not being nested or if the
 *			caller is unlocking in the reverse order that locks
 *			were acquired, the "pl" argument will typically be the
 *			value that was returned from the corresponding call to
 *			acquire the lock. The caller may need to specify a
 *			different value for "pl" if nested locks are being
 *			released in some order other that the reverse order of
 *			acquisition, so as to ensure that the interrupt
 *			priority level is kept sufficiently high to block
 *			interrupt code that might attempt to acquire locks
 *			which are still held. Although portable drivers must
 *			always specify an appropriate "pl" argument,
 *			implementations which do not require that the
 * 			interrupt priority level be raised during lock
 *			acquisition may choose to ignore this argument.
 *
 *-DESCRIPTION:
 *	UNLOCK () releases the basic lock specified by "lockp" and then sets
 *	the interrupt priority level in accordance with the value specified by
 *	"pl" (if required by the implementation).
 *
 *-RETURN VALUE:
 *	None.
 *
 *-LEVEL:
 *	Base or Interrupt.
 *
 *-NOTES:
 *	Does not sleep.
 *
 *	Driver-defined basic locks, read/write locks, and sleep locks may be
 *	held across calls to this function.
 *
 *-SEE ALSO:
 *	LOCK (), LOCK_ALLOC (), LOCK_DEALLOC (), TRYLOCK ()
 */

#if	__USE_PROTO__
void (UNLOCK) (lock_t * lockp, pl_t pl)
#else
void
UNLOCK __ARGS ((lockp, pl))
lock_t	      *	lockp;
pl_t		pl;
#endif
{
#ifdef	__TICKET_LOCK__
	ushort_t	ticket_no;
#endif

	ASSERT (lockp != NULL);
	ASSERT (splcmp (plbase, pl) <= 0 && splcmp (pl, plhi) <= 0);


	/*
	 * We assert that the lock is actually held by someone... in the case
	 * of the ticket lock, we fetch the ticket number now since we don't
	 * have an increment instruction and we'll need it later anyway.
	 */

#ifdef	__TICKET_LOCK__
	ticket_no = ATOMIC_FETCH_USHORT (lockp->bl_lock_holder);

	ASSERT (ticket_no != ATOMIC_FETCH_USHORT (lockp->bl_next_ticket));
#else
	ASSERT (ATOMIC_FETCH_UCHAR (lockp->bl_locked) != 0);
#endif

	/*
	 * Do whatever we need to do to undo the hierarchy-assertion and
	 * debugging/statistics data structures.
	 */

	LOCK_FREE_HIERARCHY (lockp->bl_hierarchy);

	UNTRACE_BASIC_LOCK (lockp);


	/*
	 * Now release the lock, either to the next ticket-holder or to
	 * whoever gets in first, depending on the locking system.
	 */

#ifdef	__TICKET_LOCK__
	if (ATOMIC_FETCH_AND_STORE_USHORT (lockp->bl_lock_holder,
					   ticket_no + 1) != ticket_no) {
		/*
		 * If we didn't read the same number twice, then someone has
		 * released the lock that we own!
		 */

		cmn_err (CE_PANIC, "UNLOCK : Ticket-lock sequence problem");
	}
#else
	ATOMIC_CLEAR_UCHAR (lockp->bl_locked);
#endif

	/*
	 * And lower out priority level to finish up.
	 */

	(void) splx (pl);
}


/*
 *-STATUS:
 *	DDI/DKI
 *
 *-NAME:
 *	RW_ALLOC ()	Allocate and initialize a read/write lock
 *
 *-SYNOPSIS:
 *	#include <sys/types.h>
 *	#include <sys/kmem.h>
 *	#include <sys/ksynch.h>
 *
 *	rwlock_t * RW_ALLOC (uchar_t hierarchy, pl_t min_pl,
 *			     lkinfo_t * lkinfop, int flag);
 *
 *-ARGUMENTS:
 *	hierarchy	Hierarchy value which asserts the order in which this
 *			lock will be acquired relative to other basic and
 *			read/write locks. "hierarchy" must be within the range
 *			of 1 through 32 inclusive and must be chosen such that
 *			locks are normally acquired in order of increasing
 *			"hierarchy" number. In other words, when acquiring a
 *			basic lock using any function other than
 *			RW_TRYRDLOCK () or RW_TRYWRLOCK () the lock being
 *			acquired must have a "hierarchy" value that is
 *			strictly greater than the "hierarchy" values
 *			associated with all locks currently held by the
 *			calling context.
 *
 *			Implementations of lock testing may differ in whether
 *			they assume a separate range of "hierarchy" values for
 *			each interrupt priority level or a single range that
 *			spans all interrupt priority levels. In order to be
 *			portable across different implementations, drivers
 *			which may acquire locks at more than one interrupt
 *			priority level should define the "hierarchy" among
 *			those locks such that the "hierarchy" is strictly
 *			increasing with increasing priority level (eg. if M
 *			is the maximum "hierarchy" value defined for any lock
 *			that may be acquired at priority level N, then M + 1
 *			should be the minimum hierarchy value for any lock
 *			that may be acquired at any priority level greater
 *			than N).
 *
 *	min_pl		Minimum priority level argument which asserts the
 *			minimum priority leel that will be passed in with any
 *			attempt to acquire this lock [see RW_RDLOCK () and
 *			RW_WRLOCK ()]. Implementations which do not require
 *			that the interrupt priority level be raised during
 *			lock acquisition may choose not to enforce the
 *			"min_pl" assertion. The valid values for this
 *			argument are as follows:
 *
 *			  plbase	Block no interrupts
 *			  pltimeout	Block functions scheduled by itimeout
 *					and dtimeout
 *			  pldisk	Block disk device interrupts
 *			  plstr		Block STREAMS interrupts
 *			  plhi		Block all interrupts
 *
 *			The notion of a "min_pl" assumes a defined order of
 *			priority levels. The following partial order is
 *			defined:
 *			  plbase < pltimeout <= pldisk, plstr <= plhi
 *
 *			The ordering of pldisk and plstr relative to each
 *			other is not defined.
 *
 *			Setting a given priority level will block interrupts
 *			associated with that level as well as all levels that
 *			are defined to be less than or equal to the specified
 *			level. In order to be portable a driver should not
 *			acquire locks at different priority levels where the
 *			relative order of those priority levels is not defined
 *			above.
 *
 *			The "min_pl" argument should specify a priority level
 *			that would be sufficient to block out any interrupt
 *			handler that might attempt to acquire this lock. In
 *			addition, potential deadlock problems involving
 *			multiple locks should be considered when defining the
 *			"min_pl" value. For example, if the normal order of
 *			acquisition of locks A and B (as defined by the lock
 *			hierarchy) is to acquire A first and then B, lock B
 *			should never be acquired at a priority level less
 *			than the "min_pl" for lock A. Therefore, the "min_pl"
 *			for lock B should be greater than or equal to the
 *			"min_pl" for lock A.
 *
 *			Note that the specification of a "min_pl" with a
 *			RW_ALLOC () call does not actually cause any
 *			interrupts to be blocked upon lock acquisition, it
 *			simply asserts that subsequent RW_RDLOCK () or
 *			RW_WRLOCK () calls to acquire this lock will pass in a
 *			priority level at least as great as "min_pl".
 *
 *	lkinfop		Pointer to a lkinfo structure. The lk_name member of
 *			the lkinfo structure points to a character string
 *			defining a name that will be associated with the lock
 *			for the purposes of statistics gathering. The name
 *			should begin with the driver prefix and should be
 *			unique to the lock or group of locks for which the
 *			driver wishes to collect a uniquely identifiable set
 *			of statistics (ie, if a given name is shared by a
 *			group of locks, the statistics of the individual locks
 *			within the group will not be uniquely identifiable).
 *			There are no flags within the lk_flags member of the
 *			lkinfo structure defined for use with RW_ALLOC ().
 *
 *			A given lkinfo structure may be shared among multiple
 *			basic locks and read/write locks but a lkinfo
 *			structure may not be shared between a basic lock and
 *			a sleep lock. The called must ensure that the lk_flags
 *			and lk_pad members of the lkinfo structure are zeroed
 *			out before passing it to RW_ALLOC ().
 *
 *	flag		Specifies whether the caller is willing to sleep
 *			waiting for memory. If "flag" is set to KM_SLEEP, the
 *			caller will sleep if necessary until sufficient memory
 *			is available. If "flag" is set to KM_NOSLEEP, the
 *			caller will not sleep, but RW_ALLOC () will return
 *			NULL if sufficient memory is not immediately
 *			available.
 *
 *-DESCRIPTION:
 *	RW_ALLOC () dynamically allocates and initialises an instance of a
 *	read/write lock. The lock is initialised to the unlocked state.
 *
 *-RETURN VALUE:
 *	Upon successful completion, RW_ALLOC () returns a pointer to the
 *	newly allocated lock. If KM_NOSLEEP is specified and sufficient
 *	memory is not immediately available, RW_ALLOC () returns a NULL
 *	pointer.
 *
 *-LEVEL:
 *	Base only if "flag" is set to KM_SLEEP. Base or Interrupt if "flag" is
 *	set to KM_NOSLEEP.
 *
 *-NOTES:
 *	May sleep if "flag" is set to KM_NOSLEEP.
 *
 *	Driver-defined basic locks and read/write locks may be held across
 *	calls to this function if "flag" is set to KM_NOSLEEP but may not be
 *	held if "flag" is KM_SLEEP.
 *
 *	Driver-defined sleep locks may be held across calls to this function
 *	regardless of the value of "flag".
 *
 *-SEE_ALSO:
 *	RW_DEALLOC (), RW_RDLOCK (), RW_TRYRDLOCK (), RW_TRYWRLOCK (),
 *	RW_UNLOCK (), RW_WRLOCK (), lkinfo
 */

#if	__USE_PROTO__
rwlock_t * (RW_ALLOC) (__lkhier_t hierarchy, pl_t min_pl, lkinfo_t * lkinfop,
		       int flag)
#else
rwlock_t *
RW_ALLOC __ARGS ((hierarchy, min_pl, lkinfop, flag))
__lkhier_t	hierarchy;
pl_t		min_pl;
lkinfo_t      *	lkinfop;
int		flag;
#endif
{
	rwlock_t      *	lockp;

	ASSERT_HIERARCHY_OK (hierarchy, lkinfop);
	ASSERT (splcmp (plbase, min_pl) <= 0 && splcmp (min_pl, plhi) <= 0);
	ASSERT (flag == KM_SLEEP || flag == KM_NOSLEEP);


	/*
	 * Allocate and initialise the data, possibly waiting for enough
	 * memory to become available.
	 */

	if ((lockp = (rwlock_t *) _lock_malloc (sizeof (* lockp),
						flag)) != NULL) {
		INIT_LNODE (& lockp->rw_node, lkinfop, & rw_locks, flag);

		lockp->rw_min_pl = min_pl;
		lockp->rw_hierarchy = hierarchy;

		ATOMIC_CLEAR_UCHAR (lockp->rw_locked);

		ATOMIC_CLEAR_USHORT (lockp->rw_readers);

		ATOMIC_CLEAR_USHORT (lockp->rw_next_ticket);
		ATOMIC_CLEAR_USHORT (lockp->rw_lock_holder);
	}

	return lockp;
}


/*
 *-STATUS:
 *	DDI/DKI
 *
 *-NAME:
 *	RW_DEALLOC ()	Deallocate an instance of a read/write lock.
 *
 *-SYNOPSIS:
 *	#include <sys/ksynch.h>
 *
 *	void RW_DEALLOC (rwlock_t * lockp);
 *
 *-ARGUMENTS:
 *	lockp		Pointer to the read/write lock to be deallocated.
 *
 *-DESCRIPTION:
 *	RW_DEALLOC () deallocates the read/write lock specified by "lockp".
 *
 *-RETURN VALUE:
 *	None.
 *
 *-LEVEL:
 *	Base or Interrupt.
 *
 *-NOTES:
 *	Does not sleep.
 *
 *	Attempting to deallocate a lock that is currently locked or is being
 *	waited for is an error and will result in undefined behavior.
 *
 *	Driver-defined basic locks, read/write locks (other than the one being
 *	deallocated), and sleep locks may be held across calls to this
 *	function.
 *
 *-SEE_ALSO:
 *	RW_ALLOC (), RW_RDLOCK (), RW_TRYRDLOCK (), RW_TRYWRLOCK (),
 *	RW_UNLOCK (), RW_WRLOCK ()
 */

#if	__USE_PROTO__
void (RW_DEALLOC) (rwlock_t * lockp)
#else
void
RW_DEALLOC __ARGS ((lockp))
rwlock_t      *	lockp;
#endif
{
	ASSERT (lockp != NULL);
	ASSERT (ATOMIC_FETCH_UCHAR (lockp->rw_locked) == 0);
	ASSERT (ATOMIC_FETCH_USHORT (lockp->rw_readers) == 0);

	ASSERT (ATOMIC_FETCH_USHORT (lockp->rw_next_ticket) ==
		ATOMIC_FETCH_USHORT (lockp->rw_lock_holder));


	/*
	 * Remove from the list of all read/write locks and free any
	 * statistics buffer space before freeing the lock itself.
	 */

	FREE_LNODE (& lockp->rw_node, & rw_locks);

	_lock_free (lockp, sizeof (* lockp));
}


/*
 *-STATUS:
 *	DDI/DKI
 *
 *-NAME:
 *	RW_RDLOCK ()	Acquire a read/write lock in read mode.
 *
 *-SYNOPSIS:
 *	#include <sys/types.h>
 *	#include <sys/ksynch.h>
 *
 *	pl_t RW_RDLOCK (rwlock_t * lockp, pl_t pl);
 *
 *-ARGUMENTS:
 *	lockp		Pointer to the read/write lock to be acquired.
 *
 *	pl		The interrupt priority level to be set while the lock
 *			is held by the caller. Because some implementations
 *			require that interrupts that might attempt to acquire
 *			the lock be blocked on which the lock is held,
 *			portable drivers must specify a "pl" value that is
 *			sufficient to block out any interrupt handler that
 *			might attempt to acquire this lock. See the
 *			description of the "min_pl" argument to RW_ALLOC ()
 *			for additional discussion. Implementations that do
 *			not require that the interrupt priority level be
 *			raised during lock acquisition may choose to ignore
 *			this argument.
 *
 *-DESCRIPTION:
 * 	RW_RDLOCK () sets the interrupt priority level in accordance with the
 *	value specified by "pl" (if required by the implementation) and
 * 	acquires the lock specified by "lockp". If the lock is not currently
 * 	available, the caller will wait until the lock is available in read
 *	mode. A read/write lock is available in read mode when the lock is
 *	not held by any context or when the lock is held by one or more
 *	readers and there are no waiting writers. It is implementation-defined
 *	whether the caller will block during the wait. Some implementations
 *	may cause the caller to spin for the duration of the wait, while on
 *	others the caller may block at some point.
 *
 *-RETURN VALUE:
 *	Upon acquiring the lock, RW_RDLOCK () returns the previous interrupt
 *	priority level.
 *
 *-LEVEL:
 *	Base or Interrupt.
 *
 *-NOTES:
 * 	Read/write locks are not recursive. A call to RW_RDLOCK () attempting
 *	to acquire a lock that is already held by the calling context may
 *	result in deadlock.
 *
 * 	Calls to RD_RDLOCK () should honor the ordering defined by the lock
 *	hierarchy [see RW_ALLOC ()] in order to avoid deadlock.
 *
 * 	Driver-defined sleep locks may be held across calls to this function.
 *
 * 	Driver-defined basic locks and read/write locks may be held across
 *	calls to this function subject to the hierarchy and recursion
 *	restrictions described above.
 *
 * 	When called from interrupt level, the "pl" argument must not specify
 *	a priority below the level at which the interrupt handler is running.
 *
 *-SEE_ALSO:
 *	RW_ALLOC (), RW_DEALLOC (), RW_TRYRDLOCK (), RW_TRYWRLOCK (),
 *	RW_UNLOCK (), RW_WRLOCK ()
 */

#if	__USE_PROTO__
pl_t (RW_RDLOCK) (rwlock_t * lockp, pl_t pl)
#else
pl_t
RW_RDLOCK __ARGS ((lockp, pl))
rwlock_t      *	lockp;
pl_t		pl;
#endif
{
	pl_t		prev_pl;
	ushort_t	ticket_no;

	ASSERT (lockp != NULL);
	ASSERT (splcmp (pl, plhi) <= 0);


	/*
	 * Enforce minimum-priority assertion, pl >= lockp->rw_min_pl. Note
	 * that splcmp () abstracts subtraction-for-comparison of priority
	 * levels, which explains the form of the assertion.
	 */

	ASSERT (splcmp (pl, lockp->rw_min_pl) >= 0);


	/*
	 * On a uniprocessor, encountering a read/write lock that is already
	 * locked is *always* an error, even on machines with many different
	 * interrupt priority levels. The hierarchy-assertion mechanism
	 * cannot always deal with this, since RW_TRYRDLOCK () can acquire
	 * locks in a different order.
	 *
	 * On a multiprocessor, we can just spin here.
	 */

	for (;;) {


		/*
		 * We block out merely the necessary interrupts at this point;
		 * we don't need a blanket interrupt blockage since the lock
		 * system works as atomically as possible.
		 */

		prev_pl = splx (pl);

		if (ATOMIC_TEST_AND_SET_UCHAR (lockp->rw_locked) == 0) {
			/*
			 * Now we have acquired the test-and-set part, see
			 * if there are any waiting writers preventing us
			 * from acquiring a shared read lock.
			 *
			 * We read the "lock holder" and "next ticket" entries
			 * in that sequence to be pessimistic, since we can
			 * assume that other CPUs might be writing to this...
			 */

			ticket_no = ATOMIC_FETCH_USHORT (lockp->rw_lock_holder);

			if (ticket_no == ATOMIC_FETCH_USHORT (lockp->rw_next_ticket))
				break;

			/*
			 * The read/write lock is held exclusively, release
			 * the test-and-set sub-lock.
			 */

			ATOMIC_CLEAR_UCHAR (lockp->rw_locked);
		}

		/*
		 * We can return the interrupt priority to whatever it was
		 * upon entry to this routine since we can't acquire the
		 * initial test-and-set lock.
		 */

		(void) splx (prev_pl);

#ifdef	__UNIPROCESSOR__
		cmn_err (CE_PANIC, "RW_RDLOCK : deadlock!");
#elif	defined (__TEST_AND_TEST__)
		/*
		 * In order to reduce contention on the test-and-set part of
		 * the read/write lock, we defer attempting to acquire that
		 * lock until there appear to be no waiting writers, and until
		 * the test-and-set lock is free before attempting to re-
		 * acquire it. Of course, more sophisticated backoff schemes
		 * might also help this approach.
		 */

		do {
			/*
			 * We use this idiom to ensure that the lock holder
			 * item is read before the next ticket item for
			 * maximum pessimism.
			 */

			ticket_no = ATOMIC_FETCH_USHORT (lockp->rw_lock_holder);

		} while (ticket_no !=
			    ATOMIC_FETCH_USHORT (lockp->rw_next_ticket) ||
			 ATOMIC_FETCH_UCHAR (lockp->rw_locked) != 0);
#endif
	}


	/*
	 * There are no waiting writers yet, so acquire the lock in read mode
	 * by incrementing the count of readers.
	 */

	ticket_no = ATOMIC_FETCH_USHORT (lockp->rw_readers);

	if (ATOMIC_FETCH_AND_STORE_USHORT (lockp->rw_readers, ticket_no + 1)
	    != ticket_no) {
		/*
		 * If we didn't read the same number twice, then the
		 * test-and-set lock protection isn't doing its job.
		 */

		cmn_err (CE_PANIC, "RW_RDLOCK : lock increment failure");
	}


#if 0
#ifdef	__UNIPROCESSOR__
	if (ticket_no != 0)
		cmn_err (CE_WARN, "RW_RDLOCK : Recursive read-lock attempt");
#endif
#endif

	/*
	 * And allow other CPUs to try and acquire tickets or increment the
	 * reader count by releasing the test-and-set lock.
	 */

	ATOMIC_CLEAR_UCHAR (lockp->rw_locked);


	/*
	 * Here we check and maintain the hierarchy assertions, and record
	 * any required debugging/statistics information about the lock.
	 */

	LOCK_COUNT_HIERARCHY (lockp->rw_hierarchy);

	TRACE_RW_LOCK (lockp);


	/*
	 * Now would be an appropriate time to check that the requested level
	 * is >= the current level on entry. Strictly speaking, this does not
	 * have to be true if the processor is currently at base level, but
	 * for now we'll discourage that behaviour.
	 */

	ASSERT (splcmp (pl, prev_pl) >= 0);

	return prev_pl;
}


/*
 *-STATUS:
 *	DDI/DKI
 *
 *-NAME:
 *	RW_TRYRDLOCK ()	Try to acquire a read/write lock in read mode.
 *
 *-SYNOPSIS:
 *	#include <sys/types.h>
 *	#include <sys/ksynch.h>
 *
 *	pl_t RW_TRYRDLOCK (rwlock_t * lockp, pl_t pl);
 *
 *-ARGUMENTS:
 *	lockp		Pointer to the read/write lock to be acquired.
 *
 *	pl		The interrupt priority level to be set while the lock
 *			is held by the caller. Because some implementations
 *			require that interrupts that might attempt to acquire
 *			the lock be blocked on the processor on which the
 *			lock is held, portable drivers must specify a "pl"
 *			value that is sufficient to block out and interrupt
 *			handler that might attempt to acquire this lock. See
 *			the description of RW_ALLOC () for additional
 *			discussion and a list of the valid values for "pl".
 *			Implementations that do not require that the interrupt
 *			priority level be raised during lock acquisition may
 *			choose to ignore this argument.
 *
 *-DESCRIPTION:
 *	If the lock specified by "lockp" is immediately available in read mode
 *	(there is not a writer holding the lock and there are no waiting
 *	writers) RW_TRYRDLOCK () sets the interrupt priority level in
 *	accordance with the value specified by "pl" (if required by the
 *	implementation) and acquires the lock in read mode. If the lock is not
 *	immediately available in read mode, the function returns without
 *	acquiring the lock.
 *
 *-RETURN VALUE:
 *	If the lock is acquired, RW_TRYRDLOCK () returns the previous
 *	interrupt priority level ("plbase" - "plhi"). If the lock is not
 *	acquired the value "invpl" is returned.
 *
 *-LEVEL:
 *	Base or Interrupt.
 *
 *-NOTES:
 *	Does not sleep.
 *
 *	RW_TRYRDLOCK () may be used to acquire a lock in a different order
 *	from the order defined by the lock hierarchy.
 *
 *	Driver-defined basic locks, read/write locks, and sleep locks may be
 *	held across calls to this function.
 *
 *	When called from interrupt level, the "pl" argument must not specify
 *	a priority level below the level at which the interrupt handler is
 *	running.
 *
 *-SEE_ALSO:
 *	RW_ALLOC (), RW_DEALLOC (), RW_RDLOCK (), RW_TRYWRLOCK (),
 *	RW_UNLOCK (), RW_WRLOCK ()
 */

#if	__USE_PROTO__
pl_t (RW_TRYRDLOCK) (rwlock_t * lockp, pl_t pl)
#else
pl_t
RW_TRYRDLOCK __ARGS ((lockp, pl))
rwlock_t      *	lockp;
pl_t		pl;
#endif
{
	pl_t		prev_pl;
	ushort_t	ticket_no;

	ASSERT (lockp != NULL);
	ASSERT (splcmp (pl, plhi) <= 0);


	/*
	 * Enforce minimum-priority assertion, pl >= lockp->bl_min_pl. Note
	 * that splcmp () abstracts subtraction-for-comparison of priority
	 * levels, which explains the form of the assertion.
	 */

	ASSERT (splcmp (pl, lockp->rw_min_pl) >= 0);


#ifdef	__TEST_AND_TEST__
	/*
	 * We test the ticket numbers to see whether there is any reason to
	 * even try acquiring the test-and-set lock that forms the ticket
	 * gate.
	 *
	 * We read the "lock holder" and "next ticket" entries in that
	 * sequence to be pessimistic, since we can assume that other CPUs
	 * might be looking at this...
	 */

	ticket_no = ATOMIC_FETCH_USHORT (lockp->rw_lock_holder);

	if (ticket_no != ATOMIC_FETCH_USHORT (lockp->rw_next_ticket))
		return invpl;
#endif


	/*
	 * We block out interrupts at this point to allow the following
	 * operations room to do their stuff in, since interrupts at an
	 * inappropriate moment can cause deadlock... of course, if the
	 * definition of the lock is such that interrupts can proceed, then
	 * that's OK, since the lock acquisition is atomic.
	 */

	prev_pl = splx (pl);


	/*
	 * Now would be an appropriate time to check that the requested level
	 * is >= the current level on entry.
	 */

	ASSERT (splcmp (pl, prev_pl) >= 0);


	/*
	 * Test to see whether the lock in question is already taken, and if
	 * not, we take it. We don't spin if this is a ticket lock, since if
	 * the basic lock is taken there is *no way* that the lock will be
	 * free for us immediately.
	 */

	if (ATOMIC_TEST_AND_SET_UCHAR (lockp->rw_locked) == 0) {
		/*
		 * Check the ticket numbers to ensure that the lock *really*
		 * is free.
		 */

		ticket_no = ATOMIC_FETCH_USHORT (lockp->rw_next_ticket);

		if (ticket_no != ATOMIC_FETCH_USHORT (lockp->rw_lock_holder)) {

			ATOMIC_CLEAR_UCHAR (lockp->rw_locked);
			goto try_failed;
		}

		/*
		 * Now acquire the lock in read mode by incrementing the
		 * count of readers.
		 */

		ticket_no = ATOMIC_FETCH_USHORT (lockp->rw_readers);

		if (ATOMIC_FETCH_AND_STORE_USHORT (lockp->rw_readers,
						   ticket_no + 1) != ticket_no) {
			/*
			 * If we didn't read the same number twice, then the
			 * test-and-set lock protection isn't doing its job.
			 */

			cmn_err (CE_PANIC, "RW_TRYRDLOCK : Increment failure");
		}


		/*
		 * Now we have the lock, we can release the test-and-set lock.
		 */

		ATOMIC_CLEAR_UCHAR (lockp->rw_locked);


		/*
		 * RW_TRYRDLOCK () bypasses the hierarchy-assertion tests,
		 * but we record the maximum acquired hierarchy level for
		 * maximum strictness checking in inner lock attempts.
		 *
		 * We also record debugging and statistics information here
		 * with TRACE_RW_LOCK ().
		 */

		LOCK_COUNT_HIERARCHY (lockp->rw_hierarchy);

		TRACE_RW_LOCK (lockp);


		/*
		 * All done! Return successfully.
		 */

		return prev_pl;
	}

try_failed:
	/*
	 * We cannot acquire the lock, so reset the priority and exit with
	 * the flag value.
	 */

	(void) splx (prev_pl);

	return invpl;
}


/*
 *-STATUS:
 *	DDI/DKI
 *
 *-NAME:
 *	RW_TRYWRLOCK ()	Try to acquire a read/write lock in write mode.
 *
 *-SYNOPSIS:
 *	#include <sys/types.h>
 *	#include <sys/ksynch.h>
 *
 *	pl_t RW_TRYWRLOCK (rwlock_t * lockp, pl_t pl);
 *
 *-ARGUMENTS:
 *	lockp		Pointer to the read/write lock to be acquired.
 *
 *	pl		The interrupt priority level to be set while the lock
 *			is held by the caller. Because some implementations
 *			require that interrupts that might attempt to acquire
 *			the lock be blocked on the processor on which the
 *			lock is held, portable drivers must specify a "pl"
 *			value that is sufficient to block out and interrupt
 *			handler that might attempt to acquire this lock. See
 *			the description of RW_ALLOC () for additional
 *			discussion and a list of the valid values for "pl".
 *			Implementations that do not require that the interrupt
 *			priority level be raised during lock acquisition may
 *			choose to ignore this argument.
 *
 *-DESCRIPTION:
 *	If the lock specified by "lockp" is immediately available in write
 *	mode (no context is holding the lock in read mode or write mode),
 *	RW_TRYWRLOCK () sets the interrupt priority level in accordance with
 *	the value specified by "pl" (if required by the implementation) and
 *	acquires the lock in write mode. If the lock is not immediately
 *	available in write mode, the function returns without acquiring the
 *	lock.
 *
 *-RETURN VALUE:
 *	If the lock is acquired, RW_TRYWRLOCK () returns the previous
 *	interrupt priority level ("plbase" - "plhi"). If the lock is not
 *	acquired the value "invpl" is returned.
 *
 *-LEVEL:
 *	Base or Interrupt.
 *
 *-NOTES:
 *	Does not sleep.
 *
 *	RW_TRYWRLOCK () may be used to acquire a lock in a different order
 *	from the order defined by the lock hierarchy.
 *
 *	Driver-defined basic locks, read/write locks, and sleep locks may be
 *	held across calls to this function.
 *
 *	When called from interrupt level, the "pl" argument must not specify
 *	a priority level below the level at which the interrupt handler is
 *	running.
 *
 *-SEE_ALSO:
 *	RW_ALLOC (), RW_DEALLOC (), RW_RDLOCK (), RW_TRYRDLOCK (),
 *	RW_UNLOCK (), RW_WRLOCK ()
 */

#if	__USE_PROTO__
pl_t (RW_TRYWRLOCK) (rwlock_t * lockp, pl_t pl)
#else
pl_t
RW_TRYWRLOCK __ARGS ((lockp, pl))
rwlock_t      *	lockp;
pl_t		pl;
#endif
{
	pl_t		prev_pl;
	ushort_t	ticket_no;

	ASSERT (lockp != NULL);
	ASSERT (splcmp (pl, plhi) <= 0);


	/*
	 * Enforce minimum-priority assertion, pl >= lockp->bl_min_pl. Note
	 * that splcmp () abstracts subtraction-for-comparison of priority
	 * levels, which explains the form of the assertion.
	 */

	ASSERT (splcmp (pl, lockp->rw_min_pl) >= 0);


#ifdef	__TEST_AND_TEST__
	/*
	 * We test the ticket numbers and reader count to see whether there
	 * is any reason to even try acquiring the test-and-set lock that
	 * forms the ticket gate.
	 *
	 * We read the "lock holder" and "next ticket" entries in that
	 * sequence to be pessimistic, since we can assume that other CPUs
	 * might be looking at this...
	 */

	ticket_no = ATOMIC_FETCH_USHORT (lockp->rw_lock_holder);

	if (ticket_no != ATOMIC_FETCH_USHORT (lockp->rw_next_ticket) ||
	    ATOMIC_FETCH_USHORT (lockp->rw_readers) != 0)
		return invpl;
#endif


	/*
	 * We block out interrupts at this point to allow the following
	 * operations room to do their stuff in, since interrupts at an
	 * inappropriate moment can cause deadlock... of course, if the
	 * definition of the lock is such that interrupts can proceed, then
	 * that's OK, since the lock acquisition is atomic.
	 */

	prev_pl = splx (pl);


	/*
	 * Now would be an appropriate time to check that the requested level
	 * is >= the current level on entry.
	 */

	ASSERT (splcmp (pl, prev_pl) >= 0);


	/*
	 * Test to see whether the lock in question is already taken, and if
	 * not, we take it. We don't spin if this is a ticket lock, since if
	 * the basic lock is taken there is *no way* that the lock will be
	 * free for us immediately.
	 */

	if (ATOMIC_TEST_AND_SET_UCHAR (lockp->rw_locked) == 0) {
		/*
		 * Check the ticket numbers and the reader count to ensure
		 * that the lock *really* is free.
		 */

		ticket_no = ATOMIC_FETCH_USHORT (lockp->rw_next_ticket);

		if (ticket_no != ATOMIC_FETCH_USHORT (lockp->rw_lock_holder) ||
		    ATOMIC_FETCH_USHORT (lockp->rw_readers) != 0) {

			ATOMIC_CLEAR_UCHAR (lockp->rw_locked);
			goto try_failed;
		}

		/*
		 * Now acquire the lock in write mode by incrementing the
		 * ticket counter.
		 */

		if (ATOMIC_FETCH_AND_STORE_USHORT (lockp->rw_next_ticket,
						   ticket_no + 1) != ticket_no) {
			/*
			 * If we didn't read the same number twice, then the
			 * test-and-set lock protection isn't doing its job.
			 */

			cmn_err (CE_PANIC, "RW_TRYWRLOCK : Increment failure");
		}


		/*
		 * Now we have the lock, we can release the test-and-set lock.
		 */

		ATOMIC_CLEAR_UCHAR (lockp->rw_locked);


		/*
		 * RW_TRYWRLOCK () bypasses the hierarchy-assertion tests,
		 * but we record the maximum acquired hierarchy level for
		 * maximum strictness checking in inner lock attempts.
		 *
		 * We also record and debugging and statistics information
		 * here with TRACE_RW_LOCK ().
		 */

		LOCK_COUNT_HIERARCHY (lockp->rw_hierarchy);

		TRACE_RW_LOCK (lockp);


		/*
		 * All done! Return success...
		 */

		return prev_pl;
	}

try_failed:
	/*
	 * We cannot acquire the lock, so reset the priority and exit with
	 * the flag value.
	 */

	(void) splx (prev_pl);

	return invpl;
}


/*
 *-STATUS:
 *	DDI/DKI
 *
 *-NAME:
 *	RW_UNLOCK ()	Release a read/write lock.
 *
 *-SYNOPSIS:
 *	#include <sys/types.h>
 *	#include <sys/ksynch.h>
 *
 *	pl_t RW_UNLOCK (rwlock_t * lockp, pl_t pl);
 *
 *-ARGUMENTS:
 *	lockp		Pointer to the read/write lock to be released.
 *
 *	pl		The interrupt priority level to be set after releasing
 *			the lock. See the description of the "min_pl" argument
 *			to RW_ALLOC () for a list of the valid values for
 *			"pl". If lock calls are not being nested or if the
 *			caller is unlocking in the reverse order that locks
 *			were acquired, the "pl" argument will typically be the
 *			value that was returned from the corresponding call to
 *			acquire the lock. The caller may need to specify a
 *			different value for "pl" if nested locks are being
 *			released in some order other that the reverse order of
 *			acquisition, so as to ensure that the interrupt
 *			priority level is kept sufficiently high to block
 *			interrupt code that might attempt to acquire locks
 *			which are still held. Although portable drivers must
 *			always specify an appropriate "pl" argument,
 *			implementations which do not require that the
 * 			interrupt priority level be raised during lock
 *			acquisition may choose to ignore this argument.
 *
 *-DESCRIPTION:
 *	RW_UNLOCK () releases the basic lock specified by "lockp" and then
 *	sets the interrupt priority level in accordance with the value
 *	specified by "pl" (if required by the implementation).
 *
 *-RETURN VALUE:
 *	None.
 *
 *-LEVEL:
 *	Base or Interrupt.
 *
 *-NOTES:
 *	Does not sleep.
 *
 *	Driver-defined basic locks, read/write locks, and sleep locks may be
 *	held across calls to this function.
 *
 *-SEE_ALSO:
 *	RW_ALLOC (), RW_DEALLOC (), RW_RDLOCK (), RW_TRYRDLOCK (),
 *	RW_TRYWRLOCK (), RW_WRLOCK ()
 */

#if	__USE_PROTO__
void (RW_UNLOCK) (rwlock_t * lockp, pl_t pl)
#else
void
RW_UNLOCK __ARGS ((lockp, pl))
rwlock_t      *	lockp;
pl_t		pl;
#endif
{
	ushort_t	ticket_no;

	ASSERT (lockp != NULL);
	ASSERT (splcmp (plbase, pl) <= 0 && splcmp (pl, plhi) <= 0);


	/*
	 * Undo whatever the hierarchy-assertion and debugging/statistics
	 * mechanisms do.
	 */

	LOCK_FREE_HIERARCHY (lockp->rw_hierarchy);

	UNTRACE_RW_LOCK (lockp);


	/*
	 * Now release the lock... since writers have to wait for all readers
	 * to release the lock, if there any readers then we are releasing
	 * in read mode.
	 */

	if (ATOMIC_FETCH_USHORT (lockp->rw_readers) != 0) {
		/*
		 * In order to safely decrement the reader count, we have to
		 * acquire the test-and-set lock in the absence of an
		 * atomic decrement facility.
		 */

		while (ATOMIC_TEST_AND_SET_UCHAR (lockp->rw_locked) != 0) {

#ifdef	__UNIPROCESSOR__
			cmn_err (CE_PANIC, "RW_UNLOCK : deadlock!");
#elif	defined (__TEST_AND_TEST__)
			/*
			 * To reduce contention, we wait until the test-and-
			 * set lock is free before attempting to re-acquire
			 * it.
			 */

			while (ATOMIC_FETCH_UCHAR (lockp->rw_locked) != 0)
				/* DO NOTHING */;
#endif
		}

		ticket_no = ATOMIC_FETCH_USHORT (lockp->rw_readers);

		if (ATOMIC_FETCH_AND_STORE_USHORT (lockp->rw_readers,
						   ticket_no - 1) != ticket_no ) {
			/*
			 * If we didn't read the same number twice, then the
			 * test-and-set lock protection isn't doing its job.
			 */

			cmn_err (CE_PANIC, "RW_UNLOCK : Decrement failure");
		}

		/*
		 * Now free the test-and-set lock.
		 */

		ATOMIC_CLEAR_UCHAR (lockp->rw_locked);

	} else if (ticket_no = ATOMIC_FETCH_USHORT (lockp->rw_lock_holder),
		   ticket_no != ATOMIC_FETCH_USHORT (lockp->rw_next_ticket)) {
		/*
		 * Release the lock to the next ticket holder or the waiting
		 * readers if there are no waiting ticket-holders.
		 */


		if (ATOMIC_FETCH_AND_STORE_USHORT (lockp->rw_lock_holder,
						   ticket_no + 1) != ticket_no ) {
			/*
			 * If we didn't read the same number twice, then the
			 * test-and-set lock protection isn't doing its job.
			 */

			cmn_err (CE_PANIC, "RW_UNLOCK : Increment failure");
		}

	} else
		cmn_err (CE_PANIC, "RW_UNLOCK : not locked");


	/*
	 * And lower out priority level to finish up.
	 */

	(void) splx (pl);
}


/*
 *-STATUS:
 *	DDI/DKI
 *
 *-NAME:
 *	RW_WRLOCK ()	Acquire a read/write lock in write mode.
 *
 *-SYNOPSIS:
 *	#include <sys/types.h>
 *	#include <sys/ksynch.h>
 *
 *	pl_t RW_WRLOCK (rwlock_t * lockp, pl_t pl);
 *
 *-ARGUMENTS:
 *	lockp		Pointer to the read/write lock to be acquired.
 *
 *	pl		The interrupt priority level to be set while the lock
 *			is held by the caller. Because some implementations
 *			require that interrupts that might attempt to acquire
 *			the lock be blocked on which the lock is held,
 *			portable drivers must specify a "pl" value that is
 *			sufficient to block out any interrupt handler that
 *			might attempt to acquire this lock. See the
 *			description of the "min_pl" argument to RW_ALLOC ()
 *			for additional discussion. Implementations that do
 *			not require that the interrupt priority level be
 *			raised during lock acquisition may choose to ignore
 *			this argument.
 *
 *-DESCRIPTION:
 * 	RW_WRLOCK () sets the interrupt priority level in accordance with the
 *	value specified by "pl" (if required by the implementation) and
 * 	acquires the lock specified by "lockp". If the lock is not currently
 * 	available, the caller will wait until the lock is available in write
 *	mode. A read/write lock is available in write mode when the lock is
 *	not held by any context. It is implementation-defined whether the
 *	caller will block during the wait. Some implementations may cause the
 *	caller to spin for the duration of the wait, while on others the
 *	caller may block at some point.
 *
 *-RETURN VALUE:
 *	Upon acquiring the lock, RW_WRLOCK () returns the previous interrupt
 *	priority level.
 *
 *-LEVEL:
 *	Base or Interrupt.
 *
 *-NOTES:
 * 	Read/write locks are not recursive. A call to RW_WRLOCK () attempting
 *	to acquire a lock that is already held by the calling context may
 *	result in deadlock.
 *
 * 	Calls to RD_WRLOCK () should honor the ordering defined by the lock
 *	hierarchy [see RW_ALLOC ()] in order to avoid deadlock.
 *
 * 	Driver-defined sleep locks may be held across calls to this function.
 *
 * 	Driver-defined basic locks and read/write locks may be held across
 *	calls to this function subject to the hierarchy and recursion
 *	restrictions described above.
 *
 * 	When called from interrupt level, the "pl" argument must not specify
 *	a priority below the level at which the interrupt handler is running.
 *
 *-SEE_ALSO:
 *	RW_ALLOC (), RW_DEALLOC (), RW_RDLOCK (), RW_TRYRDLOCK (),
 *	RW_TRYWRLOCK (), RW_UNLOCK ()
 */

#if	__USE_PROTO__
pl_t (RW_WRLOCK) (rwlock_t * lockp, pl_t pl)
#else
pl_t
RW_WRLOCK __ARGS ((lockp, pl))
rwlock_t      *	lockp;
pl_t		pl;
#endif
{
	pl_t		prev_pl;
	ushort_t	ticket_no;

	ASSERT (lockp != NULL);
	ASSERT (splcmp (pl, plhi) <= 0);


	/*
	 * Enforce minimum-priority assertion, pl >= lockp->rw_min_pl. Note
	 * that splcmp () abstracts subtraction-for-comparison of priority
	 * levels, which explains the form of the assertion.
	 */

	ASSERT (splcmp (pl, lockp->rw_min_pl) >= 0);


	/*
	 * On a uniprocessor, encountering a read/write lock that is already
	 * locked is *always* an error, even on machines with many different
	 * interrupt priority levels. The hierarchy-assertion mechanism
	 * cannot always deal with this, since RW_TRYRDLOCK () can acquire
	 * locks in a different order.
	 *
	 * On a multiprocessor, we can just spin here. Note that since LOCK ()
	 * is defined as requiring values of "pl" greater than the current
	 * interrupt priority level, the use of splraise () by
	 * TEST_AND_SET_LOCK () is legitimate.
	 */

	prev_pl = TEST_AND_SET_LOCK (lockp->rw_locked, pl, "RW_WRLOCK");


	/*
	 * Take a ticket...
	 */

	ticket_no = ATOMIC_FETCH_USHORT (lockp->rw_next_ticket);

	if (ATOMIC_FETCH_AND_STORE_USHORT (lockp->rw_next_ticket,
					   ticket_no + 1) != ticket_no) {
		/*
		 * If we didn't read the same number twice, then the
		 * test-and-set lock protection isn't doing its job.
		 */

		cmn_err (CE_PANIC, "RW_WRLOCK : ticket increment failure");
	}


	/*
	 * Allow other CPUs access to the lock data structures by releasing
	 * the test-and-set lock.
	 */

	ATOMIC_CLEAR_UCHAR (lockp->rw_locked);


	/*
	 * Now, let's wait for our number to come up and for all the readers
	 * to release their shared locks.
	 */

	while (ATOMIC_FETCH_USHORT (lockp->rw_lock_holder) != ticket_no ||
	       ATOMIC_FETCH_USHORT (lockp->rw_readers) != 0) {
		/*
		 * At this point we might want to implement some form of
		 * proportional backoff to reduce memory traffic. Later.
		 *
		 * Another hot contender for this spot is detecting failures
		 * on other CPUs...
		 */

#ifdef	__UNIPROCESSOR__
		cmn_err (CE_PANIC, "RW_WRLOCK : deadlock on ticket");
#endif
	}


	/*
	 * Now would be an appropriate time to check that the requested level
	 * is >= the current level on entry. Strictly speaking, this does not
	 * have to be true if the processor is currently at base level, but
	 * for now we'll discourage that behaviour.
	 */

	ASSERT (splcmp (pl, prev_pl) >= 0);


	/*
	 * Test the lock-acquisition-hierarchy assertions.
	 */

	ASSERT (ddi_cpu_data ()->dc_max_hierarchy < lockp->rw_hierarchy);

	LOCK_COUNT_HIERARCHY (lockp->rw_hierarchy);


	/*
	 * Since it may be useful for post-mortem debugging to record some
	 * information about the context which acquired the lock, we defer
	 * to some generic recorder function or macro.
	 *
	 * Note that in general it does not appear to be possible to make
	 * detailed assertions about the relation between the contexts in
	 * which a lock is acquired and/or released. In particular, it
	 * might be possible for a basic lock to be tied to some device
	 * hardware-related operation where a lock might be acquired on one
	 * CPU and released on another.
	 *
	 * In addition, any lock statistics are kept by this operation.
	 */

	TRACE_RW_LOCK (lockp);

	return prev_pl;
}


/*
 *-STATUS:
 *	DDI/DKI
 *
 *-NAME:
 *	SLEEP_ALLOC ()	Allocate and initialize a sleep lock.
 *
 *-SYNOPSIS:
 *	#include <sys/types.h>
 *	#include <sys/kmem.h>
 *	#include <sys/ksynch.h>
 *
 *	sleep_t * SLEEP_ALLOC (int arg, lkinfo_t * lkinfop, int flag);
 *
 *-ARGUMENTS:
 *	arg		Placeholder for future use. "arg" must be equal to
 *			zero.
 *
 *
 *	lkinfop		Pointer to a lkinfo structure. The lk_name member of
 *			the lkinfo structure points to a character string
 *			defining a name that will be associated with the lock
 *			for the purposes of statistics gathering. The name
 *			should begin with the driver prefix and should be
 *			unique to the lock or group of locks for which the
 *			driver wishes to collect a uniquely identifiable set
 *			of statistics (ie, if a given name is shared by a
 *			group of locks, the statistics of the individual locks
 *			within the group will not be uniquely identifiable).
 *
 *			The only bit flag currently specified within the
 *			"lk_flags" member of the lkinfo structure is the
 *			LK_NOSTATS flag, which specifies that statistics are
 *			not to be collected for this particular lock.
 *
 *			A given lkinfo structure may be shared among multiple
 *			sleep but a lkinfo structure may not be shared between
 *			a sleep lock and a basic or read/write lock. The
 *			called must ensure that the "lk_pad" member of the
 *			"lkinfo" structure is zeroed out before passing it to
 *			SLEEP_ALLOC ().
 *
 *	flag		Specifies whether the caller is willing to sleep
 *			waiting for memory. If "flag" is set to KM_SLEEP, the
 *			caller will sleep if necessary until sufficient memory
 *			is available. If "flag" is set to KM_NOSLEEP, the
 *			caller will not sleep, but SLEEP_ALLOC () will return
 *			NULL if sufficient memory is not immediately
 *			available.
 *
 *-DESCRIPTION:
 *	SLEEP_ALLOC () dynamically allocates and initialises an instance of a
 *	sleep lock. The lock is initialised to the unlocked state.
 *
 *-RETURN VALUE:
 *	Upon successful completion, SLEEP_ALLOC () returns a pointer to the
 *	newly allocated lock. If KM_NOSLEEP is specified and sufficient
 *	memory is not immediately available, SLEEP_ALLOC () returns a NULL
 *	pointer.
 *
 *-LEVEL:
 *	Base only if "flag" is set to KM_SLEEP. Base or Interrupt if "flag" is
 *	set to KM_NOSLEEP.
 *
 *-NOTES:
 *	May sleep if "flag" is set to KM_NOSLEEP.
 *
 *	Driver-defined basic locks and read/write locks may be held across
 *	calls to this function if "flag" is set to KM_NOSLEEP but may not be
 *	held if "flag" is KM_SLEEP.
 *
 *	Driver-defined sleep locks may be held across calls to this function
 *	regardless of the value of "flag".
 *
 *-SEE_ALSO:
 *	SLEEP_DEALLOC (), SLEEP_LOCK (), SLEEP_LOCK_SIG (),
 *	SLEEP_LOCKAVAIL (), SLEEP_LOCKOWNED (), SLEEP_TRYLOCK (),
 *	SLEEP_UNLOCK (), lkinfo
 */

#if	__USE_PROTO__
sleep_t * (SLEEP_ALLOC) (int arg, lkinfo_t * lkinfop, int flag)
#else
sleep_t *
SLEEP_ALLOC __ARGS ((arg, lkinfop, flag))
int		arg;
lkinfo_t      *	lkinfop;
int		flag;
#endif
{
	sleep_t	      *	lockp;

	ASSERT (arg == 0);
	ASSERT (flag == KM_SLEEP || flag == KM_NOSLEEP);
	ASSERT (lkinfop != NULL);


	/*
	 * Allocate and initialise the data, possibly waiting for enough
	 * memory to become available.
	 */

	if ((lockp = (sleep_t *) _lock_malloc (sizeof (* lockp),
					       flag)) != NULL) {
		INIT_LNODE (& lockp->sl_node, lkinfop, & sleep_locks, flag);
		PLIST_INIT (lockp->sl_plist);
	}

	return lockp;
}


/*
 *-STATUS:
 *	DDI/DKI
 *
 *-NAME:
 *	SLEEP_DEALLOC ()	Deallocate an instance of a sleep lock.
 *
 *-SYNOPSIS:
 *	#include <sys/ksynch.h>
 *
 *	void SLEEP_DEALLOC (sleep_t * lockp);
 *
 *-ARGUMENTS:
 *	lockp		Pointer to the sleep lock to be deallocated.
 *
 *-DESCRIPTION:
 *	SLEEP_DEALLOC () deallocates the sleep lock specified by "lockp".
 *
 *-RETURN VALUE:
 *	None.
 *
 *-LEVEL:
 *	Base or Interrupt.
 *
 *-NOTES:
 *	Does not sleep.
 *
 *	Attempting to deallocate a lock that is currently locked or is being
 *	waited for is an error and will result in undefined behavior.
 *
 *	Driver-defined basic locks, read/write locks, and sleep locks (other
 *	than the one being deallocated) may be held across calls to this
 *	function.
 *
 *-SEE_ALSO:
 *	SLEEP_ALLOC (), SLEEP_LOCK (), SLEEP_LOCK_SIG (), SLEEP_LOCKAVAIL (),
 *	SLEEP_LOCKOWNED (), SLEEP_TRYLOCK (), SLEEP_UNLOCK ()
 */

#if	__USE_PROTO__
void (SLEEP_DEALLOC) (sleep_t * lockp)
#else
void
SLEEP_DEALLOC __ARGS ((lockp))
sleep_t	      *	lockp;
#endif
{
	ASSERT (lockp != NULL);

	/*
	 * Remove from the list of all sleep locks and free any statistics
	 * buffer space before freeing the lock itself.
	 */

	PLIST_DESTROY (lockp->sl_plist);

	FREE_LNODE (& lockp->sl_node, & sleep_locks);

	_lock_free (lockp, sizeof (* lockp));
}


/*
 *-STATUS:
 *	DDI/DKI
 *
 *-NAME:
 *	SLEEP_LOCK ()	Acquire a sleep lock.
 *
 *-SYNOPSIS:
 *	#include <sys/ksynch.h>
 *
 *	void SLEEP_LOCK (sleep_t * lockp, int priority);
 *
 *-ARGUMENTS:
 *	lockp		Pointer to the sleep lock to be acquired.
 *
 *	priority	A hint to the scheduling policy as to the relative
 *			priority the caller wishes to be assigned while
 *			running in the kernel after waking up. The valid
 *			values for this argument are as follows:
 *
 *			pridisk	    Priority appropriate for disk driver
 *			prinet	    Priority appropriate for network driver.
 *			pritty	    Priority appropriate for terminal driver.
 *			pritape     Priority appropriate for tape driver.
 *			prihi	    High priority.
 *			primed	    Medium priority.
 *			prilo	    Low priority.
 *
 *			Drivers may use these values to request a priority
 *			appropriate to a given type of device or to request a
 *			priority that is high, medium or low relative to other
 *			activities within the kernel.
 *
 *			It is also permissible to specify positive or negative
 *			offsets from the values defined above. Positive
 *			offsets result in favourable priority. The maximum
 *			allowable offset in all cases is 3 (eg. pridisk+3
 *			and pridisk-3 are valid values by pridisk+4 and
 *			pridisk-4 are not valid). Offsets can be useful in
 *			defining the relative importance of different locks or
 *			resources that may be hald by a given driver. In
 *			general, a higher relative priority should be used
 *			when the caller is attempting to acquire a highly-
 *			contended lock or resource, or when the caller is
 *			already holding one or more locks or kernel resources
 *			upon entry to SLEEP_LOCK ().
 *
 *			The exact semantics of the "priority" argument is
 *			specific to the scheduling class of the caller, and
 *			some scheduling classes may choose to ignore the
 8			argument for the purposes of assigning a scheduling
 *			priority.
 *
 *-DESCRIPTION:
 *	SLEEP_LOCK () acquires the sleep lock specified by "lockp". If the
 *	lock is not immediately available, the caller is put to sleep (the
 *	caller's execution is suspended and other processes may be scheduled)
 *	until the lock becomes available to the caller, at which point the
 *	caller wakes up and returns with the lock held.
 *
 *	The caller will not be interrupted by signals while sleeping inside
 *	SLEEP_LOCK ().
 *
 *-RETURN VALUE:
 *	None.
 *
 *-LEVEL:
 *	Base level only.
 *
 *-NOTES:
 *	May sleep.
 *
 *	Sleep locks are not recursive. A call to SLEEP_LOCK () attempting to
 *	acquire a lock that is currently held by the calling context will
 *	result in deadlock.
 *
 *	Driver-defined basic locks and read/write locks may not be held across
 *	calls to this function.
 *
 *	Driver-defined sleep locks may be held across calls to this function
 *	subject to the recursion restrictions described above.
 *
 *-SEE_ALSO:
 *	SLEEP_ALLOC (), SLEEP_DEALLOC (), SLEEP_LOCK_SIG (),
 *	SLEEP_LOCKAVAIL (), SLEEP_LOCKOWNED (), SLEEP_TRYLOCK (),
 *	SLEEP_UNLOCK ()
 */

#if	__USE_PROTO__
void (SLEEP_LOCK) (sleep_t * lockp, int priority)
#else
void
SLEEP_LOCK __ARGS ((lockp, priority))
sleep_t	      *	lockp;
int		priority;
#endif
{
	pl_t		prev_pl;

	ASSERT (lockp != NULL);
	ASSERT_BASE_LEVEL ();


	/*
	 * Take the path of least resistance; if the sleep lock is not
	 * currently held, then just acquire it and get out.
	 */

	if (ATOMIC_TEST_AND_SET_UCHAR (lockp->sl_locked) == 0) {
		/*
		 * We successfully acquired it! Just leave after writing in
		 * the info for SLEEP_LOCKAVAIL ().
		 */

		lockp->sl_holder = PROC_HANDLE ();
		return;
	}


	/*
	 * OK, we do it the hard way.
	 *
	 * The sleep lock might have been unlocked while we waited to lock
	 * the process list, so we retest the item.
	 */

	prev_pl = PLIST_LOCK (lockp->sl_plist, "SLEEP_LOCK");

	if (ATOMIC_TEST_AND_SET_UCHAR (lockp->sl_locked) != 0) {
		/*
		 * No, we need to wait. Note the cast to void... we assume
		 * that MAKE_SLEEPING () will filter out all improper attempts
		 * to wake us.
		 *
		 * We assert that whoever woke us up will have left the sleep
		 * locked in the "locked" state for us. While *we* won't have
		 * the process-list locked, someone else might so we can't
		 * make an assertion on that.
		 */

		(void) MAKE_SLEEPING (lockp->sl_plist, priority,
				      SLEEP_NO_SIGNALS);

		ASSERT (lockp->sl_holder == PROC_HANDLE ());
		ASSERT (ATOMIC_FETCH_UCHAR (lockp->sl_locked) != 0);

		(void) splx (prev_pl);
	} else {
		/*
		 * Write the owner information for SLEEP_LOCKOWNED () and
		 * release the test-and-set lock on the process list.
		 */

		lockp->sl_holder = PROC_HANDLE ();

		PLIST_UNLOCK (lockp->sl_plist, prev_pl);
	}
}


/*
 *-STATUS:
 *	DDI/DKI
 *
 *-NAME:
 *	SLEEP_LOCKAVAIL ()	Query whether a sleep lock is available.
 *
 *-SYNOPSIS:
 *	#include <sys/types.h>
 *	#include <sys/ksynch.h>
 *
 *	bool_t SLEEP_LOCKAVAIL (sleep_t * lockp);
 *
 *-ARGUMENTS:
 *	lockp		Pointer to the sleep lock to be queried.
 *
 *-DESCRIPTION:
 *	SLEEP_LOCKAVAIL () returns an indication of whether the sleep lock
 *	specified by "lockp" is currently available.
 *
 *	The state of the lock may change and the value returned may no longer
 *	be valid by the time the caller sees it. The caller is expected to
 *	understand that this is "stale data" and is either using it as a
 *	heuristic or has arranged for the data to be meaningful by other
 *	means.
 *
 *-RETURN VALUE:
 *	SLEEP_LOCKAVAIL () returns TRUE (a non-zero value) if the lock was
 *	available or FALSE (zero) if the lock was not available.
 *
 *-LEVEL:
 *	Base or Interrupt.
 *
 *-NOTES:
 *	Does not sleep.
 *
 *	Driver-defined basic locks, read/write locks and sleep locks may be
 *	held across calls to this function.
 *
 *-SEE_ALSO:
 *	SLEEP_ALLOC (), SLEEP_DEALLOC (), SLEEP_LOCK (), SLEEP_LOCK_SIG (),
 *	SLEEP_LOCKOWNED (), SLEEP_TRYLOCK (), SLEEP_UNLOCK ()
 */

#if	__USE_PROTO__
bool_t (SLEEP_LOCKAVAIL) (sleep_t * lockp)
#else
bool_t
SLEEP_LOCKAVAIL __ARGS ((lockp))
sleep_t	      *	lockp;
#endif
{
	ASSERT (lockp != NULL);

	return ATOMIC_FETCH_UCHAR (lockp->sl_locked) == 0;
}


/*
 *-STATUS:
 *	DDI/DKI
 *
 *-NAME:
 *	SLEEP_LOCKOWNED ()  Query whether a sleep lock is held by the caller.
 *
 *-SYNOPSIS:
 *	#include <sys/types.h>
 *	#include <sys/ksynch.h>
 *
 *	bool_t SLEEP_LOCKOWNED (sleep_t * lockp);
 *
 *-ARGUMENTS:
 *	lockp		Pointer to the sleep lock to be queried.
 *
 *-DESCRIPTION:
 *	SLEEP_LOCKOWNED () returns an indication of whether the sleep lock
 *	specified by "lockp" is held by the calling context.
 *
 *	SLEEP_LOCKOWNED () is intended for use only within ASSERT ()
 *	expressions [see ASSERT ()] and other code that is conditionally
 *	compiled under the DEBUG compilation option. The SLEEP_LOCKOWNED ()
 *	function is only defined under the DEBUG compilation option, and
 *	therefore calls to SLEEP_LOCKOWNED () will not compile when DEBUG is
 *	not defined.
 *
 *-RETURN VALUE:
 *	SLEEP_LOCKOWNED () returns TRUE (a non-zero value) if the lock is
 *	currently held by the calling context or FALSE (zero) if the lock is
 *	not currently held by the calling context.
 *
 *-LEVEL:
 *	Base or Interrupt.
 *
 *-NOTES:
 *	Does not sleep.
 *
 *	Driver-defined basic locks, read/write locks and sleep locks may be
 *	held across calls to this function.
 *
 *-SEE_ALSO:
 *	SLEEP_ALLOC (), SLEEP_DEALLOC (), SLEEP_LOCK (), SLEEP_LOCK_SIG (),
 *	SLEEP_LOCKAVAIL (), SLEEP_TRYLOCK (), SLEEP_UNLOCK ()
 */

#if	__USE_PROTO__
bool_t (SLEEP_LOCKOWNED) (sleep_t * lockp)
#else
bool_t
SLEEP_LOCKOWNED __ARGS ((lockp))
sleep_t	      *	lockp;
#endif
{
	ASSERT (lockp != NULL);

	return lockp->sl_holder == PROC_HANDLE ();
}


/*
 *-STATUS:
 *	DDI/DKI
 *
 *-NAME:
 *	SLEEP_LOCK_SIG ()	Acquire a sleep lock.
 *
 *-SYNOPSIS:
 *	#include <sys/types.h>
 *	#include <sys/ksynch.h>
 *
 *	bool_t SLEEP_LOCK_SIG (sleep_t * lockp, int priority);
 *
 *-ARGUMENTS:
 *	lockp		Pointer to the sleep lock to be acquired.
 *
 *	priority	A hint to the scheduling policy as to the relative
 *			priority the caller wishes to be assigned while
 *			running in the kernel after waking up. The valid
 *			values for this argument are as follows:
 *
 *			pridisk	    Priority appropriate for disk driver
 *			prinet	    Priority appropriate for network driver.
 *			pritty	    Priority appropriate for terminal driver.
 *			pritape     Priority appropriate for tape driver.
 *			prihi	    High priority.
 *			primed	    Medium priority.
 *			prilo	    Low priority.
 *
 *			Drivers may use these values to request a priority
 *			appropriate to a given type of device or to request a
 *			priority that is high, medium or low relative to other
 *			activities within the kernel.
 *
 *			It is also permissible to specify positive or negative
 *			offsets from the values defined above. Positive
 *			offsets result in favourable priority. The maximum
 *			allowable offset in all cases is 3 (eg. pridisk+3
 *			and pridisk-3 are valid values by pridisk+4 and
 *			pridisk-4 are not valid). Offsets can be useful in
 *			defining the relative importance of different locks or
 *			resources that may be hald by a given driver. In
 *			general, a higher relative priority should be used
 *			when the caller is attempting to acquire a highly-
 *			contended lock or resource, or when the caller is
 *			already holding one or more locks or kernel resources
 *			upon entry to SLEEP_LOCK_SIG ().
 *
 *			The exact semantics of the "priority" argument to the
 *			scheduling class of the caller, and some scheduling
 *			classes may choose to ignore the argument for the
 *			purposes of assigning a scheduling priority.
 *
 *-DESCRIPTION:
 *	SLEEP_LOCK_SIG () acquires the sleep lock specified by "lockp". If the
 *	lock is not immediately available, the caller is put to sleep (the
 *	caller's execution is suspended and other processes may be scheduled)
 *	until the lock becomes available to the caller, at which point the
 *	caller wakes up and returns with the lock held.
 *
 *	SLEEP_LOCK_SIG () may be interrupted by a signal, in which case it may
 *	return early without acquiring the lock.
 *
 *	If the function is interrupted by a job control stop signal (eg
 *	SIGSTOP, SIGTSTP, SIGTTIN, SIGTTOU) which results in the caller
 *	entering a stopped state, the SLEEP_LOCK_SIG () function will
 *	transparently retry the lock operation upon continuing (the caller
 *	will not return without the lock).
 *
 *	If the function is interrupted by a signal other than a job control
 *	signal, or by a job control signal that does not result in the caller
 *	stopping (because the signal has a non-default disposition), the
 *	SLEEP_LOCK_SIG () function will return early without acquiring the
 *	lock.
 *
 *-RETURN VALUE:
 *	SLEEP_LOCK_SIG () returns TRUE (a non-zero value) if the lock is
 *	successfully acquired or FALSE (zero) if the function returned early
 *	because of a signal.
 *
 *-LEVEL:
 *	Base level only.
 *
 *-NOTES:
 *	May sleep.
 *
 *	Sleep locks are not recursive. A call to SLEEP_LOCK_SIG () attempting
 *	to acquire a lock that is currently held by the calling context will
 *	result in deadlock.
 *
 *	Driver-defined basic locks and read/write locks may not be held across
 *	calls to this function.
 *
 *	Driver-defined sleep locks may be held across calls to this function
 *	subject to the recursion restrictions described above.
 *
 *-SEE_ALSO:
 *	SLEEP_ALLOC (), SLEEP_DEALLOC (), SLEEP_LOCK (), SLEEP_LOCKAVAIL (),
 *	SLEEP_LOCKOWNED (), SLEEP_TRYLOCK (), SLEEP_UNLOCK (), signals
 */

#if	__USE_PROTO__
bool_t (SLEEP_LOCK_SIG) (sleep_t * lockp, int priority)
#else
bool_t
SLEEP_LOCK_SIG __ARGS ((lockp, priority))
sleep_t	      *	lockp;
int		priority;
#endif
{
	pl_t		prev_pl;

	ASSERT (lockp != NULL);
	ASSERT_BASE_LEVEL ();

	/*
	 * Take the path of least resistance; if the sleep lock is not
	 * currently held, then just acquire it and get out.
	 */

	if (ATOMIC_TEST_AND_SET_UCHAR (lockp->sl_locked) == 0) {
		/*
		 * We successfully acquired it! Just leave after writing in
		 * the info for SLEEP_LOCKAVAIL ().
		 */

		lockp->sl_holder = PROC_HANDLE ();
		return TRUE;
	}


	/*
	 * OK, we do it the hard way.
	 *
	 * The sleep lock might have been unlocked while we waited to lock
	 * the process list, so we retest that member.
	 */

	prev_pl = PLIST_LOCK (lockp->sl_plist, "SLEEP_LOCK");

	if (ATOMIC_TEST_AND_SET_UCHAR (lockp->sl_locked) != 0) {
		/*
		 * No, we need to wait.
		 */

		if (MAKE_SLEEPING (lockp->sl_plist, priority,
				   SLEEP_INTERRUPTIBLE) == PROCESS_SIGNALLED) {
			/*
			 * We were signalled. MAKE_SLEEPING () will have
			 * released the test-and-set lock on the process list,
			 * so we just have to reset the interrupt priority
			 * level and return an indication to the caller.
			 */

			(void) splx (prev_pl);

			return FALSE;
		}

		/*
		 * We assert that whoever woke us up will have left the sleep
		 * locked in the "locked" state for us. While *we* won't have
		 * the process-list locked, someone else might so we can't
		 * make an assertion on that.
		 */

		ASSERT (lockp->sl_holder == PROC_HANDLE ());
		ASSERT (ATOMIC_FETCH_UCHAR (lockp->sl_locked) != 0);

		(void) splx (prev_pl);
	} else {
		/*
		 * Write the owner information for SLEEP_LOCKAVAIL () and
		 * release the test-and-set lock on the process list.
		 */

		lockp->sl_holder = PROC_HANDLE ();

		PLIST_UNLOCK (lockp->sl_plist, prev_pl);
	}

	return TRUE;
}


/*
 *-STATUS:
 *	DDI/DKI
 *
 *-NAME:
 *	SLEEP_TRYLOCK ()	Try to acquire a sleep lock.
 *
 *-SYNOPSIS:
 *	#include <sys/types.h>
 *	#include <sys/ksynch.h>
 *
 *	bool_t SLEEP_TRYLOCK (sleep_t * lockp);
 *
 *-ARGUMENTS:
 *	lockp		Pointer to the sleep lock to be acquired.
 *
 *-DESCRIPTION:
 *	If the lock specified by "lockp" is immediately available (can be
 *	acquired without sleeping) SLEEP_TRYLOCK () acquires the lock. If the
 *	lock is not immediately available, the function returns without
 *	acquiring the lock.
 *
 *-RETURN VALUE:
 *	SLEEP_TRYLOCK () returns TRUE (a non-zero value) if the lock is
 *	successfully acquired or FALSE (zero) if the lock is not acquired.
 *
 *-LEVEL:
 *	Base or interrupt.
 *
 *-NOTES:
 *	Does not sleep.
 *
 *	Driver-defined basic locks, read/write locks and sleep locks may be
 *	held across calls to this function.
 *
 *-SEE_ALSO:
 *	SLEEP_ALLOC (), SLEEP_DEALLOC (), SLEEP_LOCK (), SLEEP_LOCK_SIG (),
 *	SLEEP_LOCKAVAIL (), SLEEP_LOCKOWNED (), SLEEP_UNLOCK ()
 */

#if	__USE_PROTO__
bool_t (SLEEP_TRYLOCK) (sleep_t * lockp)
#else
bool_t
SLEEP_TRYLOCK __ARGS ((lockp))
sleep_t	      *	lockp;
#endif
{
	ASSERT (lockp != NULL);

	if (ATOMIC_TEST_AND_SET_UCHAR (lockp->sl_locked) == 0) {
		lockp->sl_holder = PROC_HANDLE ();
		return TRUE;
	}

	return FALSE;
}


/*
 *-STATUS:
 *	DDI/DKI
 *
 *-NAME:
 *	SLEEP_UNLOCK ()	Release a sleep lock.
 *
 *-SYNOPSIS:
 *	#include <sys/ksynch.h>
 *
 *	void SLEEP_UNLOCK (sleep_t * lockp);
 *
 *-ARGUMENTS:
 *	lockp		Pointer to the sleep lock to be released.
 *
 *-DESCRIPTION:
 *	SLEEP_UNLOCK () releases the sleep lock specified by "lockp". If there
 *	are processes waiting for the lock, one of the waiting processes is
 *	awakened.
 *
 *-RETURN VALUE:
 *	None.
 *
 *-LEVEL:
 *	Base or interrupt.
 *
 *-NOTES:
 *	Does not sleep.
 *
 *	Driver-defined basic locks, read/write locks and sleep locks may be
 *	held across calls to this function.
 *
 *-SEE_ALSO:
 *	SLEEP_ALLOC (), SLEEP_DEALLOC (), SLEEP_LOCK (), SLEEP_LOCK_SIG (),
 *	SLEEP_LOCKAVAIL (), SLEEP_LOCKOWNED (), SLEEP_TRYLOCK ()
 */

#if	__USE_PROTO__
void (SLEEP_UNLOCK) (sleep_t * lockp)
#else
void
SLEEP_UNLOCK __ARGS ((lockp))
sleep_t	      *	lockp;
#endif
{
	pl_t		prev_pl;

	/*
	 * Make some assertions. Note that we *don't* assert that our
	 * PROC_HANDLE () matches "sl_holder", since thanks to
	 * SLEEP_TRYLOCK () allowing interrupt contexts to acquire sleep
	 * locks we can't rely on that.
	 */

	ASSERT (lockp != NULL);
	ASSERT (ATOMIC_FETCH_UCHAR (lockp->sl_locked));


	/*
	 * When we are unlocking the sleep lock, we must lock the process
	 * list so we can test whether there are any waiting processes to
	 * be given the lock.
	 */

	prev_pl = PLIST_LOCK (lockp->sl_plist, "SLEEP_UNLOCK");


	/*
	 * If there are no waiters, we unlock the sleep lock, otherwise we
	 * wake the first waiting process and leave the lock in the locked
	 * state for the process to simply take over from us later.
	 *
	 * In order to cross-check things with the sleep function, we write
	 * the expected processes' identity into the lock.
	 */

	if ((lockp->sl_holder = WAKE_ONE (lockp->sl_plist)) == NULL) {
		/*
		 * No waiting processes, give the lock away.
		 */

		ATOMIC_CLEAR_UCHAR (lockp->sl_locked);
	}

	PLIST_UNLOCK (lockp->sl_plist, prev_pl);
}


/*
 *-STATUS:
 *	DDI/DKI
 *
 *-NAME:
 *	SV_ALLOC ()	Allocate and initialize a synchronization variable.
 *
 *-SYNOPSIS:
 *	#include <sys/kmem.h>
 *	#include <sys/ksynch.h>
 *
 *	sv_t * SV_ALLOC (int flag);
 *
 *-ARGUMENTS:
 *	flag		Specifies whether the caller is willing to sleep
 *			waiting for memory. If "flag" is set to KM_SLEEP, the
 *			caller will sleep if necessary until sufficient memory
 *			is available. If "flag" is set to KM_NOSLEEP, the
 *			caller will not sleep, but SLEEP_ALLOC () will return
 *			NULL if sufficient memory is not immediately
 *			available.
 *
 *-DESCRIPTION:
 *	SV_ALLOC () dynamically allocates and initialises an instance of a
 *	synchronization variable.
 *
 *-RETURN VALUE:
 *	Upon successful completion, SV_ALLOC () returns a pointer to the newly
 *	allocated synchronization variable. If KM_NOSLEEP is specified and
 *	sufficient memory is not immediately available, SV_ALLOC () returns a
 *	NULL pointer.
 *
 *-LEVEL:
 *	Base only if "flag" is set to KM_SLEEP. Base or Interrupt if "flag" is
 *	set to KM_NOSLEEP.
 *
 *-NOTES:
 *	May sleep if "flag" is set to KM_NOSLEEP.
 *
 *	Driver-defined basic locks and read/write locks may be held across
 *	calls to this function if "flag" is set to KM_NOSLEEP but may not be
 *	held if "flag" is KM_SLEEP.
 *
 *	Driver-defined sleep locks may be held across calls to this function
 *	regardless of the value of "flag".
 *
 *-SEE_ALSO:
 *	SV_BROADCAST (), SV_DEALLOC (), SV_SIGNAL (), SV_WAIT (),
 *	SV_WAIT_SIG ()
 */

#if	__USE_PROTO__
sv_t * (SV_ALLOC) (int flag)
#else
sv_t *
SV_ALLOC __ARGS ((flag))
int		flag;
#endif
{
	sv_t	      *	svp;

	ASSERT (flag == KM_SLEEP || flag == KM_NOSLEEP);

	/*
	 * Allocate and initialise the data, possibly waiting for enough
	 * memory to become available.
	 */

	if ((svp = (sv_t *) _lock_malloc (sizeof (* svp),
					  flag)) != NULL) {
		INIT_LNODE (& svp->sv_node, NULL, & synch_vars, flag);
		PLIST_INIT (svp->sv_plist);
	}

	return svp;
}


/*
 *-STATUS:
 *	DDI/DKI
 *
 *-NAME:
 *	SV_BROADCAST ()	Wake up all processes sleeping on a synchronization
 *			variable.
 *
 *-SYNOPSIS:
 *	#include <sys/ksynch.h>
 *
 *	void * SV_BROADCAST (sv_t * svp, int flags);
 *
 *-ARGUMENTS:
 *	svp		Pointer to the synchronization variable to be
 *			broadcast signalled.
 *
 *	flags		Bit field for flags. No flags are defined for use in
 *			drivers and the "flags" argument must be set to zero.
 *
 *-DESCRIPTION:
 *	If one or more processes are blocked on the synchronization variable
 *	specified by "svp", SV_BROADCAST () wakes up all of the blocked
 *	processes. Note that synchronization variables are stateless, and
 *	therefore calls to SV_BROADCAST () only affect processes currently
 *	blocked on the synchronization variable and have not effect on
 *	processes that block on the synchronization variable at a later time.
 *
 *-RETURN VALUE:
 *	None.
 *
 *-LEVEL:
 *	Base or interrupt.
 *
 *-NOTES:
 *	Does not sleep.
 *
 *	Driver-defined basic locks and read/write locks may be held across
 *	calls to this function if "flag" is set to KM_NOSLEEP but may not be
 *	held if "flag" is KM_SLEEP.
 *
 *	Driver-defined basic locks, read/write locks and sleep locks may be
 *	held across calls to this function.
 *
 *-SEE_ALSO:
 *	SV_ALLOC (), SV_DEALLOC (), SV_SIGNAL (), SV_WAIT (), SV_WAIT_SIG ()
 */

#if	__USE_PROTO__
void (SV_BROADCAST) (sv_t * svp, int flags)
#else
void
SV_BROADCAST __ARGS ((svp, flags))
sv_t	      *	svp;
int		flags;
#endif
{
	pl_t		prev_pl;

	ASSERT (flags == 0);
	ASSERT (svp != NULL);

	/*
	 * Lock the list (required by WAKE_ALL ()), wake the sleepers, and
	 * unlock the list.
	 */

	prev_pl = PLIST_LOCK (svp->sv_plist, "SV_BROADCAST");

	WAKE_ALL (svp->sv_plist);

	PLIST_UNLOCK (svp->sv_plist, prev_pl);
}


/*
 *-STATUS:
 *	DDI/DKI
 *
 *-NAME:
 *	SV_DEALLOC ()	Deallocate an instance of a synchronization variable.
 *
 *-SYNOPSIS:
 *	#include <sys/ksynch.h>
 *
 *	void SV_DEALLOC (sv_t * svp);
 *
 *-ARGUMENTS:
 *	svp		Pointer to the synchronization variable to be
 *			deallocated.
 *
 *-DESCRIPTION:
 *	SV_DEALLOC () deallocates the synchronization variable specified by
 *	"svp".
 *
 *-RETURN VALUE:
 *	None.
 *
 *-LEVEL:
 *	Base or Interrupt.
 *
 *-NOTES:
 *	Does not sleep.
 *
 *	Driver-defined basic locks, read/write locks, and sleep locks may be
 *	held across calls to this function.
 *
 *-SEE_ALSO:
 *	SV_ALLOC (), SV_BROADCAST (), SV_SIGNAL (), SV_WAIT (), SV_WAIT_SIG ()
 */

#if	__USE_PROTO__
void (SV_DEALLOC) (sv_t * svp)
#else
void
SV_DEALLOC __ARGS ((svp))
sv_t	      *	svp;
#endif
{
	ASSERT (svp != NULL);

	/*
	 * Remove from the list of all synchronization variables and free any
	 * statistics buffer space before freeing the lock itself.
	 */

	PLIST_DESTROY (svp->sv_plist);

	FREE_LNODE (& svp->sv_node, & synch_vars);

	_lock_free (svp, sizeof (* svp));
}


/*
 *-STATUS:
 *	DDI/DKI
 *
 *-NAME:
 *	SV_SIGNAL ()	Wake up one process sleeping on a synchronization
 *			variable.
 *
 *-SYNOPSIS:
 *	#include <sys/ksynch.h>
 *
 *	void SV_SIGNAL (sv_t * svp, int flags);
 *
 *-ARGUMENTS:
 *	svp		Pointer to the synchronization variable to be
 *			signalled.
 *
 *	flags		Bit field for flags. No flags are defined for use in
 *			drivers and the "flags" argument must be set to zero.
 *
 *-DESCRIPTION:
 *	If one or more processes are blocked on the synchronization variable
 *	specified by "svp", SV_SIGNAL () wakes up a single blocked process.
 *	Note that synchronization variables are stateless, and therefore
 *	calls to SV_SIGNAL only affect processes currently blocked on the
 *	synchronization variable and have no effect on processes that block on
 *	the synchronization variable at a later time.
 *
 *-RETURN VALUE:
 *	None.
 *
 *-LEVEL:
 *	Base or Interrupt.
 *
 *-NOTES:
 *	Does not sleep.
 *
 *	Driver-defined basic locks, read/write locks, and sleep locks may be
 *	held across calls to this function.
 *
 *-SEE_ALSO:
 *	SV_ALLOC (), SV_BROADCAST (), SV_DEALLOC (), SV_WAIT (),
 *	SV_WAIT_SIG ()
 */

#if	__USE_PROTO__
void (SV_SIGNAL) (sv_t * svp, int flags)
#else
void
SV_SIGNAL __ARGS ((svp, flags))
sv_t	      *	svp;
int		flags;
#endif
{
	pl_t		prev_pl;

	ASSERT (flags == 0);
	ASSERT (svp != NULL);

	/*
	 * Lock the list (required by WAKE_ONE ()), wake a sleeper, and
	 * unlock the list.
	 */

	prev_pl = PLIST_LOCK (svp->sv_plist, "SV_SIGNAL");

	(void) WAKE_ONE (svp->sv_plist);

	PLIST_UNLOCK (svp->sv_plist, prev_pl);
}


/*
 *-STATUS:
 *	DDI/DKI
 *
 *-NAME:
 *	SV_WAIT ()	Sleep on a synchronization variable.
 *
 *-SYNOPSIS:
 *	#include <sys/types.h>
 *	#include <sys/ksynch.h>
 *
 *	void SV_WAIT (sv_t * svp, int priority, lock_t * lkp);
 *
 *-ARGUMENTS:
 *	svp		Pointer to the synchronization variable on which to
 *			sleep.
 *
 *	priority	A hint to the scheduling policy as to the relative
 *			priority the caller wishes to be assigned while
 *			running in the kernel after waking up. The valid
 *			values for this argument are as follows:
 *
 *			pridisk	    Priority appropriate for disk driver
 *			prinet	    Priority appropriate for network driver.
 *			pritty	    Priority appropriate for terminal driver.
 *			pritape     Priority appropriate for tape driver.
 *			prihi	    High priority.
 *			primed	    Medium priority.
 *			prilo	    Low priority.
 *
 *			Drivers may use these values to request a priority
 *			appropriate to a given type of device or to request a
 *			priority that is high, medium or low relative to other
 *			activities within the kernel.
 *
 *			It is also permissible to specify positive or negative
 *			offsets from the values defined above. Positive
 *			offsets result in favourable priority. The maximum
 *			allowable offset in all cases is 3 (eg. pridisk+3
 *			and pridisk-3 are valid values by pridisk+4 and
 *			pridisk-4 are not valid). Offsets can be useful in
 *			defining the relative importance of different locks or
 *			resources that may be hald by a given driver. In
 *			general, a higher relative priority should be used
 *			when the caller is attempting to acquire a highly-
 *			contended lock or resource, or when the caller is
 *			already holding one or more locks or kernel resources
 *			upon entry to SV_WAIT ().
 *
 *			The exact semantics of the "priority" argument is
 *			specific to the scheduling class of the caller, and
 *			some scheduling classes may choose to ignore the
 *			argument for the purposes of assigning a scheduling
 *			priority.
 *
 *	lkp		Pointer to a basic lock which must be locked when
 *			SV_WAIT () is called. The basic lock is released when
 *			the calling process goes to sleep, as described below.
 *-DESCRIPTION:
 *	SV_WAIT () causes the calling process to go to sleep (the caller's
 *	execution is suspended and other processes may be scheduled) waiting
 *	for a call to SV_SIGNAL () or SV_BROADCAST () for the synchronization
 *	variable specified by "svp".
 *
 *	The basic lock specified by "lkp" must be held by the caller upon
 *	entry. The lock is released and the interrupt priority level is set to
 *	plbase after the process is queued on the synchronization variable but
 *	prior to switching context switching to another process. When the
 *	caller returns from SV_WAIT () the basic lock is not held and the
 *	interrupt priority level is equal to plbase.
 *
 *	The caller will not be interrupted by signals while sleeping inside
 *	SV_WAIT ().
 *
 *-RETURN VALUE:
 *	None.
 *
 *-LEVEL:
 *	Base level only.
 *
 *-NOTES:
 *	May sleep.
 *
 *	Driver-defined basic locks and read/write locks may not be held across
 *	calls to this function.
 *
 *	Driver-defined sleep locks may be held across calls to this function.
 *
 *-SEE_ALSO:
 *	SV_ALLOC (), SV_BROADCAST (), SV_DEALLOC (), SV_SIGNAL (),
 *	SV_WAIT_SIG ()
 */

#if	__USE_PROTO__
void (SV_WAIT) (sv_t * svp, int priority, lock_t * lkp)
#else
void
SV_WAIT __ARGS ((svp, priority, lkp))
sv_t	      *	svp;
int		priority;
lock_t	      *	lkp;
#endif
{
	ASSERT (svp != NULL);
	ASSERT (lkp != NULL);
	ASSERT_BASE_LEVEL ();

	/*
	 * First off, we have to lock the process list. After this is done we
	 * can safely release the client's basic lock, since once we have our
	 * lock the rest of the wait operation will proceed atomically.
	 */

	(void) PLIST_LOCK (svp->sv_plist, "SV_WAIT");

	UNLOCK (lkp, plhi);

	(void) MAKE_SLEEPING (svp->sv_plist, priority, SLEEP_NO_SIGNALS);

	(void) splbase ();		/* let's make sure... */
}


/*
 *-STATUS:
 *	DDI/DKI
 *
 *-NAME:
 *	SV_WAIT_SIG ()	Sleep on a synchronization variable.
 *
 *-SYNOPSIS:
 *	#include <sys/types.h>
 *	#include <sys/ksynch.h>
 *
 *	bool_t SV_WAIT_SIG (sv_t * svp, int priority, lock_t * lkp);
 *
 *-ARGUMENTS:
 *	svp		Pointer to the synchronization variable on which to
 *			sleep.
 *
 *	priority	A hint to the scheduling policy as to the relative
 *			priority the caller wishes to be assigned while
 *			running in the kernel after waking up. The valid
 *			values for this argument are as follows:
 *
 *			pridisk	    Priority appropriate for disk driver
 *			prinet	    Priority appropriate for network driver.
 *			pritty	    Priority appropriate for terminal driver.
 *			pritape     Priority appropriate for tape driver.
 *			prihi	    High priority.
 *			primed	    Medium priority.
 *			prilo	    Low priority.
 *
 *			Drivers may use these values to request a priority
 *			appropriate to a given type of device or to request a
 *			priority that is high, medium or low relative to other
 *			activities within the kernel.
 *
 *			It is also permissible to specify positive or negative
 *			offsets from the values defined above. Positive
 *			offsets result in favourable priority. The maximum
 *			allowable offset in all cases is 3 (eg. pridisk+3
 *			and pridisk-3 are valid values by pridisk+4 and
 *			pridisk-4 are not valid). Offsets can be useful in
 *			defining the relative importance of different locks or
 *			resources that may be hald by a given driver. In
 *			general, a higher relative priority should be used
 *			when the caller is attempting to acquire a highly-
 *			contended lock or resource, or when the caller is
 *			already holding one or more locks or kernel resources
 *			upon entry to SV_WAIT_SIG ().
 *
 *			The exact semantics of the "priority" argument is
 *			specific to the scheduling class of the caller, and
 *			some scheduling classes may choose to ignore the
 *			argument for the purposes of assigning a scheduling
 *			priority.
 *
 *	lkp		Pointer to a basic lock which must be locked when
 *			SV_WAIT_SIG () is called. The basic lock is released
 *			when the calling process goes to sleep, as described
 *			below.
 *
 *-DESCRIPTION:
 *	SV_WAIT_SIG () causes the calling process to go to sleep (the caller's
 *	execution is suspended and other processes may be scheduled) waiting
 *	for a call to SV_SIGNAL () or SV_BROADCAST () for the synchronization
 *	variable specified by "svp".
 *
 *	The basic lock specified by "lkp" must be held by the caller upon
 *	entry. The lock is released and the interrupt priority level is set to
 *	plbase after the process is queued on the synchronization variable but
 *	prior to switching context switching to another process. When the
 *	caller returns from SV_WAIT_SIG () the basic lock is not held and the
 *	interrupt priority level is equal to plbase.
 *
 *	SV_WAIT_SIG () may be interrupted by a signal, in which case it will
 *	return early without waiting for a call to SV_SIGNAL () or
 *	SV_BROADCAST ().
 *
 *	If the function is interrupted by a job control signal (eg SIGSTOP,
 *	SIGTSTP, SIGTTIN, SIGTTOU) which results in the caller entering a
 *	stopped state, when continued the SV_WAIT_SIG () function will return
 *	TRUE as if the process had been awakened by a call to SV_SIGNAL () or
 *	SV_BROADCAST ().
 *
 *	If the caller is interrupted by a signal other than a job control
 *	signal, or by a job control signal that does not result in the caller
 *	stopping (because the signal has a non-default disposition), the
 *	SV_WAIT_SIG () call will return FALSE.
 *
 *-RETURN VALUE:
 *	SV_WAIT_SIG () returns TRUE (a non-zero value) if the caller woke up
 *	because of a call to SV_SIGNAL () or SV_BROADCAST (), or if the caller
 *	was stopped and subsequently continued. SV_WAIT_SIG () returns FALSE
 *	(zero) if the caller woke up and returned early because of a signal
 *	other than a job control stop signal, or by a job control signal that
 *	did not result in the caller stopping because the signal has a non-
 *	default disposition.
 *
 *-LEVEL:
 *	Base level only.
 *
 *-NOTES:
 *	May sleep.
 *
 *	Driver-defined basic locks and read/write locks may not be held across
 *	calls to this function.
 *
 *	Driver-defined sleep locks may be held across calls to this function.
 *
 *-SEE_ALSO:
 *	SV_ALLOC (), SV_BROADCAST (), SV_DEALLOC (), SV_SIGNAL (), SV_WAIT ()
 */

#if	__USE_PROTO__
bool_t (SV_WAIT_SIG) (sv_t * svp, int priority, lock_t * lkp)
#else
bool_t
SV_WAIT_SIG __ARGS ((svp, priority, lkp))
sv_t	      *	svp;
int		priority;
lock_t	      *	lkp;
#endif
{
	bool_t		not_signalled;

	ASSERT (svp != NULL);
	ASSERT (lkp != NULL);
	ASSERT_BASE_LEVEL ();

	/*
	 * First off, we have to lock the process list. After this is done we
	 * can safely release the client's basic lock, since once we have our
	 * lock the rest of the wait operation will proceed atomically.
	 */

	(void) PLIST_LOCK (svp->sv_plist, "SV_WAIT_SIG");

	UNLOCK (lkp, plhi);

	not_signalled = MAKE_SLEEPING (svp->sv_plist, priority,
				       SLEEP_INTERRUPTIBLE) != PROCESS_SIGNALLED;

	(void) splbase ();	/* let's make sure */

	return not_signalled;
}


/*
 * This function exercises some of the most basic facilities of the locking
 * system. It is difficult to assure that the code presented in this file
 * will work correctly, because the nature of the service provided is a
 * temporal guarantee that can't be verified without performing arbitrary
 * fine-grained interleavings of at least two execution paths through each
 * combination of interacting functions.
 *
 * So, the best we can do here in a portable fashion is to present some tests
 * of the observable properties of the above code, and to (hopefully) exercise
 * the ASSERT () statements in the above for some minimal self-checking.
 *
 * One problem with the use of ASSERT () tests is that it's difficult to
 * build negative tests. The argument to this function indicates a negative
 * tests that should be performed, presumably in conjunction with some test
 * script capable of verifying an exception report against a previous run.
 *
 * Our tests use a lock interrupt level appropriate to the environment; in
 * user-mode tests under Coherent, disabling interrupts is prohibited
 */

#if	__COHERENT__
# define	test_pl		plbase
#else
# define	test_pl		plhi
#endif

#if	__USE_PROTO__
int (LOCK_TESTS) (int negative)
#else
int
LOCK_TESTS __ARGS ((negative))
int		negative;
#endif
{
	pl_t		prev_pl;

	lock_t	      *	basic_lock;
	rwlock_t      *	rw_lock;
	sleep_t	      *	sleep_lock;
	sv_t	      *	synch_var;

	static lkinfo_t	basic_info = { "test basic lock" };
	static lkinfo_t	rw_info = { "test read/write lock" };
	static lkinfo_t	sleep_info = { "test sleep lock" };

	/*
	 * Allocate some locks that we can play with...
	 */

	basic_lock = LOCK_ALLOC (32, test_pl, & basic_info, KM_SLEEP);
	rw_lock = RW_ALLOC (10, test_pl, & rw_info, KM_SLEEP);
	sleep_lock = SLEEP_ALLOC (0, & sleep_info, KM_SLEEP);
	synch_var = SV_ALLOC (KM_SLEEP);


	/*
	 * Basic locks.
	 */

	if ((prev_pl = TRYLOCK (basic_lock, test_pl)) == invpl ||
	    TRYLOCK (basic_lock, test_pl) != invpl)
		return -1;


	UNLOCK (basic_lock, prev_pl);

	prev_pl = LOCK (basic_lock, test_pl);

	if (TRYLOCK (basic_lock, test_pl) != invpl)
		return -1;

	UNLOCK (basic_lock, prev_pl);

	/*
	 * Read/write locks.
	 */

	if ((prev_pl = RW_TRYRDLOCK (rw_lock, test_pl)) == invpl ||
	    RW_TRYRDLOCK (rw_lock, test_pl) == invpl ||
	    RW_TRYWRLOCK (rw_lock, test_pl) != invpl)
		return -1;

	(void) RW_RDLOCK (rw_lock, test_pl);

	if (RW_TRYWRLOCK (rw_lock, test_pl) != invpl)
		return -1;

	RW_UNLOCK (rw_lock, prev_pl);
	RW_UNLOCK (rw_lock, prev_pl);
	RW_UNLOCK (rw_lock, prev_pl);


	if ((prev_pl = RW_TRYWRLOCK (rw_lock, test_pl)) == invpl ||
	    RW_TRYWRLOCK (rw_lock, test_pl) != invpl ||
	    RW_TRYRDLOCK (rw_lock, test_pl) != invpl)
		return -1;

	RW_UNLOCK (rw_lock, prev_pl);


	prev_pl = RW_WRLOCK (rw_lock, test_pl);

	if (RW_TRYWRLOCK (rw_lock, test_pl) != invpl ||
	    RW_TRYRDLOCK (rw_lock, test_pl) != invpl)
		return -1;

	RW_UNLOCK (rw_lock, prev_pl);


	/*
	 * Sleep locks.
	 */

	if (SLEEP_LOCKOWNED (sleep_lock) == TRUE ||
	    SLEEP_LOCKAVAIL (sleep_lock) == FALSE ||
	    SLEEP_TRYLOCK (sleep_lock) == FALSE ||
	    SLEEP_LOCKOWNED (sleep_lock) == FALSE ||
	    SLEEP_LOCKAVAIL (sleep_lock) == TRUE ||
	    SLEEP_TRYLOCK (sleep_lock) == TRUE)
		return -1;

	SLEEP_UNLOCK (sleep_lock);

	if (SLEEP_LOCKOWNED (sleep_lock) == TRUE ||
	    SLEEP_LOCKAVAIL (sleep_lock) == FALSE ||
	    SLEEP_LOCK_SIG (sleep_lock, prilo) == FALSE ||
	    SLEEP_LOCKOWNED (sleep_lock) == FALSE ||
	    SLEEP_LOCKAVAIL (sleep_lock) == TRUE ||
	    SLEEP_TRYLOCK (sleep_lock) == TRUE)
		return -1;

	SLEEP_UNLOCK (sleep_lock);

	SLEEP_LOCK (sleep_lock, prilo);

	if (SLEEP_LOCKOWNED (sleep_lock) == FALSE ||
	    SLEEP_LOCKAVAIL (sleep_lock) == TRUE ||
	    SLEEP_TRYLOCK (sleep_lock) == TRUE)
		return -1;

	SLEEP_UNLOCK (sleep_lock);


	/*
	 * Synchronisation variables: this is impossible to test without
	 * access to timeout functions. Once DDI/DKI timeout functions are
	 * available, we can use those to at least begin to exercise the
	 * notion of synchronization.
	 */


	/*
	 * Negative testing... need to add this!
	 */

	/*
	 * Clean up and bail out.
	 */

	LOCK_DEALLOC (basic_lock);
	RW_DEALLOC (rw_lock);
	SLEEP_DEALLOC (sleep_lock);
	SV_DEALLOC (synch_var);

	return 0;
}
