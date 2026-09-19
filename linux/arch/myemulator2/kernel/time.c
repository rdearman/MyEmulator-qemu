// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/clocksource.h>
#include <linux/clockchips.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <asm/irq.h>

static u64 myemulator2_clock_read(struct clocksource *cs)
{
	return 0;
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
