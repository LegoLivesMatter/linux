// SPDX-License-Identifier: GPL-2.0-only

#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/mfd/core.h>
#include <linux/notifier.h>
#include <linux/reboot.h>

#include <linux/mfd/88pm88x.h>

#define PM88X_INV_INT BIT(0)
#define PM88X_INT_CLEAR	BIT(1)
#define PM88X_INT_READ_CLEAR 0
#define PM88X_INT_WRITE_CLEAR BIT(1)
#define PM88X_INT_MASK_MODE BIT(2)

/* interrupt status registers */
#define PM88X_REG_IRQ_STATUS1			0x05

#define PM88X_REG_IRQ1				0x0a
#define PM88X_REG_IRQ1_ONKEY			BIT(0)
#define PM88X_REG_IRQ1_EXTON			BIT(1) // unused
#define PM88X_REG_IRQ1_CHARGER			BIT(2)
#define PM88X_REG_IRQ1_BATTERY			BIT(3) // unused
#define PM88X_REG_IRQ1_RTC			BIT(4)
#define PM88X_REG_IRQ1_CLASSD			BIT(5) // unused
#define PM88X_REG_IRQ1_XO			BIT(6) // unused
#define PM88X_REG_IRQ1_GPIO			BIT(7) // unused

#define PM88X_REG_IRQ2				0x0b
#define PM88X_REG_IRQ2_BATTERY_VOLTAGE		BIT(0)
#define PM88X_REG_IRQ2_RESERVED1		BIT(1) // unused
#define PM88X_REG_IRQ2_VBUS			BIT(2) // unused
#define PM88X_REG_IRQ2_ITEMP			BIT(3) // unused
#define PM88X_REG_IRQ2_BUCK_PGOOD		BIT(4) // unused
#define PM88X_REG_IRQ2_LDO_PGOOD		BIT(5) // unused
#define PM88X_REG_IRQ2_RESERVED6		BIT(6) // unused
#define PM88X_REG_IRQ2_RESERVED7		BIT(7) // unused

#define PM88X_REG_IRQ3				0x0c
#define PM88X_REG_IRQ3_GPADC0			BIT(0)
#define PM88X_REG_IRQ3_GPADC1			BIT(1)
#define PM88X_REG_IRQ3_GPADC2			BIT(2)
#define PM88X_REG_IRQ3_GPADC3			BIT(3)
#define PM88X_REG_IRQ3_MICROPHONE 	   	BIT(4)
#define PM88X_REG_IRQ3_HEADSET			BIT(5)
#define PM88X_REG_IRQ3_GND  	 	   	BIT(6) // unused
#define PM88X_REG_IRQ3_RESERVED7		BIT(7) // unused

#define PM88X_REG_IRQ4				0x0d
#define PM88X_REG_IRQ4_CHARGER_FAIL		BIT(0)
#define PM88X_REG_IRQ4_CHARGER_DONE		BIT(1)
#define PM88X_REG_IRQ4_RESERVED2		BIT(2) // unused
#define PM88X_REG_IRQ4_OTG_FAIL			BIT(3)
#define PM88X_REG_IRQ4_RESERVED4		BIT(4) // unused
#define PM88X_REG_IRQ4_CHARGER_ILIM		BIT(5) // unused
#define PM88X_REG_IRQ4_BATTERY_CC		BIT(6)
#define PM88X_REG_IRQ4_RESERVED7		BIT(7) // unused

#define PM88X_SW_PDOWN	BIT(5)

#define PM880_BUCK_NAME		"88pm880-buck"
#define PM880_LDO_NAME		"88pm880-ldo"
#define PM88X_VIRTUAL_REGULATOR_NAME "88pm88x-vr"

#define PM886_BUCK_NAME		"88pm886-buck"
#define PM886_LDO_NAME		"88pm886-ldo"

#define PM88X_ADDR(client, offset) (client->addr + offset)

#define CELL_DEV(_name, _r, _compatible, _id) { \
	.name = _name, \
	.of_compatible = _compatible, \
	.num_resources = ARRAY_SIZE(_r), \
	.resources = _r, \
	.id = _id, \
	}

static const struct resource pm886_buck_resources[] = {
	{
	.name = PM886_BUCK_NAME,
	},
};

static const struct resource pm886_ldo_resources[] = {
	{
	.name = PM886_LDO_NAME,
	},
};

static const struct resource vr_resources[] = {
	{
	.name = PM88X_VIRTUAL_REGULATOR_NAME,
	},
};

