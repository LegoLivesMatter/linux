#include <linux/kernel.h>

#include <dt-bindings/clock/marvell,pxa1908.h>

#include "clk.h"
#include "clk-pll-helanx.h"

#define APMU_CLK_GATE_CTRL	0x40
#define MPMU_UART_PLL		0x14

#define POSR_PLL2_LOCK		BIT(29)
#define POSR_PLL3_LOCK		BIT(30)
#define POSR_PLL4_LOCK		BIT(31)

struct pxa1908_clk_unit {
	struct mmp_clk_unit unit;
	void __iomem *mpmu_base;
	void __iomem *apmu_base;
	void __iomem *apbc_base;
	void __iomem *apbcp_base;
	void __iomem *apbs_base;
};

static struct mmp_param_fixed_rate_clk fixed_rate_clks[] = {
	{PXA1908_CLK_CLK32, "clk32", NULL, 0, 32768},
	{PXA1908_CLK_VCTCXO, "vctcxo", NULL, 0, 26000000},
	{PXA1908_CLK_PLL1_614, "pll1_624", NULL, 0, 624000000},
	{PXA1908_CLK_PLL1_416, "pll1_416", NULL, 0, 416000000},
	{PXA1908_CLK_PLL1_499, "pll1_499", NULL, 0, 499000000},
	{PXA1908_CLK_PLL1_832, "pll1_832", NULL, 0, 832000000},
	{PXA1908_CLK_PLL1_1248, "pll1_1248", NULL, 0, 1248000000},
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
};

static struct mmp_factor_masks uart_factor_masks = {
	.factor = 2,
	.num_mask = 0x1fff,
	.den_mask = 0x1fff,
	.num_shift = 16,
	.den_shift = 0,
};

static struct mmp_clk_factor_tbl uart_factor_tbl[] = {
	{.num = 8125, .den = 1536},	/* 14.745MHz */
};

static DEFINE_SPINLOCK(pll1_lock);
static struct mmp_param_general_gate_clk pll1_gate_clks[] = {
	{PXA1908_CLK_PLL1_D2_GATE, "pll1_d2_gate", "pll1_d2", 0, APMU_CLK_GATE_CTRL, 29, 0, &pll1_lock},
	{PXA1908_CLK_PLL1_416_GATE, "pll1_416_gate", "pll1_416", 0, APMU_CLK_GATE_CTRL, 27, 0, &pll1_lock},
	{PXA1908_CLK_PLL1_624_GATE, "pll1_624_gate", "pll1_624", 0, APMU_CLK_GATE_CTRL, 26, 0, &pll1_lock},
	{PXA1908_CLK_PLL1_832_GATE, "pll1_832_gate", "pll1_832", 0, APMU_CLK_GATE_CTRL, 30, 0, &pll1_lock},
	{PXA1908_CLK_PLL1_1248_GATE, "pll1_1248_gate", "pll1_1248", 0, APMU_CLK_GATE_CTRL, 28, 0, &pll1_lock},
};

enum pll {
	PLL2 = 0,
	PLL3,
	PLL4,
	MAX_PLL_NUM,
};

enum pll_type {
	VCO,
	OUT,
	OUTP,
	MAX_PLL_TYPE,
};

static struct mmp_vco_params pll_vco_params[MAX_PLL_NUM] = {
	{
		.vco_min = 1200000000UL,
		.vco_max = 3000000000UL,
		.lock_enable_bit = POSR_PLL2_LOCK,
	},
	{
		.vco_min = 1200000000UL,
		.vco_max = 3000000000UL,
		.lock_enable_bit = POSR_PLL3_LOCK,
	},
	{
		.vco_min = 1200000000UL,
		.vco_max = 3000000000UL,
		.lock_enable_bit = POSR_PLL4_LOCK,
	},
};

static struct mmp_pll_params pll_pll_params[MAX_PLL_NUM] = {
};

static struct mmp_pll_params pll_pllp_params[MAX_PLL_NUM] = {
};

