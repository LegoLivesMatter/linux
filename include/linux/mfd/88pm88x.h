#ifndef __LINUX_MFD_88PM88X_H
#define __LINUX_MFD_88PM88X_H

#include <linux/mfd/core.h>

#define PM880_A0_WHOAMI		0xb0
#define PM880_A1_WHOAMI		0xb1
#define PM886_A0_WHOAMI		0x00
#define PM886_A1_WHOAMI		0xa1

/* TODO:
 * check the values of the following definitions against DS once again
 * maybe add page specifications (see DS comments)
 * check which of these are really used and what for where unclear
 * FIELD_PREP?
 * */
#define PM88X_REG_WHOAMI		0x00
#define PM88X_REG_STATUS1		BIT(0)
#define PM88X_REG_CHARGER_DETECT	BIT(2)
#define PM88X_REG_BATTERY_DETECT	BIT(3)

#define PM88X_REG_MISC_CONFIG1		0x14
#define PM88X_REG_LONGKEY_RESET		BIT(3)

#define PM88X_REG_WATCHDOG		0x1d

#define PM88X_REG_LOWPOWER2		0x21
#define PM88X_REG_LOWPOWER4		0x23

#define PM88X_REG_CLK_CTRL1		0x25

#define PM88X_REG_GPIO_CTRL1		0x30

#define PM88X_REG_GPIO0_VAL_MASK	BIT(0)
#define PM88X_REG_GPIO0_MODE_MASK	GENMASK(3, 1)
#define PM88X_REG_GPIO1_VAL_MASK	BIT(4)
#define PM88X_REG_GPIO1_MODE_MASK	GENMASK(7, 5)
#define PM88X_REG_GPIO1_SET_DVC		BIT(6)

#define PM88X_REG_GPIO_CTRL2		0x31
#define PM88X_REG_GPIO2_VAL_MASK	BIT(0)
#define PM88X_REG_GPIO2_MODE_MASK	GENMASK(3, 1)

#define PM88X_REG_GPIO_CTRL3		0x32

#define PM88X_REG_GPIO_CTRL4		0x33
#define PM88X_REG_GPIO_5V_1_VAL_MASK	BIT(0)
#define PM88X_REG_GPIO_5V_1_MODE_MASK	GENMASK(3, 1)
#define PM88X_REG_GPIO_5V_2_VAL_MASK	BIT(4)
#define PM88X_REG_GPIO_5V_2_MODE_MASK	GENMASK(7, 5)

#define PM88X_REG_BK_OSC_CTRL1		0x50
#define PM88X_REG_BK_OSC_CTRL3		0x52

#define PM88X_REG_RTC_ALARM_CTRL1	0xd0
#define PM88X_REG_RTC_ALARM_WAKEUP	BIT(4)
#define PM88X_REG_RTC_USE_XO		BIT(7)

#define PM88X_REG_AON_CTRL2		0xe2
#define PM88X_REG_AON_CTRL3		0xe3
#define PM88X_REG_AON_CTRL4		0xe4
#define PM88X_REG_AON_CTRL7		0xe7

#define PM88X_REG_RTC_RESERVED1		0xea
#define PM88X_REG_RTC_RESERVED2		0xeb
#define PM88X_REG_RTC_RESERVED3		0xec
#define PM88X_REG_RTC_RESERVED4		0xed
#define PM88X_REG_RTC_SPARE5		0xee
#define PM88X_REG_RTC_SPARE6		0xef

#define PM88X_REG_GPADC_CONFIG1		0x01

#define PM88X_REG_GPADC_CONFIG2		0x02
#define PM88X_REG_GPADC0_MEAS_EN	BIT(2)
#define PM88X_REG_GPADC1_MEAS_EN	BIT(3)
#define PM88X_REG_GPADC2_MEAS_EN	BIT(4)
#define PM88X_REG_GPADC3_MEAS_EN	BIT(5)

#define PM88X_REG_GPADC_CONFIG3		0x03

#define PM88X_REG_GPADC_CONFIG6		0x06
#define PM88X_REG_GPADC_CONFIG8		0x08

#define PM88X_REG_GPADC0_LOW_TH		0x20
#define PM88X_REG_GPADC1_LOW_TH		0x21
#define PM88X_REG_GPADC2_LOW_TH		0x22
#define PM88X_REG_GPADC3_LOW_TH		0x23

#define PM88X_REG_GPADC0_UPP_TH		0x30
#define PM88X_REG_GPADC1_UPP_TH		0x31
#define PM88X_REG_GPADC2_UPP_TH		0x32
#define PM88X_REG_GPADC3_UPP_TH		0x33

#define PM88X_REG_VBUS_MEAS1		0x4a
#define PM88X_REG_GPADC0_MEAS1		0x54
#define PM88X_REG_GPADC1_MEAS1		0x56
#define PM88X_REG_GPADC2_MEAS1		0x58
#define PM88X_REG_GPADC3_MEAS1		0x5a

#define PM88X_REG_GPADC_RTC_SPARE6	0xc6

#define PM88X_REG_CHARGER_CONFIG1	0x28

struct pm88x_data {
	unsigned int whoami;
	struct mfd_cell *devs;
	unsigned int num_devs;
	struct reg_sequence *presets;
	unsigned int num_presets;
	/* FIXME: is this chip specific or should it be a DT option as in DS? */
	int irq_mode;
};

struct pm88x_chip {
	struct i2c_client *client;
	struct regmap_irq_chip_data *irq_data;
	const struct pm88x_data *data;
	struct regmap *regmap;
	struct regmap *ldo_regmap;
	struct regmap *buck_regmap;
	struct i2c_client *ldo_page;
	struct i2c_client *buck_page;
};
#endif /* __LINUX_MFD_88PM88X_H */
