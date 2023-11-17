/* registers */
#define PM88X_ID 				0x00
#define PM88X_STATUS1		0x01
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

struct pm88x_chip {
	struct i2c_client *client;
	struct regmap_irq_chip_data *irq_data;
	unsigned int whoami;
	struct regmap *regmap;
	int irq_mode;
};
