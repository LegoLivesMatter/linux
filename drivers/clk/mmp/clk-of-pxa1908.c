// SPDX-License-Identifier: GPL-2.0-only
#include <linux/bits.h>
#include <linux/clk-provider.h>
#include <linux/of_address.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/units.h>

#include <dt-bindings/clock/marvell,pxa1908.h>

#include "clk.h"

#define APBC_INDEX_TO_OFFSET(n)	((n - 1) * 4)

#define APMU_CLK_GATE_CTRL	0x40
#define MPMU_UART_PLL		0x14

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
};

static struct mmp_clk_factor_masks uart_factor_masks = {
	.factor = 2,
	.num_mask = GENMASK(12, 0),
	.den_mask = GENMASK(12, 0),
	.num_shift = 16,
	.den_shift = 0,
};

static struct u32_fract uart_factor_tbl[] = {
	{.numerator = 8125, .denominator = 1536},	/* 14.745MHz */
};

static DEFINE_SPINLOCK(pll1_lock);
static struct mmp_param_general_gate_clk pll1_gate_clks[] = {
	{PXA1908_CLK_PLL1_D2_GATE, "pll1_d2_gate", "pll1_d2", 0, APMU_CLK_GATE_CTRL, 29, 0, &pll1_lock},
	{PXA1908_CLK_PLL1_416_GATE, "pll1_416_gate", "pll1_416", 0, APMU_CLK_GATE_CTRL, 27, 0, &pll1_lock},
	{PXA1908_CLK_PLL1_624_GATE, "pll1_624_gate", "pll1_624", 0, APMU_CLK_GATE_CTRL, 26, 0, &pll1_lock},
	{PXA1908_CLK_PLL1_832_GATE, "pll1_832_gate", "pll1_832", 0, APMU_CLK_GATE_CTRL, 30, 0, &pll1_lock},
	{PXA1908_CLK_PLL1_1248_GATE, "pll1_1248_gate", "pll1_1248", 0, APMU_CLK_GATE_CTRL, 28, 0, &pll1_lock},
};

static void pxa1908_pll_init(struct pxa1908_clk_unit *pxa_unit)
{
	struct mmp_clk_unit *unit = &pxa_unit->unit;

	mmp_register_fixed_rate_clks(unit, fixed_rate_clks,
					ARRAY_SIZE(fixed_rate_clks));

	mmp_register_fixed_factor_clks(unit, fixed_factor_clks,
					ARRAY_SIZE(fixed_factor_clks));

	mmp_clk_register_factor("uart_pll", "pll1_d4",
			CLK_SET_RATE_PARENT,
			pxa_unit->mpmu_base + MPMU_UART_PLL,
			&uart_factor_masks, uart_factor_tbl,
			ARRAY_SIZE(uart_factor_tbl), NULL);

}

static DEFINE_SPINLOCK(pwm0_lock);
static DEFINE_SPINLOCK(pwm2_lock);

static DEFINE_SPINLOCK(uart0_lock);
static DEFINE_SPINLOCK(uart1_lock);
static DEFINE_SPINLOCK(uart2_lock);

static const char * const uart_parent_names[] = {"pll1_117", "uart_pll"};
static const char * const ssp_parent_names[] = {"pll1_d16", "pll1_d48", "pll1_d24", "pll1_d12"};