const struct mfd_cell pm886_devs[] = {
	CELL_DEV(PM886_BUCK_NAME, pm886_buck_resources, "marvell,88pm886-buck1", 0),
	CELL_DEV(PM886_BUCK_NAME, pm886_buck_resources, "marvell,88pm886-buck2", 1),
	CELL_DEV(PM886_BUCK_NAME, pm886_buck_resources, "marvell,88pm886-buck3", 2),
	CELL_DEV(PM886_BUCK_NAME, pm886_buck_resources, "marvell,88pm886-buck4", 3),
	CELL_DEV(PM886_BUCK_NAME, pm886_buck_resources, "marvell,88pm886-buck5", 4),
	CELL_DEV(PM886_LDO_NAME, pm886_ldo_resources, "marvell,88pm886-ldo1", 5),
	CELL_DEV(PM886_LDO_NAME, pm886_ldo_resources, "marvell,88pm886-ldo2", 6),
	CELL_DEV(PM886_LDO_NAME, pm886_ldo_resources, "marvell,88pm886-ldo3", 7),
	CELL_DEV(PM886_LDO_NAME, pm886_ldo_resources, "marvell,88pm886-ldo4", 8),
	CELL_DEV(PM886_LDO_NAME, pm886_ldo_resources, "marvell,88pm886-ldo5", 9),
	CELL_DEV(PM886_LDO_NAME, pm886_ldo_resources, "marvell,88pm886-ldo6", 10),
	CELL_DEV(PM886_LDO_NAME, pm886_ldo_resources, "marvell,88pm886-ldo7", 11),
	CELL_DEV(PM886_LDO_NAME, pm886_ldo_resources, "marvell,88pm886-ldo8", 12),
	CELL_DEV(PM886_LDO_NAME, pm886_ldo_resources, "marvell,88pm886-ldo9", 13),
	CELL_DEV(PM886_LDO_NAME, pm886_ldo_resources, "marvell,88pm886-ldo10", 14),
	CELL_DEV(PM886_LDO_NAME, pm886_ldo_resources, "marvell,88pm886-ldo11", 15),
	CELL_DEV(PM886_LDO_NAME, pm886_ldo_resources, "marvell,88pm886-ldo12", 16),
	CELL_DEV(PM886_LDO_NAME, pm886_ldo_resources, "marvell,88pm886-ldo13", 17),
	CELL_DEV(PM886_LDO_NAME, pm886_ldo_resources, "marvell,88pm886-ldo14", 18),
	CELL_DEV(PM886_LDO_NAME, pm886_ldo_resources, "marvell,88pm886-ldo15", 19),
	CELL_DEV(PM886_LDO_NAME, pm886_ldo_resources, "marvell,88pm886-ldo16", 20),
	CELL_DEV(PM88X_VIRTUAL_REGULATOR_NAME, vr_resources, "marvell,88pm886-buck1-slp", 21),
};

static const struct resource pm880_buck_resources[] = {
	{
	.name = PM880_BUCK_NAME,
	},
};

static const struct resource pm880_ldo_resources[] = {
	{
	.name = PM880_LDO_NAME,
	},
};

