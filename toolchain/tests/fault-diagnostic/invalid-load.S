/* Deliberately fault on an unmapped user address for kernel diagnostics. */
	.text
	.globl _start
	.type _start,@function
_start:
	lui r1,0xdead
	ori r1,r1,0xbeef
	lw r2,0(r1)
1:
	j 1b
	.size _start,.-_start
