/* hosts.c - handles lists of uucp hosts.
 * part of uucheck
 */

#include <stdio.h>
#include "checkperms.h"

struct hostname_struct {
	char	*hostname;
	struct hostname_struct	*next_hostname;
};

typedef struct hostname_struct hostname_type;

static hostname_type *first_hostname = (hostname_type *) NULL;

char *
first_host(command)
	char *command;
{
	FILE *fp;
	hostname_type *current_hostname; /* Linked list walker.  */

	/* Here is where we open Permissions and L.sys and extract a list
         * of all the hosts we might talk to.  This list will be walked
	 * through by "next_host()".
	 */

	if ((fp = popen(command, "r")) == NULL) {
		FATAL("Can not create list of host names.\n", NULL);
	} /* if can not open pipe to uuname */
	
	/* Read all the output of uuname into out linked list.  */

	if (fgets(bigbuf, MAX_NAME + 1, fp) != NULL) {
	    bigbuf[strlen(bigbuf) - 1 ] = (char) NULL; /* Wipe out trailing nl.  */
	    /* Make space for the first entry.  */
	    first_hostname = (hostname_type *) malloc(sizeof(hostname_type));
	    /* Move up to the new entry.  */
	    current_hostname = first_hostname;
	    /* Copy the first hostname into the list.  */
	    copy_str(&current_hostname->hostname, bigbuf);
	    current_hostname->next_hostname = (hostname_type *) NULL;
	} /* if (fgets(bigbuf, MAX_NAME + 1, fp) != NULL) */
	
	while (fgets(bigbuf, MAX_NAME + 1, fp) != NULL) {
	    bigbuf[strlen(bigbuf) - 1] = (char) NULL; /* Wipe out trailing nl.  */
	    /* Make space for the next entry.  */
	    current_hostname->next_hostname =
		(hostname_type *) malloc(sizeof(hostname_type));
	    /* Move up to the new entry.  */
	    current_hostname = current_hostname->next_hostname;
	    /* Copy the next hostname into the list.  */
	    copy_str(&current_hostname->hostname, bigbuf);
	    current_hostname->next_hostname = (hostname_type *) NULL;
	} /* while (fgets(bigbuf, MAX_NAME + 1, fp) != NULL) */

	/* Is this list only one entry long?  */
	if (first_hostname->next_hostname != NULL ) {
		last_host = FALSE;
	} else {
		last_host = TRUE;
	}
	return(first_hostname->hostname);
} /* first_host() */

/* Destructively return the next host to be processed.  */
char *
next_host()
{
	hostname_type *tmp;

	if ( first_hostname != NULL ) {
		free(first_hostname->hostname); /* Throw away the last name.  */
		tmp=first_hostname;
		first_hostname = first_hostname->next_hostname;
		free(tmp); /* Throw away the structure it was stored in.  */
		if (first_hostname->next_hostname == (hostname_type *) NULL) {
			last_host = TRUE; /* About to return the last host in the list.  */	
		} /* if this is the last host  */
		return(first_hostname->hostname);
	} else {
		return(NULL);
	} /* if fist_hostname != NULL (no hosts left) */
	FATAL("Unreachable statement in next_host() reached.\n", NULL);
} /* next_host() */
