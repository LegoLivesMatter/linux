#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regmap.h>

#define PM880_WHOAMI 1
#define PM886_WHOAMI 2

#define PM88X_REG_ID 0

enum 88pm88x_chips {
	PM880,
	PM886,
};

enum pm88x_irq {
	PM88X_IRQ_ONKEY,
};

static struct pm88x_chip {
	struct i2c_client *client;
	struct device *dev;
	struct regmap_irq_chip_data *irq_data;
	int irq;
	long whoami;
	struct regmap *regmap;
	unsigned int chip_id;
};

static const struct resource onkey_resources[] = {
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

static int pm88x_probe(struct i2c_client *client,
		const struct i2c_device_id *id) {
	struct pm88x_chip *chip;
	int ret = 0;
	struct regmap *regmap;

	chip = devm_kzalloc(&client->dev, sizeof(struct pm88x_chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->client = client;

	regmap = devm_regmap_init_i2c(client, &pm88x_i2c_regmap);
	if (IS_ERR(regmap)) {
		ret = PTR_ERR(regmap);
		dev_err(&client->dev, "Failed to allocate register map: %d\n", ret);
		return ret;
	}
	chip->regmap = regmap;

	chip->irq = client->irq;
	chip->dev = client->dev;
	i2c_set_clientdata(chip->client, chip);

	device_init_wakeup(&client->dev, 1);

	// TODO: pages

	ret = regmap_read(chip->regmap, PM88X_REG_ID, &chip->chip_id);
	if (ret) {
		dev_err(chip->dev, "Failed to read chip ID: %d\n", ret);
		return ret;
	}

	// TODO: init IRQ

	mfd_add_devices(chip->dev, 0, &onkey_devs[0], ARRAY_SIZE(onkey_devs),
			&onkey_resources[0], 0, NULL);

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

static const struct i2c_device_id pm88x_i2c_id[] {
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
