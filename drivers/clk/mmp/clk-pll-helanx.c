#include <linux/kernel.h>
#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/slab.h>

#include "clk.h"
#include "clk-pll-helanx.h"

#define pll_readl(reg)			readl_relaxed(reg)
#define pll_readl_cr(pll)		pll_readl(pll->params->cr_reg)
#define pll_readl_pll_swcr(pll)		pll_readl(pll->params->pll_swcr)
#define pll_readl_sscctrl(ssc_params)	pll_readl(ssc_params->ssc_ctrl)
#define pll_readl_ssccfg(ssc_params)	pll_readl(ssc_params->ssc_cfg)

#define pll_writel(val, reg)		writel_relaxed(val, reg)
#define pll_writel_cr(val, pll)		pll_writel(val, pll->params->cr_reg)
#define pll_writel_pll_swcr(val, pll)	pll_writel(val, pll->params->pll_swcr)
#define pll_writel_sscctrl(val, ssc_params) \
	pll_writel(val, ssc_params->ssc_ctrl)
#define pll_writel_ssccfg(val, ssc_params) \
	pll_writel(val, ssc_params->ssc_cfg)

#define MHZ (1000*1000)

union pll_cr {
	struct {
		unsigned refdiv:5,
			 fbdev:9,
			 reserved:5,
			 pu:1,
			 reserved1:12
	} b;
	unsigned int v;
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
			 reserved:3
	} b;
	unsigned int v;
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
			 reserved1:9
	} b;
	unsigned int v;
};

union pll_ssc_conf {
	struct {
		unsigned ssc_range:11,
			 reserved:5,
			 ssc_freq_div:16
	} b;
	unsigned int v;
};

static unsigned int __get_vco_freq(struct clk_hw *hw);

static struct intpi_range pll_intpi_tbl[] = {
	{2500, 3000, 8},
	{2000, 2500, 6},
	{1500, 2000, 5},
};

static unsigned int __clk_pll_vco2intpi(struct clk_vco *vco)
{
	unsigned int intpi, vco_rate, i;

	vco_rate = __get_vco_freq(&vco->hw) / MHZ;
	intpi = 6;

	for (i = 0; i < ARRAY_SIZE(pll_intpi_tbl); i++) {
		if ((vco_rate >= pll_intpi_tbl[i].vco_min) &&
			(vco_rate <= pll_intpi_tbl[i].vco_max)) {
				intpi =  pll_intpi_tbl[i].value;
				break;
		}
	}

	if (i == ARRAY_SIZE(pll_intpi_tbl))
		pr_err("Unsupported VCO frequency for intpi\n");

	return intpi;
}

static void __clk_get_sscdivrng(enum ssc_mode mode, unsigned long rate,
		unsigned int amplitude, unsigned int base, unsigned long vco,
		unsigned int *div, unsigned int *rng)
{
	unsigned int vco_avg;

	if (amplitude > (50 * base / 1000))
		pr_err("Amplitude cannot exceed 5\n");

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
		pr_err("Unsupported SSC mode\n");
		return;
	}

	if (*div == 0)
		*div = 1;

	*rng = (1 << 26) / (*div * base / amplitude);
}

static void config_ssc(struct clk_vco *vco, unsigned long new_rate)
{
	struct ssc_params *ssc_params = vco->params->ssc_params;
	unsigned int div = 0, rng = 0;
	union pll_ssc_conf ssc_conf;

	__clk_get_sscdivrng(ssc_params->ssc_mode,
			ssc_params->desired_mod_freq,
			ssc_params->amplitude,
			ssc_params->base, new_rate, &div, &rng);

	ssc_conf.v = pll_readl_ssccfg(ssc_params);
	ssc_conf.b.ssc_freq_div = div & 0xfff0;
	ssc_conf.b.ssc_range = rng;
	pll_writel_ssccfg(ssc_conf.v, ssc_params);
}

