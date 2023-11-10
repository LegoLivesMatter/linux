#include <linux/kernel.h>

static const struct of_device_id pm88x_onkey_of_match[] = {
	{ .compatible = "marvell,88pm88x-onkey", },
	{ },
};

static struct platform_driver pm88x_onkey = {
	.driver = {
		.name = "88pm88x-onkey",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(pm88x_onkey_of_match),
	},
	.probe = pm88x_onkey_probe,
	.remove = pm88x_onkey_remove,
};

MODULE_DESCRIPTION("Marvell 88PM88X onkey driver");
