// SPDX-License-Identifier: GPL-2.0-only
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/mfd/core.h>
#include <linux/notifier.h>
#include <linux/reboot.h>

#include <linux/mfd/88pm88x.h>

/* interrupt status registers */
#define PM88X_REG_IRQ_STATUS1			0x05

#define PM88X_REG_IRQ1				0x0a
#define PM88X_IRQ1_ONKEY			BIT(0)
#define PM88X_IRQ1_EXTON			BIT(1) // unused
#define PM88X_IRQ1_CHARGER			BIT(2)
#define PM88X_IRQ1_BATTERY			BIT(3) // unused
#define PM88X_IRQ1_RTC				BIT(4)
#define PM88X_IRQ1_CLASSD			BIT(5) // unused
#define PM88X_IRQ1_XO				BIT(6) // unused
#define PM88X_IRQ1_GPIO				BIT(7) // unused

#define PM88X_REG_IRQ2				0x0b
#define PM88X_IRQ2_BATTERY_VOLTAGE		BIT(0)
#define PM88X_IRQ2_RESERVED1			BIT(1) // unused
#define PM88X_IRQ2_VBUS				BIT(2) // unused
#define PM88X_IRQ2_ITEMP			BIT(3) // unused
#define PM88X_IRQ2_BUCK_PGOOD			BIT(4) // unused
#define PM88X_IRQ2_LDO_PGOOD			BIT(5) // unused
#define PM88X_IRQ2_RESERVED6			BIT(6) // unused
#define PM88X_IRQ2_RESERVED7			BIT(7) // unused

#define PM88X_REG_IRQ3				0x0c
#define PM88X_IRQ3_GPADC0			BIT(0)
#define PM88X_IRQ3_GPADC1			BIT(1)
#define PM88X_IRQ3_GPADC2			BIT(2)
#define PM88X_IRQ3_GPADC3			BIT(3)
#define PM88X_IRQ3_MICROPHONE 	   		BIT(4)
#define PM88X_IRQ3_HEADSET			BIT(5)
#define PM88X_IRQ3_GND  	 	   	BIT(6) // unused
#define PM88X_IRQ3_RESERVED7			BIT(7) // unused

#define PM88X_REG_IRQ4				0x0d
#define PM88X_IRQ4_CHARGER_FAIL			BIT(0)
#define PM88X_IRQ4_CHARGER_DONE			BIT(1)
#define PM88X_IRQ4_RESERVED2			BIT(2) // unused
#define PM88X_IRQ4_OTG_FAIL			BIT(3)
#define PM88X_IRQ4_RESERVED4			BIT(4) // unused
#define PM88X_IRQ4_CHARGER_ILIM			BIT(5) // unused
#define PM88X_IRQ4_BATTERY_CC			BIT(6)
#define PM88X_IRQ4_RESERVED7			BIT(7) // unused

enum pm88x_irq_number {
	PM88X_IRQ_ONKEY,
	PM88X_IRQ_EXTON,
	PM88X_IRQ_CHARGER_GOOD,
	PM88X_IRQ_BATTTERY,
	PM88X_IRQ_RTC,
	PM88X_IRQ_CLASSD,
	PM88X_IRQ_XO,
	PM88X_IRQ_GPIO,

	PM88X_IRQ_BATTERY_VOLTAGE,

	PM88X_IRQ_VBUS,
	PM88X_IRQ_ITEMP,
	PM88X_IRQ_BUCK_PGOOD,
	PM88X_IRQ_LDO_PGOOD,

	PM88X_IRQ_GPADC0,
	PM88X_IRQ_GPADC1,
	PM88X_IRQ_GPADC2,
	PM88X_IRQ_GPADC3,
	PM88X_IRQ_MICROPHONE,
	PM88X_IRQ_HEADSET,
	PM88X_IRQ_GND,

	PM88X_IRQ_CHARGER_FAIL,
	PM88X_IRQ_CHARGER_DONE,

