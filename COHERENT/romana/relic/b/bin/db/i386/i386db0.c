/*
 * db/i386/i386db0.c
 * A debugger.
 * i386 global variables and tables.
 */

#include <sys/ptrace.h>
#include "i386db.h"

/*
 * Global variables.
 */
BIN	bin = { TRAP };			/* Breakpoint instruction	*/
int	NDP_flag;			/* NDP instructions are usable	*/
ADDR_T	sys_add;			/* Address of op after syscall	*/
int	sys_flag;			/* Executing a system call	*/
BIN	sys_in;				/* Instruction after sys call	*/
UREG	ureg;				/* Child program registers	*/

/*
 * Format strings table.
 */
char *formtab[4][4] ={
	"%4d",				/* "bd" */
	"%3u",				/* "bu" */
	"%04o",				/* "bo" */
	"%02X",				/* "bx" */
	"%6d",				/* "wd" */
	"%5u",				/* "wu" */
	"%07o",				/* "wo" */
	"%04X",				/* "wx" */
	"%10ld",			/* "ld" */
	"%11lu",			/* "lu" */
	"%012lo",			/* "lo" */
	"%08lX",			/* "lx" */
	"%8ld",				/* "vd" */
	"%8lu",				/* "vu" */
	"%09lo",			/* "vo" */
	"%06lX"				/* "vx" */
};

/*
 * Register name table.
 * Map register names to pseudo-offsets in user segment; cf. <sys/ptrace.h>.
 * This is used to obtain offsets for ptrace() calls to read/write registers.
 * The register name lookup code in i386db1.c/is_reg() also recognizes
 * "xy" as the word register equivalent of dword register "exy".
 * Code using this table does not reference the copy of the child registers
 * kept in ureg.
 * The order in the table corresponds to the order of printing for :r.
 * Other PTRACE_* entries could be recognized but currently are not.
 */
REGNAME	reg_name[NREGS] = {
	{ RF_16, 	"cs",	PTRACE_CS },
	{ RF_1632, 	"eip",	PTRACE_EIP },
	{ RF_16,	"ss",	PTRACE_SS },
	{ RF_1632,	"fw",	PTRACE_EFL },
	{ RF_16,	"ds",	PTRACE_DS },
	{ RF_16,	"es",	PTRACE_ES },
	{ RF_16,	"fs",	PTRACE_FS },
	{ RF_16,	"gs",	PTRACE_GS },
	{ RF_1632,	"eax",	PTRACE_EAX },
	{ RF_1632,	"ebx",	PTRACE_EBX },
	{ RF_1632,	"ecx",	PTRACE_ECX },
	{ RF_1632, 	"edx",	PTRACE_EDX },
	{ RF_1632,	"esp",	PTRACE_UESP },		/* UESP */
	{ RF_1632,	"ebp",	PTRACE_EBP },
	{ RF_1632, 	"edi",	PTRACE_EDI },
	{ RF_1632,	"esi",	PTRACE_ESI },
	{ RF_NDP, 	"st0",	PTRACE_FP_ST0 },
	{ RF_NDP,	"st1",	PTRACE_FP_ST1 },
	{ RF_NDP,	"st2",	PTRACE_FP_ST2 },
	{ RF_NDP,	"st3",	PTRACE_FP_ST3 },
	{ RF_NDP,	"st4",	PTRACE_FP_ST4 },
	{ RF_NDP,	"st5",	PTRACE_FP_ST5 },
	{ RF_NDP,	"st6",	PTRACE_FP_ST6 },
	{ RF_NDP,	"st7",	PTRACE_FP_ST7 }
};

#if	0
/*
 * Table of system calls.
 */
char *sysitab[NMICALL] ={
	NULL,
	"exit",
	"fork",
	"read",
	"write",
	"open",
	"close",
	"wait",
	"creat",
	"link",
	"unlink",
	"exece",
	"chdir",
	NULL,
	"mknod",
	"chmod",
	"chown",
	"brk",
	"stat",
	"lseek",
	"getpid",
	"mount",
	"umount",
	"setuid",
	"getuid",
	"stime",
	"ptrace",
	"alarm",
	"fstat",
	"pause",
	"utime",
	NULL,
	NULL,
	"access",
	"nice",
	"ftime",
	"sync",
	"kill",
	NULL,
	NULL,
	NULL,
	"dup",
	"pipe",
	"times",
	"profil",
	"unique",
	"setgid",
	"getgid",
	"signal",
	NULL,
	NULL,
	"acct",
	NULL,
	NULL,
	"ioctl",
	NULL,
	"getegid",
	"geteuid",
	NULL,
	NULL,
	"umask",
	"chroot",
	"setpgrp",
	"getpgrp",
	"sload",
	"suload",
	"fcntl",
	"poll",
	"msgctl",
	"msgget",
	"msgrcv",
	"msgsnd",
	"alarm2",
	"tick"
};
#endif

/* end of db/i386/i386db0.c */
