/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Marvell Helan family PLL clock driver.
 *
 * Copyright (C) 2022
 * Duje Mihanović <duje.mihanovic@skole.hr>
 *
 * Based on vendor driver:
 * Copyright (C) 2012 Marvell
 * Zhoujie Wu <zjwu@marvell.com>
 */

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/clk.h>

#include "clk.h"
#include "clk-pll-helanx.h"

#define pll_readl(reg)				readl_relaxed(reg)
#define pll_readl_cr(p)				pll_readl(p->params->cr)
#define pll_readl_swcr(p)			pll_readl(p->params->swcr)

#define pll_writel(val, reg)			writel_relaxed(val, reg)
#define pll_writel_cr(val, p)			pll_writel(val, p->params->cr)
#define pll_writel_swcr(val, p)			pll_writel(val, p->params->swcr)

#define pll_readl_ssc_ctrl(ssc_params)		pll_readl(ssc_params->ssc_ctrl)
#define pll_readl_ssc_cfg(ssc_params)		pll_readl(ssc_params->ssc_cfg)
#define pll_writel_ssc_ctrl(val, ssc_params)	pll_writel(val, ssc_params->ssc_ctrl)
#define pll_writel_ssc_cfg(val, ssc_params)		pll_writel(val, ssc_params->ssc_cfg)

#define MHZ (1000 * 1000)

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

union pll_ssc_ctrl {
	struct {
		unsigned pi_en:1,
			 reset_pi:1,
			 ssc_mode:1,
			 ssc_clk_en:1,
			 reset_ssc:1,
			 pi_loop_mode:1,
			 clk_det_en:1,
			 reserved:9,
			 intpi:4,
			 intpr:3,
			 reserved1:9;
	} b;
	int v;
};

union pll_ssc_cfg {
	struct {
		unsigned ssc_range:11,
			 reserved:5,
			 ssc_freq_div:16;
	} b;
	int v;
};

static int clk_pll_is_enabled(struct clk_hw *hw)
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

	if (!clk_pll_is_enabled(hw))
		return 0;

	cr.v = pll_readl_cr(vco);
	return DIV_ROUND_UP(4 * 26 * cr.b.fbdiv,
			(cr.b.refdiv ? cr.b.refdiv : 0)) * MHZ;
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

	if (params->freq_table)
		for (int i = 0; i < params->freq_table_size; i++) {
			if (params->freq_table[i].output_rate <= rate &&
					max < params->freq_table[i].output_rate)
				max = params->freq_table[i].output_rate;
		}
	else {
		div = 104;
		rate = rate / MHZ;
		fbd = rate * refd / div;
		max = DIV_ROUND_UP(div * fbd, refd);
		max *= MHZ;
	}
	return max;
}

static struct intpi_range vco_intpi_tbl[] = {
	{2500, 3000, 8},
	{2000, 2500, 6},
	{1500, 2000, 5},
};

static unsigned int clk_vco_freq2intpi(struct clk_vco *vco)
{
	unsigned int vco_freq = clk_vco_get_rate(&vco->hw, 0), intpi = 6, i;

	for (i = 0; i < ARRAY_SIZE(vco_intpi_tbl); i++) {
		if ((vco_freq >= vco_intpi_tbl[i].min) &&
				(vco_freq <= vco_intpi_tbl[i].max)) {
			intpi = vco_intpi_tbl[i].value;
			break;
		}
	}
	BUG_ON(i == ARRAY_SIZE(vco_intpi_tbl));

	return intpi;
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
		}
	}
	BUG_ON(i == ARRAY_SIZE(kvco_rng_table));
}

static void clk_vco_get_ssc_divrng(enum ssc_mode mode, unsigned long rate,
		unsigned int amplitude, unsigned int base, unsigned long vco,
		unsigned int *div, unsigned int *rng)
{
	unsigned int vco_avg;

	BUG_ON(amplitude > (50 * base / 1000));

	switch (mode) {
	case CENTER_SPREAD:
		vco_avg = vco;
		*div = (vco_avg / rate) >> 4;
		break;
	case DOWN_SPREAD:
		vco_avg = vco - (vco >> 1) / base * amplitude;
		*div = (vco_avg / rate) >> 3;
		break;
	default:
		pr_err("Unsupported SSC mode!\n");
		return;
	}

	if (*div == 0)
		*div = 1;

	*rng = BIT(26) / (*div * base / amplitude);
}

