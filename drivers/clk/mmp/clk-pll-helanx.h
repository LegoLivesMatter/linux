#ifndef __MACH_MMP_CLK_PLL_HELANX_H
#define __MACH_MMP_CLK_PLL_HELANX_H

struct kvco_range {
	int vco_min;
	int vco_max;
	u8 kvco;
	u8 vrng;
};

/* Vendor driver uses a map which maps values 2^0-2^7 to their logarithms. */
#define HELANX_DIVIDER_COUNT 8

struct intpi_range {
	int min;
	int max;
	u8 value;
};

struct mmp_vco_params {
	unsigned long vco_min;
	unsigned long vco_max;
	void __iomem *cr; /* Control Register */
	void __iomem *swcr; /* Software Control Register */
	void __iomem *lock_reg;
	u32 lock_enable_bit;
	unsigned long default_rate;
};

struct clk_vco {
	struct clk_hw hw;
	spinlock_t *lock;
	u32 flags;
	struct mmp_vco_params *params;
};

struct clk_pll {
	struct clk_hw hw;
	const char *parent;
	spinlock_t *lock;
	u32 flags;
	void __iomem *swcr;
};

#define to_clk_vco(vco_hw) container_of(vco_hw, struct clk_vco, hw)
#define to_clk_pll(pll_hw) container_of(pll_hw, struct clk_pll, hw)

/* VCO flags */
#define HELANX_VCO_SSC_FEAT		BIT(0)
#define HELANX_VCO_SSC_AON		BIT(1)
#define HELANX_VCO_28NM			BIT(2)
#define HELANX_VCO_SKIP_DEF_RATE	BIT(3)

extern struct clk *helanx_register_clk_vco(const char *name,
		const char *parent_name, unsigned long flags, u32 vco_flags,
		spinlock_t *lock, struct mmp_vco_params *params);

/* PLL flags */
#define HELANX_PLLOUT			BIT(0)
#define HELANX_PLLOUTP			BIT(1)

extern struct clk *helanx_register_clk_pll(const char *name,
		const char *parent_name, unsigned long flags, u32 pll_flags,
		spinlock_t *lock, void __iomem *swcr);

#endif