static void __enable_ssc(struct ssc_params *params, unsigned int intpi)
{
	union pll_ssc_ctrl ctrl;

	ctrl.v = pll_readl_sscctrl(ssc_params);
	ctrl.b.intpi = intpi;
	ctrl.b.intpr = 4;
	ctrl.b.ssc_mode = ssc_params->ssc_mode;
	ctrl.b.pi_en = 1;
	ctrl.b.clk_det_en = 1;
	ctrl.b.reset_pi = 1;
	ctrl.b.reset_ssc = 1;
	ctrl.b.pi_loop_mode = 0;
	ctrl.b.ssc_clk_en = 0;
	pll_writel_sscctrl(ctrl.v, ssc_params);

	udelay(2);
	ctrl.b.reset_ssc = 0;
	ctrl.b.reset_pi = 0;
	pll_writel_sscctrl(ctrl.v, ssc_params);

	udelay(2);
	ctrl.b.pi_loop_mode = 1;
	pll_writel_sscctrl(ctrl.v, ssc_params);

	udelay(2);
	ctrl.b.ssc_clk_en = 1;
	pll_writel_sscctrl(ctrl.v, ssc_params);
}

static void enable_pll_ssc(struct clk_vco *vco)
{
	struct ssc_params *params = vco->params->ssc_params;
	unsigned int intpi = __clk_pll_vco2intpi(vco);

	__enable_ssc(params, intpi);
}

static void __disable_ssc(struct ssc_params *params)
{
	union pll_ssc_ctrl ctrl;
	ctrl.v = pll_read_sscctrl(params);
	ctrl.b.ssc_clk_en = 0;
	pll_writel_sscctrl(ctrl.v, params);
	udelay(100);

	ctrl.b.pi_loop_mode = 0;
	pll_writel_sscctrl(ctrl.v, params);
	udelay(2);

	ctrl.b.pi_en = 0;
	pll_writel_sscctrl(ctrl.v, params);
}

static void disable_pll_ssc(struct clk_vco *vco)
{
	struct ssc_params *params = vco->params->ssc_params;

	__disable_ssc(params);
}

static struct kvco_range kvco_rng_table_28nm[] = {
	{2600, 3000, 15, 0},
	{2400, 2600, 14, 0},
	{2200, 2400, 13, 0},
	{2000, 2200, 12, 0},
	{1750, 2000, 11, 0},
	{1500, 1750, 10, 0},
	{1350, 1500, 9, 0},
	{1200, 1350, 8, 0},
};

static void  __clk_vco_rate2rng(struct clk_vco *vco, unsigned long rate,
		unsigned int *kvco, unsigned int *vco_rng)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(kvco_rng_table_28nm); i++) {
		if (rate >= kvco_rng_table_28nm[i].vco_min &&
			rate <= kvco_rng_table_28nm[i].vco_max) {
			*kvco = kvco_rng_table_28nm[i].kvco;
			*vco_rng = kvco_rng_tbl[i].vrng;
			return;
		}
	}
	BUG_ON(i == size);
	return;
}

static int __pll_is_enabled(struct clk_hw *hw)
{
	struct clk_vco *vco = to_clk_vco(hw);
	union pll_cr cr;

	cr.v = pll_readl_cr(vco);
	/* Values 1-3 mean PLL is enabled, 0 means it's disabled. */
	return cr.b.pu & 1;
}

static unsigned int __get_vco_freq(struct clk_hw *hw)
{
	unsigned int pll_vco, pllrefd, pllfbd;
	struct clk_vco *vco = to_clk_vco(hw);
	union pll_cr cr;

	if (!__pll_is_enabled(hw))
		return 0;

	cr.v = pll_readl_cr(vco);
	pllrefd = cr.b.refdiv;
	pllfbd = cr.b.fbdiv;

	if (!pllrefd)
		pllrefd = 1;
	pll_vco = DIV_ROUND_UP(4 * 26 * pllfbd, pllrefd);

	hw->clk->rate = pll_vco * MHZ;
	return pll_vco * MHZ;
}

static void __pll_vco_cfg(struct clk_vco *vco)
{
	union pll_swcr swcr;

	swcr.v = pll_readl_pll_swcr(vco);
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
	pll_writel_pll_swcr(swcr.v, vco);
}

