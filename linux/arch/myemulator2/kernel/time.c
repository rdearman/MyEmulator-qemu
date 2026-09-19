// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/clocksource.h>
#include <linux/clockchips.h>
#include <linux/interrupt.h>
#include <linux/irqflags.h>
#include <linux/io.h>
#include <asm/irq.h>

static inline u32 myemulator2_read_time_lo(void)
{
	u32 value;
	asm volatile("mfsr %0, time_lo" : "=r"(value));
	return value;
}

static inline u32 myemulator2_read_time_hi(void)
{
	u32 value;
	asm volatile("mfsr %0, time_hi" : "=r"(value));
	return value;
}

static inline u64 myemulator2_read_time(void)
{
	u32 lo = myemulator2_read_time_lo();
	return ((u64)myemulator2_read_time_hi() << 32) | lo;
}

static inline void myemulator2_write_timecmp(u64 value)
{
	u32 lo = value;
	u32 hi = value >> 32;
	/* The hardware commits TIMECMP when the high word is written. */
	asm volatile("mtsr timecmp_lo, %0\n\tmtsr timecmp_hi, %1"
		     :: "r"(lo), "r"(hi) : "memory");
}

static u64 myemulator2_clock_read(struct clocksource *cs)
{
	return myemulator2_read_time();
}

static struct clocksource myemulator2_clocksource = {
	.name = "myemulator2-time",
	.rating = 200,
	.read = myemulator2_clock_read,
	.mask = CLOCKSOURCE_MASK(64),
	.flags = CLOCK_SOURCE_IS_CONTINUOUS,
};

static int myemulator2_set_next_event(unsigned long delta,
		struct clock_event_device *evt)
{
	myemulator2_write_timecmp(myemulator2_read_time() + delta);
	return 0;
}

static struct clock_event_device myemulator2_clockevent = {
	.name = "myemulator2-timer",
	.features = CLOCK_EVT_FEAT_ONESHOT,
	.set_next_event = myemulator2_set_next_event,
	.rating = 200,
};

void __init time_init(void)
{
	clocksource_register_hz(&myemulator2_clocksource, 1000000);
	clockevents_config_and_register(&myemulator2_clockevent, 1000000, 1, 0xffffffff);
}

void myemulator2_timer_interrupt(void)
{
	/* The CPU has accepted IRQ1, but Linux's hard-IRQ accounting is
	 * expressed through the live IPL.  Keep the generic hard-IRQ path
	 * running with interrupts disabled; RFE restores the saved SR on
	 * return to the interrupted context. */
	local_irq_disable();
	irq_enter();
	if (myemulator2_clockevent.event_handler)
		myemulator2_clockevent.event_handler(&myemulator2_clockevent);
	irq_exit();
}
