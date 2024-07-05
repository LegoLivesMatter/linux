// SPDX-License-Identifier: GPL-2.0-only
#include <linux/bits.h>
#include <linux/clk-provider.h>
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/units.h>

#include <dt-bindings/clock/marvell,pxa1908.h>

#include "clk.h"
#include "clk-pll-helanx.h"

#define APBS_PLL1_CTRL		0x100

#define MPMU_UART_PLL		0x14
#define MPMU_PLL2CR		0x34
#define MPMU_PLL3CR		0x1c
#define MPMU_PLL4CR		0x50
#define MPMU_POSR		0x10
#define MPMU_POSR_PLL2_LOCK	BIT(29)
#define MPMU_POSR_PLL3_LOCK	BIT(30)
#define MPMU_POSR_PLL4_LOCK	BIT(31)

#define APB_SPARE_PLL2CR	0x104
#define APB_SPARE_PLL3CR	0x108
#define APB_SPARE_PLL4CR	0x124

#define MPMU_NR_CLKS		39

struct pxa1908_clk_unit {
	struct mmp_clk_unit unit;
	void __iomem *base;
	void __iomem *apbs_base;
};

static struct mmp_param_fixed_rate_clk fixed_rate_clks[] = {
	{PXA1908_CLK_CLK32, "clk32", NULL, 0, 32768},
	{PXA1908_CLK_VCTCXO, "vctcxo", NULL, 0, 26 * HZ_PER_MHZ},
	{PXA1908_CLK_PLL1_624, "pll1_624", NULL, 0, 624 * HZ_PER_MHZ},
	{PXA1908_CLK_PLL1_416, "pll1_416", NULL, 0, 416 * HZ_PER_MHZ},
	{PXA1908_CLK_PLL1_499, "pll1_499", NULL, 0, 499 * HZ_PER_MHZ},
	{PXA1908_CLK_PLL1_832, "pll1_832", NULL, 0, 832 * HZ_PER_MHZ},
	{PXA1908_CLK_PLL1_1248, "pll1_1248", NULL, 0, 1248 * HZ_PER_MHZ},
};

static struct mmp_param_fixed_factor_clk fixed_factor_clks[] = {
	{PXA1908_CLK_PLL1_D2, "pll1_d2", "pll1_624", 1, 2, 0},
	{PXA1908_CLK_PLL1_D4, "pll1_d4", "pll1_d2", 1, 2, 0},
	{PXA1908_CLK_PLL1_D6, "pll1_d6", "pll1_d2", 1, 3, 0},
	{PXA1908_CLK_PLL1_D8, "pll1_d8", "pll1_d4", 1, 2, 0},
	{PXA1908_CLK_PLL1_D12, "pll1_d12", "pll1_d6", 1, 2, 0},
	{PXA1908_CLK_PLL1_D13, "pll1_d13", "pll1_624", 1, 13, 0},
	{PXA1908_CLK_PLL1_D16, "pll1_d16", "pll1_d8", 1, 2, 0},
	{PXA1908_CLK_PLL1_D24, "pll1_d24", "pll1_d12", 1, 2, 0},
	{PXA1908_CLK_PLL1_D48, "pll1_d48", "pll1_d24", 1, 2, 0},
	{PXA1908_CLK_PLL1_D96, "pll1_d96", "pll1_d48", 1, 2, 0},
	{PXA1908_CLK_PLL1_32, "pll1_32", "pll1_d13", 2, 3, 0},
	{PXA1908_CLK_PLL1_208, "pll1_208", "pll1_d2", 2, 3, 0},
	{PXA1908_CLK_PLL1_117, "pll1_117", "pll1_624", 3, 16, 0},
	{PXA1908_CLK_PLL2VCODIV3, "pll2_div3", "pll2_vco", 1, 3, 0},
	{PXA1908_CLK_PLL3VCODIV3, "pll3_div3", "pll3_vco", 1, 3, 0},
	{PXA1908_CLK_PLL4VCODIV3, "pll4_div3", "pll4_vco", 1, 3, 0},
};

