// SPDX-License-Identifier: GPL-2.0-only
/*
 *
 * Copyright (C) 2014 Marvell
 */

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/stat.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/reboot.h>
#include <linux/irqflags.h>

#define REBOOT_TIME (0x20)
#define MPMU_APRR	(0x1020)
#define MPMU_APRR_WDTR	(1<<4)

/* Watchdog Timer Registers Offset */
#define TMR_WMER	(0x0064)
#define TMR_WMR		(0x0068)
#define TMR_WVR		(0x006c)
#define TMR_WCR		(0x0098)
#define TMR_WSR		(0x0070)
#define TMR_WFAR	(0x009c)
#define TMR_WSAR	(0x00A0)

static struct resource *rtc_br0_mem, *wdt_mem, *mpmu_mem;
static void __iomem *rtc_br0_reg, *wdt_base, *mpmu_base;

/* use watchdog to reset system */
void pxa_wdt_reset(void __iomem *watchdog_virt_base, void __iomem *mpmu_vaddr)
{
	u32 reg;
	void __iomem *mpmu_aprr;

	BUG_ON(!watchdog_virt_base);
	BUG_ON(!mpmu_vaddr);

	/* reset counter */
	writel(0xbaba, watchdog_virt_base + TMR_WFAR);
	writel(0xeb10, watchdog_virt_base + TMR_WSAR);
	writel(0x1, watchdog_virt_base + TMR_WCR);

	/*
	 * enable WDT
	 * 1. write 0xbaba to match 1st key
	 * 2. write 0xeb10 to match 2nd key
	 * 3. enable wdt count, generate interrupt when expires
	 */
	writel(0xbaba, watchdog_virt_base + TMR_WFAR);
	writel(0xeb10, watchdog_virt_base + TMR_WSAR);
	writel(0x3, watchdog_virt_base + TMR_WMER);

	/* negate hardware reset to the WDT after system reset */
	mpmu_aprr = mpmu_vaddr + MPMU_APRR;
	reg = readl(mpmu_aprr) | MPMU_APRR_WDTR;
	writel(reg, mpmu_aprr);

	/* clear previous WDT status */
	writel(0xbaba, watchdog_virt_base + TMR_WFAR);
	writel(0xeb10, watchdog_virt_base + TMR_WSAR);
	writel(0, watchdog_virt_base + TMR_WSR);

	writel(0xbaba, watchdog_virt_base + TMR_WFAR);
	writel(0xeb10, watchdog_virt_base + TMR_WSAR);
	writel(REBOOT_TIME, watchdog_virt_base + TMR_WMR);
}

static int do_pxa_reset(struct notifier_block *this, unsigned long mode,
						void *data)
{
	u32 backup;
	int i;
	const char *cmd = "bootloader";

	pr_emerg("lets try\n");

	if (cmd && (!strcmp(cmd, "recovery")
		|| !strcmp(cmd, "bootloader") || !strcmp(cmd, "boot")
		|| !strcmp(cmd, "product") || !strcmp(cmd, "prod")
		|| !strcmp(cmd, "fastboot") || !strcmp(cmd, "fast"))) {
		for (i = 0, backup = 0; i < 4; i++) {
			backup <<= 8;
			backup |= *(cmd + i);
		}
		do {
			writel(backup, rtc_br0_reg);
		} while (readl(rtc_br0_reg) != backup);
	}

	pr_emerg("lets reset\n");

	pxa_wdt_reset(wdt_base, mpmu_base);

	/* Give a grace period for failure to restart of 1s */
	mdelay(1000);

	local_irq_enable();
	pr_emerg("Restart failed\n");

	return NOTIFY_DONE;
}

static struct notifier_block pxa_restart_nb = {
	.notifier_call = do_pxa_reset,
	.priority = 128,
};

static const struct of_device_id pxa_reset_of_match[] = {
	{.compatible = "marvell,pxa-reset",},
	{}
};

static int pxa_reset_probe(struct platform_device *pdev)
{
	int ret;

	wdt_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (wdt_mem == NULL) {
		dev_err(&pdev->dev, "no memory resource specified for WDT\n");
		return -ENOENT;
	}

	wdt_base = devm_ioremap(&pdev->dev, wdt_mem->start,
					resource_size(wdt_mem));
	if (IS_ERR(wdt_base))
		return PTR_ERR(wdt_base);

	mpmu_mem = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (mpmu_mem == NULL) {
		dev_err(&pdev->dev, "no memory resource specified for MPMU\n");
		return -ENOENT;
	}

	mpmu_base = devm_ioremap(&pdev->dev, mpmu_mem->start,
					 resource_size(mpmu_mem));
	if (IS_ERR(mpmu_base))
		return PTR_ERR(mpmu_base);

	rtc_br0_mem = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (rtc_br0_mem == NULL) {
		dev_err(&pdev->dev, "no memory resource specified\n");
		return -ENOENT;
	}

	rtc_br0_reg = devm_ioremap(&pdev->dev, rtc_br0_mem->start,
					resource_size(rtc_br0_mem));
	if (IS_ERR(rtc_br0_reg))
		return PTR_ERR(rtc_br0_reg);
	ret = register_restart_handler(&pxa_restart_nb);
	if (ret)
		return ret;

	dev_info(&pdev->dev, "Reboot driver registered\n");
	return 0;
}

MODULE_DEVICE_TABLE(of, pxa_reset_of_match);

static struct platform_driver pxa_reset_driver = {
	.probe = pxa_reset_probe,
	.driver = {
		.name = "pxa-reset",
		.of_match_table = of_match_ptr(pxa_reset_of_match),
	},
};

static int __init pxa_reset_init(void)
{
	return platform_driver_register(&pxa_reset_driver);
}

device_initcall(pxa_reset_init);
