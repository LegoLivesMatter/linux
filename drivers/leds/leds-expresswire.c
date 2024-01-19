// SPDX-License-Identifier: GPL-2.0-only
/*
 * Shared library for Kinetic's ExpressWire protocol.
 * This protocol works by pulsing the ExpressWire IC's control GPIO.
 * ktd2692 and ktd2801 are known to use this protocol.
 */

#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/leds-expresswire.h>

void expresswire_power_off(struct expresswire_common_props *props)
{
	gpiod_set_value_cansleep(props->ctrl_gpio, 0);
	usleep_range(props->timing.poweroff_us, props->timing.poweroff_us * 2);
}
EXPORT_SYMBOL_NS(expresswire_power_off, EXPRESSWIRE);

void expresswire_enable(struct expresswire_common_props *props)
{
	gpiod_set_value(props->ctrl_gpio, 1);
	udelay(props->timing.detect_delay_us);
	gpiod_set_value(props->ctrl_gpio, 0);
	udelay(props->timing.detect_us);
	gpiod_set_value(props->ctrl_gpio, 1);
}
EXPORT_SYMBOL_NS(expresswire_enable, EXPRESSWIRE);

void expresswire_start(struct expresswire_common_props *props)
{
	gpiod_set_value(props->ctrl_gpio, 1);
	udelay(props->timing.data_start_us);
}
EXPORT_SYMBOL_NS(expresswire_start, EXPRESSWIRE);

void expresswire_end(struct expresswire_common_props *props)
{
	gpiod_set_value(props->ctrl_gpio, 0);
	udelay(props->timing.end_of_data_low_us);
	gpiod_set_value(props->ctrl_gpio, 1);
	udelay(props->timing.end_of_data_high_us);
}
EXPORT_SYMBOL_NS(expresswire_end, EXPRESSWIRE);

void expresswire_set_bit(struct expresswire_common_props *props, bool bit)
{
	if (bit) {
		gpiod_set_value(props->ctrl_gpio, 0);
		udelay(props->timing.short_bitset_us);
		gpiod_set_value(props->ctrl_gpio, 1);
		udelay(props->timing.long_bitset_us);
	} else {
		gpiod_set_value(props->ctrl_gpio, 0);
		udelay(props->timing.long_bitset_us);
		gpiod_set_value(props->ctrl_gpio, 1);
		udelay(props->timing.short_bitset_us);
	}
}
EXPORT_SYMBOL_NS(expresswire_set_bit, EXPRESSWIRE);
