/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include "scd4x.h"

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS   10000

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)

/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

int main(void)
{
	int ret;
	const struct device *sensor;

	printk("Zephyr Example Application\n");

	sensor = DEVICE_DT_GET(DT_NODELABEL(scd4x0));
	if (!device_is_ready(sensor)) {
		printk("Sensor not ready");
		return 0;
	}

	if (!gpio_is_ready_dt(&led)) {
		return 0;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return 0;
	}

	// Check I2C peripheral on bus, send a bytes to each address and check ACK */
	const struct scd4x_config *config = sensor->config;
	for (uint8_t i = 0; i < 128; i++) {
		uint8_t data = 0x00;
		ret = i2c_write(config->i2c.bus, &data, 1, i);
		if (ret == 0) {
			printk("I2C peripheral found at address 0x%02x\n", i);
		}
		k_sleep(K_MSEC(10));
	}

	struct sensor_value serial_number;
	ret = sensor_attr_get(sensor, SENSOR_CHAN_CO2,
					SCD4X_ATTR_SERIAL_NUMBER, &serial_number);
	if (ret < 0) {
		printk("Could not get attribute (%d)", ret);
		return 0;
	}

	// print val1 and val2 in hex format
	printk("Serial number: 0x%08x%08x\n", serial_number.val2, serial_number.val1);

	while (1) {
		struct sensor_value val;

		ret = sensor_sample_fetch(sensor);
		if (ret < 0) {
			printk("Could not fetch sample (%d)\n", ret);
		}

		ret = sensor_channel_get(sensor, SENSOR_CHAN_CO2, &val);
		if (ret < 0) {
			printk("Could not get sample (%d)", ret);
			return 0;
		}
		printk("CO2 value: %d ppm\n", val.val1);

		ret = sensor_channel_get(sensor, SENSOR_CHAN_HUMIDITY, &val);
		if (ret < 0) {
			printk("Could not get sample (%d)", ret);
			return 0;
		}
		printk("Humidity value: %d.%06d %%RH\n", val.val1, val.val2);

		ret = sensor_channel_get(sensor, SENSOR_CHAN_AMBIENT_TEMP, &val);
		if (ret < 0) {
			printk("Could not get sample (%d)", ret);
			return 0;
		}
		printk("Temperature value: %d.%06d C\n", val.val1, val.val2);

		ret = gpio_pin_toggle_dt(&led);
		if (ret < 0) {
			return 0;
		}
		k_msleep(SLEEP_TIME_MS);
	}
	return 0;
}
