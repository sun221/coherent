/ This file contains assembly-language implementations of processor priority-
/ level manipulation functions that would be in-lined using GCC or typical
/ MS-DOS tools. See "spl.c" for routine documentation.

		.unixorder
		.include	struct.inc
		.include	pl.inc

/ Because the DDI/DKI processor-priority manipulation system I have chosen
/ to implement uses interrupt masks, we need access to a reasonable amount
/ of per-CPU global information...
/
/	The current abstract IPL.
/	The current mask for the 'plbase' abstract IPL.
/	The table of additional masks for abstract IPLs.
/
/ This implementation is for Coherent, uniprocessor only. To make life easier
/ for this code we have put the relevant parts of the "ddi_cpu_data" structure
/ at the front. Coherent 'as' has no structure facility, so we use .set
/ Wouldn't this be easier if there was a .struct facility that knew about
/ the COFF/ABI structure alignment rules?

/ struct defcpu {
		.struct	defer
		.member		df_tab,ptr
		.member		df_read,uchar
		.member		df_max,uchar
		.member		df_write,uchar
		.member		df_rlock,uchar
		.member		df_wlock,ptr
		.ends	defer
/ };
		.typedef	defer_t,defer

/ struct dcdata {
		.struct	dcdata
		.member		dc_cpu_id,uint
		.member		dc_base_mask,ulong
		.member		dc_int_level,uchar
		.member		dc_user_level,uchar
		.member		dc_ipl,uchar
		.member		dc_defint,defer_t
		.member		dc_defproc,defer_t
		.ends	dcdata
/ };
		.typedef	dcdata_t,dcdata
/ struct dgdata {
					/ member offsets
		.struct	dgdata
		.member		dg_defint,defer_t
		.ends	dgdata
/ };
		.typedef	dgdata_t,dgdata

cpu		.define	__ddi_cpu_data
glob		.define	__ddi_global_data

		.extern	cpu,dcdata_t
		.extern	glob,dgdata_t

		.set	PICM,0x21
		.set	SPICM,0xA1

		.globl	_masktab			/ array of longs

		.text

		.globl	splbase
		.globl	spltimeout
		.globl	spldisk
		.globl	splstr
		.globl	splhi
		.globl	splx
		.globl	splraise
		.globl	splcmp
		.globl	__SET_BASE_MASK
		.globl	__CHEAP_DISABLE_INTS
		.globl	__CHEAP_ENABLE_INTS
		.globl	DDI_BASE_SLAVE_MASK
		.globl	DDI_BASE_MASTER_MASK

/ pl_t splbase (void)
splbase:	mov	$plbase, %ecx
		jmp	setnew

spltimeout:	mov	$pltimeout, %ecx
		jmp	setnew

spldisk:	mov	$pldisk, %ecx
		jmp	setnew

splstr:		mov	$plstr, %ecx
		jmp	setnew

splhi:		mov	$plhi, %ecx
		jmp	setnew

/ pl_t splx (pl_t _newpl)
splx:
		movzxb	4(%esp), %ecx			/ new pl
setnew:
		movl	_masktab(%ecx,4), %eax		/ new mask

	/ send the mask out to the PICs after including the base mask info.
		orl	cpu+dc_base_mask, %eax
		outb	$PICM
		xchgb	%ah,%al
		outb	$SPICM

	/ swap the new IPL for the old one, and return the old one.
		movzxb	cpu+dc_ipl, %eax
		movb	%cl, cpu+dc_ipl

		ret

/ pl_t splraise (pl_t _newpl)
splraise:
		movzxb	4(%esp), %ecx			/ new pl

		cmpb	cpu+dc_ipl, %cl
		jg	setnew

		movzxb	cpu+dc_ipl, %eax		/ just return old
		ret

/ int splcmp (pl_t l, pl_t r)
splcmp:		movzxb	4(%esp), %eax			/ left
		sub	4(%esp), %eax			/ - right
		jz	l_eq_r				/ if eq, %eax == 0
/ take advantage of 80x86 mov not changing condition codes.
		mov	$-1, %eax
		jl	l_lt_r				/ if <, %eax == -1
		mov	$1, %eax			/ if >, %eax == 1
l_eq_r:
l_lt_r:
		ret

/ void __SET_BASE_MASK (intmask_t newmask)
__SET_BASE_MASK:
		mov	4(%esp), %eax			/ new mask
		mov	%eax, cpu+dc_base_mask		/ store it
		outb	$PICM				/ mask low
		mov	%ah, %al
		outb	$SPICM				/ mask high
		ret

/ void __CHEAP_DISABLE_INTS (void)
__CHEAP_DISABLE_INTS:
		cli
		ret

/ void __CHEAP_ENABLE_INTS (void)
__CHEAP_ENABLE_INTS:
		sti
		ret

/ These function should only be called from base level, so that we don't
/ have to iterate over all kinds of junk to work out what the real mask
/ level should be. This condition is trivially satisfied at the moment because
/ these are only called by a Coherent's functions, which aren't protected by
/ any kind of spl... (), just (possibly) an sphi ()/spl ().

/ void DDI_BASE_MASTER_MASK (uchar_t mask)
DDI_BASE_MASTER_MASK:
		mov	4(%esp), %al
		mov	%al, cpu+dc_base_mask
		outb	$PICM
		ret

/ void DDI_BASE_SLAVE_MASK (uchar_t mask)
DDI_BASE_SLAVE_MASK:
		mov	4(%esp), %al
		mov	%al, cpu+dc_base_mask+1
		outb	$SPICM
		ret
