/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __LINUX_MFD_88PM88X_H
#define __LINUX_MFD_88PM88X_H

#include <linux/mfd/core.h>

#define PM886_A1_WHOAMI		0xa1

#define PM88X_REG_ID			0x00

#define PM88X_REG_STATUS1		0x01
#define PM88X_ONKEY_STS1		BIT(0)

#define PM88X_REG_MISC_CONFIG1		0x14
#define PM88X_SW_PDOWN			BIT(5)

#define PM88X_REG_MISC_CONFIG2		0x15
#define PM88X_INT_INV			BIT(0)
#define PM88X_INT_CLEAR			BIT(1)
#define PM88X_INT_RC			0x00
#define PM88X_INT_WC			BIT(1)
#define PM88X_INT_MASK_MODE		BIT(2)

#define PM88X_REG_WDOG			0x1d

#define PM88X_REG_LOWPOWER2		0x21
#define PM88X_REG_LOWPOWER4		0x23

#define PM88X_REG_GPIO_CTRL1		0x30

#define PM88X_REG_GPIO_CTRL2		0x31

#define PM88X_REG_GPIO_CTRL3		0x32

#define PM88X_REG_GPIO_CTRL4		0x33

#define PM88X_REG_BK_OSC_CTRL1		0x50
#define PM88X_REG_BK_OSC_CTRL3		0x52

#define PM88X_REG_AON_CTRL2		0xe2

enum pm88x_regmap_index {
	PM88X_REGMAP_BASE,

	PM88X_REGMAP_NR
};

struct pm88x_data {
	unsigned int whoami;
	struct reg_sequence *presets;
	unsigned int num_presets;
};

struct pm88x_chip {
	struct i2c_client *client;
	struct regmap_irq_chip_data *irq_data;
	const struct pm88x_data *data;
	struct regmap *regmaps[PM88X_REGMAP_NR];
};
#endif /* __LINUX_MFD_88PM88X_H */
