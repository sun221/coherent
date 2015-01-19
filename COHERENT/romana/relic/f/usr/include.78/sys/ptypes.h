/* (-lgl
 * 	COHERENT 386 Device Driver Kit release 2.0
 * 	Copyright (c) 1982, 1992 by Mark Williams Company.
 * 	All rights reserved. May not be copied without permission.
 -lgl) */
/*
 * ptypes.h -- portable types.  Change the typedefs in this file
 * to match the local architecture.
 */
#ifndef __SYS_PTYPES_H__
#define __SYS_PTYPES_H__

/* This file ought to be rewritten to adjust itself based on the contents
 * of the ANSI file limits.h.
 */

typedef signed char int8;
#define MAXINT8		((int8) 127)
typedef unsigned char uint8;
#define MAXUINT8	((uint8) 255)
typedef short int16;
#define MAXINT16	((int16) 32767)
typedef unsigned short uint16;
#define MAXUINT16	((uint16) 65535)
typedef long int32;
#define MAXINT32	((int32) 2^31 - 1)
typedef unsigned long uint32;
#define MAXUINT32	((uint32) (((uint32) 2^32) - 1))

#endif /* ifdef PTYPES_H */