static void clk_vco_config_ssc(struct clk_vco *vco, unsigned long new_rate)
{
	struct ssc_params *params = vco->params->ssc_params;
	unsigned int div = 0, rng = 0;
	union pll_ssc_cfg cfg;

	clk_vco_get_ssc_divrng(params->ssc_mode, params->desired_mod_freq,
			params->amplitude, params->base, new_rate, &div, &rng);

	cfg.v = pll_readl_ssc_cfg(params);
	cfg.b.ssc_freq_div = div & 0xfff0;
	cfg.b.ssc_range = rng;
	pll_writel_ssc_cfg(cfg.v, params);
}

static int clk_vco_set_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long parent_rate)
{
	unsigned int i, kvco = 0, vcovnrg, refd, fbd;
	unsigned long flags;
	struct clk_vco *vco = to_clk_vco(hw);
	struct mmp_vco_params *params = vco->params;
	union pll_swcr swcr;
	union pll_cr cr;

	if (clk_pll_is_enabled(hw)) {
		pr_err("%s: clock is enabled, ignoring set_rate\n", __func__);
		return 0;
	}

	rate /= MHZ;

	if (params->freq_table) {
		for (i = 0; i < params->freq_table_size; i++) {
			if (rate == params->freq_table[i].output_rate) {
				kvco = params->freq_table[i].kvco;
				vcovnrg = params->freq_table[i].vcovnrg;
				refd = params->freq_table[i].refd;
				fbd = params->freq_table[i].fbd;
				break;
			}
		}
		BUG_ON(i == params->freq_table_size);
	} else {
		clk_vco_rate2rng(vco, rate, &kvco, &vcovnrg);
		/*
		 * According to vendor, refd needs to be calculated with some
		 * sort of function rather than a number, but no such function
		 * exists.
		 */
		refd = 3;
		fbd = rate * refd / 104;
	}

	spin_lock_irqsave(vco->lock, flags);
	swcr.v = pll_readl_swcr(vco);
	swcr.b.kvco = kvco;
	pll_writel_swcr(swcr.v, vco);

	cr.v = pll_readl_cr(vco);
	cr.b.refdiv = refd;
	cr.b.fbdiv = fbd;
	pll_writel_cr(cr.v, vco);
	spin_unlock_irqrestore(vco->lock, flags);

	if (vco->flags & HELANX_VCO_SSC_FEAT)
		clk_vco_config_ssc(vco, rate);
	return 0;
}

static void clk_vco_enable_ssc(struct clk_vco *vco)
{
	union pll_ssc_ctrl ctrl;
	struct ssc_params *params = vco->params->ssc_params;;

	ctrl.v = pll_readl_ssc_ctrl(params);
	ctrl.b.intpi = clk_vco_freq2intpi(vco);
	ctrl.b.intpr = 4;
	ctrl.b.ssc_mode = params->ssc_mode;
	ctrl.b.pi_en = 1;
	ctrl.b.clk_det_en = 1;
	ctrl.b.reset_pi = 1;
	ctrl.b.reset_ssc = 1;
	ctrl.b.pi_loop_mode = 0;
	ctrl.b.ssc_clk_en = 0;
	pll_writel_ssc_ctrl(ctrl.v, params);

	udelay(2);
	ctrl.b.reset_ssc = 0;
	ctrl.b.reset_pi = 0;
	pll_writel_ssc_ctrl(ctrl.v, params);

	udelay(2);
	ctrl.b.pi_loop_mode = 1;
	pll_writel_ssc_ctrl(ctrl.v, params);

	udelay(2);
	ctrl.b.ssc_clk_en = 1;
	pll_writel_ssc_ctrl(ctrl.v, params);
}

static int clk_vco_enable(struct clk_hw *hw)
{
	unsigned int delaytime = 14;
	unsigned long flags;
	struct clk_vco *vco = to_clk_vco(hw);
	struct mmp_vco_params *params = vco->params;
	union pll_cr cr;

	if (clk_pll_is_enabled(hw))
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

	if (vco->flags & HELANX_VCO_SSC_FEAT)
		if (((vco->flags & HELANX_VCO_SSC_AON) && !params->ssc_enabled)
				|| !(vco->flags & HELANX_VCO_SSC_AON)) {
			clk_vco_enable_ssc(vco);
			params->ssc_enabled = true;
		}

	return 0;
}

