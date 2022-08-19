#ifndef __MACH_MMP_CLK_PLL_HELANX_H
#define __MACH_MMP_CLK_PLL_HELANX_H

struct kvco_range {
	int vco_min;
	int vco_max;
	u8 kvco;
	u8 vrng;
};

struct div_map {
	unsigned int div;
	unsigned int hw_val;
};
#define HELANX_DIVIDER_COUNT 8

struct intpi_range {
	int vco_min;
	int vco_max;
	u8 value;
};

enum ssc_mode {
	CENTER_SPREAD,
	DOWN_SPREAD
};

struct ssc_params {
	enum ssc_mode ssc_mode;
	int base;
	unsigned int amplitude;
	int desired_mod_freq;

	void __iomem *ssc_ctrl;
	void __iomem *ssc_cfg;
};

struct mmp_vco_freq_table {
	unsigned long output_rate;
	u16 refd;
	u16 fbd;
	u8 kvco;
	u8 vcovnrg;
};

struct mmp_vco_params {
	unsigned long vco_min;
	unsigned long vco_max;
	void __iomem *cr_reg;
	void __iomem *pll_swcr;
	void __iomem *lock_reg;
	u32 lock_enable_bit;
	unsigned long default_rate;

	struct mmp_vco_freq_table *freq_table;
	int freq_table_size;

	bool ssc_enabled;
	struct ssc_params *ssc_params;
};

struct mmp_pll_params {
	void __iomem *pll_swcr;
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
	struct mmp_pll_params *params;
};

#define to_clk_vco(vco_hw) container_of(vco_hw, struct clk_vco, hw)
#define to_clk_pll(pll_hw) container_of(pll_hw, struct clk_pll, hw)

/* VCO flags */
#define HELANX_VCO_SSC_FEAT		BIT(0)
#define HELANX_VCO_SSC_AON		BIT(1)
#define HELANX_VCO_28NM			BIT(2)
#define HElANX_VCO_SKIP_DEF_RATE	BIT(3)

extern struct clk *helanx_clk_register_vco(const char *name,
		const char *parent_name, unsigned long flags, u32 vco_flags,
		spinlock_t *lock, struct mmp_vco_params *params);

/* PLL flags */
#define HELANX_PLLOUT			BIT(0)
#define HELANX_PLLOUTP			BIT(1)

extern struct clk *helanx_clk_register_pll(const char *name,
		const char *parent_name, unsigned long flags, u32 vco_flags,
		spinlock_t *lock, struct mmp_pll_params *params);

#endif
