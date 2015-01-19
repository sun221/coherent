/	module name dbtest
	.alignoff

	.text
	.globl main
main:
	push	%ebp
	movl	%ebp, %esp
	push	%edi
.L3:
	addl	12(%ebp), $4
	movl	%edi, 12(%ebp)
	cmpl	(%edi), $0
	je	.L2
	push	(%edi)
	call	display
	pop	%ecx
	jmp	.L3
.L2:
	push	$0
	call	exit
	pop	%ecx
	pop	%edi
	leave
	ret
	.align	4
	.globl display
display:
	push	%ebp
	movl	%ebp, %esp
	push	8(%ebp)
	push	$.L5
	call	printf
	addl	%esp, $8
	leave
	ret
	.align	4
	.align	4
.L5:
	.byte	"Got a %s\n", 0
	.align	4

	.data
	.align	1
	.globl version
version:
	.byte	86
	.byte	101
	.byte	114
	.byte	32
	.byte	104
	.byte	101
	.byte	108
	.byte	108
	.byte	111
	.byte	32
	.byte	116
	.byte	104
	.byte	101
	.byte	114
	.byte	101
	.byte	0
