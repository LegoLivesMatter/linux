#include <linux/kernel.h>
#include <linux/of_address.h>

#include <dt-bindings/clock/marvell,pxa1908.h>

#include "clk.h"
#include "clk-pll-helanx.h"

#define APMU_CLK_GATE_CTRL	0x40
#define MPMU_UART_PLL		0x14

#define APBS_PLL1_CTRL		0x100

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
	void __iomem *ciu_base;
};

static struct mmp_param_fixed_rate_clk fixed_rate_clks[] = {
	{PXA1908_CLK_CLK32, "clk32", NULL, 0, 32768},
	{PXA1908_CLK_VCTCXO, "vctcxo", NULL, 0, 26000000},
	{PXA1908_CLK_PLL1_624, "pll1_624", NULL, 0, 624000000},
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

static struct mmp_clk_factor_masks uart_factor_masks = {
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

		.vcoclk_flag = 0,
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

		.vcoclk_flag = 0,
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

		.vcoclk_flag = 0,
		.vco_flag = HELANX_VCO_SKIP_DEF_RATE,
		.outclk_flag = CLK_SET_RATE_PARENT,
		.out_flag = HELANX_PLLOUT,
		.outp_flag = HELANX_PLLOUTP,

		.vco_idx = PXA1908_CLK_PLL4VCO,
		.out_idx = PXA1908_CLK_PLL4,
		.outp_idx = PXA1908_CLK_PLL4P,
		.vco_d3_idx = PXA1908_CLK_PLL4VCODIV3,
	},
};

static void pxa1908_pll_init(struct pxa1908_clk_unit *pxa_unit)
{
	struct mmp_clk_unit *unit = &pxa_unit->unit;

	mmp_register_fixed_rate_clks(unit, fixed_rate_clks,
					ARRAY_SIZE(fixed_rate_clks));

	mmp_register_fixed_factor_clks(unit, fixed_factor_clks,
					ARRAY_SIZE(fixed_factor_clks));

	/*
	clk = clk_register_gate(NULL, "pll1_499_en", "pll1_499", 0,
			pxa_unit->apbs_base + APBS_PLL1_CTRL,
			31, 0, NULL);
	mmp_clk_add(unit, PXA1908_CLK_PLL1_499_EN, clk);
	*/

	mmp_clk_register_factor("uart_pll", "pll1_d4",
			CLK_SET_RATE_PARENT,
			pxa_unit->mpmu_base + MPMU_UART_PLL,
			&uart_factor_masks, uart_factor_tbl,
			ARRAY_SIZE(uart_factor_tbl), NULL);

	/*mmp_register_general_gate_clks(unit, pll1_gate_clks,
			pxa_unit->apmu_base, ARRAY_SIZE(pll1_gate_clks));
			*/
}

static DEFINE_SPINLOCK(pwm0_lock);
static DEFINE_SPINLOCK(pwm2_lock);

static DEFINE_SPINLOCK(uart0_lock);
static DEFINE_SPINLOCK(uart1_lock);
static DEFINE_SPINLOCK(uart2_lock);

static const char *uart_parent_names[] = {"pll1_117", "uart_pll"};
static const char *ssp_parent_names[] = {"pll1_d16", "pll1_d48", "pll1_d24", "pll1_d12"};

static struct mmp_param_gate_clk apbc_gate_clks[] = {
	{PXA1908_CLK_TWSI0, "twsi0_clk", "pll1_32", CLK_SET_RATE_PARENT, PXA1908_CLK_TWSI0 * 4, 0x7, 3, 0, 0, NULL},
	{PXA1908_CLK_TWSI1, "twsi1_clk", "pll1_32", CLK_SET_RATE_PARENT, PXA1908_CLK_TWSI1 * 4, 0x7, 3, 0, 0, NULL},
	{PXA1908_CLK_TWSI3, "twsi3_clk", "pll1_32", CLK_SET_RATE_PARENT, PXA1908_CLK_TWSI3 * 4, 0x7, 3, 0, 0, NULL},
	{PXA1908_CLK_GPIO, "gpio_clk", "vctcxo", CLK_SET_RATE_PARENT, PXA1908_CLK_GPIO * 4, 0x7, 3, 0, 0, NULL},
	{PXA1908_CLK_KPC, "kpc_clk", "clk32", CLK_SET_RATE_PARENT, PXA1908_CLK_KPC * 4, 0x7, 3, 0, MMP_CLK_GATE_NEED_DELAY, NULL},
	{PXA1908_CLK_RTC, "rtc_clk", "clk32", CLK_SET_RATE_PARENT, PXA1908_CLK_RTC * 4, 0x87, 0x83, 0, MMP_CLK_GATE_NEED_DELAY, NULL},
	{PXA1908_CLK_PWM0, "pwm0_clk", "pwm01_apb_share", CLK_SET_RATE_PARENT, PXA1908_CLK_PWM0 * 4, 0x2, 2, 0, 0, &pwm0_lock},
	{PXA1908_CLK_PWM1, "pwm1_clk", "pwm01_apb_share", CLK_SET_RATE_PARENT, PXA1908_CLK_PWM1 * 4, 0x6, 2, 0, 0, NULL},
	{PXA1908_CLK_PWM2, "pwm2_clk", "pwm23_apb_share", CLK_SET_RATE_PARENT, PXA1908_CLK_PWM2 * 4, 0x2, 2, 0, 0, NULL},
	{PXA1908_CLK_PWM3, "pwm3_clk", "pwm23_apb_share", CLK_SET_RATE_PARENT, PXA1908_CLK_PWM3 * 4, 0x6, 2, 0, 0, NULL},
	{PXA1908_CLK_UART0, "uart0_clk", "uart0_mux", CLK_SET_RATE_PARENT, PXA1908_CLK_UART0 * 4, 0x7, 3, 0, 0, &uart0_lock},
	{PXA1908_CLK_UART1, "uart1_clk", "uart1_mux", CLK_SET_RATE_PARENT, PXA1908_CLK_UART1 * 4, 0x7, 3, 0, 0, &uart1_lock},
};

