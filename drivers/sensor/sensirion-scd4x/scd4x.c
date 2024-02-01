/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT sensirion_scd4x

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/crc.h>
LOG_MODULE_REGISTER(scd4x, CONFIG_SENSOR_LOG_LEVEL);

#include "scd4x.h"

#define U16_TO_BYTE_ARRAY(u, ba)                                                                   \
    do {                                                                                           \
        ba[0] = (u >> 8) & 0xFF;                                                                   \
        ba[1] = u & 0xFF;                                                                          \
    } while (0)

#define BYTE_ARRAY_TO_U16(ba, u)                                                                   \
    do {                                                                                           \
        u = (ba[0] << 8) | (ba[1]);                                                                \
    } while (0)

#define MAX_READ_SIZE (3)
#define CRC8_POLYNOMIAL (0x31)
#define CRC8_INIT (0xFF)

/**
 * @brief User define structure accessible through the API dev->data
 *
 */
struct scd4x_data {
    uint16_t co2_ppm;
    int32_t temperature;
    int32_t humidity;
};

static int scd4x_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
    struct scd4x_data *data = dev->data;

    // Check if sensor is ready
    uint16_t ready;
    scd4x_read(dev, SCD4X_CMD_GET_DATA_READY_STATUS, 1, &ready);
    if ((ready & 0x03FF) == 0) {
        return -EBUSY;
    }

    // Read measurements
    uint16_t measurements[3];
    scd4x_read(dev, SCD4X_CMD_READ_MEASUREMENT, 3, measurements);

    data->co2_ppm = measurements[0];
    /* Temperature in degrees Celsius is T = -45 + 175 * rawValue / (2^16 - 1)
    We multiply by 10^7 to use fixed point arithmetic. 175 / (2^16 - 1) * 10^7 = 26703 */
    data->temperature = measurements[1] * 26703 - 450000000;
    /* Humidity in percent is RH = rawValue / (2^16 - 1) * 100
    We multiply by 10^7 to use fixed point arithmetic. 100 / (2^16 - 1) * 10^7 = 15259 */
    data->humidity = measurements[2] * 15259;

    return 0;
}

static int scd4x_channel_get(
        const struct device *dev, enum sensor_channel chan, struct sensor_value *val)
{
    struct scd4x_data *data = dev->data;

    if (chan != SENSOR_CHAN_CO2 && chan != SENSOR_CHAN_AMBIENT_TEMP
            && chan != SENSOR_CHAN_HUMIDITY) {
        return -ENOTSUP;
    }

    if (chan == SENSOR_CHAN_CO2) {
        val->val1 = (int32_t)data->co2_ppm;
        val->val2 = 0;
        return 0;
    }

    if (chan == SENSOR_CHAN_AMBIENT_TEMP) {
        val->val1 = data->temperature / 10000000;
        val->val2 = (data->temperature % 10000000) / 10;
        return 0;
    }

    if (chan == SENSOR_CHAN_HUMIDITY) {
        val->val1 = data->humidity / 10000000;
        val->val2 = (data->humidity % 10000000) / 10;
        return 0;
    }

    return 0;
}

static int scd4x_attr_set(const struct device *dev,
        enum sensor_channel chan,
        enum sensor_attribute attr,
        const struct sensor_value *val)
{
    if (chan != SENSOR_CHAN_PROX) {
        return -ENOTSUP;
    }

    return -ENOTSUP;
}

static int scd4x_attr_get(const struct device *dev,
        enum sensor_channel chan,
        enum sensor_attribute attr,
        struct sensor_value *val)
{
    if (chan != SENSOR_CHAN_CO2) {
        return -ENOTSUP;
    }

    if (attr == (enum sensor_attribute)SCD4X_ATTR_SERIAL_NUMBER) {
        /* Read device ID */
        uint16_t device_id[3];
        scd4x_read(dev, SCD4X_CMD_GET_SERIAL_NUMBER, 3, device_id);
        val->val1 = device_id[1] << 16 | device_id[0];
        val->val2 = device_id[2] & 0xFFFF;
        return 0;
    }

    return -ENOTSUP;
}

static const struct sensor_driver_api scd4x_api = {
    .sample_fetch = &scd4x_sample_fetch,
    .channel_get = &scd4x_channel_get,
    .attr_set = &scd4x_attr_set,
    .attr_get = &scd4x_attr_get,
};

