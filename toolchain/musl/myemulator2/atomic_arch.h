#define a_barrier a_barrier
static inline void a_barrier(void)
{
	__asm__ __volatile__("" ::: "memory");
}

#define a_cas a_cas
static inline int a_cas(volatile int *p, int t, int s)
{
	__asm__ __volatile__("cas %0, %1, 0(%2)"
		: "+r"(t) : "r"(s), "r"(p) : "memory");
	return t;
}
