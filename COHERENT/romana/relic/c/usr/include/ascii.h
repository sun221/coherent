/* (-lgl
 * 	COHERENT Version 3.0
 * 	Copyright (c) 1982, 1990 by Mark Williams Company.
 * 	All rights reserved. May not be copied without permission.
 -lgl) */
#ifndef	ASCII_H
#define	ASCII_H

/*
 * Ascii Macro Definitions.
 */

#define A_NUL	0x00	/* Null			*/
#define A_SOH	0x01	/* Start Of Header	*/
#define A_STX	0x02	/* Start Of Text	*/
#define A_ETX	0x03	/* End Of Text		*/
#define A_EOT	0x04	/* End Of Transmission	*/
#define A_ENQ	0x05	/* Enquiry		*/
#define A_ACK	0x06	/* Acknowledge		*/
#define A_BEL	0x07	/* Bell			*/
#define A_BS	0x08	/* Backspace		*/
#define A_HT	0x09	/* Horizontal Tab	*/
#define A_NL	0x0A	/* New Line		*/
#define A_LF	0x0A	/* Line Feed		*/
#define A_VT	0x0B	/* Vertical Tabulation	*/
#define A_FF	0x0C	/* Form Feed		*/
#define A_CR	0x0D	/* Carriage Return	*/
#define A_SO	0x0E	/* Stand Out		*/
#define A_SI	0x0F	/* Stand In		*/
#define A_DLE	0x10	/* Data Link Escape	*/
#define A_DC1	0x11	/* Data Ctrl 1 - XON	*/
#define A_DC2	0x12	/* Data Ctrl 2		*/
#define A_DC3	0x13	/* Data Ctrl 3 - XOFF	*/
#define A_DC4	0x14	/* Data Ctrl 4		*/
#define A_NAK	0x15	/* Negative Acknowledge	*/
#define A_SYN	0x16	/* Synchronization	*/
#define A_ETB	0x17	/* 			*/
#define A_CAN	0x18	/* Cancel		*/
#define A_EM	0x19	/*			*/
#define A_SUB	0x1A	/*			*/
#define A_ESC	0x1B	/* Escape		*/
#define A_FS	0x1C	/*			*/
#define A_GS	0x1D	/*			*/
#define A_RS	0x1E	/*			*/
#define A_US	0x1F	/*			*/
#define A_DEL	0x7F	/* Delete		*/

#endif
