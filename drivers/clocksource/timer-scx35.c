#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/clockchips.h>
#include <linux/io.h>
#include <linux/clocksource.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/sched_clock.h>

struct scx35_gptimer {
	void __iomem *base;
	int irq;
	u32 latch;
};

static struct scx35_gptimer gptimers[4];

static void __iomem *syscnt_base;
static int syscnt_irq;

static struct clock_event_device *local_evt[4];

#define TIMER_LOAD(base, id)	(base + 0x20 * (id))
#define TIMER_VALUE(base, id)	(base + 0x20 * (id) + 0x4)
#define TIMER_CTL(base, id)		(base + 0x20 * (id) + 0x8)
#define TIMER_INT(base, id)		(base + 0x20 * (id) + 0xC)
#define TIMER_CNT_RD(base, id)	(base + 0x20 * (id) + 0x10)

#define ONESHOT_MODE 			(0 << 6)
#define PERIOD_MODE				(1 << 6)

#define TIMER_DISABLE			(0 << 7)
#define TIMER_ENABLE			(1 << 7)

#define TIMER_INT_EN			(1 << 0)
#define TIMER_INT_CLR			(1 << 3)
#define TIMER_INT_BUSY			(1 << 4)
#define TIMER_NEW				(1 << 8)

#define EVENT_TIMER				0
#define BC_TIMER				1
#define SOURCE_TIMER			1

#define SYSCNT_ALARM			(syscnt_base)
#define SYSCNT_COUNT			(syscnt_base + 0x4)
#define SYSCNT_CTL				(syscnt_base + 0x8)
#define SYSCNT_SHADOW_CNT		(syscnt_base + 0xC)

static int e_cpu = 0;

#define BC_CPU					1
static int BC_IRQ = 0;

static u64 scx35_aon_clocksource_read(struct clocksource *cs)
{
	return ~readl(TIMER_CNT_RD(gptimers[e_cpu].base, SOURCE_TIMER));
}

static void scx35_aon_clocksource_resume(struct clocksource *cs)
{
	writel(TIMER_ENABLE | PERIOD_MODE, TIMER_CTL(gptimers[e_cpu].base, SOURCE_TIMER));
}

static void scx35_aon_clocksource_suspend(struct clocksource *cs)
{
	writel(TIMER_DISABLE | PERIOD_MODE, TIMER_CTL(gptimers[e_cpu].base, SOURCE_TIMER));
}

struct clocksource aon_clocksource = {
	.name = "aon_timer1",
	.rating = 300,
	.read = scx35_aon_clocksource_read,
	.mask = CLOCKSOURCE_MASK(32),
	.flags = CLOCK_SOURCE_IS_CONTINUOUS,
	.resume = scx35_aon_clocksource_resume,
	.suspend = scx35_aon_clocksource_suspend,
};

static irqreturn_t scx35_gptimer_interrupt(int irq, void *dev)
{
	unsigned int value;
	int cpu = smp_processor_id();
	struct clock_event_device **evt = dev;

	value = readl(TIMER_INT(gptimers[cpu].base, EVENT_TIMER));
	value |= TIMER_INT_CLR;
	writel(value, TIMER_INT(gptimers[cpu].base, EVENT_TIMER));

	if (evt[cpu]->event_handler)
		evt[cpu]->event_handler(evt[cpu]);

	return IRQ_HANDLED;
}

static u64 scx35_sched_clock_read(void)
{
	return readl(TIMER_CNT_RD(gptimers[0].base, SOURCE_TIMER));
}

static int bctimer_set_next_event(unsigned long evt, struct clock_event_device *dev)
{
	while (TIMER_INT_BUSY & readl(TIMER_INT(gptimers[BC_CPU].base, BC_TIMER))) ;
	writel(TIMER_DISABLE | ONESHOT_MODE, TIMER_CTL(gptimers[BC_CPU].base, BC_TIMER));
	writel(evt, TIMER_LOAD(gptimers[BC_CPU].base, BC_TIMER));
	writel(TIMER_ENABLE | ONESHOT_MODE, TIMER_CTL(gptimers[BC_CPU].base, BC_TIMER));
	return 0;
}

