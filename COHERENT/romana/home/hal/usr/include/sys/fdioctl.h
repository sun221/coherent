/* (-lgl
 *	Coherent 386 release 4.2
 *	Copyright (c) 1982, 1993 by Mark Williams Company.
 *	All rights reserved. May not be copied without permission.
 *	For copying permission and licensing info, write licensing@mwc.com
 -lgl) */

#ifndef __SYS_FDIOCTL_H__
#define __SYS_FDIOCTL_H__

/*
 * Floppy I/O control commands.
 */

#define FDIOC		('F'<<8)
#define FDFORMAT	(FDIOC|1)	/* Format diskette track */

/*
 * Formatting information is largely supplied
 * by the low nibble of the minor device number opened for formatting
 * which will specify the number of heads and the track density.
 *	0	1 head, 40 tracks, 8 sectors per track
 *	1	2 heads, 40 tracks, 8 sectors per track
 *	2	2 heads, 80 tracks, 8 sectors per track
 *	3	1 head, 40 tracks, 9 sectors per track
 *	4	2 heads, 40 tracks, 9 sectors per track
 *	5	2 heads, 80 tracks, 9 sectors per track
 * Each FDFORMAT command will format a single track.
 * The parameter block consists of an array of fform structures
 * one for each sector being formatted specifying the cylinder,
 * head, sector number, and size of the sector.
 */

struct fform {
	char	ff_cylin;	/* 0 .. number of tracks - 1 */
	char	ff_head;	/* 0 or 1 */
	char	ff_sect;	/* 1 .. number of sectors */
	char	ff_size;	/* 1, 2, or 3 for 256, 512, or 1024 */
};

#endif	/* ! defined (__SYS_FDIOCTL_H__) */

