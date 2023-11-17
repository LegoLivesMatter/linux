#define PM88X_STATUS1 1

struct pm88x_chip {
	struct i2c_client *client;
	struct regmap_irq_chip_data *irq_data;
	unsigned int whoami;
	struct regmap *regmap;
	int irq_mode;
};