static void clk_pll_vco_init(struct clk_hw *hw)
{
	struct clk_vco *vco = to_clk_vco(hw);
	unsigned long vco_rate;
	unsigned int vco_rngl, vco_rngh, tmp;
	struct mmp_vco_params *params = vco->params;

	if (!__pll_is_enabled(hw)) {
		pr_info("%s is not enabled", hw->clk->name);
		__pll_vco_cfg(vco);
	} else {
		vco_rate = __get_vco_freq(hw) / MHZ;
		if (vco->flags & HELANX_PLL_SKIP_DEF_RATE) {
			hw->clk->rate = vco_rate * MHZ;
		} else {
			tmp = params->default_rate / MHZ;
			if (tmp != vco_rate) {
				vco_rngh = tmp + tmp * 2 / 100;
				vco_rngl = tmp + tmp * 2 / 100;
				BUG_ON(!((vco_rngl <= vco_rate) &&
					(vco_rate <= vco_rngh)));
			}
			hw->clk->rate = params->default_rate;
		}

		if (vco->flags & HELANX_PLL_SSC_FEAT) {
			config_ssc(vco, hw->clk->rate);
			enable_pll_ssc(vco);
			params->ssc_enabled = true;
		}

		pr_info("%s has been enabled @ %luMHz\n", hw->clk->name, vco_rate);
	}
}

static int clk_pll_vco_enable(struct clk_hw *hw)
{
	unsigned int delaytime = 14;
	unsigned long flags;
	struct clk_vco *vco = to_clk_vco(hw);
	struct mmp_vco_params *params = vco->params;
	union pll_cr cr;

	if (__pll_is_enabled(hw))
		return 0;

	spin_lock_irqsave(vco->lock, flags);
	cr.v = pll_readl_cr(vco);
	cr.b.pu = 1;
	pll_writel_cr(cr.v, vco);
	spin_unlock_irqrestore(vco->lock, flags);

	udelay(30);
	while ((!(__raw_readl(params->lock_reg) & params->lock_enable_bit))
		&& delaytime) {
		udelay(5);
		delaytime--;
	}
	BUG_ON(!delaytime);

	if (vco->flags & HELANX_PLL_SSC_FEAT)
		if (((vco->flags & HELANX_PLL_SSC_AON) && !params->ssc_enabled)
		|| !(vco->flags & HELANX_PLL_SSC_AON)) {
			enable_pll_ssc(vco);
			params->ssc_enabled = true;
		}

	return 0;
}

static void clk_pll_vco_disable(struct clk_hw *hw)
{
	unsigned long flags;
	struct clk_vco *vco = to_clk_vco(hw);
	struct mmp_vco_params *params = vco->params;
	union pll_cr cr;

	spin_lock_irqsave(vco->lock, flags);
	cr.v = pll_readl_cr(vco);
	cr.b.pu = 0;
	pll_writel_cr(cr.v);
	spin_unlock_irqrestore(vco->lock, flags);

	if ((vco->flags & HELANX_PLL_SSC_FEAT) &&
			!(vco->flags & HELANX_PLL_SSC_AON)) {
		disable_pll_ssc(vco);
		params->ssc_enabled = false;
	}
}

static int clk_pll_vco_setrate(struct clk_hw *hw, unsigned long rate,
		unsigned long parent_rate)
{
	unsigned int i, kvco = 0, vcovnrg, refd, fbd;
	unsigned long flags, old_rate = hw->clk->rate;
	struct clk_vco *vco = to_clk_vco(hw);
	struct mmp_vco_params *params = vco->params;
	union pll_swcr swcr;
	union pll_cr cr;

	if (__pll_is_enabled(hw)) {
		pr_err("%s is enabled, ignoring setrate!\n",
			hw->clk->name);
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
		__clk_vco_rate2rng(vco, rate, &kvco, &vcovnrg);
		/* According to downstream, refd should be calculated with some
		 * function instead of using a fixed number. No such function
		 * has been found, so we continue to use a number. */
		refd = 3;
		fbd = rate * refd / 104;
	}

	spin_lock_irqsave(vco->lock, flags);
	swcr.v = pll_readl_pll_swcr(vco);
	swcr.b.kvco = kvco;
	pll_writel_pll_swcr(swcr.v, vco);

	cr.v = pll_readl_cr(vco);
	cr.b.refdiv = refd;
	cr.b.fbdiv = fbd;
	pll_writel_cr(cr.v, vco);

	hw->clk->rate = rate;
	spin_unlock_irqrestore(vco->lock, flags);

	if (vco->flags & HELANX_PLL_SSC_FEAT)
		config_ssc(vco, rate);

	pr_debug("%s has been reclocked from %lu to %lu\n",
		hw->clk->name, old_rate, rate);
	return 0;
}