	PM88X_IRQ_FLASH_FAIL,
	PM88X_IRQ_OTG_FAIL,
	PM88X_IRQ_CHARGER_ILIM,

	PM88X_IRQ_BATTERY_CC,

	PM88X_MAX_IRQ,
};

static struct regmap_irq pm88x_regmap_irqs[] = {
	REGMAP_IRQ_REG(PM88X_IRQ_ONKEY, 0, PM88X_IRQ1_ONKEY),
};

static struct regmap_irq_chip pm88x_regmap_irq_chip = {
	.name = "88pm88x",
	.irqs = pm88x_regmap_irqs,
	.num_irqs = ARRAY_SIZE(pm88x_regmap_irqs),
	.num_regs = 4,
	.status_base = PM88X_REG_IRQ_STATUS1,
	.ack_base = PM88X_REG_IRQ_STATUS1,
	.unmask_base = PM88X_REG_IRQ1,
};

static struct reg_sequence pm880_presets[] = {
	/* disable watchdog */
	REG_SEQ0(PM88X_REG_WATCHDOG, 0x01),
	/* output 32 kHz from XO */
	REG_SEQ0(PM88X_REG_AON_CTRL2, 0x2a),
	/* OSC_FREERUN = 1, to lock FLL */
	REG_SEQ0(PM88X_REG_BK_OSC_CTRL1, 0x0f),
	/* XO_LJ = 1, enable low jitter for 32 kHz */
	REG_SEQ0(PM88X_REG_LOWPOWER2, 0x20),
	/* enable LPM for internal reference group in sleep */
	REG_SEQ0(PM88X_REG_LOWPOWER4, 0xc0),
	/* set the duty cycle of charger DC/DC to max */
	REG_SEQ0(PM88X_REG_BK_OSC_CTRL3, 0xc0),
};

static struct reg_sequence pm886_presets[] = {
	/* disable watchdog */
	REG_SEQ0(PM88X_REG_WATCHDOG, 0x01),
	/* GPIO1: DVC, GPIO0: input */
	REG_SEQ0(PM88X_REG_GPIO_CTRL1, 0x40),
	/* GPIO2: input */
	REG_SEQ0(PM88X_REG_GPIO_CTRL2, 0x00),
	/* DVC2, DVC1 */
	REG_SEQ0(PM88X_REG_GPIO_CTRL3, 0x44),
	/* GPIO5V_1:input, GPIO5V_2: input */
	REG_SEQ0(PM88X_REG_GPIO_CTRL4, 0x00),
	/* output 32 kHz from XO */
	REG_SEQ0(PM88X_REG_AON_CTRL2, 0x2a),
	/* OSC_FREERUN = 1, to lock FLL */
	REG_SEQ0(PM88X_REG_BK_OSC_CTRL1, 0x0f),
	/* XO_LJ = 1, enable low jitter for 32 kHz */
	REG_SEQ0(PM88X_REG_LOWPOWER2, 0x20),
	/* OV_VSYS and UV_VSYS1 comparators on VSYS disabled, VSYS_OVER_TH : 5.6V */
	REG_SEQ0(PM88X_REG_LOWPOWER4, 0xc8),
	/* set the duty cycle of charger DC/DC to max */
	REG_SEQ0(PM88X_REG_BK_OSC_CTRL3, 0xc0),
};

static struct resource onkey_resources[] = {
	DEFINE_RES_IRQ_NAMED(PM88X_IRQ_ONKEY, "88pm88x-onkey"),
};

static struct mfd_cell pm88x_devs[] = {
	{
		.name = "88pm88x-onkey",
		.num_resources = ARRAY_SIZE(onkey_resources),
		.resources = onkey_resources,
		.id = -1,
	},
};

static struct mfd_cell pm880_devs[] = {
};

static struct mfd_cell pm886_devs[] = {
};

