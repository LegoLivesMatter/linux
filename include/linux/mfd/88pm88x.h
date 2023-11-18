#ifndef __LINUX_MFD_88PM88X_H
#define __LINUX_MFD_88PM88X_H

#include <linux/mfd/core.h>

#define PM880_WHOAMI 0xb1
#define PM886_WHOAMI 0xa1

/* registers */
#define PM88X_ID 				0x00
#define PM88X_STATUS1		0x01
#define PM88X_WATCHDOG 		0x1d
#define PM88X_MISC_CONFIG1	0x14
#define PM88X_MISC_CONFIG2	0x15
#define PM88X_LONKEY_RST	BIT(3)
#define PM88X_AON_CTRL2		0xe2
#define PM88X_AON_CTRL3		0xe3
#define PM88X_AON_CTRL4		0xe4
#define PM88X_BK_OSC_CTRL1	0x50
#define PM88X_BK_OSC_CTRL3	0x52
#define PM88X_LOWPOWER2		0x21
#define PM88X_LOWPOWER4		0x23
#define PM88X_GPIO_CTRL1	0x30
#define PM88X_GPIO_CTRL2	0x31
#define PM88X_GPIO_CTRL3	0x32
#define PM88X_GPIO_CTRL4	0x33

struct pm88x_data {
	unsigned int whoami;
	struct mfd_cell *devs;
	unsigned int num_devs;
	struct reg_sequence *presets;
	unsigned int num_presets;
};

struct pm88x_chip {
	struct i2c_client *client;
	struct regmap_irq_chip_data *irq_data;
	const struct pm88x_data *data;
	struct regmap *regmap;
	int irq_mode;
	struct regmap *ldo_regmap;
	struct regmap *buck_regmap;
	struct i2c_client *ldo_page;
	struct i2c_client *buck_page;
};
#endif /* __LINUX_MFD_88PM88X_H */
