#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regmap.h>

#define PM880_WHOAMI 1
#define PM886_WHOAMI 2

enum 88pm88x_chips {
	PM880,
	PM886,
};

enum pm88x_irq {
	PM88X_IRQ_ONKEY,
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
	return 0;
}

static int pm88x_remove(struct i2c_client *i2c) {
	return 0;
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

static int __init pm88x_init(void) {
	int ret;

	ret = i2c_add_driver(&pm88x_i2c_driver);
	if (ret) {
		pr_err("Failed to register I2C driver: %d\n", ret);
		return ret;
	}
	return 0;
}
subsys_initcall(pm88x_init);

static void __exit pm88x_exit(void) {
	i2c_del_driver(&pm88x_i2c_driver);
}
module_exit(pm88x_exit);

MODULE_DESCRIPTION("88PM88X PMIC driver");
