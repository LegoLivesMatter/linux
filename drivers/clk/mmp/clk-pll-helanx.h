#ifndef __MACH_MMP_CLK_PLL_HELANX_H
#define __MACH_MMP_CLK_PLL_HELANX_H

#define MPMU_POSR		0x10
#define MPMU_POSR_PLL2_LOCK	BIT(29)
#define MPMU_POSR_PLL3_LOCK	BIT(30)
#define MPMU_POSR_PLL4_LOCK	BIT(31)

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

struct mmp_param_vco {
	unsigned int id;
	char *name;
	const char *parent_name;
	unsigned long clk_flags;
	u32 vco_flags;
	unsigned int cr_offset; /* Control Register Offset */
	unsigned int swcr_offset; /* Software Control Register Offset */
	spinlock_t *lock;
	unsigned long default_rate;
	unsigned long vco_min;
	unsigned long vco_max;
	u32 lock_enable_bit;
};

struct clk_vco {
	struct clk_hw hw;
	void __iomem *mpmu_base;
	void __iomem *apbs_base;
	struct mmp_param_vco *params;
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

extern struct clk *helanx_register_clk_vco(struct mmp_param_vco *params,
		void __iomem *mpmu_base, void __iomem *apbs_base);

/* PLL flags */
#define HELANX_PLLOUT			BIT(0)
#define HELANX_PLLOUTP			BIT(1)

extern struct clk *helanx_register_clk_pll(const char *name,
		const char *parent_name, unsigned long flags, u32 pll_flags,
		spinlock_t *lock, void __iomem *swcr);

#endif