struct plat_pll_info {
	spinlock_t lock;
	const char *vco_name;
	const char *out_name;
	const char *outp_name;
	const char *vco_d3_name;
	/* clk flags */
	unsigned long vco_flag;
	unsigned long vcoclk_flag;
	unsigned long out_flag;
	unsigned long outclk_flag;
	unsigned long outp_flag;
	unsigned long outpclk_flag;
	/* dt index */
	unsigned int vco_idx; /* VCO index */
	unsigned int out_idx; /* output index */
	unsigned int outp_idx; /* pllp output index? */
	unsigned int vco_d3_idx; /* VCO div 3 index */
};

struct plat_pll_info pll_platinfo[MAX_PLL_NUM] = {
	{
		.vco_name = "pll2_vco",
		.out_name = "pll2",
		.outp_name = "pll2p",
		.vco_d3_name = "pll2_d3",

		.vcoclk_flag = CLK_IS_ROOT,
		.out_flag = HELANX_PLLOUT,
		.outp_flag = HELANX_PLLOUTP,

		.vco_idx = PXA1908_CLK_PLL2VCO,
		.out_idx = PXA1908_CLK_PLL2,
		.outp_idx = PXA1908_CLK_PLL2P,
		.vco_d3_idx = PXA1908_CLK_PLL2VCODIV3,
	},
	{
		.vco_name = "pll3_vco",
		.out_name = "pll3",
		.outp_name = "pll3p",
		.vco_d3_name = "pll3_d3",

		.vcoclk_flag = CLK_IS_ROOT,
		.outpclk_flag = CLK_SET_RATE_PARENT,
		.out_flag = HELANX_PLLOUT,
		.outp_flag = HELANX_PLLOUTP,

		.vco_idx = PXA1908_CLK_PLL3VCO,
		.out_idx = PXA1908_CLK_PLL3,
		.outp_idx = PXA1908_CLK_PLL3P,
		.vco_d3_idx = PXA1908_CLK_PLL3VCODIV3,
	},
	{
		.vco_name = "pll4_vco",
		.out_name = "pll4",
		.outp_name = "pll4p",
		.vco_d3_name = "pll4_d3",

		.vcoclk_flag = CLK_IS_ROOT,
		.vco_flag = HELANX_PLL_SKIP_DEF_RATE,
		.outclk_flag = CLK_SET_RATE_PARENT,
		.out_flag = HELANX_PLLOUT,
		.outp_flag = HELANX_PLLOUTP,

		.vcodtidx = PXA1908_CLK_PLL4VCO,
		.outdtidx = PXA1908_CLK_PLL4,
		.outpdtidx = PXA1908_CLK_PLL4P,
		.vco_d3_idx = PXA1908_CLK_PLL4VCODIV3,
	},
};

static void pxa1908_pll_init(struct pxa1908_clk_unit *pxa_unit)
{
	struct clk *clk;
	struct mmp_clk_unit *unit = &pxa_unit->unit;

	mmp_register_fixed_rate_clks(unit, fixed_rate_clks,
					ARRAY_SIZE(fixed_rate_clks));

	mmp_register_fixed_factor_clks(unit, fixed_factor_clks,
					ARRAY_SIZE(fixed_factor_clks));

	clk = clk_register_gate(NULL, "pll1_499_en", "pll1_499", 0,
			pxa_unit->apbs_base + APBS_PLL1_CTRL,
			31, 0, NULL);
	mmp_clk_add(unit, PXA1908_CLK_PLL1_499_EN, clk);

	clk = mmp_clk_register_factor("uart_pll", "pll1_d4",
			CLK_SET_RATE_PARENT,
			pxa_unit->mpmu_base + MPMU_UART_PLL,
			&uart_factor_masks, uart_factor_tbl,
			ARRAY_SIZE(uart_factor_tbl), NULL);

	mmp_register_general_gate_clks(unit, pll1_gate_clks,
			pxa_unit->apmu_base, ARRAY_SIZE(pll1_gate_clks));
}
