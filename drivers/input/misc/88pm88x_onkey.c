#include <linux/kernel.h>
#include <linux/input.h>

#include <linux/mfd/88pm88x.h>

#define PM88X_ONKEY_STS1 1

struct pm88x_onkey_data {
	struct input_dev *onkey;
	struct pm88x_chip *chip;
	int irq;
};

static irqreturn_t pm88x_onkey_interrupt(int irq, void *dummy) {
	struct pm88x_onkey_data *data = (pm88x_onkey_data *)dummy;
	unsigned int val;
	int ret = 0;

	// TODO: reset LONKEY reset time?

	ret = regmap_read(data->chip->regmap, PM88X_STATUS1, &val);
	if (ret) {
		dev_err(data->onkey->dev.parent,
				"Failed to read status: %d\n", ret);
		return ret; // FIXME: IRQ_NONE?
	}
	val &= PM88X_ONKEY_STS1;

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
	if (data->irq < 0) {
		return -EINVAL;
	}

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

	data->onkey->evbit[0] = BIT_MASK(EV_KEY);
	data->onkey->keybit[BIT_WORD(KEY_POWER)] = BIT_MASK(KEY_POWER);
	data->onkey->name = "Power button";
	data->onkey->id.bus_type = BUS_I2C;
	data->onkey->dev.parent = &pdev->dev;
err_free_dev:
	input_free_device(data->onkey);
err_free_irq:
	devm_free_irq(&pdev->dev, data->irq, data);
	return err;
}

static const struct of_device_id pm88x_onkey_of_match[] = {
	{ .compatible = "marvell,88pm88x-onkey", },
	{ },
};
MODULE_DEVICE_TABLE(of, pm88x_onkey_of_match);

static struct platform_driver pm88x_onkey_driver = {
	.driver = {
		.name = "88pm88x-onkey",
		.of_match_table = of_match_ptr(pm88x_onkey_of_match),
	},
	.probe = pm88x_onkey_probe,
};
module_platform_driver(pm88x_onkey_driver);

MODULE_DESCRIPTION("Marvell 88PM88X onkey driver");
