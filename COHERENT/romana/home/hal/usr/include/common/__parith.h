/* (-lgl
 *	Coherent 386 release 4.2
 *	Copyright (c) 1982, 1993 by Mark Williams Company.
 *	All rights reserved. May not be copied without permission.
 *	For copying permission and licensing info, write licensing@mwc.com
 -lgl) */

#ifndef	__COMMON___PARITH_H__
#define	__COMMON___PARITH_H__

/*
 * This internal header file defines the internal data type "__ptr_arith_t".
 *
 * This type represents the smallest unsigned integral type to which a
 * C-language pointer can be safely converted without loss of information,
 * and vice versa.  This type is not guaranteed to hold safely the
 * values of pointer types with implementation-specific decoration such as
 * a "far" attribute, or such implementation-specific entities as physical
 * addresses.
 */

#include <common/feature.h>

#if	__COHERENT__

typedef	unsigned int	__ptr_arith_t;

#else

# error	The integral type equivalent to a pointer is not known.

#endif

#endif	/* ! defined (__COMMON___PARITH_H__) */
