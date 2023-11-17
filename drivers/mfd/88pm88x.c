#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/mfd/core.h>

#define PM880_WHOAMI 0xb1
#define PM886_WHOAMI 0xa1

/* registers */
/* common */
#define PM88X_ID 				0x00
#define PM88X_WATCHDOG 		0x1d
#define PM88X_AON_CTRL2		0xe2
#define PM88X_BK_OSC_CTRL1	0x50
#define PM88X_BK_OSC_CTRL3	0x52
#define PM88X_LOWPOWER2		0x21
#define PM88X_LOWPOWER4		0x23
#define PM88X_GPIO_CTRL1	0x30
#define PM88X_GPIO_CTRL2	0x31
#define PM88X_GPIO_CTRL3	0x32
#define PM88X_GPIO_CTRL4	0x33

#define PM88X_ONKEY_INT_ENA1 1
#define PM88X_INT_STATUS1 5
#define PM88X_INT_ENA_1 0xa

#define PM88X_MISC_CONFIG2 0x15
#define PM88X_INV_INT BIT(0)
#define PM88X_INT_CLEAR	BIT(1)
#define PM88X_INT_RC 0
#define PM88X_INT_WC BIT(1)
#define PM88X_INT_MASK_MODE BIT(2)

enum pm88x_chips {
	PM880,
	PM886,
};

enum pm88x_irq {
	PM88X_IRQ_ONKEY,
};

static const struct regmap_irq pm88x_regmap_irqs[] = {
	REGMAP_IRQ_REG(PM88X_IRQ_ONKEY, 0, PM88X_ONKEY_INT_ENA1),
};

static struct regmap_irq_chip pm88x_regmap_irq_chip = {
	.name = "88pm88x",
	.irqs = pm88x_regmap_irqs,
	.num_irqs = ARRAY_SIZE(pm88x_regmap_irqs),
	.num_regs = 4,
	.status_base = PM88X_INT_STATUS1,
	.ack_base = PM88X_INT_STATUS1,
	.unmask_base = PM88X_INT_ENA_1,
};

struct pm88x_chip {
	struct i2c_client *client;
	struct regmap_irq_chip_data *irq_data;
	long whoami;
	struct regmap *regmap;
	unsigned int chip_id;
	int irq_mode;
};

/* TODO: understand these presets */
static const struct reg_sequence pm880_patch[] = {
	REG_SEQ0(PM88X_WDOG, 0x1),				/* disable watchdog */
	REG_SEQ0(PM88X_AON_CTRL2, 0x2a),		/* output 32 kHz from XO */
	REG_SEQ0(PM88X_BK_OSC_CTRL1, 0x0f),	/* OSC_FREERUN = 1, to lock FLL */
	REG_SEQ0(PM88X_LOWPOWER2, 0x20),		/* XO_LJ = 1, enable low jitter for 32 kHz */
	REG_SEQ0(PM88X_LOWPOWER4, 0xc0),		/* enable LPM for internal reference group in sleep */
	REG_SEQ0(PM88X_BK_OSC_CTRL3, 0xc0),	/* set the duty cycle of charger DC/DC to max */
};

static const struct reg_sequence pm886_patch[] = {
	REG_SEQ0(PM88X_WDOG, 0x1),				/* disable watchdog */
	REG_SEQ0(PM88X_GPIO_CTRL1, 0x40),	/* gpio1: dvc, gpio0: input, */
	REG_SEQ0(PM88X_GPIO_CTRL2, 0x00),	/* gpio2: input */
	REG_SEQ0(PM88X_GPIO_CTRL3, 0x44),	/* dvc2, dvc1 */
	REG_SEQ0(PM88X_GPIO_CTRL4, 0x00),	/* gpio5v_1:input, gpio5v_2: input */
	REG_SEQ0(PM88X_AON_CTRL2, 0x2a),		/* output 32 kHz from XO */
	REG_SEQ0(PM88X_BK_OSC_CTRL1, 0x0f),	/* OSC_FREERUN = 1, to lock FLL */
	REG_SEQ0(PM88X_LOWPOWER2, 0x20),		/* XO_LJ = 1, enable low jitter for 32 kHz */
	REG_SEQ0(PM88X_LOWPOWER4, 0xc8),		/* OV_VSYS and UV_VSYS1 comparators on VSYS disabled, VSYS_OVER_TH : 5.6V */
	REG_SEQ0(PM88X_BK_OSC_CTRL3, 0xc0),	/* set the duty cycle of charger DC/DC to max */
};

