// SPDX-License-Identifier: GPL-2.0-only
#include <linux/container_of.h>
#include <linux/kernel.h>
#include <linux/linear_range.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>

#include <linux/mfd/88pm88x.h>

#define PM88X_REG_LDO_EN1		0x09
#define PM88X_REG_LDO_EN2		0x0a

#define PM88X_REG_BUCK_EN		0x08

#define PM88X_REG_LDO1_VOUT		0x20
#define PM88X_REG_LDO2_VOUT		0x26
#define PM88X_REG_LDO3_VOUT		0x2c
#define PM88X_REG_LDO4_VOUT		0x32
#define PM88X_REG_LDO5_VOUT		0x38
#define PM88X_REG_LDO6_VOUT		0x3e
#define PM88X_REG_LDO7_VOUT		0x44
#define PM88X_REG_LDO8_VOUT		0x4a
#define PM88X_REG_LDO9_VOUT		0x50
#define PM88X_REG_LDO10_VOUT		0x56
#define PM88X_REG_LDO11_VOUT		0x5c
#define PM88X_REG_LDO12_VOUT		0x62
#define PM88X_REG_LDO13_VOUT		0x68
#define PM88X_REG_LDO14_VOUT		0x6e
#define PM88X_REG_LDO15_VOUT		0x74
#define PM88X_REG_LDO16_VOUT		0x7a

#define PM886_REG_BUCK1_VOUT		0xa5
#define PM886_REG_BUCK2_VOUT		0xb3
#define PM886_REG_BUCK3_VOUT		0xc1
#define PM886_REG_BUCK4_VOUT		0xcf
#define PM886_REG_BUCK5_VOUT		0xdd

#define PM88X_LDO_VSEL_MASK		0x0f
#define PM88X_BUCK_VSEL_MASK		0x7f

struct pm88x_regulator {
	struct regulator_desc desc;
	int max_uA;
};

static int pm88x_regulator_get_ilim(struct regulator_dev *rdev)
{
	struct pm88x_regulator *data = rdev_get_drvdata(rdev);

	if (!data) {
		dev_err(&rdev->dev, "Failed to get regulator data\n");
		return -EINVAL;
	}
	return data->max_uA;
}

static const struct regulator_ops pm88x_ldo_ops = {
	.list_voltage = regulator_list_voltage_table,
	.map_voltage = regulator_map_voltage_iterate,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.get_current_limit = pm88x_regulator_get_ilim,
};

static const struct regulator_ops pm88x_buck_ops = {
	.list_voltage = regulator_list_voltage_linear_range,
	.map_voltage = regulator_map_voltage_linear_range,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.get_current_limit = pm88x_regulator_get_ilim,
};

static const unsigned int pm88x_ldo_volt_table1[] = {
	1700000, 1800000, 1900000, 2500000, 2800000, 2900000, 3100000, 3300000,
};

static const unsigned int pm88x_ldo_volt_table2[] = {
	1200000, 1250000, 1700000, 1800000, 1850000, 1900000, 2500000, 2600000,
	2700000, 2750000, 2800000, 2850000, 2900000, 3000000, 3100000, 3300000,
};

static const unsigned int pm88x_ldo_volt_table3[] = {
	1700000, 1800000, 1900000, 2000000, 2100000, 2500000, 2700000, 2800000,
};

static const struct linear_range pm88x_buck_volt_ranges1[] = {
	REGULATOR_LINEAR_RANGE(600000, 0, 79, 12500),
	REGULATOR_LINEAR_RANGE(1600000, 80, 84, 50000),
};

static const struct linear_range pm88x_buck_volt_ranges2[] = {
	REGULATOR_LINEAR_RANGE(600000, 0, 79, 12500),
	REGULATOR_LINEAR_RANGE(1600000, 80, 114, 50000),
};

static struct pm88x_regulator pm88x_ldo2 = {
	.desc = {
		.name = "LDO2",
		.id = PM88X_REGULATOR_ID_LDO2,
		.regulators_node = "regulators",
		.of_match = "ldo2",
		.ops = &pm88x_ldo_ops,
		.type = REGULATOR_VOLTAGE,
		.enable_reg = PM88X_REG_LDO_EN1,
		.enable_mask = BIT(1),
		.volt_table = pm88x_ldo_volt_table1,
		.n_voltages = ARRAY_SIZE(pm88x_ldo_volt_table1),
		.vsel_reg = PM88X_REG_LDO2_VOUT,
		.vsel_mask = PM88X_LDO_VSEL_MASK,
	},
	.max_uA = 100000,
};

