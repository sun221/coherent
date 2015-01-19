//////////
/ i8086 C string library.
/ strpbrk()
/ ANSI 4.11.5.4.
//////////

//////////
/ char *
/ strpbrk(String1, String2)
/ char *String1, *String2;
/
/ Return a pointer to the first char in String1
/ which matches any character in String2.
//////////

#include <larges.h>

String1	=	LEFTARG
String2	=	String1+DPL

	Enter(strpbrk_)
	Les	di, String1(bp)	/ String1 address to ES:DI
	sub	ax, ax		/ Clear AL for NULL return value
#if	LARGEDATA
	mov	dx, ax		/ and clear DX likewise
#endif
	cld

1:	movb	ah, Pes (di)	/ Get char from String1 to AH
	inc	di		/ and point to next
	orb	ah, ah
	je	3f		/ No match found, return NULL
	Lds	si, String2(bp)	/ String2 address to DS:SI

2:	lodsb			/ String2 character to AL
	orb	al, al
	je	1b		/ No match in String2, try next String1 char
	cmpb	ah, al
	jne	2b		/ Mismatch, try next String2 char
	dec	di		/ Match, return pointer
	mov	ax, di		/ in AX
#ifdef LARGEDATA
	mov	dx, es		/ or DX:AX
#endif

3:	Leave