const struct mfd_cell pm880_devs[] = {
	CELL_DEV(PM880_BUCK_NAME, pm880_buck_resources, "marvell,88pm880-buck1a", 0),
	CELL_DEV(PM880_BUCK_NAME, pm880_buck_resources, "marvell,88pm880-buck2", 1),
	CELL_DEV(PM880_BUCK_NAME, pm880_buck_resources, "marvell,88pm880-buck3", 2),
	CELL_DEV(PM880_BUCK_NAME, pm880_buck_resources, "marvell,88pm880-buck4", 3),
	CELL_DEV(PM880_BUCK_NAME, pm880_buck_resources, "marvell,88pm880-buck5", 4),
	CELL_DEV(PM880_BUCK_NAME, pm880_buck_resources, "marvell,88pm880-buck6", 5),
	CELL_DEV(PM880_BUCK_NAME, pm880_buck_resources, "marvell,88pm880-buck7", 6),
	CELL_DEV(PM880_LDO_NAME, pm880_ldo_resources, "marvell,88pm880-ldo1", 7),
	CELL_DEV(PM880_LDO_NAME, pm880_ldo_resources, "marvell,88pm880-ldo2", 8),
	CELL_DEV(PM880_LDO_NAME, pm880_ldo_resources, "marvell,88pm880-ldo3", 9),
	CELL_DEV(PM880_LDO_NAME, pm880_ldo_resources, "marvell,88pm880-ldo4", 10),
	CELL_DEV(PM880_LDO_NAME, pm880_ldo_resources, "marvell,88pm880-ldo5", 11),
	CELL_DEV(PM880_LDO_NAME, pm880_ldo_resources, "marvell,88pm880-ldo6", 12),
	CELL_DEV(PM880_LDO_NAME, pm880_ldo_resources, "marvell,88pm880-ldo7", 13),
	CELL_DEV(PM880_LDO_NAME, pm880_ldo_resources, "marvell,88pm880-ldo8", 14),
	CELL_DEV(PM880_LDO_NAME, pm880_ldo_resources, "marvell,88pm880-ldo9", 15),
	CELL_DEV(PM880_LDO_NAME, pm880_ldo_resources, "marvell,88pm880-ldo10", 16),
	CELL_DEV(PM880_LDO_NAME, pm880_ldo_resources, "marvell,88pm880-ldo11", 17),
	CELL_DEV(PM880_LDO_NAME, pm880_ldo_resources, "marvell,88pm880-ldo12", 18),
	CELL_DEV(PM880_LDO_NAME, pm880_ldo_resources, "marvell,88pm880-ldo13", 19),
	CELL_DEV(PM880_LDO_NAME, pm880_ldo_resources, "marvell,88pm880-ldo14", 20),
	CELL_DEV(PM880_LDO_NAME, pm880_ldo_resources, "marvell,88pm880-ldo15", 21),
	CELL_DEV(PM880_LDO_NAME, pm880_ldo_resources, "marvell,88pm880-ldo16", 22),
	CELL_DEV(PM880_LDO_NAME, pm880_ldo_resources, "marvell,88pm880-ldo17", 23),
	CELL_DEV(PM880_LDO_NAME, pm880_ldo_resources, "marvell,88pm880-ldo18", 24),
	CELL_DEV(PM88X_VIRTUAL_REGULATOR_NAME, vr_resources, "marvell,88pm880-buck1a-slp", 25),
	CELL_DEV(PM88X_VIRTUAL_REGULATOR_NAME, vr_resources, "marvell,88pm880-buck1a-audio", 26),
};

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
	REGMAP_IRQ_REG(PM88X_IRQ_ONKEY, 0, PM88X_REG_IRQ1_ONKEY),
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
	REG_SEQ0(PM88X_REG_WATCHDOG, 0x1),	/* disable watchdog */
	REG_SEQ0(PM88X_REG_AON_CTRL2, 0x2a),	/* output 32 kHz from XO */
	REG_SEQ0(PM88X_REG_BK_OSC_CTRL1, 0x0f),	/* OSC_FREERUN = 1, to lock FLL */
	REG_SEQ0(PM88X_REG_LOWPOWER2, 0x20),	/* XO_LJ = 1, enable low jitter for 32 kHz */
	REG_SEQ0(PM88X_REG_LOWPOWER4, 0xc0),	/* enable LPM for internal reference group in sleep */
	REG_SEQ0(PM88X_REG_BK_OSC_CTRL3, 0xc0),	/* set the duty cycle of charger DC/DC to max */
};