static struct resource onkey_resources[] = {
	DEFINE_RES_IRQ_NAMED(PM88X_IRQ_ONKEY, "88pm88x-onkey"),
};

static struct mfd_cell onkey_devs[] = {
	{
		.name = "88pm88x-onkey",
		.num_resources = 1,
		.resources = &onkey_resources[0],
		.id = -1,
	},
};

static const struct regmap_config pm88x_i2c_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xfe,
};

static int pm88x_probe(struct i2c_client *client) {
	struct pm88x_chip *chip;
	int mask, data, ret = 0;

	chip = devm_kzalloc(&client->dev, sizeof(struct pm88x_chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->client = client;
	i2c_set_clientdata(chip->client, chip);

	device_init_wakeup(&client->dev, 1);

	chip->regmap = devm_regmap_init_i2c(client, &pm88x_i2c_regmap);
	if (IS_ERR(chip->regmap)) {
		ret = PTR_ERR(chip->regmap);
		dev_err(chip->dev, "Failed to initialize regmap: %d\n", ret);
		return ret;
	}

	ret = regmap_read(chip->regmap, PM88X_ID, &chip->chip_id);
	if (ret) {
		dev_err(chip->client->dev, "Failed to read chip ID: %d\n", ret);
		return ret;
	}

	/* TODO: set irq_mode: downstream sets this via DT, could we set it here based on chip ID? */

	mask = PM88X_INV_INT | PM88X_INT_CLEAR | PM88X_INT_MASK_MODE;
	data = chip->irq_mode ? PM88X_INT_WC : PM88X_INT_RC;
	ret = regmap_update_bits(chip->regmap, PM88X_MISC_CONFIG2, mask, data);
	if (ret) {
		dev_err(chip->client->dev, "Failed to set interrupt mode: %d\n", ret);
		return ret;
	}

	ret = regmap_add_irq_chip(chip->regmap, chip->client->irq, IRQF_ONESHOT, -1,
				  &pm88x_regmap_irq_chip, &chip->irq_data);
	if (ret) {
		dev_err(chip->client->dev, "Failed to request IRQ: %d\n", ret);
		return ret;
	}

	ret = mfd_add_devices(chip->dev, 0, &onkey_devs[0], ARRAY_SIZE(onkey_devs),
			&onkey_resources[0], 0, NULL);
	if (ret) {
		dev_err(chip->client->dev, "Failed to add onkey device: %d\n", ret);
		goto err_subdevices;
	}

	switch (chip->whoami) {
		case PM880_WHOAMI:
			ret = regmap_register_patch(chip->regmap, pm880_patch, ARRAY_SIZE(pm880_patch));
			break;
		case PM886_WHOAMI:
			ret = regmap_register_patch(chip->regmap, pm886_patch, ARRAY_SIZE(pm886_patch));
			break;
	}
	if (ret) {
		dev_err(chip->client->dev, "Failed to register regmap patch: %d\n", ret);
		goto err_patch;
	}

	return 0;

err_patch:
	mfd_remove_devices(chip->client->dev);
err_subdevices:
	regmap_del_irq_chip(chip->client->irq, chip->irq_data);

	return ret;
}

static void pm88x_remove(struct i2c_client *i2c) {
	struct pm88x_chip *chip = dev_get_drvdata(&i2c->dev);
	mfd_remove_devices(chip->dev);
	regmap_del_irq_chip(chip->irq, chip->irq_data);
}

const struct of_device_id pm88x_of_match[] = {
	{ .compatible = "marvell,88pm880", .data = (void *)PM880 },
	{ .compatible = "marvell,88pm886", .data = (void *)PM886 },
	{},
};

static const struct i2c_device_id pm88x_i2c_id[] = {
	{ "88pm880", PM880 },
	{ "88pm886", PM886 },
	{  },
};
MODULE_DEVICE_TABLE(i2c, pm88x_i2c_id);

static struct i2c_driver pm88x_i2c_driver = {
	.driver = {
		.name = "88pm88x",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(pm88x_of_match),
	},
	.probe = pm88x_probe,
	.remove = pm88x_remove,
	.id_table = pm88x_i2c_id,
};
module_i2c_driver(pm88x_i2c_driver);

MODULE_DESCRIPTION("88PM88X PMIC driver");
