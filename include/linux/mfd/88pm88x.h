#ifndef __LINUX_MFD_88PM88X_H
#define __LINUX_MFD_88PM88X_H

#include <linux/mfd/core.h>

#define PM880_WHOAMI 0xb1
#define PM886_WHOAMI 0xa1

#define PM88X_ID_VOTG 0
/* registers */
#define PM88X_ID 				0x00
#define PM88X_STATUS1		0x01
#define PM88X_WATCHDOG 		0x1d
#define PM88X_MISC_CONFIG1	0x14
#define PM88X_MISC_CONFIG2	0x15
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

enum {
	PM886_ID_BUCK1 = 0,
	PM886_ID_BUCK2,
	PM886_ID_BUCK3,
	PM886_ID_BUCK4,
	PM886_ID_BUCK5,

	PM886_ID_BUCK_MAX = 5,
};

enum {
	PM886_ID_LDO1 = 0,
	PM886_ID_LDO2,
	PM886_ID_LDO3,
	PM886_ID_LDO4,
	PM886_ID_LDO5,
	PM886_ID_LDO6,
	PM886_ID_LDO7,
	PM886_ID_LDO8,
	PM886_ID_LDO9,
	PM886_ID_LDO10,
	PM886_ID_LDO11,
	PM886_ID_LDO12,
	PM886_ID_LDO13,
	PM886_ID_LDO14,
	PM886_ID_LDO15,
	PM886_ID_LDO16 = 15,

	PM886_ID_LDO_MAX = 16,
};

enum {
	PM886_ID_BUCK1_SLP = 0,
};

enum {
	PM880_ID_BUCK1A = 0,
	PM880_ID_BUCK2,
	PM880_ID_BUCK3,
	PM880_ID_BUCK4,
	PM880_ID_BUCK5,
	PM880_ID_BUCK6,
	PM880_ID_BUCK7,

	PM880_ID_BUCK_MAX = 7,
};

enum {
	PM880_ID_BUCK1A_SLP = 0,
	PM880_ID_BUCK1B_SLP,
};

enum {
	PM880_ID_BUCK1A_AUDIO = 0,
	PM880_ID_BUCK1B_AUDIO,
};

enum {
	PM880_ID_LDO1 = 0,
	PM880_ID_LDO2,
	PM880_ID_LDO3,
	PM880_ID_LDO4,
	PM880_ID_LDO5,
	PM880_ID_LDO6,
	PM880_ID_LDO7,
	PM880_ID_LDO8,
	PM880_ID_LDO9,
	PM880_ID_LDO10,
	PM880_ID_LDO11,
	PM880_ID_LDO12,
	PM880_ID_LDO13,
	PM880_ID_LDO14 = 13,
	PM880_ID_LDO15,
	PM880_ID_LDO16 = 15,

	PM880_ID_LDO17 = 16,
	PM880_ID_LDO18 = 17,

	PM880_ID_LDO_MAX = 18,
};
#define PM88X_ID_REG			(0x0)

#define PM88X_STATUS1			(0x1)
#define PM88X_CHG_DET			(1 << 2)
#define PM88X_BAT_DET			(1 << 3)

#define PM88X_MISC_CONFIG1		(0x14)
#define PM88X_LONKEY_RST		(1 << 3)

#define PM88X_WDOG			(0x1d)

#define PM88X_LOWPOWER2			(0x21)
#define PM88X_LOWPOWER4			(0x23)

/* clk control register */
#define PM88X_CLK_CTRL1			(0x25)

/* gpio */
#define PM88X_GPIO_CTRL1		(0x30)
#define PM88X_GPIO0_VAL_MSK		(0x1 << 0)
#define PM88X_GPIO0_MODE_MSK		(0x7 << 1)
#define PM88X_GPIO1_VAL_MSK		(0x1 << 4)
#define PM88X_GPIO1_MODE_MSK		(0x7 << 5)
#define PM88X_GPIO1_SET_DVC		(0x2 << 5)

#define PM88X_GPIO_CTRL2		(0x31)
#define PM88X_GPIO2_VAL_MSK		(0x1 << 0)
#define PM88X_GPIO2_MODE_MSK		(0x7 << 1)

#define PM88X_GPIO_CTRL3		(0x32)

#define PM88X_GPIO_CTRL4		(0x33)
#define PM88X_GPIO5V_1_VAL_MSK		(0x1 << 0)
#define PM88X_GPIO5V_1_MODE_MSK		(0x7 << 1)
#define PM88X_GPIO5V_2_VAL_MSK		(0x1 << 4)
#define PM88X_GPIO5V_2_MODE_MSK		(0x7 << 5)

#define PM88X_BK_OSC_CTRL1		(0x50)
#define PM88X_BK_OSC_CTRL3		(0x52)

