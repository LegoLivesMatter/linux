// SPDX-License-Identifier: GPL-2.0-only
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/mfd/core.h>
#include <linux/notifier.h>
#include <linux/reboot.h>

#include <linux/mfd/88pm88x.h>

/* interrupt status registers */
#define PM88X_REG_INT_STATUS1			0x05

#define PM88X_REG_INT_ENA_1			0x0a
#define PM88X_INT_ENA1_ONKEY			BIT(0)

enum pm88x_irq_number {
	PM88X_IRQ_ONKEY,

	PM88X_MAX_IRQ
};

static struct regmap_irq pm88x_regmap_irqs[] = {
	REGMAP_IRQ_REG(PM88X_IRQ_ONKEY, 0, PM88X_INT_ENA1_ONKEY),
};

static struct regmap_irq_chip pm88x_regmap_irq_chip = {
	.name = "88pm88x",
	.irqs = pm88x_regmap_irqs,
	.num_irqs = ARRAY_SIZE(pm88x_regmap_irqs),
	.num_regs = 4,
	.status_base = PM88X_REG_INT_STATUS1,
	.ack_base = PM88X_REG_INT_STATUS1,
	.unmask_base = PM88X_REG_INT_ENA_1,
};

static struct reg_sequence pm886_presets[] = {
	/* disable watchdog */
	REG_SEQ0(PM88X_REG_WDOG, 0x01),
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

static struct resource pm88x_onkey_resources[] = {
	DEFINE_RES_IRQ_NAMED(PM88X_IRQ_ONKEY, "88pm88x-onkey"),
};

static struct mfd_cell pm886_devs[] = {
	{
		.name = "88pm88x-onkey",
		.of_compatible = "marvell,88pm88x-onkey",
		.num_resources = ARRAY_SIZE(pm88x_onkey_resources),
		.resources = pm88x_onkey_resources,
	},
};

static struct pm88x_data pm886_a1_data = {
	.whoami = PM886_A1_WHOAMI,
	.presets = pm886_presets,
	.num_presets = ARRAY_SIZE(pm886_presets),
	.devs = pm886_devs,
	.num_devs = ARRAY_SIZE(pm886_devs),
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
			PM88X_SW_PDOWN, PM88X_SW_PDOWN);
	if (ret) {
		dev_err(&chip->client->dev, "Failed to power off the device: %d\n", ret);
		return NOTIFY_BAD;
	}
	return NOTIFY_DONE;
}

static int pm88x_initialize_subregmaps(struct pm88x_chip *chip)
{
	struct i2c_client *page;
	struct regmap *regmap;
	int ret;

	/* LDO page */
	page = devm_i2c_new_dummy_device(&chip->client->dev, chip->client->adapter,
					chip->client->addr + PM88X_PAGE_OFFSET_LDO);
	if (IS_ERR(page)) {
		ret = PTR_ERR(page);
		dev_err(&chip->client->dev, "Failed to initialize LDO client: %d\n",
				ret);
		return ret;
	}
	regmap = devm_regmap_init_i2c(page, &pm88x_i2c_regmap);
	if (IS_ERR(regmap)) {
		ret = PTR_ERR(regmap);
		dev_err(&chip->client->dev, "Failed to initialize LDO regmap: %d\n",
				ret);
		return ret;
	}
	chip->regmaps[PM88X_REGMAP_LDO] = regmap;
	/* buck regmap is the same as LDO */
	chip->regmaps[PM88X_REGMAP_BUCK] = regmap;

	return 0;
}

static int pm88x_setup_irq(struct pm88x_chip *chip)
{
	int ret;

	/* set interrupt clearing mode to clear on write */
	ret = regmap_update_bits(chip->regmaps[PM88X_REGMAP_BASE], PM88X_REG_MISC_CONFIG2,
			PM88X_INT_INV | PM88X_INT_CLEAR | PM88X_INT_MASK_MODE,
			PM88X_INT_WC);
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

	ret = regmap_read(chip->regmaps[PM88X_REGMAP_BASE], PM88X_REG_ID, &chip_id);
	if (ret) {
		dev_err(&client->dev, "Failed to read chip ID: %d\n", ret);
		return ret;
	}
	if (chip->data->whoami != chip_id) {
		dev_err(&client->dev, "Device reported wrong chip ID: %u\n", chip_id);
		return -EINVAL;
	}

	ret = pm88x_initialize_subregmaps(chip);
	if (ret)
		return ret;

	ret = pm88x_setup_irq(chip);
	if (ret)
		return ret;

	ret = devm_mfd_add_devices(&client->dev, 0, chip->data->devs, chip->data->num_devs,
			NULL, 0, regmap_irq_get_domain(chip->irq_data));
	if (ret) {
		dev_err(&client->dev, "Failed to add devices: %d\n", ret);
		return ret;
	}

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
