/* Exception entry is intentionally kept in one small assembly boundary.
 * The CPU has already built the 16-byte frame at SSP.  The complete register
 * save/restore path will be extended as user mode is enabled. */
#include <linux/linkage.h>

	.section .text,"ax"
	.align 2
	.globl myemulator2_exception_entry
	.type myemulator2_exception_entry,@function
myemulator2_exception_entry:
	call myemulator2_exception_dispatch
	ret
	.size myemulator2_exception_entry, .-myemulator2_exception_entry