static struct pm88x_data pm880_a1_data = {
	.whoami = PM880_A1_WHOAMI,
	.devs = pm880_devs,
	.num_devs = ARRAY_SIZE(pm880_devs),
	.presets = pm880_presets,
	.num_presets = ARRAY_SIZE(pm880_presets),
	.irq_mode = 1,
};

static struct pm88x_data pm886_a1_data = {
	.whoami = PM886_A1_WHOAMI,
	.devs = pm886_devs,
	.num_devs = ARRAY_SIZE(pm886_devs),
	.presets = pm886_presets,
	.num_presets = ARRAY_SIZE(pm886_presets),
	.irq_mode = 1,
};

static const struct regmap_config pm88x_i2c_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xfe,
};

static int pm88x_power_off_handler(struct sys_off_data *data)
{
	struct pm88x_chip *chip = data->cb_data;
	int ret;

	ret = regmap_update_bits(chip->regmaps[PM88X_REGMAP_BASE], PM88X_REG_MISC_CONFIG1,
			PM88X_SW_POWERDOWN, PM88X_SW_POWERDOWN);
	if (ret) {
		dev_err(&chip->client->dev, "Failed to power off the device: %d\n", ret);
		return NOTIFY_BAD;
	}
	return NOTIFY_DONE;
}

static int pm88x_setup_irq(struct pm88x_chip *chip)
{
	int mask, data, ret;

	mask = PM88X_IRQ_INV | PM88X_IRQ_CLEAR | PM88X_IRQ_MODE_MASK;
	data = chip->data->irq_mode ? PM88X_IRQ_WRITE_CLEAR : PM88X_IRQ_READ_CLEAR;
	ret = regmap_update_bits(chip->regmaps[PM88X_REGMAP_BASE], PM88X_REG_MISC_CONFIG2,
			mask, data);
	if (ret) {
		dev_err(&chip->client->dev, "Failed to set interrupt clearing mode: %d\n", ret);
		return ret;
	}

	ret = devm_regmap_add_irq_chip(&chip->client->dev, chip->regmaps[PM88X_REGMAP_BASE],
			chip->client->irq, IRQF_ONESHOT, -1, &pm88x_regmap_irq_chip,
			&chip->irq_data);
	if (ret) {
		dev_err(&chip->client->dev, "Failed to request IRQ: %d\n", ret);
		return ret;
	}

	return 0;
}

static int pm88x_mfd_add_devices(struct pm88x_chip *chip)
{
	int ret;

	/* add common devices */
	ret = devm_mfd_add_devices(&chip->client->dev, 0, pm88x_devs, ARRAY_SIZE(pm88x_devs),
			NULL, 0, regmap_irq_get_domain(chip->irq_data));
	if (ret) {
		dev_err(&chip->client->dev, "Failed to add common devices: %d\n", ret);
		return ret;
	}

	/* add chip-specific devices */
	ret = devm_mfd_add_devices(&chip->client->dev, 0, chip->data->devs, chip->data->num_devs,
			NULL, 0, regmap_irq_get_domain(chip->irq_data));
	if (ret) {
		dev_err(&chip->client->dev, "Failed to add chip-specific devices: %d\n", ret);
		return ret;
	}

	return 0;
}

