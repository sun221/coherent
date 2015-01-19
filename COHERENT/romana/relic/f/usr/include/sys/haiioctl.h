/***********************************************************************
 *  Module: haiioctl.h
 *  
 *  Definitions and declarations needed to implement and use the haiioctl
 *  call.
 *  
 *  Copyright (c) 1993 Christopher Sean Hilton All rights reserved.
 *
 *  Last Modified: Sat Jul 24 08:08:06 1993 by [chris]
 *
 *  $Id$
 *
 *  $Log$
 */

#ifndef _HAIIOCTL_H_
#define _HAIIOCTL_H_

#include <sys/haiscsi.h>
 
#define HAI_IOC		0x49414800	/* "\0HAI"  on intel machines */
#define HAIINQUIRY	(HAI_IOC | 1)	/* Inquiry Command */
#define HAIMDSNS0	(HAI_IOC | 2)	/* Group 0 Mode Sense */
#define HAIMDSLCT0	(HAI_IOC | 3)	/* Group 0 Mode Select */
#define HAIMDSNS2	(HAI_IOC | 4)	/* Group 2 Mode Sense */
#define HAIMDSLCT2	(HAI_IOC | 5)	/* Group 2 Mode Select */
#define HAIUSERCDB	(HAI_IOC | 6)	/* User Selected command (be careful) */

/***********************************************************************
 *  haiusercdb  --  Use this layout to get I/O Control info to or from
 *                  a particular device on the scsi bus.  Note well
 *                  that you will need to be the super user in order
 *                  to use the HAIUSERCDB I/O Control.
 */

typedef struct haiusercdb_s *haiusercdb_p;

typedef struct haiusercdb_s {
	cdb_t		cdb;			/* CDB you want to run */
	char		sensebuf[SENSELEN];	/* Sensebuf for returned errors */
	unsigned short	timeout,		/* Time to live */
			xferdir;		/* Transfer direction */
	size_t		buflen;			/* Buffer length */
	char		buf[0];			/* Start of buffer (C++ okay!?) */
} haiusercdb_t;

#endif	/* _HAIIOCTL_H_ */

/* End of file */
