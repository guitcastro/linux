// SPDX-License-Identifier: GPL-2.0-only
/*
 * Kinetic KTD3137 Backlight LED Driver
 *
 * Copyright (c) 2026 Guilherme Castro
 *
 * Based on downstream Xiaomi driver by Kinetic Technologies.
 */

#include <linux/backlight.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regmap.h>

#define KTD3137_REG_DEV_ID		0x00
#define KTD3137_REG_SW_RESET		0x01
#define KTD3137_REG_MODE		0x02
#define KTD3137_REG_CONTROL		0x03
#define KTD3137_REG_RATIO_LSB		0x04
#define KTD3137_REG_RATIO_MSB		0x05
#define KTD3137_REG_PWM			0x06
#define KTD3137_REG_RAMP_ON		0x07
#define KTD3137_REG_TRANS_RAMP		0x08
#define KTD3137_REG_FLASH_SETTING	0x09
#define KTD3137_REG_STATUS		0x0a

#define KTD3137_MODE_ENABLE		BIT(0)

#define KTD3137_MAX_BRIGHTNESS_11BIT	2047
#define KTD3137_MAX_BRIGHTNESS_8BIT	255

struct ktd3137 {
	struct i2c_client *client;
	struct regmap *regmap;
	struct gpio_desc *enable_gpio;
	bool using_lsb;
};

static int ktd3137_update_brightness(struct backlight_device *bl)
{
	struct ktd3137 *ktd = bl_get_data(bl);
	int brightness = backlight_get_brightness(bl);

	if (brightness > 0) {
		/* Enable backlight */
		regmap_update_bits(ktd->regmap, KTD3137_REG_MODE,
				   KTD3137_MODE_ENABLE, KTD3137_MODE_ENABLE);

		if (ktd->using_lsb) {
			regmap_update_bits(ktd->regmap, KTD3137_REG_RATIO_LSB,
					   0x07, brightness & 0x07);
			regmap_write(ktd->regmap, KTD3137_REG_RATIO_MSB,
				     brightness >> 3);
		} else {
			regmap_write(ktd->regmap, KTD3137_REG_RATIO_MSB,
				     brightness);
		}
	} else {
		/* Disable backlight */
		regmap_update_bits(ktd->regmap, KTD3137_REG_MODE,
				   KTD3137_MODE_ENABLE, 0);
	}

	return 0;
}

static const struct backlight_ops ktd3137_backlight_ops = {
	.options = BL_CORE_SUSPENDRESUME,
	.update_status = ktd3137_update_brightness,
};

static const struct regmap_config ktd3137_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = KTD3137_REG_STATUS,
};

static int ktd3137_init_hw(struct ktd3137 *ktd)
{
	unsigned int dev_id;
	int ret;

	ret = regmap_read(ktd->regmap, KTD3137_REG_DEV_ID, &dev_id);
	if (ret)
		return ret;

	dev_info(&ktd->client->dev, "KTD3137 device ID: 0x%02x\n", dev_id);

	/* Set mode: backlight enabled, I2C control */
	ret = regmap_write(ktd->regmap, KTD3137_REG_MODE, 0x99);
	if (ret)
		return ret;

	/* Enable all LED channels */
	ret = regmap_update_bits(ktd->regmap, KTD3137_REG_PWM, 0x9f, 0x9f);
	if (ret)
		return ret;

	return 0;
}

static int ktd3137_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct backlight_properties props;
	struct backlight_device *bl;
	struct ktd3137 *ktd;
	int ret;

	ktd = devm_kzalloc(dev, sizeof(*ktd), GFP_KERNEL);
	if (!ktd)
		return -ENOMEM;

	ktd->client = client;

	ktd->regmap = devm_regmap_init_i2c(client, &ktd3137_regmap_config);
	if (IS_ERR(ktd->regmap))
		return dev_err_probe(dev, PTR_ERR(ktd->regmap),
				     "Failed to init regmap\n");

	ktd->enable_gpio = devm_gpiod_get_optional(dev, "enable",
						    GPIOD_OUT_HIGH);
	if (IS_ERR(ktd->enable_gpio))
		return dev_err_probe(dev, PTR_ERR(ktd->enable_gpio),
				     "Failed to get enable GPIO\n");

	ktd->using_lsb = device_property_read_bool(dev, "kinetic,using-lsb");

	ret = ktd3137_init_hw(ktd);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to initialize HW\n");

	memset(&props, 0, sizeof(props));
	props.type = BACKLIGHT_RAW;
	if (ktd->using_lsb) {
		props.max_brightness = KTD3137_MAX_BRIGHTNESS_11BIT;
		props.brightness = KTD3137_MAX_BRIGHTNESS_11BIT;
	} else {
		props.max_brightness = KTD3137_MAX_BRIGHTNESS_8BIT;
		props.brightness = KTD3137_MAX_BRIGHTNESS_8BIT;
	}

	device_property_read_u32(dev, "default-brightness",
				 &props.brightness);

	bl = devm_backlight_device_register(dev, "ktd3137", dev, ktd,
					    &ktd3137_backlight_ops, &props);
	if (IS_ERR(bl))
		return dev_err_probe(dev, PTR_ERR(bl),
				     "Failed to register backlight\n");

	backlight_update_status(bl);

	i2c_set_clientdata(client, ktd);

	return 0;
}

static void ktd3137_remove(struct i2c_client *client)
{
	struct ktd3137 *ktd = i2c_get_clientdata(client);

	if (ktd->enable_gpio)
		gpiod_set_value_cansleep(ktd->enable_gpio, 0);
}

static const struct of_device_id ktd3137_of_match[] = {
	{ .compatible = "kinetic,ktd3137" },
	{ }
};
MODULE_DEVICE_TABLE(of, ktd3137_of_match);

static const struct i2c_device_id ktd3137_id[] = {
	{ "ktd3137" },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ktd3137_id);

static struct i2c_driver ktd3137_driver = {
	.driver = {
		.name = "ktd3137",
		.of_match_table = ktd3137_of_match,
	},
	.probe = ktd3137_probe,
	.remove = ktd3137_remove,
	.id_table = ktd3137_id,
};
module_i2c_driver(ktd3137_driver);

MODULE_DESCRIPTION("Kinetic KTD3137 Backlight LED Driver");
MODULE_AUTHOR("Guilherme Castro");
MODULE_LICENSE("GPL");
