#ifndef	__COMMON__WAIT_H__
#define	__COMMON__WAIT_H__

/*
 * This internal header file is intended as the sole point of definition for
 * the manifest constants and macros related to the POSIX.1 wait () system
 * call. The definitions of these constants and macros is kept here to avoid
 * entanglement in namespace-reservation issues.
 *
 * Note that the POSIX.1 standard does not require that the W... contants be
 * integral constant expressions suitable for use in #if-expressions, so we
 * use that freedom to built enumerations instead.
 */

enum {
	__WSIGMASK	= 0x7F,
	__WSTOPFLG	= 0x7F,
	__WCOREFLG	= 0x80,

	__WEXITED	= 1,
	__WTRAPPED	= 2,
	__WCONTINUED	= 8,

	__WUNTRACED	= 4,
	__WNOHANG	= 64,
	__WNOWAIT	= 128
};

#define	__WCONTFLAG	0xFFFF

#define	__WIFCONTINUED(stat)	(((stat) & __WCONTFLAG) == __WCONTFLAG)
#define	__WCOREDUMP(stat)	((stat) & __WCOREFLG)

#define	__WLOBYTE(stat)		((stat) & 0xFF)
#define	__WHIBYTE(stat)		(((stat) >> 8) & 0xFF)

#define	__WIFEXITED(stat)	(__WLOBYTE (stat) == 0)
#define	__WEXITSTATUS(stat)	(__WHIBYTE (stat))

#define	__WIFSIGNALED(stat)	(__WLOBYTE (stat) > 0 && __WHIBYTE (stat) == 0)
#define	__WTERMSIG(stat)	(__WLOBYTE (stat) & __WSIGMASK)

#define	__WIFSTOPPED(stat)	(__WLOBYTE (stat) == __WSTOPFLG && \
				 __WHIBYTE (stat) != 0)
#define	__WSTOPSIG(stat)	(__WHIBYTE (stat))

#endif	/* ! defined (__COMMON__WAIT_H__) */