static int pm88x_initialize_regmaps(struct pm88x_chip *chip)
{
	struct i2c_client *page;
	struct regmap *regmap;
	int ret;

	/* LDO page */
	page = devm_i2c_new_dummy_device(&chip->client->dev, chip->client->adapter, chip->client->addr + PM88X_PAGE_OFFSET_LDO);
	if (IS_ERR(page)) {
		ret = PTR_ERR(page);
		dev_err(&chip->client->dev, "Failed to initialize LDO: %d\n", ret);
		return ret;
	}
	regmap = devm_regmap_init_i2c(page, &pm88x_i2c_regmap);
	if (IS_ERR(regmap)) {
		ret = PTR_ERR(regmap);
		dev_err(&chip->client->dev, "Failed to initialize LDO regmap: %d\n", ret);
		return ret;
	}
	chip->regmaps[PM88X_REGMAP_LDO] = regmap;
	/* power page is the same as LDO */
	chip->regmaps[PM88X_REGMAP_POWER] = regmap;

	/* buck page */
	switch (chip->data->whoami) {
	case PM880_A1_WHOAMI:
		page = devm_i2c_new_dummy_device(&chip->client->dev, chip->client->adapter, chip->client->addr + PM880_PAGE_OFFSET_BUCK);
		if (IS_ERR(page)) {
			ret = PTR_ERR(page);
			dev_err(&chip->client->dev, "Failed to initialize buck page: %d\n", ret);
			return ret;
		}
		regmap = devm_regmap_init_i2c(page, &pm88x_i2c_regmap);
		if (IS_ERR(regmap)) {
			ret = PTR_ERR(regmap);
			dev_err(&chip->client->dev, "Failed to initialize buck regmap: %d\n", ret);
			return ret;
		}
		break;
	case PM886_A1_WHOAMI:
		regmap = chip->regmaps[PM88X_REGMAP_LDO];
		break;
	}
	chip->regmaps[PM88X_REGMAP_BUCK] = regmap;

	return 0;
}

static int pm88x_probe(struct i2c_client *client)
{
	struct pm88x_chip *chip;
	int ret = 0;
	unsigned int chip_id;

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->client = client;
	chip->data = device_get_match_data(&client->dev);
	i2c_set_clientdata(client, chip);

	device_init_wakeup(&client->dev, 1);

	chip->regmaps[PM88X_REGMAP_BASE] = devm_regmap_init_i2c(client, &pm88x_i2c_regmap);
	if (IS_ERR(chip->regmaps[PM88X_REGMAP_BASE])) {
		ret = PTR_ERR(chip->regmaps[PM88X_REGMAP_BASE]);
		dev_err(&client->dev, "Failed to initialize regmap: %d\n", ret);
		return ret;
	}

	ret = regmap_read(chip->regmaps[PM88X_REGMAP_BASE], PM88X_REG_WHOAMI, &chip_id);
	if (ret) {
		dev_err(&client->dev, "Failed to read chip ID: %d\n", ret);
		return ret;
	}
	if (chip->data->whoami != chip_id) {
		dev_err(&client->dev, "Device reported wrong chip ID: %u\n", chip_id);
		return -EINVAL;
	}

	ret = pm88x_initialize_regmaps(chip);
	if (ret)
		return ret;

	ret = pm88x_setup_irq(chip);
	if (ret)
		return ret;

	ret = pm88x_mfd_add_devices(chip);
	if (ret)
		return ret;

	ret = regmap_register_patch(chip->regmaps[PM88X_REGMAP_BASE], chip->data->presets,
			chip->data->num_presets);
	if (ret) {
		dev_err(&client->dev, "Failed to register regmap patch: %d\n", ret);
		return ret;
	}

	ret = devm_register_power_off_handler(&client->dev, pm88x_power_off_handler, chip);
	if (ret) {
		dev_err(&client->dev, "Failed to register power off handler: %d\n", ret);
		return ret;
	}

	return 0;
}

const struct of_device_id pm88x_of_match[] = {
	{ .compatible = "marvell,88pm880-a1", .data = &pm880_a1_data },
	{ .compatible = "marvell,88pm886-a1", .data = &pm886_a1_data },
	{ },
};

static struct i2c_driver pm88x_i2c_driver = {
	.driver = {
		.name = "88pm88x",
		.of_match_table = of_match_ptr(pm88x_of_match),
	},
	.probe = pm88x_probe,
};
module_i2c_driver(pm88x_i2c_driver);

MODULE_DESCRIPTION("Marvell 88PM88X PMIC driver");
MODULE_AUTHOR("Karel Balej <balejk@matfyz.cz>");
MODULE_LICENSE("GPL");