struct mmp_param_pll {
	unsigned int id;
	char *name;
	const char *parent_name;
	unsigned long clk_flags;
	u32 pll_flags;
	unsigned int swcr_offset;
	spinlock_t *lock;
	unsigned long default_rate;
};

static DEFINE_SPINLOCK(pll2_lock);
static DEFINE_SPINLOCK(pll3_lock);
static DEFINE_SPINLOCK(pll4_lock);

/* NOTE: the default rate is ONLY applicable for downstream ddr_mode=1 (533M). */
static struct mmp_param_pll plls[] = {
	{PXA1908_CLK_PLL2, "pll2", "pll2_vco", 0, HELANX_PLLOUT, APB_SPARE_PLL2CR, &pll2_lock, 1057 * HZ_PER_MHZ },
	{PXA1908_CLK_PLL3, "pll3", "pll3_vco", 0, HELANX_PLLOUT, APB_SPARE_PLL3CR, &pll3_lock, 1526 * HZ_PER_MHZ },
	{PXA1908_CLK_PLL4, "pll4", "pll4_vco", CLK_SET_RATE_PARENT, HELANX_PLLOUT, APB_SPARE_PLL4CR, &pll4_lock, 1595 * HZ_PER_MHZ },
	{PXA1908_CLK_PLL2P, "pll2p", "pll2_vco", 0, HELANX_PLLOUTP, APB_SPARE_PLL2CR, &pll2_lock, 528 * HZ_PER_MHZ },
	{PXA1908_CLK_PLL3P, "pll3p", "pll3_vco", CLK_SET_RATE_PARENT, HELANX_PLLOUTP, APB_SPARE_PLL3CR, &pll3_lock, 1526 * HZ_PER_MHZ },
	{PXA1908_CLK_PLL4P, "pll4p", "pll4_vco", 0, HELANX_PLLOUTP, APB_SPARE_PLL4CR, &pll4_lock, 797 * HZ_PER_MHZ },
};

struct mmp_param_vco {
	unsigned int id;
	char *name;
	unsigned long clk_flags;
	u32 vco_flags;
	unsigned int cr_offset;
	unsigned int swcr_offset;
	spinlock_t *lock;
	struct mmp_vco_params params;
};

/* NOTE: the default rate is ONLY applicable for downstream ddr_mode=1 (533M). */
static struct mmp_param_vco vcos[] = {
	{PXA1908_CLK_PLL2VCO, "pll2_vco", 0, 0, MPMU_PLL2CR, APB_SPARE_PLL2CR, &pll2_lock,
		{
			.default_rate = 2115 * HZ_PER_MHZ,
			.vco_min = 1200000000UL,
			.vco_max = 3000000000UL,
			.lock_enable_bit = MPMU_POSR_PLL2_LOCK
		}
	},
	{PXA1908_CLK_PLL3VCO, "pll3_vco", 0, 0, MPMU_PLL3CR, APB_SPARE_PLL3CR, &pll3_lock,
		{
			.default_rate = 1526 * HZ_PER_MHZ,
			.vco_min = 1200000000UL, .vco_max = 3000000000UL,
			.lock_enable_bit = MPMU_POSR_PLL3_LOCK
		}
	},
	{PXA1908_CLK_PLL4VCO, "pll4_vco", 0, HELANX_VCO_SKIP_DEF_RATE, MPMU_PLL4CR, APB_SPARE_PLL4CR, &pll4_lock,
		{
			.default_rate = 1595 * HZ_PER_MHZ,
			.vco_min = 1200000000UL,
			.vco_max = 3000000000UL,
			.lock_enable_bit = MPMU_POSR_PLL4_LOCK
		}
	},
};

static struct u32_fract uart_factor_tbl[] = {
	{.numerator = 8125, .denominator = 1536},	/* 14.745MHz */
};

static struct mmp_clk_factor_masks uart_factor_masks = {
	.factor = 2,
	.num_mask = GENMASK(12, 0),
	.den_mask = GENMASK(12, 0),
	.num_shift = 16,
	.den_shift = 0,
};

