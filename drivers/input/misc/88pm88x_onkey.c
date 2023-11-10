#include <linux/kernel.h>
#include <linux/input.h>

#include <linux/mfd/88pm88x.h>

struct pm88x_onkey_data {
	struct input_dev *onkey;
	struct pm88x_chip *chip;
	struct regmap *regmap;
};

static irqreturn_t pm88x_onkey_interrupt(int irq, void *dummy) {
	struct pm88x_onkey_data *data = (pm88x_onkey_data *)dummy;
	unsigned int val;
	int ret = 0;

	// TODO

	input_report_key(data->onkey, KEY_POWER, val);
	input_sync(data->onkey);

	return IRQ_HANDLED;
}

static int pm88x_onkey_probe(struct platform_device *pdev) {
	struct pm88x_chip *chip = dev_get_drvdata(pdev->dev.parent);
	struct pm88x_onkey_data *data;
	int err;

	data = devm_kzalloc(&pdev->dev, sizeof(struct pm88x_onkey_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->chip = chip;

	data->irq = platform_get_irq(pdev, 0);
	// TODO: handle failure

	// TODO: get regmap

	data->onkey = input_allocate_device();
	if (!data->onkey) {
		dev_err(&pdev->dev, "Failed to allocate input device\n");
		err = -ENOMEM;
		goto err_free_irq;
	}

	err = input_register_device(data->onkey);
	if (err) {
		dev_err(&pdev->dev, "Failed to register device\n");
		goto err_free_dev;
	}
err_free_dev:
	input_free_device(data->onkey);
err_free_irq:
	free_irq(); //TODO
	return err;
}

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
