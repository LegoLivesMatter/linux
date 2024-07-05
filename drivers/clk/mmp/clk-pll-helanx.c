/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Marvell Helan family PLL clock driver.
 *
 * Copyright (C) 2024
 * Duje Mihanović <duje.mihanovic@skole.hr>
 *
 * Based on vendor driver:
 * Copyright (C) 2012 Marvell
 * Zhoujie Wu <zjwu@marvell.com>
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/units.h>

#include "clk.h"
#include "clk-pll-helanx.h"

#define pll_readl(reg)				readl_relaxed(reg)
#define pll_readl_cr(p)				pll_readl(p->params->cr)
#define pll_readl_swcr(p)			pll_readl(p->params->swcr)

#define pll_writel(val, reg)			writel_relaxed(val, reg)
#define pll_writel_cr(val, p)			pll_writel(val, p->params->cr)
#define pll_writel_swcr(val, p)			pll_writel(val, p->params->swcr)

union pll_cr {
	struct {
		unsigned refdiv:5,
			 fbdiv:9,
			 reserved:5,
			 pu:1,
			 reserved1:12;
	} b;
	int v;
};

union pll_swcr {
	struct {
		unsigned avvd1815_sel:1,
			 vddm:2,
			 vddl:3,
			 icp:4,
			 pll_bw_sel:1,
			 kvco:4,
			 ctune:2,
			 diff_div_sel:3,
			 se_div_sel:3,
			 diff_en:1,
			 bypass_en:1,
			 se_gating_en:1,
			 fd:3,
			 reserved:3;
	} b;
	int v;
};

static int clk_vco_is_enabled(struct clk_hw *hw)
{
	struct clk_vco *vco = to_clk_vco(hw);
	union pll_cr cr;

	cr.v = pll_readl_cr(vco);
	return cr.b.pu;
}

static unsigned long clk_vco_get_rate(struct clk_hw *hw, unsigned long parent_rate)
{
	struct clk_vco *vco = to_clk_vco(hw);
	union pll_cr cr;

	if (!clk_vco_is_enabled(hw))
		return 0;

	cr.v = pll_readl_cr(vco);
	return DIV_ROUND_UP(4 * 26 * cr.b.fbdiv,
			(cr.b.refdiv ? cr.b.refdiv : 0)) * HZ_PER_MHZ;
}

static long clk_vco_round_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long *parent_rate)
{
	struct clk_vco *vco = to_clk_vco(hw);
	int fbd, refd = 3, div;
	unsigned long max = 0;
	struct mmp_vco_params *params = vco->params;

	if (rate > params->vco_max || rate < params->vco_min) {
		pr_err("Rate %lu out of range!\n", rate);
		return -EINVAL;
	}

	div = 104;
	rate = rate / HZ_PER_MHZ;
	fbd = rate * refd / div;
	max = DIV_ROUND_UP(div * fbd, refd);
	max *= HZ_PER_MHZ;

	return max;
}

static struct kvco_range kvco_rng_table[] = {
	{2600, 3000, 0xf, 0},
	{2400, 2600, 0xe, 0},
	{2200, 2400, 0xd, 0},
	{2000, 2200, 0xc, 0},
	{1750, 2000, 0xb, 0},
	{1500, 1750, 0xa, 0},
	{1350, 1500, 0x9, 0},
	{1200, 1350, 0x8, 0},
};

static void clk_vco_rate2rng(struct clk_vco *vco, unsigned long rate,
		unsigned int *kvco, unsigned int *vco_rng)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(kvco_rng_table); i++) {
		if (rate >= kvco_rng_table[i].vco_min &&
				rate <= kvco_rng_table[i].vco_max) {
			*kvco = kvco_rng_table[i].kvco;
			*vco_rng = kvco_rng_table[i].vrng;
			return;
		}
	}
	BUG_ON(i == ARRAY_SIZE(kvco_rng_table));
}

static int clk_vco_enable(struct clk_hw *hw)
{
	unsigned int delaytime = 14;
	unsigned long flags;
	struct clk_vco *vco = to_clk_vco(hw);
	struct mmp_vco_params *params = vco->params;
	union pll_cr cr;

	if (clk_vco_is_enabled(hw))
		return 0;

	spin_lock_irqsave(vco->lock, flags);
	cr.v = pll_readl_cr(vco);
	cr.b.pu = 1;
	pll_writel_cr(cr.v, vco);
	spin_unlock_irqrestore(vco->lock, flags);

	udelay(30);
	while (!(readl_relaxed(params->lock_reg) & params->lock_enable_bit)
			&& delaytime) {
		udelay(5);
		delaytime--;
	}
	BUG_ON(!delaytime);

	return 0;
}

static void clk_vco_disable(struct clk_hw *hw)
{
	unsigned long flags;
	struct clk_vco *vco = to_clk_vco(hw);
	union pll_cr cr;

	spin_lock_irqsave(vco->lock, flags);
	cr.v = pll_readl_cr(vco);
	cr.b.pu = 0;
	pll_writel_cr(cr.v, vco);
	spin_unlock_irqrestore(vco->lock, flags);
}

