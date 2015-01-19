/* (-lgl
 *	Coherent 386 release 4.2
 *	Copyright (c) 1982, 1993 by Mark Williams Company.
 *	All rights reserved. May not be copied without permission.
 *	For copying permission and licensing info, write licensing@mwc.com
 -lgl) */

#ifndef	__COMMON___CADDR_H__
#define	__COMMON___CADDR_H__

/*
 * This internal header file is intended as the sole point of definition for
 * the internal data type "__caddr_t".  It is equivalent to the System V data
 * type "caddr_t", but has a internal name to avoid intrusion upon the user
 * namespace.
 */

typedef	char	      *	__caddr_t;

#endif	/* ! defined (__COMMON___CADDR_H__) */

