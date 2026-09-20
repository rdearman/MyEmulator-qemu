#define __SYSCALL_LL_E(x) \
((union { long long ll; long l[2]; }){ .ll = x }).l[0], \
((union { long long ll; long l[2]; }){ .ll = x }).l[1]
#define __SYSCALL_LL_O(x) __SYSCALL_LL_E((x))

/* MyEmulator2 syscall ABI: r1 is the syscall number/result and r2-r7 are
 * the six Linux arguments.  Fixed-register variables preserve that ABI while
 * allowing musl's architecture-independent syscall wrappers to be reused. */
#define __MYEMU_SYSCALL asm volatile ("syscall 0" : "+r"(r1) : \
    "r"(r2), "r"(r3), "r"(r4), "r"(r5), "r"(r6), "r"(r7) : "memory")

static inline long __syscall0(long n)
{
	register long r1 __asm__("r1") = n;
	register long r2 __asm__("r2");
	register long r3 __asm__("r3");
	register long r4 __asm__("r4");
	register long r5 __asm__("r5");
	register long r6 __asm__("r6");
	register long r7 __asm__("r7");
	__MYEMU_SYSCALL;
	return r1;
}

#define __MYEMU_SYSCALL_N(N) \
static inline long __syscall##N(long n, \
    long a, long b, long c, long d, long e, long f) \
{ \
    register long r1 __asm__("r1") = n; \
    register long r2 __asm__("r2") = a; \
    register long r3 __asm__("r3") = b; \
    register long r4 __asm__("r4") = c; \
    register long r5 __asm__("r5") = d; \
    register long r6 __asm__("r6") = e; \
    register long r7 __asm__("r7") = f; \
    __MYEMU_SYSCALL; \
    return r1; \
}

static inline long __syscall6(long, long, long, long, long, long, long);

static inline long __syscall1(long n, long a)
{
	return __syscall6(n, a, 0, 0, 0, 0, 0);
}
static inline long __syscall2(long n, long a, long b)
{
	return __syscall6(n, a, b, 0, 0, 0, 0);
}
static inline long __syscall3(long n, long a, long b, long c)
{
	return __syscall6(n, a, b, c, 0, 0, 0);
}
static inline long __syscall4(long n, long a, long b, long c, long d)
{
	return __syscall6(n, a, b, c, d, 0, 0);
}
static inline long __syscall5(long n, long a, long b, long c, long d, long e)
{
	return __syscall6(n, a, b, c, d, e, 0);
}

__MYEMU_SYSCALL_N(6)
#undef __MYEMU_SYSCALL_N
#undef __MYEMU_SYSCALL
