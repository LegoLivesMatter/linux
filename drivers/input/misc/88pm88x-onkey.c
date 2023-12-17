// SPDX-License-Identifier: GPL-2.0-only
#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/regmap.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/of.h>

#include <linux/mfd/88pm88x.h>

struct pm88x_onkey {
	struct input_dev *idev;
	struct pm88x_chip *chip;
	int irq;
};

static irqreturn_t pm88x_onkey_irq_handler(int irq, void *data)
{
	struct pm88x_onkey *onkey = data;
	unsigned int val;
	int ret = 0;

	ret = regmap_read(onkey->chip->regmaps[PM88X_REGMAP_BASE], PM88X_REG_STATUS1, &val);
	if (ret) {
		dev_err(onkey->idev->dev.parent, "Failed to read status: %d\n", ret);
		return IRQ_NONE;
	}
	val &= PM88X_ONKEY_STS1;

	input_report_key(onkey->idev, KEY_POWER, val);
	input_sync(onkey->idev);

	return IRQ_HANDLED;
}

static int pm88x_onkey_probe(struct platform_device *pdev)
{
	struct pm88x_chip *chip = dev_get_drvdata(pdev->dev.parent);
	struct pm88x_onkey *onkey;
	int err;

	onkey = devm_kzalloc(&pdev->dev, sizeof(*onkey), GFP_KERNEL);
	if (!onkey)
		return -ENOMEM;

	onkey->chip = chip;

	onkey->irq = platform_get_irq(pdev, 0);
	if (onkey->irq < 0) {
		dev_err(&pdev->dev, "Failed to get IRQ\n");
		return -EINVAL;
	}

	onkey->idev = devm_input_allocate_device(&pdev->dev);
	if (!onkey->idev) {
		dev_err(&pdev->dev, "Failed to allocate input device\n");
		return -ENOMEM;
	}

	onkey->idev->name = "88pm88x-onkey";
	onkey->idev->phys = "88pm88x-onkey/input0";
	onkey->idev->id.bustype = BUS_I2C;
	onkey->idev->dev.parent = &pdev->dev;
	onkey->idev->evbit[0] = BIT_MASK(EV_KEY);
	onkey->idev->keybit[BIT_WORD(KEY_POWER)] = BIT_MASK(KEY_POWER);

	err = devm_request_threaded_irq(&pdev->dev, onkey->irq, NULL, pm88x_onkey_irq_handler,
			IRQF_ONESHOT | IRQF_NO_SUSPEND, "onkey", onkey);
	if (err) {
		dev_err(&pdev->dev, "Failed to request IRQ: %d\n", err);
		return err;
	}

	err = input_register_device(onkey->idev);
	if (err) {
		dev_err(&pdev->dev, "Failed to register input device: %d\n", err);
		return err;
	}

	device_init_wakeup(&pdev->dev, 1);

	return 0;
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
MODULE_AUTHOR("Karel Balej <balejk@matfyz.cz>");
MODULE_LICENSE("GPL");