static struct mmp_param_gate_clk apbc_gate_clks[] = {
	{PXA1908_CLK_TWSI0, "twsi0_clk", "pll1_32", CLK_SET_RATE_PARENT, APBC_INDEX_TO_OFFSET(PXA1908_CLK_TWSI0), 0x7, 3, 0, 0, NULL},
	{PXA1908_CLK_TWSI1, "twsi1_clk", "pll1_32", CLK_SET_RATE_PARENT, APBC_INDEX_TO_OFFSET(PXA1908_CLK_TWSI1), 0x7, 3, 0, 0, NULL},
	{PXA1908_CLK_TWSI3, "twsi3_clk", "pll1_32", CLK_SET_RATE_PARENT, APBC_INDEX_TO_OFFSET(PXA1908_CLK_TWSI3), 0x7, 3, 0, 0, NULL},
	{PXA1908_CLK_GPIO, "gpio_clk", "vctcxo", CLK_SET_RATE_PARENT, APBC_INDEX_TO_OFFSET(PXA1908_CLK_GPIO), 0x7, 3, 0, 0, NULL},
	{PXA1908_CLK_KPC, "kpc_clk", "clk32", CLK_SET_RATE_PARENT, APBC_INDEX_TO_OFFSET(PXA1908_CLK_KPC), 0x7, 3, 0, MMP_CLK_GATE_NEED_DELAY, NULL},
	{PXA1908_CLK_RTC, "rtc_clk", "clk32", CLK_SET_RATE_PARENT, APBC_INDEX_TO_OFFSET(PXA1908_CLK_RTC), 0x87, 0x83, 0, MMP_CLK_GATE_NEED_DELAY, NULL},
	{PXA1908_CLK_PWM0, "pwm0_clk", "pwm01_apb_share", CLK_SET_RATE_PARENT, APBC_INDEX_TO_OFFSET(PXA1908_CLK_PWM0), 0x2, 2, 0, 0, &pwm0_lock},
	{PXA1908_CLK_PWM1, "pwm1_clk", "pwm01_apb_share", CLK_SET_RATE_PARENT, APBC_INDEX_TO_OFFSET(PXA1908_CLK_PWM1), 0x6, 2, 0, 0, NULL},
	{PXA1908_CLK_PWM2, "pwm2_clk", "pwm23_apb_share", CLK_SET_RATE_PARENT, APBC_INDEX_TO_OFFSET(PXA1908_CLK_PWM2), 0x2, 2, 0, 0, NULL},
	{PXA1908_CLK_PWM3, "pwm3_clk", "pwm23_apb_share", CLK_SET_RATE_PARENT, APBC_INDEX_TO_OFFSET(PXA1908_CLK_PWM3), 0x6, 2, 0, 0, NULL},
	{PXA1908_CLK_UART0, "uart0_clk", "uart0_mux", CLK_SET_RATE_PARENT, APBC_INDEX_TO_OFFSET(PXA1908_CLK_UART0), 0x7, 3, 0, 0, &uart0_lock},
	{PXA1908_CLK_UART1, "uart1_clk", "uart1_mux", CLK_SET_RATE_PARENT, APBC_INDEX_TO_OFFSET(PXA1908_CLK_UART1), 0x7, 3, 0, 0, &uart1_lock},
};