static void pxa1908_pll_init(struct pxa1908_clk_unit *pxa_unit)
{
	struct mmp_clk_unit *unit = &pxa_unit->unit;
	struct clk *clk;

	mmp_register_fixed_rate_clks(unit, fixed_rate_clks,
					ARRAY_SIZE(fixed_rate_clks));

	mmp_register_fixed_factor_clks(unit, fixed_factor_clks,
					ARRAY_SIZE(fixed_factor_clks));

	mmp_clk_register_factor("uart_pll", "pll1_d4",
			CLK_SET_RATE_PARENT,
			pxa_unit->base + MPMU_UART_PLL,
			&uart_factor_masks, uart_factor_tbl,
			ARRAY_SIZE(uart_factor_tbl), NULL);

	clk = clk_register_gate(NULL, "pll1_499_gate", "pll1_499",
			0, pxa_unit->apbs_base + APBS_PLL1_CTRL, 31, 0, NULL);
	mmp_clk_add(unit, PXA1908_CLK_PLL1_499_EN, clk);

	struct mmp_param_pll *pll;
	for (int i = 0; i < ARRAY_SIZE(plls); i++) {
		pll = &plls[i];

		clk = helanx_register_clk_pll(pll->name, pll->parent_name,
				pll->clk_flags, pll->pll_flags, pll->lock,
				pxa_unit->apbs_base + pll->swcr_offset);
		clk_set_rate(clk, pll->default_rate);
		mmp_clk_add(unit, pll->id, clk);
	}

	struct mmp_param_vco *vco;
	for (int i = 0; i < ARRAY_SIZE(vcos); i++) {
		vco = &vcos[i];

		vco->params.cr = pxa_unit->base + vco->cr_offset;
		vco->params.swcr = pxa_unit->apbs_base + vco->swcr_offset;
		vco->params.lock_reg = pxa_unit->base + MPMU_POSR;

		clk = helanx_register_clk_vco(vco->name, 0, vco->clk_flags,
				vco->vco_flags, vco->lock, &vco->params);
		clk_set_rate(clk, vco->params.default_rate);
		mmp_clk_add(unit, vco->id, clk);
	}
}

static int pxa1908_mpmu_probe(struct platform_device *pdev)
{
	struct pxa1908_clk_unit *pxa_unit;

	pxa_unit = devm_kzalloc(&pdev->dev, sizeof(*pxa_unit), GFP_KERNEL);
	if (IS_ERR(pxa_unit))
		return PTR_ERR(pxa_unit);

	pxa_unit->base = devm_platform_ioremap_resource_byname(pdev, "mpmu");
	if (IS_ERR(pxa_unit->base))
		return PTR_ERR(pxa_unit->base);
	pxa_unit->apbs_base = devm_platform_ioremap_resource_byname(pdev, "apbs");
	if (IS_ERR(pxa_unit->apbs_base))
		return PTR_ERR(pxa_unit->apbs_base);

	mmp_clk_init(pdev->dev.of_node, &pxa_unit->unit, MPMU_NR_CLKS);

	pxa1908_pll_init(pxa_unit);

	return 0;
}

static const struct of_device_id pxa1908_mpmu_match_table[] = {
	{ .compatible = "marvell,pxa1908-mpmu" },
	{ }
};
MODULE_DEVICE_TABLE(of, pxa1908_mpmu_match_table);

static struct platform_driver pxa1908_mpmu_driver = {
	.probe = pxa1908_mpmu_probe,
	.driver = {
		.name = "pxa1908-mpmu",
		.of_match_table = pxa1908_mpmu_match_table
	}
};
module_platform_driver(pxa1908_mpmu_driver);

MODULE_AUTHOR("Duje Mihanović <duje.mihanovic@skole.hr>");
MODULE_DESCRIPTION("Marvell PXA1908 MPMU Clock Driver");
MODULE_LICENSE("GPL");