static struct pm88x_regulator pm88x_ldo15 = {
	.desc = {
		.name = "LDO15",
		.id = PM88X_REGULATOR_ID_LDO15,
		.regulators_node = "regulators",
		.of_match = "ldo15",
		.ops = &pm88x_ldo_ops,
		.type = REGULATOR_VOLTAGE,
		.enable_reg = PM88X_REG_LDO_EN2,
		.enable_mask = BIT(6),
		.volt_table = pm88x_ldo_volt_table2,
		.n_voltages = ARRAY_SIZE(pm88x_ldo_volt_table2),
		.vsel_reg = PM88X_REG_LDO15_VOUT,
		.vsel_mask = PM88X_LDO_VSEL_MASK,
	},
	.max_uA = 200000,
};

static struct pm88x_regulator pm886_buck2 = {
	.desc = {
		.name = "buck2",
		.id = PM886_REGULATOR_ID_BUCK2,
		.regulators_node = "regulators",
		.of_match = "buck2",
		.ops = &pm88x_buck_ops,
		.type = REGULATOR_VOLTAGE,
		.n_voltages = 115,
		.linear_ranges = pm88x_buck_volt_ranges2,
		.n_linear_ranges = ARRAY_SIZE(pm88x_buck_volt_ranges2),
		.vsel_reg = PM886_REG_BUCK2_VOUT,
		.vsel_mask = PM88X_BUCK_VSEL_MASK,
		.enable_reg = PM88X_REG_BUCK_EN,
		.enable_mask = BIT(1),
	},
	.max_uA = 1200000,
};

static struct pm88x_regulator *pm88x_regulators[] = {
	[PM88X_REGULATOR_ID_LDO2] = &pm88x_ldo2,
	[PM88X_REGULATOR_ID_LDO15] = &pm88x_ldo15,
	[PM886_REGULATOR_ID_BUCK2] = &pm886_buck2,
};

static int pm88x_regulator_probe(struct platform_device *pdev)
{
	struct pm88x_chip *chip = dev_get_drvdata(pdev->dev.parent);
	struct regulator_config rcfg = { };
	struct pm88x_regulator *regulator;
	struct regulator_desc *rdesc;
	struct regulator_dev *rdev;
	int ret;

	if (pdev->id < 0 || pdev->id == PM88X_REGULATOR_ID_BUCKS
			|| pdev->id >= PM88X_REGULATOR_ID_SENTINEL) {
		dev_err(&pdev->dev, "Invalid regulator ID: %d\n", pdev->id);
		return -EINVAL;
	}

	rcfg.dev = pdev->dev.parent;
	regulator = pm88x_regulators[pdev->id];
	rdesc = &regulator->desc;
	rcfg.driver_data = regulator;
	rcfg.regmap = chip->regmaps[rdesc->id > PM88X_REGULATOR_ID_BUCKS ?
		PM88X_REGMAP_BUCK : PM88X_REGMAP_LDO];
	rdev = devm_regulator_register(&pdev->dev, rdesc, &rcfg);
	if (IS_ERR(rdev)) {
		ret = PTR_ERR(rdev);
		dev_err(&pdev->dev, "Failed to register %s: %d",
				rdesc->name, ret);
		return ret;
	}

	return 0;
}

const struct of_device_id pm88x_regulator_of_match[] = {
	{ .compatible = "marvell,88pm88x-regulator", },
	{ },
};

static struct platform_driver pm88x_regulator_driver = {
	.driver = {
		.name = "88pm88x-regulator",
		.of_match_table = of_match_ptr(pm88x_regulator_of_match),
	},
	.probe = pm88x_regulator_probe,
};
module_platform_driver(pm88x_regulator_driver);

MODULE_DESCRIPTION("Marvell 88PM88X PMIC regulator driver");
MODULE_AUTHOR("Karel Balej <balejk@matfyz.cz>");
MODULE_LICENSE("GPL");
