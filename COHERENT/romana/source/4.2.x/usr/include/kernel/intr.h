/* (-lgl
 *	Coherent 386 release 4.2
 *	Copyright (c) 1982, 1993 by Mark Williams Company.
 *	All rights reserved. May not be copied without permission.
 *	For copying permission and licensing info, write licensing@mwc.com
 -lgl) */

#ifndef	__KERNEL_INTR_H__
#define	__KERNEL_INTR_H__

/*
 * Some definitions relating to setting up interrupt vectors in Coherent 4.2
 *
 * The assembly-language code that handles the incoming interrupt deals with
 * saving the CPU context and then pulls some information out of the following
 * table for manipulating the PIC context and for passing information to the
 * C-language interrupt handler.
 *
 * A similar structure is defined in the kernel assembly-language library
 * code; keep that in synch with this, or else...
 */

#include <common/ccompat.h>
#include <common/_intmask.h>

typedef	struct __interrupt_control	__int_control_t;

typedef	void	     (*	__int_func_t)	__PROTO ((int _arg,
						  __int_control_t * _info));

struct __interrupt_control {
	intmask_t	_int_mask;	/* masking information */
	__int_func_t	_int_func;	/* handler to call */
	int		_int_arg;	/* argument for handler */
	int		_int_count;	/* entry count */
	__VOID__      *	_int_stat;	/* reserved for statistics */

	unsigned long	__reserved [3];	/* reserved */
};

#define	__DECLARE_INT(mask, func, arg) { (mask), (func), (arg) }

#endif	/* ! defined (__KERNEL_INTR_H__) */