static struct reg_sequence pm886_presets[] = {
	REG_SEQ0(PM88X_REG_WATCHDOG, 0x1),	/* disable watchdog */
	REG_SEQ0(PM88X_REG_GPIO_CTRL1, 0x40),	/* GPIO1: DVC, GPIO0: input */
	REG_SEQ0(PM88X_REG_GPIO_CTRL2, 0x00),	/* GPIO2: input */
	REG_SEQ0(PM88X_REG_GPIO_CTRL3, 0x44),	/* DVC2, DVC1 */
	REG_SEQ0(PM88X_REG_GPIO_CTRL4, 0x00),	/* GPIO5V_1:input, GPIO5V_2: input */
	REG_SEQ0(PM88X_REG_AON_CTRL2, 0x2a),	/* output 32 kHz from XO */
	REG_SEQ0(PM88X_REG_BK_OSC_CTRL1, 0x0f),	/* OSC_FREERUN = 1, to lock FLL */
	REG_SEQ0(PM88X_REG_LOWPOWER2, 0x20),	/* XO_LJ = 1, enable low jitter for 32 kHz */
	REG_SEQ0(PM88X_REG_LOWPOWER4, 0xc8),	/* OV_VSYS and UV_VSYS1 comparators on VSYS disabled, VSYS_OVER_TH : 5.6V */
	REG_SEQ0(PM88X_REG_BK_OSC_CTRL3, 0xc0),	/* set the duty cycle of charger DC/DC to max */
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

static struct pm88x_data pm880_data = {
	.whoami = PM880_WHOAMI,
	.devs = pm880_devs,
	.num_devs = ARRAY_SIZE(pm880_devs),
	.presets = pm880_presets,
	.num_presets = ARRAY_SIZE(pm880_presets),
	.irq_mode = 0,
};

static struct pm88x_data pm886_data = {
	.whoami = PM886_WHOAMI,
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
	int ret = regmap_update_bits(chip->regmap, PM88X_REG_MISC_CONFIG1, PM88X_SW_PDOWN, PM88X_SW_PDOWN);
	if (ret) {
		dev_err(&chip->client->dev, "Failed to power off the device: %d\n", ret);
		return NOTIFY_BAD;
	}
	return NOTIFY_DONE;
}

static int pm88x_setup_irq(struct pm88x_chip *chip)
{
	int mask, data, ret;

	mask = PM88X_INV_INT | PM88X_INT_CLEAR | PM88X_INT_MASK_MODE;
	data = chip->data->irq_mode ? PM88X_INT_WRITE_CLEAR : PM88X_INT_READ_CLEAR;
	ret = regmap_update_bits(chip->regmap, PM88X_REG_MISC_CONFIG2, mask, data);
	if (ret) {
		dev_err(&chip->client->dev, "Failed to set interrupt clearing mode: %d\n", ret);
		return ret;
	}

	ret = devm_regmap_add_irq_chip(&chip->client->dev, chip->regmap, chip->client->irq,
			IRQF_ONESHOT, -1, &pm88x_regmap_irq_chip, &chip->irq_data);
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
		dev_err(&chip->client->dev, "Failed to add %s devices: %d\n",
				chip->data->whoami == PM880_WHOAMI ? "PM880" : "PM886", ret);
		return ret;
	}

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

	chip->regmap = devm_regmap_init_i2c(client, &pm88x_i2c_regmap);
	if (IS_ERR(chip->regmap)) {
		ret = PTR_ERR(chip->regmap);
		dev_err(&client->dev, "Failed to initialize regmap: %d\n", ret);
		return ret;
	}

	ret = regmap_read(chip->regmap, PM88X_REG_WHOAMI, &chip_id);
	if (ret) {
		dev_err(&client->dev, "Failed to read chip ID: %d\n", ret);
		return ret;
	}
	if (chip->data->whoami != chip_id) {
		dev_err(&client->dev, "Device reported wrong chip ID: %u\n", chip_id);
		return -EINVAL;
	}

	chip->ldo_page = devm_i2c_new_dummy_device(&client->dev, client->adapter, PM88X_ADDR(client, 1));
	if (IS_ERR(chip->ldo_page)) {
		ret = PTR_ERR(chip->ldo_page);
		dev_err(&client->dev, "Failed to initialize LDO page: %d\n", ret);
		return ret;
	}
	chip->ldo_regmap = devm_regmap_init_i2c(chip->ldo_page, &pm88x_i2c_regmap);
	if (IS_ERR(chip->ldo_regmap)) {
		ret = PTR_ERR(chip->ldo_regmap);
		dev_err(&client->dev, "Failed to initialize LDO regmap: %d\n", ret);
		return ret;
	}
	switch (chip->data->whoami) {
	case PM880_WHOAMI:
		chip->buck_page = devm_i2c_new_dummy_device(&client->dev, client->adapter, PM88X_ADDR(client, 4));
		if (IS_ERR(chip->buck_page)) {
			ret = PTR_ERR(chip->buck_page);
			dev_err(&client->dev, "Failed to initialize BUCK page: %d\n", ret);
			return ret;
		}
		chip->buck_regmap = devm_regmap_init_i2c(chip->buck_page, &pm88x_i2c_regmap);
		if (IS_ERR(chip->buck_regmap)) {
			ret = PTR_ERR(chip->buck_regmap);
			dev_err(&client->dev, "Failed to initialize BUCK regmap: %d\n", ret);
			return ret;
		}
		break;
	case PM886_WHOAMI:
		chip->buck_regmap = chip->ldo_regmap;
		break;
	}

	ret = pm88x_setup_irq(chip);
	if (ret)
		return ret;

	ret = pm88x_mfd_add_devices(chip);
	if (ret)
		return ret;

	ret = regmap_register_patch(chip->regmap, chip->data->presets, chip->data->num_presets);
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
	{ .compatible = "marvell,88pm880", .data = &pm880_data },
	{ .compatible = "marvell,88pm886", .data = &pm886_data },
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

MODULE_DESCRIPTION("88PM88X PMIC driver");
MODULE_AUTHOR("Karel Balej <balejk@matfyz.cz>");
/* TODO: read up on GPL */
MODULE_LICENSE("GPL");