static unsigned long clk_vco_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	return hw->clk->rate;
}

static long clk_vco_round_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long *parent_rate)
{
	struct clk_vco *vco = to_clk_vco(hw);
	int fbd, refd = 3, div;
	unsigned long max_rate = 0;
	int i;
	struct mmp_vco_params *params = vco->params;

	if (rate > params->vco_max || rate < params->vco_min) {
		pr_err("%luMHz out of range!\n", rate);
		return -EINVAL;
	}

	if (params->freq_table) {
		for (i = 0; i < params->freq_table_size; i++) {
			if (params->freq_table[i].output_rate <= rate) {
				if (max_rate <
				params->freq_table[i].output_rate)
					max_rate =
						params->freq_table[i].output_rate;
			}
		}
	} else {
		div = 104;
		rate /= MHZ;
		fbd = rate * refd / div;
		max_rate = DIV_ROUND_UP(div * fbd, refd);
		max_rate *= MHZ;
	}

	return max_rate;
}

static struct clk_ops clk_vco_ops = {
	.init = clk_pll_vco_init,
	.enable = clk_pll_vco_enable,
	.disable = clk_pll_vco_disable,
	.set_rate = clk_pll_vco_setrate,
	.recalc_rate = clk_vco_recalc_rate,
	.round_rate = clk_vco_round_rate,
	.is_enabled = __pll_is_enabled,
};

static unsigned int __clk_pll_calc_div(struct clk_pll *pll, unsigned long rate,
		unsigned long parent_rate, unsigned int *div)
{
	int i;

	*div = 0;
	rate /= MHZ;
	parent_rate /= MHZ;

	/* Dividers range from 2^0 to 2^7 */
	for (i = 1; i < HELANX_DIVIDER_COUNT; i++) {
		if ((rate <= (parent_rate / (1 << (i-1)))) &&
				(rate > (parent_rate / (1 << i)))) {
			*div = 1 << (i-1);
			return i-1;
		}
	}

	/* Rate is higher than all acceptable ones, use the smallest divider */
	*div = 1;
	return 0;
}

static unsigned int __pll_div_hwval2div(struct clk_vco *vco,
		unsigned int hw_val)
{
	for (int i = 0; i < HELANX_DIVIDER_COUNT; i++) {
		if (hw_val == i)
			return 1 << i;
	}
	BUG_ON(i == HELANX_DIVIDER_COUNT);
	return 0;
}

static void clk_pll_init(struct clk_hw *hw)
{
	unsigned long parent_rate;
	struct clk_pll *pll = to_clk_pll(hw);
	int div, div_hw;
	union pll_swcr swcr;
	struct clk *parent = hw->clk->parent;
	struct clk_vco *vco = to_clk_vco(hw);

	if (!__pll_is_enabled(parent->hw)) {
		pr_info("%s is not enabled\n", hw->clk->name);
		return;
	}

	BUG_ON(!(pll->flags & (HELANX_PLLOUT | HELANX_PLLPOUT)));

	parent_rate = clk_get_rate(parent) / MHZ;

	swcr.v = pll_readl_pll_swcr(pll);
	if (pll->flags & HELANX_PLLOUT)
		div_hw = swcr.b.se_div_sel;
	else
		div_hw = swcr.b.diff_div_sel;
	div = __pll_div_hwval2div(vco, div_hw);
	hw->clk->rate = parent_rate / div * MHZ;
	pr_info("%s enabled at %luHz\n", hw->clk->name, hw->clk->rate);
}

