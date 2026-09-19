/* Minimal freestanding entry point for compiler/QEMU tests. */
.text
.global _start
.type _start, @function
_start:
        call main
        halt
.size _start, .-_start