#define PM88X_RTC_ALARM_CTRL1		(0xd0)
#define PM88X_ALARM_WAKEUP		(1 << 4)
#define PM88X_USE_XO			(1 << 7)

#define PM88X_AON_CTRL2			(0xe2)
#define PM88X_AON_CTRL3			(0xe3)
#define PM88X_AON_CTRL4			(0xe4)
#define PM88X_AON_CTRL7			(0xe7)

/* 0xea, 0xeb, 0xec, 0xed are reserved by RTC */
#define PM88X_RTC_SPARE5		(0xee)
#define PM88X_RTC_SPARE6		(0xef)
/*-------------------------------------------------------------------------*/

/*--power page:------------------------------------------------------------*/

/*-------------------------------------------------------------------------*/

/*--gpadc page:------------------------------------------------------------*/

#define PM88X_GPADC_CONFIG1		(0x1)

#define PM88X_GPADC_CONFIG2		(0x2)
#define PM88X_GPADC0_MEAS_EN		(1 << 2)
#define PM88X_GPADC1_MEAS_EN		(1 << 3)
#define PM88X_GPADC2_MEAS_EN		(1 << 4)
#define PM88X_GPADC3_MEAS_EN		(1 << 5)

#define PM88X_GPADC_CONFIG3		(0x3)

#define PM88X_GPADC_CONFIG6		(0x6)
#define PM88X_GPADC_CONFIG8		(0x8)

#define PM88X_GPADC0_LOW_TH		(0x20)
#define PM88X_GPADC1_LOW_TH		(0x21)
#define PM88X_GPADC2_LOW_TH		(0x22)
#define PM88X_GPADC3_LOW_TH		(0x23)

#define PM88X_GPADC0_UPP_TH		(0x30)
#define PM88X_GPADC1_UPP_TH		(0x31)
#define PM88X_GPADC2_UPP_TH		(0x32)
#define PM88X_GPADC3_UPP_TH		(0x33)

#define PM88X_VBUS_MEAS1		(0x4A)
#define PM88X_GPADC0_MEAS1		(0x54)
#define PM88X_GPADC1_MEAS1		(0x56)
#define PM88X_GPADC2_MEAS1		(0x58)
#define PM88X_GPADC3_MEAS1		(0x5A)


/*--charger page:------------------------------------------------------------*/
#define PM88X_CHG_CONFIG1		(0x28)
#define PM88X_CHGBK_CONFIG6		(0x50)
/*-------------------------------------------------------------------------*/

/*--test page:-------------------------------------------------------------*/

/*-------------------------------------------------------------------------*/

#define PM886_BUCK1_VOUT	(0xa5)
#define PM886_BUCK1_1_VOUT	(0xa6)
#define PM886_BUCK1_2_VOUT	(0xa7)
#define PM886_BUCK1_3_VOUT	(0xa8)
#define PM886_BUCK1_4_VOUT	(0x9a)
#define PM886_BUCK1_5_VOUT	(0x9b)
#define PM886_BUCK1_6_VOUT	(0x9c)
#define PM886_BUCK1_7_VOUT	(0x9d)

/*
 * buck sleep mode control registers:
 * 00-disable,
 * 01/10-sleep voltage,
 * 11-active voltage
 */
#define PM886_BUCK1_SLP_CTRL	(0xa2)
#define PM886_BUCK2_SLP_CTRL	(0xb0)
#define PM886_BUCK3_SLP_CTRL	(0xbe)
#define PM886_BUCK4_SLP_CTRL	(0xcc)
#define PM886_BUCK5_SLP_CTRL	(0xda)

/*
 * ldo sleep mode control registers:
 * 00-disable,
 * 01/10-sleep voltage,
 * 11-active voltage
 */
#define PM886_LDO1_SLP_CTRL	(0x21)
#define PM886_LDO2_SLP_CTRL	(0x27)
#define PM886_LDO3_SLP_CTRL	(0x2d)
#define PM886_LDO4_SLP_CTRL	(0x33)
#define PM886_LDO5_SLP_CTRL	(0x39)
#define PM886_LDO6_SLP_CTRL	(0x3f)
#define PM886_LDO7_SLP_CTRL	(0x45)
#define PM886_LDO8_SLP_CTRL	(0x4b)
#define PM886_LDO9_SLP_CTRL	(0x51)
#define PM886_LDO10_SLP_CTRL	(0x57)
#define PM886_LDO11_SLP_CTRL	(0x5d)
#define PM886_LDO12_SLP_CTRL	(0x63)
#define PM886_LDO13_SLP_CTRL	(0x69)
#define PM886_LDO14_SLP_CTRL	(0x6f)
#define PM886_LDO15_SLP_CTRL	(0x75)
#define PM886_LDO16_SLP_CTRL	(0x7b)