static int clk_pll_setrate(struct clk_hw *hw, unsigned long new_rate,
		unsigned long best_parent_rate)
{
	unsigned int div_hwval, div;
	unsigned long flags;
	struct clk_pll *pll = to_clk_pll(hw);
	struct clk *parent = hw->clk->parent;
	union pll_swcr swcr;

	if (__pll_is_enabled(parent->hw)) {
		pr_info("%s is enabled, ignoring setrate\n",
				hw->clk->name);
		return;
	}

	BUG_ON(!(pll->flags & (HELANX_PLLOUT | HELANX_PLLPOUT)));

	div_hwval = __clk_pll_calc_div(pll, new_rate, best_parent_rate, &div);

	spin_lock_irqsave(pll->lock, flags);
	swcr.v = pll_readl_pll_swcr(pll);
	if (pll->flags & HELANX_PLLOUT)
		swcr.b.se_div_sel = div_hwval;
	else
		swcr.b.diff_div_sel = div_hwval;
	pll_writel_pll_swcr(swcr.v, pll);
	hw->clk->rate = new_rate;
	spin_unlock_irqrestore(pll->lock, flags);

	pr_debug("%s reclocked from %lu to %lu\n", hw->clk->name,
			hw->clk->rate, new_rate);
	return 0;
}

static unsigned long clk_pll_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	return hw->clk->rate;
}

static long clk_pll_round_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long *prate)
{
	struct clk *parent = hw->clk->parent;
	struct clk_vco *vco = to_clk_vco(parent->hw);
	struct mmp_vco_params *vco_params = vco->params;
	unsigned long delta = 104 / 3, new_rate, max_rate = 0, parent_rate;
	bool need_chg_prate = false;

	parent_rate = *prate / MHZ;
	rate /= MHZ;

	if (rate <= parent_rate) {
		for (int i = 0; i < HELANX_DIVIDER_COUNT; i++) {
			new_rate = parent_rate / (1 << i);
			if (new_rate <= rate)
				if (max_rate < new_rate)
					max_rate = new_rate;
		}
		if (hw->clk->flags & CLK_SET_RATE_PARENT) {
			if (abs(rate - max_rate) <= delta)
				return max_rate * MHZ;
			else
				need_chg_prate = true;
		} else
			return max_rate * MHZ;
	}
	if ((rate > parent_rate) || need_chg_prate) {
		if (!(hw->clk->flags & CLK_SET_RATE_PARENT)) {
			WARN_ON(1);
			return parent_rate;
		}
		for (i = 0; i < HELANX_DIVIDER_COUNT; i++) {
			max_rate = rate * (1 << i) * MHZ;
			if (max_rate <= vco_params->vco_max &&
					max_rate >= vco_params->vco_min)
				break;
		}
		*prate = rate * MHZ * (1 << i);
	}
	return rate * MHZ;
}

static int clk_pll_is_enabled(struct clk_hw *hw)
{
	struct clk *parent = hw->clk->parent;
	return __pll_is_enabled(parent->hw);
}

static struct clk_ops clk_pll_ops = {
	.init = clk_pll_init,
	.set_rate = clk_pll_setrate,
	.recalc_rate = clk_pll_recalc_rate,
	.round_rate = clk_pll_round_rate,
	.is_enabled = clk_pll_is_enabled,
};

struct clk *helanx_clk_register_vco(const char *name, const char *parent_name,
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
	vco->hw.init = init;
	vco->params = params;

	clk = clk_register(NULL, &vco->hw);
	if (IS_ERR(clk))
		kfree(vco);

	return clk;
}

struct clk *helanx_clk_register_pll(const char *name, const char *parent_name,
		unsigned long flags, u32 pll_flags, spinlock_t *lock,
		struct mmp_vco_params *params)
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
	pll->params = params;
	pll->hw.init = &init;

	clk = clk_register(NULL, &pll->hw);
	if (IS_ERR(clk))
		kfree(pll);

	return clk;
}