static void clk_vco_disable_ssc(struct clk_vco *vco)
{
	struct ssc_params *params = vco->params->ssc_params;
	union pll_ssc_ctrl ctrl;

	ctrl.v = pll_readl_ssc_ctrl(params);
	ctrl.b.ssc_clk_en = 0;
	pll_writel_ssc_ctrl(ctrl.v, params);
	udelay(100);

	ctrl.b.pi_loop_mode = 0;
	pll_writel_ssc_ctrl(ctrl.v, params);
	udelay(2);

	ctrl.b.pi_en = 0;
	pll_writel_ssc_ctrl(ctrl.v, params);
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

	if ((vco->flags & HELANX_VCO_SSC_FEAT) &&
			!(vco->flags & HELANX_VCO_SSC_AON)) {
		clk_vco_disable_ssc(vco);
		vco->params->ssc_enabled = false;
	}
}

static int clk_vco_init(struct clk_hw *hw)
{
	struct clk_vco *vco = to_clk_vco(hw);
	unsigned long vco_rate;
	struct mmp_vco_params *params = vco->params;

	if (!clk_pll_is_enabled(hw)) {
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
	} else {
		vco_rate = clk_vco_get_rate(hw, 0) / MHZ;
		if (vco->flags & HELANX_VCO_SSC_FEAT) {
			clk_vco_config_ssc(vco, vco_rate);
			clk_vco_enable_ssc(vco);
			params->ssc_enabled = true;
		}
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
	.is_enabled = clk_pll_is_enabled,
};

struct clk *helanx_register_clk_vco(const char *name, const char *parent_name,
		unsigned long flags, u32 vco_flags, spinlock_t *lock,
		struct mmp_vco_params *params)
{
	struct clk_vco *vco;
	struct clk *clk;
	struct clk_init_data *init;

	vco = kzalloc(sizeof(*vco), GFP_KERNEL);
	if (!vco)
		return NULL;
	init = kzalloc(sizeof(*init), GFP_KERNEL);
	if (!init)
		return NULL;

	init->name = name;
	init->ops = &clk_vco_ops;
	init->flags = flags | CLK_SET_RATE_GATE;
	init->parent_names = (parent_name ? &parent_name : NULL);
	init->num_parents = (parent_name ? 1 : 0);

	vco->flags = vco_flags;
	vco->lock = lock;
	vco->hw.init = init;
	vco->params = params;

	clk = clk_register(NULL, &vco->hw);
	if (IS_ERR(clk)) {
		kfree(vco);
		kfree(init);
	}

	return clk;
}

static unsigned int clk_pll_calc_div(struct clk_pll *pll, unsigned long rate,
		unsigned long parent_rate)
{
	rate /= MHZ;
	parent_rate /= MHZ;

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

	swcr.v = pll_readl_swcr(pll);
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
	swcr.v = pll_readl_swcr(pll);
	if (pll->flags & HELANX_PLLOUT)
		swcr.b.se_div_sel = div;
	else
		swcr.b.diff_div_sel = div;
	pll_writel_swcr(swcr.v, pll);
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

static struct clk_ops clk_pll_ops = {
	.init = clk_pll_init,
	.set_rate = clk_pll_set_rate,
	.recalc_rate = clk_pll_get_rate,
	.round_rate = clk_pll_round_rate,
	.is_enabled = clk_pll_is_enabled,
};

struct clk *helanx_register_clk_pll(const char *name, const char *parent_name,
		unsigned long flags, u32 pll_flags, spinlock_t *lock,
		struct mmp_pll_params *params)
{
	struct clk_pll *pll;
	struct clk *clk;
	struct clk_init_data *init;

	pll = kzalloc(sizeof(*pll), GFP_KERNEL);
	if (!pll)
		return NULL;
	init = kzalloc(sizeof(*init), GFP_KERNEL);
	if (!init)
		return NULL;

	init->name = name;
	init->ops = &clk_pll_ops;
	init->flags = flags | CLK_SET_RATE_GATE;
	init->parent_names = (parent_name ? &parent_name : NULL);
	init->num_parents = (parent_name ? 1 : 0);

	pll->flags = pll_flags;
	pll->lock = lock;
	pll->params = params;
	pll->hw.init = init;

	clk = clk_register(NULL, &pll->hw);
	if (IS_ERR(clk)) {
		kfree(pll);
		kfree(init);
	}

	return clk;
}
