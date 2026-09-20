#define LDSO_ARCH "myemulator2"
#define TPOFF_K 0

/* Values from the MyEmulator2 ELF psABI; musl's generic loader only needs
 * the absolute/relative word relocation during the first static port. */
#define R_MYEMULATOR2_32 1

#define REL_SYMBOLIC R_MYEMULATOR2_32
#define REL_RELATIVE R_MYEMULATOR2_32

#define CRTJMP(pc, sp) __asm__ __volatile__( \
	"add r13, %1, r0\n" \
	"jr %0" : : "r"(pc), "r"(sp) : "memory")