static int bctimer_set_state_oneshot(struct clock_event_device *dev)
{
	writel((32768 + HZ/2) / HZ, TIMER_LOAD(gptimers[BC_CPU].base, BC_TIMER));
	writel(TIMER_ENABLE | ONESHOT_MODE, TIMER_CTL(gptimers[BC_CPU].base, BC_TIMER));
	writel(TIMER_INT_EN, TIMER_INT(gptimers[BC_CPU].base, BC_TIMER));
	return 0;
}

static int bctimer_set_state_periodic(struct clock_event_device *dev)
{
	writel(TIMER_DISABLE | PERIOD_MODE, TIMER_CTL(gptimers[BC_CPU].base, BC_TIMER));
	writel((32768 + HZ/2) / HZ, TIMER_LOAD(gptimers[BC_CPU].base, BC_TIMER));
	writel(TIMER_ENABLE | PERIOD_MODE, TIMER_LOAD(gptimers[BC_CPU].base, BC_TIMER));
	writel(TIMER_INT_EN, TIMER_INT(gptimers[BC_CPU].base, BC_TIMER));
	return 0;
};

static int bctimer_set_state_oneshot_stopped(struct clock_event_device *dev)
{
	unsigned int saved;
	writel(TIMER_INT_CLR, TIMER_INT(gptimers[BC_CPU].base, BC_TIMER));
	saved = readl(TIMER_CTL(gptimers[BC_CPU].base, BC_TIMER)) & PERIOD_MODE;
	writel(TIMER_DISABLE | saved, TIMER_CTL(gptimers[BC_CPU].base, BC_TIMER));
	return 0;
}

static int bctimer_resume(struct clock_event_device *dev)
{
	unsigned int saved;
	saved = readl(TIMER_CTL(gptimers[BC_CPU].base, BC_TIMER)) & PERIOD_MODE;
	writel(TIMER_ENABLE | saved, TIMER_CTL(gptimers[BC_CPU].base, BC_TIMER));
	return 0;
}

static struct clock_event_device bctimer_event = {
	.features = CLOCK_EVT_FEAT_ONESHOT,
	.shift = 32,
	.rating = 150,
	.set_next_event = bctimer_set_next_event,
	.set_state_oneshot = bctimer_set_state_oneshot,
	.set_state_periodic = bctimer_set_state_periodic,
	.set_state_oneshot_stopped = bctimer_set_state_oneshot_stopped,
	.tick_resume = bctimer_resume,
	.name = "bctimer_event",
	.cpumask = cpu_all_mask,
};

static irqreturn_t bctimer_interrupt(int irq, void *cookie)
{
	unsigned int value;
	struct clock_event_device *evt = cookie;

	value = readl(TIMER_INT(gptimers[BC_CPU].base, BC_TIMER));
	value |= TIMER_INT_CLR;
	writel(value, TIMER_INT(gptimers[BC_CPU].base, BC_TIMER));

	if (evt->event_handler)
		evt->event_handler(evt);

	return IRQ_HANDLED;
}