static struct mmp_param_mux_clk apbc_mux_clks[] = {
	{0, "uart0_mux", uart_parent_names, ARRAY_SIZE(uart_parent_names), CLK_SET_RATE_PARENT, PXA1908_CLK_UART0 * 4, 4, 3, 0, &uart0_lock},
	{0, "uart1_mux", uart_parent_names, ARRAY_SIZE(uart_parent_names), CLK_SET_RATE_PARENT, PXA1908_CLK_UART1 * 4, 4, 3, 0, &uart1_lock},
	{0, "uart2_mux", uart_parent_names, ARRAY_SIZE(uart_parent_names), CLK_SET_RATE_PARENT, PXA1908_CLK_UART2 * 4, 4, 3, 0, &uart2_lock},
	{0, "ssp0_mux", ssp_parent_names, ARRAY_SIZE(ssp_parent_names), 0, PXA1908_CLK_SSP0 * 4, 4, 3, 0, NULL},
	{0, "ssp2_mux", ssp_parent_names, ARRAY_SIZE(ssp_parent_names), 0, PXA1908_CLK_SSP2 * 4, 4, 3, 0, NULL},
};

static void pxa1908_apb_periph_clk_init(struct pxa1908_clk_unit *pxa_unit)
{
	struct mmp_clk_unit *unit = &pxa_unit->unit;

	mmp_clk_register_gate(NULL, "pwm01_apb_share", "pll1_d48",
			CLK_SET_RATE_PARENT,
			pxa_unit->apbc_base + PXA1908_CLK_PWM0 * 4,
			0x5, 1, 0, 0, &pwm0_lock);
	mmp_clk_register_gate(NULL, "pwm23_apb_share", "pll1_d48",
			CLK_SET_RATE_PARENT,
			pxa_unit->apbc_base + PXA1908_CLK_PWM2 * 4,
			0x5, 1, 0, 0, &pwm2_lock);
	mmp_register_mux_clks(unit, apbc_mux_clks, pxa_unit->apbc_base,
			ARRAY_SIZE(apbc_mux_clks));
	mmp_register_gate_clks(unit, apbc_gate_clks, pxa_unit->apbc_base,
			ARRAY_SIZE(apbc_gate_clks));
}

static void __init pxa1908_apbc_clk_init(struct device_node *np)
{
	struct pxa1908_clk_unit *pxa_unit;

	pxa_unit = kzalloc(sizeof(*pxa_unit), GFP_KERNEL);
	if (!pxa_unit)
		return;

	pxa_unit->apbc_base = of_iomap(np, 0);
	if (!pxa_unit->apbc_base) {
		pr_err("failed to map apbc registers\n");
		kfree(pxa_unit);
		return;
	}

	mmp_clk_init(np, &pxa_unit->unit, PXA1908_APBC_NR_CLKS);

	pxa1908_apb_periph_clk_init(pxa_unit);

	pr_notice("apbc ready\n");
}
CLK_OF_DECLARE(pxa1908_apbc, "marvell,pxa1908-apbc", pxa1908_apbc_clk_init);

static void __init pxa1908_mpmu_clk_init(struct device_node *np)
{
	struct pxa1908_clk_unit *pxa_unit;

	pxa_unit = kzalloc(sizeof(*pxa_unit), GFP_KERNEL);
	if (!pxa_unit)
		return;

	pxa_unit->mpmu_base = of_iomap(np, 0);
	if (!pxa_unit->mpmu_base) {
		pr_err("failed to map mpmu registers\n");
		kfree(pxa_unit);
		return;
	}

	mmp_clk_init(np, &pxa_unit->unit, PXA1908_MPMU_NR_CLKS);

	pxa1908_pll_init(pxa_unit);

	pr_notice("mpmu ready\n");
}
CLK_OF_DECLARE(pxa1908_mpmu, "marvell,pxa1908-mpmu", pxa1908_mpmu_clk_init);