#define PM880_BUCK1_VOUT	(0x28)

#define PM880_BUCK1A_VOUT	(0x28) /* voltage 0 */
#define PM880_BUCK1A_1_VOUT	(0x29)
#define PM880_BUCK1A_2_VOUT	(0x2a)
#define PM880_BUCK1A_3_VOUT	(0x2b)
#define PM880_BUCK1A_4_VOUT	(0x2c)
#define PM880_BUCK1A_5_VOUT	(0x2d)
#define PM880_BUCK1A_6_VOUT	(0x2e)
#define PM880_BUCK1A_7_VOUT	(0x2f)
#define PM880_BUCK1A_8_VOUT	(0x30)
#define PM880_BUCK1A_9_VOUT	(0x31)
#define PM880_BUCK1A_10_VOUT	(0x32)
#define PM880_BUCK1A_11_VOUT	(0x33)
#define PM880_BUCK1A_12_VOUT	(0x34)
#define PM880_BUCK1A_13_VOUT	(0x35)
#define PM880_BUCK1A_14_VOUT	(0x36)
#define PM880_BUCK1A_15_VOUT	(0x37)

#define PM880_BUCK1B_VOUT	(0x40)
#define PM880_BUCK1B_1_VOUT	(0x41)
#define PM880_BUCK1B_2_VOUT	(0x42)
#define PM880_BUCK1B_3_VOUT	(0x43)
#define PM880_BUCK1B_4_VOUT	(0x44)
#define PM880_BUCK1B_5_VOUT	(0x45)
#define PM880_BUCK1B_6_VOUT	(0x46)
#define PM880_BUCK1B_7_VOUT	(0x47)
#define PM880_BUCK1B_8_VOUT	(0x48)
#define PM880_BUCK1B_9_VOUT	(0x49)
#define PM880_BUCK1B_10_VOUT	(0x4a)
#define PM880_BUCK1B_11_VOUT	(0x4b)
#define PM880_BUCK1B_12_VOUT	(0x4c)
#define PM880_BUCK1B_13_VOUT	(0x4d)
#define PM880_BUCK1B_14_VOUT	(0x4e)
#define PM880_BUCK1B_15_VOUT	(0x4f)

/* buck7 has dvc function */
#define PM880_BUCK7_VOUT	(0xb8) /* voltage 0 */
#define PM880_BUCK7_1_VOUT	(0xb9)
#define PM880_BUCK7_2_VOUT	(0xba)
#define PM880_BUCK7_3_VOUT	(0xbb)

/*
 * buck sleep mode control registers:
 * 00-disable,
 * 01/10-sleep voltage,
 * 11-active voltage
 */
#define PM880_BUCK1A_SLP_CTRL	(0x27)
#define PM880_BUCK1B_SLP_CTRL	(0x3c)
#define PM880_BUCK2_SLP_CTRL	(0x54)
#define PM880_BUCK3_SLP_CTRL	(0x6c)
/* TODO: there are 7 controls bit for buck4~7 */
#define PM880_BUCK4_SLP_CTRL	(0x84)
#define PM880_BUCK5_SLP_CTRL	(0x94)
#define PM880_BUCK6_SLP_CTRL	(0xa4)
#define PM880_BUCK7_SLP_CTRL	(0xb4)

/*
 * ldo sleep mode control registers:
 * 00-disable,
 * 01/10-sleep voltage,
 * 11-active voltage
 */
#define PM880_LDO1_SLP_CTRL	(0x21)
#define PM880_LDO2_SLP_CTRL	(0x27)
#define PM880_LDO3_SLP_CTRL	(0x2d)
#define PM880_LDO4_SLP_CTRL	(0x33)
#define PM880_LDO5_SLP_CTRL	(0x39)
#define PM880_LDO6_SLP_CTRL	(0x3f)
#define PM880_LDO7_SLP_CTRL	(0x45)
#define PM880_LDO8_SLP_CTRL	(0x4b)
#define PM880_LDO9_SLP_CTRL	(0x51)
#define PM880_LDO10_SLP_CTRL	(0x57)
#define PM880_LDO11_SLP_CTRL	(0x5d)
#define PM880_LDO12_SLP_CTRL	(0x63)
#define PM880_LDO13_SLP_CTRL	(0x69)
#define PM880_LDO14_SLP_CTRL	(0x6f)
#define PM880_LDO15_SLP_CTRL	(0x75)
#define PM880_LDO16_SLP_CTRL	(0x7b)
#define PM880_LDO17_SLP_CTRL	(0x81)
#define PM880_LDO18_SLP_CTRL	(0x87)

#endif /* __LINUX_MFD_88PM88X_H */
