#ifndef _ASM_MYEMULATOR2_IRQ_H
#define _ASM_MYEMULATOR2_IRQ_H
#define NR_IRQS 8
#define IRQ_TIMER 1
#define IRQ_CONSOLE 4
#define IRQ_VIRTIO_NET 5
#define irq_canonicalize(irq) (irq)
#endif