static void __init timer_init(void)
{
	int i, j, ret;
	struct clock_event_device *evt;

	writel(readl((void *) 0x402e0000) | BIT(11) | BIT(10) | BIT(12), (void *) 0x402e0000);
	writel(readl((void *) 0x402e0004) | BIT(9) | BIT(10), (void *) 0x402e0004);

	for (i = 0; i < 4; i++) {
		for (j = 0; j < 3; j++) {
			writel(TIMER_DISABLE, TIMER_CTL(gptimers[i].base, j));
			writel(TIMER_INT_CLR, TIMER_INT(gptimers[i].base, j));
		}
	}

	sched_clock_register(scx35_sched_clock_read, 32, 32768);

	for (i = 0; i < 4; i++) {
		ret = request_irq(gptimers[i].irq, scx35_gptimer_interrupt,
				IRQF_TIMER | IRQF_NOBALANCING | IRQD_IRQ_DISABLED
				| IRQF_PERCPU, "gptimer", local_evt);
		if (ret)
			printk(KERN_ERR "scx35-timer: failed to request irq for %d\n",
					gptimers[i].irq);
	}

	writel(0, TIMER_INT(gptimers[e_cpu].base, SOURCE_TIMER));
	writel(TIMER_DISABLE | TIMER_NEW | PERIOD_MODE, TIMER_CTL(gptimers[e_cpu].base, SOURCE_TIMER));
	writel(ULONG_MAX, TIMER_LOAD(gptimers[e_cpu].base, SOURCE_TIMER));
	writel(TIMER_NEW | PERIOD_MODE, TIMER_CTL(gptimers[e_cpu].base, SOURCE_TIMER));
	writel(TIMER_ENABLE | TIMER_NEW | PERIOD_MODE, TIMER_CTL(gptimers[e_cpu].base, SOURCE_TIMER));

	printk("Registering clocksource device\n");
	if (clocksource_register_hz(&aon_clocksource, 32768))
		printk(KERN_ERR "Couldn't register clocksource\n");

	/* enable syscnt */

	printk("Registering syscnt\n");
	writel(0, SYSCNT_CTL);
	clocksource_mmio_init(SYSCNT_SHADOW_CNT, "syscnt", 1000, 200, 32,
			clocksource_mmio_readw_up);

	evt = &bctimer_event;
	evt->irq = BC_IRQ;
	evt->mult = div_sc(32768, NSEC_PER_SEC, evt->shift);
	evt->max_delta_ns = clockevent_delta2ns(ULONG_MAX, evt);
	evt->min_delta_ns = clockevent_delta2ns(2, evt);

	ret = request_irq(BC_IRQ, bctimer_interrupt, IRQD_IRQ_DISABLED | IRQF_TIMER
			| IRQF_IRQPOLL, "bctimer", evt);
	if (ret)
		pr_err("Failed to register clockevent\n");

	clockevents_register_device(evt);
}

static irqreturn_t syscnt_isr(int irq, void *cookie)
{
	writel(8, SYSCNT_CTL);
	return IRQ_HANDLED;
}

static void __init scx35_timer_of_parse(struct device_node *node)
{
	void *addr = NULL;
	int i, irq, ret;

	syscnt_base = of_iomap(node, 0);
	if (!syscnt_base)
		panic("Failed to map syscnt iomem!\n");

	syscnt_irq = irq_of_parse_and_map(node, 5);
	if (syscnt_irq < 0)
		panic("Failed to map syscnt irq!\n");

	ret = request_irq(syscnt_irq,
			syscnt_isr, IRQF_TRIGGER_HIGH | IRQF_NO_SUSPEND, "syscnt", NULL);
	if (ret)
		panic("Failed to register syscnt irq!\n");

	BC_IRQ = irq_of_parse_and_map(node, 0);
	if (BC_IRQ < 0)
		panic("Failed to map bc irq!\n");

	for (i = 0; i < 4; i++) {
		addr = of_iomap(node, i + 1);
		if (!addr)
			panic("scx35-timer: Failed to map timer MMIO\n");
		gptimers[i].base = addr;

		irq = irq_of_parse_and_map(node, i + 1);
		if (irq < 0)
			panic("scx35-timer: Failed to map timer IRQ\n");
		gptimers[i].irq = irq;
	}
}

static int __init scx35_timer_init(struct device_node *node)
{
	printk("timer-scx35: Initializing\n");
	scx35_timer_of_parse(node);
	timer_init();

	return 0;
}

TIMER_OF_DECLARE(sprd_scx35, "sprd,scx35-timer", scx35_timer_init);
