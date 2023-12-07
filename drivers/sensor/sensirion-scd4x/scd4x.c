/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT sensirion_scd4x

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(scd4x, CONFIG_SENSOR_LOG_LEVEL);

/**
 * @brief User define structure accessible through the API dev->data
 * 
 */
struct scd4x_data {
	int state;
};

/**
 * @brief User define structure accessible through the dev->config
 * 
 */
struct scd4x_config {
	struct gpio_dt_spec input;
};

static int scd4x_sample_fetch(const struct device *dev,
				      enum sensor_channel chan)
{
	const struct scd4x_config *config = dev->config;
	struct scd4x_data *data = dev->data;

	data->state = gpio_pin_get_dt(&config->input);

	return 0;
}

static int scd4x_channel_get(const struct device *dev,
				     enum sensor_channel chan,
				     struct sensor_value *val)
{
	struct scd4x_data *data = dev->data;

	if (chan != SENSOR_CHAN_PROX) {
		return -ENOTSUP;
	}

	val->val1 = data->state;

	return 0;
}

static const struct sensor_driver_api scd4x_api = {
	.sample_fetch = &scd4x_sample_fetch,
	.channel_get = &scd4x_channel_get,
};

static int scd4x_init(const struct device *dev)
{
	const struct scd4x_config *config = dev->config;

	int ret;

	if (!device_is_ready(config->input.port)) {
		LOG_ERR("Input GPIO not ready");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&config->input, GPIO_INPUT);
	if (ret < 0) {
		LOG_ERR("Could not configure input GPIO (%d)", ret);
		return ret;
	}

	return 0;
}

#define SCD4X_INIT(i)						       \
	static struct scd4x_data scd4x_data_##i;	       \
									       \
	static const struct scd4x_config scd4x_config_##i = {  \
		.input = GPIO_DT_SPEC_INST_GET(i, input_gpios),		       \
	};								       \
									       \
	DEVICE_DT_INST_DEFINE(i, scd4x_init, NULL,		       \
			      &scd4x_data_##i,			       \
			      &scd4x_config_##i, POST_KERNEL,	       \
			      CONFIG_SENSOR_INIT_PRIORITY, &scd4x_api);

DT_INST_FOREACH_STATUS_OKAY(SCD4X_INIT)
