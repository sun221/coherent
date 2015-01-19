/* (-lgl
 *	Coherent 386 release 4.2
 *	Copyright (c) 1982, 1993 by Mark Williams Company.
 *	All rights reserved. May not be copied without permission.
 *	For copying permission and licensing info, write licensing@mwc.com
 -lgl) */

#ifndef	__SYS_DMAC_H__
#define	__SYS_DMAC_H__

#define	DMA	0x00			/* Primary 8237 base port */
#define	SDMA	0xC0			/* Secondary 8237 base port */
#define	DMAPAGE	0x80			/* DMA page registers */
#define	CLEARFL	0x0C			/* Clear F/L offset */
#define	SETMASK	0x0A			/* Set DMA mask offset */
#define	SETMODE	0x0B			/* Set DMA mode offset */
#define	RDMEM	0x48			/* Mode, read memory */
#define	WRMEM	0x44			/* Mode, write memory */
#define	MASKOFF	0x00			/* Mask bit off */
#define	MASKON	0x04			/* Mask bit on */

/* For compatibility with other DDK's. */
#define DMA_Wrmode      0x48    /* single, read, increment, no auto-init */
#define DMA_Rdmode      0x44    /* single, write, increment, no auto-init */

enum {
	/* Channels 0-3 are for 8-bit transfers. */
	DMA_CH0 = 0,
	DMA_CH1 = 1,
	DMA_CH2 = 2,
	DMA_CH3 = 3,

	/* Channels 4-7 are for 8-bit transfers. */
	DMA_CH4 = 4,
	DMA_CH5 = 5,
	DMA_CH6 = 6,
	DMA_CH7 = 7
};
                               
/* For use as the the "wflag" argument to dmaon (). */
enum {
	DMA_TO_MEM = 0,
	DMA_FROM_MEM = 1
};

#ifdef _KERNEL

int		dmaon	__PROTO ((int chan, paddr_t paddr, unsigned int count,
			  int wflag));
void		dmago	__PROTO ((int chan));
int		dmaoff	__PROTO ((int chan));

#endif	/* defined (_KERNEL) */

#endif	/* ! defined (__SYS_DMAC_H__) */
