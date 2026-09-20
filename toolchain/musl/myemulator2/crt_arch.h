__asm__(
".section .text\n"
".global " START "\n"
".type " START ",%function\n"
START ":\n"
"add r1, r13, r0\n"
"j " START "_c\n"
);
