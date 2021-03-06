/* (-lgl
 *	Coherent 386 release 4.2
 *	Copyright (c) 1982, 1993 by Mark Williams Company.
 *	All rights reserved. May not be copied without permission.
 *	For copying permission and licensing info, write licensing@mwc.com
 -lgl) */

#ifndef	__ARPA_INET_H__
#define	__ARPA_INET_H__

/*
 * Function prototypes for internet address manipulation functions.
 */

#include <common/ccompat.h>
#include <netinet/in.h>

__EXTERN_C_BEGIN__

__inaddr_t	inet_addr	__PROTO ((char * _cp));
int		inet_network	__PROTO ((char * _cp));
struct in_addr	inet_makeaddr	__PROTO ((int _net, int _lna));
int		inet_lnaof	__PROTO ((struct in_addr in));
int		inet_netof	__PROTO ((struct in_addr in));
char	      *	inet_ntoa	__PROTO ((struct in_addr in));

__EXTERN_C_END__

#endif	/* ! defined (__ARPA_INET_H__) */

