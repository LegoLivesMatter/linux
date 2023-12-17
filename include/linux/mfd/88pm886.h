/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __MFD_88PM886_H
#define __MFD_88PM886_H

#include <linux/i2c.h>
#include <linux/regmap.h>

#define PM886_A1_CHIP_ID		0xa1

#define PM886_REG_ID			0x00

#define PM886_REG_STATUS1		0x01
#define PM886_ONKEY_STS1		BIT(0)

#define PM886_REG_MISC_CONFIG1		0x14
#define PM886_SW_PDOWN			BIT(5)

#define PM886_REG_MISC_CONFIG2		0x15
#define PM886_INT_INV			BIT(0)
#define PM886_INT_CLEAR			BIT(1)
#define PM886_INT_RC			0x00
#define PM886_INT_WC			BIT(1)
#define PM886_INT_MASK_MODE		BIT(2)

struct pm886_chip {
	struct i2c_client *client;
	unsigned int chip_id;
	struct regmap *regmap;
};
#endif /* __MFD_88PM886_H */
