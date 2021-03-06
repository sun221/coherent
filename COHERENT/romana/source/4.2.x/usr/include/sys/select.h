/* (-lgl
 *	Coherent 386 release 4.2
 *	Copyright (c) 1982, 1993 by Mark Williams Company.
 *	All rights reserved. May not be copied without permission.
 *	For copying permission and licensing info, write licensing@mwc.com
 -lgl) */

#ifndef __SYS_SELECT_H__
#define __SYS_SELECT_H__

/*
 * Definitions suitable for an emulation of the BSD select () call via
 * poll ().  Note that the definitions in this header are not part of iBCS2
 * or the SVR4 ABI, so that while they match documented practice the exact
 * contents may differ from other implementations.
 */

#include <common/ccompat.h>
#include <common/__types.h>
#include <common/_limits.h>
#include <common/_tricks.h>
#include <common/_timestr.h>

/*
 * We #include <string.h> because FD_ZERO () requires access to memset ().
 */

#include <string.h>


/*
 * Note that the value of FD_SETSIZE is allowed to be set by applications as a
 * matter of historical practice; the default given here is merely a default
 * (albeit sufficiently large for most purposes). This practical maximum is
 * hard-wired into select ().
 */

#ifndef	FD_SETSIZE
# define	FD_SETSIZE	256
#endif

typedef struct __fd_set {
	__ulong_t	_fd_set [__DIVIDE_ROUNDUP_CONST (FD_SETSIZE,
				  __CHAR_BIT * sizeof (__ulong_t))];
} fd_set;

#define	__FD_MASK(b)		(1 << ((b) & (__LONG_BIT - 1)))

#define	FD_ZERO(fdp)		memset (fdp, 0, sizeof (* (fdp)))
#define FD_SET(b, fdp)		((fdp)->_fd_set [(b) / __LONG_BIT] |= \
				 __FD_MASK (b))
#define	FD_CLR(b, fdp)		((fdp)->_fd_set [(b) / __LONG_BIT] &= \
				 ~ __FD_MASK (b))
#define FD_ISSET(b,fdp)		(((fdp)->_fd_set [(b) / __LONG_BIT] & \
				  __FD_MASK (b)) != 0)

__EXTERN_C_BEGIN__

int		select		__PROTO ((int _nfds, fd_set * _readmask,
					  fd_set * _writemask,
					  fd_set * _exceptmask,
					  __timestruc_t * _timeout));

__EXTERN_C_END__

#endif	/* ! defined (__SYS_SELECT_H__) */