static int scd4x_init(const struct device *dev)
{
    const struct scd4x_config *config = dev->config;

    int ret;

    if (!device_is_ready(config->i2c.bus)) {
        LOG_ERR("I2C bus not ready");
        return -ENODEV;
    }

    // Stop periodic measurement
    ret = scd4x_send_command(dev, SCD4X_CMD_STOP_PERIODIC_MEASUREMENT);
    if (ret < 0) {
        LOG_ERR("Failed to stop periodic measurement");
        return ret;
    }

    k_sleep(K_MSEC(500)); // Wait for stop

    // Start periodic measurement
    ret = scd4x_send_command(dev, SCD4X_CMD_START_PERIODIC_MEASUREMENT);
    if (ret < 0) {
        LOG_ERR("Failed to start periodic measurement");
        return ret;
    }

    return 0;
}
/** Read buffer */
static char read_buf[3 * MAX_READ_SIZE];

static int scd4x_read(const struct device *dev, uint16_t cmd, size_t len, uint16_t *val)
{
    int ret;

    const struct scd4x_config *config = dev->config;

    U16_TO_BYTE_ARRAY(cmd, read_buf);
    ret = i2c_write_dt(&config->i2c, (const uint8_t *)&read_buf, sizeof(cmd));
    if (ret < 0) {
        LOG_ERR("Failed to write command 0x%04x, error %d", cmd, ret);
        return ret;
    }

    k_sleep(K_MSEC(1));

    /* Each response element is 3 bytes long, a 2 bytes word and a CRC8 */
    ret = i2c_read_dt(&config->i2c, read_buf, len * 3);
    if (ret < 0) {
        LOG_ERR("Failed to read response");
        return ret;
    }

    /* Verify CRC of each word */
    uint8_t crc;
    for (int i = 0; i < len; i++) {
        crc = crc8(read_buf + (3 * i), 2, CRC8_POLYNOMIAL, CRC8_INIT, false);
        if (crc != read_buf[3 * i + 2]) {
            LOG_ERR("CRC error");
            return -EIO;
        }
    }

    for (int i = 0; i < len; i++) {
        BYTE_ARRAY_TO_U16((read_buf + (3 * i)), val[i]);
    }

    return 0;
}

static int scd4x_write(const struct device *dev, uint16_t cmd, size_t len, uint16_t *val)
{
    int ret;

    const struct scd4x_config *config = dev->config;

    U16_TO_BYTE_ARRAY(cmd, read_buf);
    for (int i = 0; i < len; i++) {
        U16_TO_BYTE_ARRAY(val[i], (read_buf + (3 * i) + sizeof(cmd)));
        read_buf[3 * i + 2 + sizeof(cmd)]
                = crc8(read_buf + (3 * i) + sizeof(cmd), 2, CRC8_POLYNOMIAL, CRC8_INIT, false);
    }
    ret = i2c_write_dt(&config->i2c, (char *)&read_buf, sizeof(cmd) + len * 3);

    if (ret < 0) {
        LOG_ERR("Failed to write command 0x%04x, error %d", cmd, ret);
        return ret;
    }

    return 0;
}

static int scd4x_send_command(const struct device *dev, uint16_t cmd)
{
    return scd4x_write(dev, cmd, 0, NULL);
}

static int scd4x_send_and_fetch(const struct device *dev, uint16_t cmd, uint16_t *val)
{
    int ret;

    ret = scd4x_send_command(dev, cmd);
    if (ret < 0) {
        return ret;
    }

    k_sleep(K_MSEC(1));

    return scd4x_read(dev, cmd, 1, val);
}

#define SCD4X_INIT(i)                                                                              \
    static struct scd4x_data scd4x_data_##i;                                                       \
                                                                                                   \
    static const struct scd4x_config scd4x_config_##i = {                                          \
        .i2c = I2C_DT_SPEC_INST_GET(i),                                                            \
    };                                                                                             \
                                                                                                   \
    DEVICE_DT_INST_DEFINE(i,                                                                       \
            scd4x_init,                                                                            \
            NULL,                                                                                  \
            &scd4x_data_##i,                                                                       \
            &scd4x_config_##i,                                                                     \
            POST_KERNEL,                                                                           \
            CONFIG_SENSOR_INIT_PRIORITY,                                                           \
            &scd4x_api);

DT_INST_FOREACH_STATUS_OKAY(SCD4X_INIT)
