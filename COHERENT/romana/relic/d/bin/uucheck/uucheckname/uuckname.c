/*
 * uucheckname - Check validity of current uucp site name.
 */

#include <ctype.h>
#include "uucheck.h"
#include "uucheckname.h"

uucheckname()
{
	char uucpname[BIG]; /* Uucp name as extracted from /etc/uucpname.  */
	char *str; /* Handy character string walker.  */

	REALLYVERBOSE("\n\nChecking site name...\n");

	if (gethostname(uucpname, BIG - 1 ) == -1) {
		FATAL("uucheckname: can't open %s.\n", NAMEFILE);
	} /* if gethostname failed */

	REALLYVERBOSE("Testing name length.\n");
	if (strlen(uucpname) > MAX_NAME) {
		sprintf(bigbuf, "UUCP name %s is longer than %d characters.\n",
				uucpname, MAX_NAME);
		ERROR(bigbuf);
		sprintf(bigbuf,
			"UUCP can not handle names longer than %d characters.\n",
			MAX_NAME);
		VERBOSE(bigbuf);
	} else if (strlen(uucpname) > MAX_UNIQUE) {
		sprintf(bigbuf, "UUCP name %s is longer than %d characters.\n",
				uucpname, MAX_UNIQUE);
		WARNING(bigbuf);
		sprintf(bigbuf, 
			"Those characters beyond the %dth will be ignored.\n",
			MAX_UNIQUE);
		VERBOSE(bigbuf);
		sprintf(bigbuf, "Your sitename will be treated as %.7s.\n", uucpname);
		VERBOSE(bigbuf);
	} /* if uucpname is longer than the number of significant characters  */


	REALLYVERBOSE("Checking to see that name is alphanumeric.\n");
	/* Check to see that uucpname starts alpha.  */
	if (!isalpha(*uucpname)) {
		sprintf(bigbuf, "%s is an illegal uucp name.\n", uucpname);
		ERROR(bigbuf);
		VERBOSE("UUCP names must start with a letter.\n");
	} else {
		/* Check to see that uucpname is strictly alphanumeric.  */
		for ( str = uucpname + 1; isalnum(*str); ++str) {
			/* empty */
		}
	
		if (*str != (char) NULL) {
			sprintf(bigbuf, "%s is an illegal uucp name.\n", uucpname);
			ERROR(bigbuf);
			VERBOSE("Use only letters and numbers, and start with a letter.");
		} /* If we didn't end on a NULL, we've got an non-alphanumeric character.  */
	} /* if first character of uucpname was not alpha */

	REALLYVERBOSE("Checking to see that uucpname is strictly lowercase.\n");
	/* Check to see that uucpname is strictly lowercase.  */
	for ( str = uucpname; islower(*str) || isdigit(*str); ++str) {
		/* empty */
	}

	if (*str != (char) NULL) {
		sprintf(bigbuf, "%s is contains upper case letters.\n", uucpname);
		WARNING(bigbuf);
		VERBOSE("You ought use only lower case letters.\n");
	} /* If we didn't end on a NULL, we've got an non-alphanumeric character.  */


	REALLYVERBOSE("Checking to see if a well known sitename has been chosen.\n");
	/* Check to see if a well known sitename has been chosen.  */
	if (lookup(uucpname, well_known_names) != NULL) {
		sprintf(bigbuf, "%s is a very well known site.\n", uucpname);
		WARNING(bigbuf);
		VERBOSE("You REALLY should pick a different name...\n");
		VERBOSE("Unless, of course, you really are who you claim to be.\n");
		FIX(MESSAGE("Call Technical Support for name ideas.\n"));
	} /* if uucpname is a well-known site */


	if (fix) {
	    /* If this is not a silent session, try to get a new
	     * uucpname from the user.
	     */
	    if (!silent) {
		if (error) {
			/* Pick a new name unconditionally on error.  */
			recreate_uucpname();
		} else if (warning) {
			/* Otherwise, the user gets to choose.  */
			printf("Do you want to enter a new uucpname (y/n)? ");
			if (get_yes_or_no()) {
				recreate_uucpname();
			} /* if (get_yes_or_no()) */
		} /* if (error) else if (warning) */
	    } /* if (!silent) */
	} /* if (fix) */

	RETURN;
} /* uucheckname */