static struct mmp_param_mux_clk apbc_mux_clks[] = {
	{0, "uart0_mux", uart_parent_names, ARRAY_SIZE(uart_parent_names), CLK_SET_RATE_PARENT, APBC_INDEX_TO_OFFSET(PXA1908_CLK_UART0), 4, 3, 0, &uart0_lock},
	{0, "uart1_mux", uart_parent_names, ARRAY_SIZE(uart_parent_names), CLK_SET_RATE_PARENT, APBC_INDEX_TO_OFFSET(PXA1908_CLK_UART1), 4, 3, 0, &uart1_lock},
	{0, "ssp0_mux", ssp_parent_names, ARRAY_SIZE(ssp_parent_names), 0, APBC_INDEX_TO_OFFSET(PXA1908_CLK_SSP0), 4, 3, 0, NULL},
	{0, "ssp2_mux", ssp_parent_names, ARRAY_SIZE(ssp_parent_names), 0, APBC_INDEX_TO_OFFSET(PXA1908_CLK_SSP2), 4, 3, 0, NULL},
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

static struct mmp_param_gate_clk apbcp_gate_clks[] = {
	{PXA1908_CLK_UART2, "uart2_clk", "uart2_mux", CLK_SET_RATE_PARENT, PXA1908_CLK_UART2 * 4, 0x7, 0x3, 0x0, 0, &uart2_lock},
	{PXA1908_CLK_TWSI2, "twsi2_clk", "pll1_32", CLK_SET_RATE_PARENT, PXA1908_CLK_TWSI2 * 4, 0x7, 0x3, 0x0, 0, NULL},
	{PXA1908_CLK_AICER, "ripc_clk", NULL, 0, PXA1908_CLK_AICER * 4, 0x7, 0x2, 0x0, 0, NULL},
};

static struct mmp_param_mux_clk apbcp_mux_clks[] = {
	{0, "uart2_mux", uart_parent_names, ARRAY_SIZE(uart_parent_names), CLK_SET_RATE_PARENT, PXA1908_CLK_UART2 * 4, 4, 3, 0, &uart2_lock},
};

static void pxa1908_apb_p_periph_clk_init(struct pxa1908_clk_unit *pxa_unit)
{
	struct mmp_clk_unit *unit = &pxa_unit->unit;

	mmp_register_mux_clks(unit, apbcp_mux_clks, pxa_unit->apbcp_base,
			ARRAY_SIZE(apbcp_mux_clks));
	mmp_register_gate_clks(unit, apbcp_gate_clks, pxa_unit->apbcp_base,
			ARRAY_SIZE(apbcp_gate_clks));
}

static DEFINE_SPINLOCK(sdh0_lock);
static DEFINE_SPINLOCK(sdh1_lock);
static DEFINE_SPINLOCK(sdh2_lock);

static const char * const sdh_parent_names[] = {"pll1_416", "pll1_624"};

static struct mmp_clk_mix_config sdh_mix_config = {
	.reg_info = DEFINE_MIX_REG_INFO(3, 8, 2, 6, 11),
};

static struct mmp_param_gate_clk apmu_gate_clks[] = {
	{PXA1908_CLK_USB, "usb_clk", NULL, 0, PXA1908_CLK_USB * 4, 0x9, 0x9, 0x1, 0, NULL},
	{PXA1908_CLK_SDH0, "sdh0_clk", "sdh0_mix_clk", CLK_SET_RATE_PARENT | CLK_SET_RATE_UNGATE, PXA1908_CLK_SDH0 * 4, 0x12, 0x12, 0x0, 0, &sdh0_lock},
	{PXA1908_CLK_SDH1, "sdh1_clk", "sdh1_mix_clk", CLK_SET_RATE_PARENT | CLK_SET_RATE_UNGATE, PXA1908_CLK_SDH1 * 4, 0x12, 0x12, 0x0, 0, &sdh1_lock},
	{PXA1908_CLK_SDH2, "sdh2_clk", "sdh2_mix_clk", CLK_SET_RATE_PARENT | CLK_SET_RATE_UNGATE, PXA1908_CLK_SDH2 * 4, 0x12, 0x12, 0x0, 0, &sdh2_lock}
};

static void pxa1908_axi_periph_clk_init(struct pxa1908_clk_unit *pxa_unit)
{
	struct mmp_clk_unit *unit = &pxa_unit->unit;

	mmp_register_general_gate_clks(unit, pll1_gate_clks,
			pxa_unit->apmu_base, ARRAY_SIZE(pll1_gate_clks));

	sdh_mix_config.reg_info.reg_clk_ctrl = pxa_unit->apmu_base + PXA1908_CLK_SDH0 * 4;
	mmp_clk_register_mix(NULL, "sdh0_mix_clk", sdh_parent_names,
			ARRAY_SIZE(sdh_parent_names), CLK_SET_RATE_PARENT,
			&sdh_mix_config, &sdh0_lock);
	sdh_mix_config.reg_info.reg_clk_ctrl = pxa_unit->apmu_base + PXA1908_CLK_SDH1 * 4;
	mmp_clk_register_mix(NULL, "sdh1_mix_clk", sdh_parent_names,
			ARRAY_SIZE(sdh_parent_names), CLK_SET_RATE_PARENT,
			&sdh_mix_config, &sdh1_lock);
	sdh_mix_config.reg_info.reg_clk_ctrl = pxa_unit->apmu_base + PXA1908_CLK_SDH2 * 4;
	mmp_clk_register_mix(NULL, "sdh2_mix_clk", sdh_parent_names,
			ARRAY_SIZE(sdh_parent_names), CLK_SET_RATE_PARENT,
			&sdh_mix_config, &sdh2_lock);

	mmp_register_gate_clks(unit, apmu_gate_clks, pxa_unit->apmu_base,
			ARRAY_SIZE(apmu_gate_clks));
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
}
CLK_OF_DECLARE(pxa1908_apbc, "marvell,pxa1908-apbc", pxa1908_apbc_clk_init);

static void __init pxa1908_apbcp_clk_init(struct device_node *np)
{
	struct pxa1908_clk_unit *pxa_unit;

	pxa_unit = kzalloc(sizeof(*pxa_unit), GFP_KERNEL);
	if (!pxa_unit)
		return;

	pxa_unit->apbcp_base = of_iomap(np, 0);
	if (!pxa_unit->apbcp_base) {
		pr_err("failed to map apbcp registers\n");
		kfree(pxa_unit);
		return;
	}

	mmp_clk_init(np, &pxa_unit->unit, PXA1908_APBCP_NR_CLKS);

	pxa1908_apb_p_periph_clk_init(pxa_unit);
}
CLK_OF_DECLARE(pxa1908_apbcp, "marvell,pxa1908-apbcp", pxa1908_apbcp_clk_init);

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
}
CLK_OF_DECLARE(pxa1908_mpmu, "marvell,pxa1908-mpmu", pxa1908_mpmu_clk_init);

static void __init pxa1908_apmu_clk_init(struct device_node *np)
{
	struct pxa1908_clk_unit *pxa_unit;

	pxa_unit = kzalloc(sizeof(*pxa_unit), GFP_KERNEL);
	if (!pxa_unit)
		return;

	pxa_unit->apmu_base = of_iomap(np, 0);
	if (!pxa_unit->apmu_base) {
		pr_err("failed to map apmu registers\n");
		kfree(pxa_unit);
		return;
	}

	mmp_clk_init(np, &pxa_unit->unit, PXA1908_APMU_NR_CLKS);

	pxa1908_axi_periph_clk_init(pxa_unit);
}
CLK_OF_DECLARE(pxa1908_apmu, "marvell,pxa1908-apmu", pxa1908_apmu_clk_init);
