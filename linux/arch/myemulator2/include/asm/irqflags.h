#ifndef _ASM_MYEMULATOR2_IRQFLAGS_H
#define _ASM_MYEMULATOR2_IRQFLAGS_H
static inline unsigned long arch_local_save_flags(void)
{
	unsigned long value;
	asm volatile("mfsr %0, sr" : "=r"(value));
	return value;
}
static inline void arch_local_irq_disable(void)
{
	unsigned long value = arch_local_save_flags();
	value = (value & ~(7u << 2)) | (7u << 2);
	asm volatile("mtsr sr, %0" :: "r"(value) : "memory");
}
static inline void arch_local_irq_enable(void)
{
	unsigned long value = arch_local_save_flags();
	value &= ~(7u << 2);
	asm volatile("mtsr sr, %0" :: "r"(value) : "memory");
}
static inline void arch_local_irq_restore(unsigned long value)
{
	asm volatile("mtsr sr, %0" :: "r"(value) : "memory");
}
static inline unsigned long arch_local_irq_save(void)
{
	unsigned long value = arch_local_save_flags();
	arch_local_irq_disable();
	return value;
}
static inline bool arch_irqs_disabled_flags(unsigned long flags)
{
	return ((flags >> 2) & 7) == 7;
}
static inline bool arch_irqs_disabled(void)
{
	return arch_irqs_disabled_flags(arch_local_save_flags());
}
#endif
