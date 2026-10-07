/* REM predefined macros must describe the target data model, not the host. */
#ifdef __LP64__
#error REM must not define __LP64__
#endif
#ifndef __myemulator2__
#error missing REM target marker
#endif
#if __SIZEOF_LONG__ != 4 || __SIZEOF_POINTER__ != 4 || __SIZEOF_SIZE_T__ != 4
#error REM uses 32-bit long, pointers and size_t
#endif
int main(void) { return 0; }