static int clk_vco_set_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long parent_rate)
{
	unsigned int kvco = 0, vcovnrg, refd, fbd;
	unsigned long flags;
	struct clk_vco *vco = to_clk_vco(hw);
	union pll_swcr swcr;
	union pll_cr cr;
	int reenable = 0;

	if (clk_vco_is_enabled(hw)) {
		pr_info("%s: %s is enabled, disabling\n", __func__, clk_hw_get_name(hw));
		clk_vco_disable(hw);
		reenable = 1;
	}

	rate /= HZ_PER_MHZ;

	clk_vco_rate2rng(vco, rate, &kvco, &vcovnrg);
	/*
	 * According to vendor, refd needs to be calculated with some
	 * sort of function rather than a number, but no such function
	 * exists.
	 */
	refd = 3;
	fbd = rate * refd / 104;

	spin_lock_irqsave(vco->lock, flags);
	swcr.v = pll_readl_swcr(vco);
	swcr.b.kvco = kvco;
	pll_writel_swcr(swcr.v, vco);

	cr.v = pll_readl_cr(vco);
	cr.b.refdiv = refd;
	cr.b.fbdiv = fbd;
	pll_writel_cr(cr.v, vco);
	spin_unlock_irqrestore(vco->lock, flags);

	if (reenable)
		clk_vco_enable(hw);

	return 0;
}

static int clk_vco_init(struct clk_hw *hw)
{
	struct clk_vco *vco = to_clk_vco(hw);

	if (!clk_vco_is_enabled(hw)) {
		union pll_swcr swcr;
		swcr.v = pll_readl_swcr(vco);
		swcr.b.avvd1815_sel = 1;
		swcr.b.vddm = 1;
		swcr.b.vddl = 4;
		swcr.b.icp = 3;
		swcr.b.pll_bw_sel = 0;
		swcr.b.ctune = 1;
		swcr.b.diff_en = 1;
		swcr.b.bypass_en = 0;
		swcr.b.se_gating_en = 0;
		swcr.b.fd = 4;
		pll_writel_swcr(swcr.v, vco);
	}

	return 0;
}

static struct clk_ops clk_vco_ops = {
	.init = clk_vco_init,
	.enable = clk_vco_enable,
	.disable = clk_vco_disable,
	.set_rate = clk_vco_set_rate,
	.recalc_rate = clk_vco_get_rate,
	.round_rate = clk_vco_round_rate,
	.is_enabled = clk_vco_is_enabled,
};

struct clk *helanx_register_clk_vco(const char *name, const char *parent_name,
		unsigned long flags, u32 vco_flags, spinlock_t *lock,
		struct mmp_vco_params *params)
{
	struct clk_vco *vco;
	struct clk *clk;
	struct clk_init_data init;

	vco = kzalloc(sizeof(*vco), GFP_KERNEL);
	if (!vco)
		return NULL;

	init.name = name;
	init.ops = &clk_vco_ops;
	init.flags = flags | CLK_SET_RATE_GATE;
	init.parent_names = (parent_name ? &parent_name : NULL);
	init.num_parents = (parent_name ? 1 : 0);

	vco->flags = vco_flags;
	vco->lock = lock;
	vco->hw.init = &init;
	vco->params = params;

	clk = clk_register(NULL, &vco->hw);
	if (IS_ERR(clk)) {
		kfree(vco);
	}

	return clk;
}

static unsigned int clk_pll_calc_div(struct clk_pll *pll, unsigned long rate,
		unsigned long parent_rate)
{
	rate /= HZ_PER_MHZ;
	parent_rate /= HZ_PER_MHZ;

	for (int i = 1; i < HELANX_DIVIDER_COUNT; i++) {
		if ((rate <= parent_rate / BIT(i-1)) &&
				(rate > parent_rate / BIT(i)))
			return i - 1;
	}
	return 1;
}

static unsigned long clk_pll_get_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct clk_pll *pll = to_clk_pll(hw);
	union pll_swcr swcr;

	swcr.v = readl_relaxed(pll->swcr);
	if (pll->flags & HELANX_PLLOUT)
		return parent_rate / BIT(swcr.b.se_div_sel);
	else
		return parent_rate / BIT(swcr.b.diff_div_sel);
}

static int clk_pll_set_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long parent_rate)
{
	struct clk_pll *pll = to_clk_pll(hw);
	unsigned int div = clk_pll_calc_div(pll, rate, parent_rate);
	unsigned long flags;
	union pll_swcr swcr;

	spin_lock_irqsave(pll->lock, flags);
	swcr.v = readl_relaxed(pll->swcr);
	if (pll->flags & HELANX_PLLOUT)
		swcr.b.se_div_sel = div;
	else
		swcr.b.diff_div_sel = div;
	writel_relaxed(swcr.v, pll->swcr);
	spin_unlock_irqrestore(pll->lock, flags);

	return 0;
}

static long clk_pll_round_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long *parent_rate)
{
	/* TODO: do this properly */
	return rate;
}

static int clk_pll_init(struct clk_hw *hw)
{
	return 0;
}

static int clk_pll_is_enabled(struct clk_hw *hw)
{
	return clk_vco_is_enabled(clk_hw_get_parent(hw));
}

static struct clk_ops clk_pll_ops = {
	.init = clk_pll_init,
	.set_rate = clk_pll_set_rate,
	.recalc_rate = clk_pll_get_rate,
	.round_rate = clk_pll_round_rate,
	.is_enabled = clk_pll_is_enabled,
};

struct clk *helanx_register_clk_pll(const char *name, const char *parent_name,
		unsigned long flags, u32 pll_flags, spinlock_t *lock,
		void __iomem *swcr)
{
	struct clk_pll *pll;
	struct clk *clk;
	struct clk_init_data init;

	pll = kzalloc(sizeof(*pll), GFP_KERNEL);
	if (!pll)
		return NULL;

	init.name = name;
	init.ops = &clk_pll_ops;
	init.flags = flags | CLK_SET_RATE_GATE;
	init.parent_names = (parent_name ? &parent_name : NULL);
	init.num_parents = (parent_name ? 1 : 0);

	pll->flags = pll_flags;
	pll->lock = lock;
	pll->swcr = swcr;
	pll->hw.init = &init;

	clk = clk_register(NULL, &pll->hw);
	if (IS_ERR(clk)) {
		kfree(pll);
	}

	return clk;
}
