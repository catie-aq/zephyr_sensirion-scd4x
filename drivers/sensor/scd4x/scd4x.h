/*
 * Copyright (c) 2024 CATIE
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_SENSOR_SENSIRION_SCD4X_SCD4X_H_
#define ZEPHYR_DRIVERS_SENSOR_SENSIRION_SCD4X_SCD4X_H_

#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/types.h>

#define SCD4X_I2C_ADDRESS 0x62

/* Commands */
#define SCD4X_CMD_START_PERIODIC_MEASUREMENT 0x21B1
#define SCD4X_CMD_READ_MEASUREMENT 0xEC05
#define SCD4X_CMD_STOP_PERIODIC_MEASUREMENT 0x3F86
#define SCD4X_CMD_SET_TEMPERATURE_OFFSET 0x241D
#define SCD4X_CMD_GET_TEMPERATURE_OFFSET 0x2318
#define SCD4X_CMD_SET_SENSOR_ALTITUDE 0x2427
#define SCD4X_CMD_SET_AMBIENT_PRESSURE 0xE000
#define SCD4X_CMD_PERFORM_FORCED_RECALIBRATION 0x362F
#define SCD4X_CMD_SET_AUTO_SELF_CALIBRATION 0x2416
#define SCD4X_CMD_GET_AUTO_SELF_CALIBRATION 0x2313
#define SCD4X_CMD_START_LOW_POWER_PERIODIC_MEASUREMENT 0x21AC
#define SCD4X_CMD_GET_DATA_READY_STATUS 0xE4B8
#define SCD4C_CMD_PERSIST_SETTINGS 0x3615
#define SCD4X_CMD_GET_SERIAL_NUMBER 0x3682
#define SCD4X_CMD_PERFORM_SELF_TEST 0x3639
#define SCD4X_CMD_PERFORM_FACTORY_RESET 0x3632
#define SCD4X_CMD_REINITIALIZE 0x3646
#define SCD4X_CMD_MEASURE_SINGLE_SHOT 0x219D
#define SCD4X_CMD_MEASURE_SINGLE_SHOT_RHT_ONLY 0x2196

enum scd4x_attr {
    SCD4X_ATTR_SERIAL_NUMBER = SENSOR_ATTR_PRIV_START,
};

/**
 * @brief User define structure accessible through the dev->config
 *
 */
struct scd4x_config {
    struct i2c_dt_spec i2c;
};

static int scd4x_read(const struct device *dev, uint16_t cmd, size_t len, uint16_t *val);
static int scd4x_write(const struct device *dev, uint16_t cmd, size_t len, uint16_t *val);
static int scd4x_send_command(const struct device *dev, uint16_t cmd);
static int scd4x_send_and_fetch(const struct device *dev, uint16_t cmd, uint16_t *val);

#endif /* ZEPHYR_DRIVERS_SENSOR_SENSIRION_SCD4X_SCD4X_H_ */
