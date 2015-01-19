/* (-lgl
 *	Coherent 386 release 4.2
 *	Copyright (c) 1982, 1993 by Mark Williams Company.
 *	All rights reserved. May not be copied without permission.
 *	For copying permission and licensing info, write licensing@mwc.com
 -lgl) */

#ifndef	__COMMON_XDEBUG_H__
#define	__COMMON_XDEBUG_H__

/*
 * The COHERENT compiler's preprocessor cannot remember which header files
 * it has already included, and must work out whether it needs to rescan
 * the contents.  This header helps prevent it from processing this header
 * multiple times.
 */

#include <common/_xdebug.h>

#endif	/* ! defined (__COMMON_XDEBUG_H__) */
