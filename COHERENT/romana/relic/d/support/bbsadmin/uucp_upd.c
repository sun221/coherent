/* uucp_upd.c -> add new maillist entry to L.sys and Permissions.
                 create uucp spool directory. We don't really care about our
		 return status here. We will pront warnings and go about
		 our business. Hopefully, the user will be smart enough
		 to know to quit and fix whatever is broken.
 */


#include <stdio.h>
#include <errno.h>
#include "contents.h"

void uucp_upd()
{

FILE *permfile;
FILE *lsysfile;
char perm_entry[76];
char lsys_entry[76];
char dir_cmd[76];
int errno;

	/* build and write our Permissions entry */

	if( (permfile = fopen("/usr/lib/uucp/Permissions","a")) == NULL){
		printf("Error opening file [/usr/lib/uucp/Permissions]\n");
		printf("Error is %s\n",sys_errlist[errno]);
		printf("You will have to manually edit your uucp configuration files.\n");
		return;
	}else{
		sprintf(perm_entry,"MACHINE=%s LOGNAME=nuucp \\ \n",new_mail_rec.site);
		fprintf(permfile,"%s",perm_entry);
		sprintf(perm_entry,"\tCOMMANDS=rmail:rnews:uucp \\ \n");
		fprintf(permfile,"%s",perm_entry);
		sprintf(perm_entry,"\tREAD=/usr/spool/uucppublic \\ \n");
		fprintf(permfile,"%s",perm_entry);
		sprintf(perm_entry,"\tWRITE=/usr/spool/uucppublic/uploads \\ \n");
		fprintf(permfile,"%s",perm_entry);
		sprintf(perm_entry,"\tREQUEST=yes SENDFILES=yes\n");
		fprintf(permfile,"%s",perm_entry);
		fclose(permfile);
	}


	/* build and write our L.sys entry */

	if( (lsysfile = fopen("/usr/lib/uucp/L.sys","a")) == NULL){
		printf("Error opening file [/usr/lib/uucp/L.sys]\n");
		printf("Error is %s\n",sys_errlist[errno]);
		printf("You will have to manually edit your uucp configuration files.\n");
		return;
	}else{
		sprintf(lsys_entry,"%s Never ACU 9600 5551212 \"\" \\n in: hello",new_mail_rec.site);
		fprintf(lsysfile,"%s",lsys_entry);
		fclose(lsysfile);
	}

	/* build the spool directory */

	sprintf(dir_cmd,"su uucp /bin/mkdir /usr/spool/uucp/%s",new_mail_rec.site);
	if (system(dir_cmd)){
		printf("Spool directory creation failed.\n");
		printf("You will have to create one manually");
	}

}

