#include "sensor_driver.h"
#include "spi_driver.h"
#include <string.h>
#include <math.h>

#define MAX31855_DATA_SIZE      4
#define MAX31865_DATA_SIZE      4

/* MAX31855 raw data decode */
static void MAX31855_DecodeRaw(uint32_t raw_data, float *temp, float *cj_temp, uint8_t *fault) {
    *fault = SENSOR_FAULT_NONE;

    /* Bits 31-18: 14-bit signed thermocouple temperature (0.25°C resolution) */
    int16_t tc_raw = (int16_t)((raw_data >> 18) & 0x3FFF);
    if (tc_raw & 0x2000) { tc_raw |= 0xC000; }
    *temp = tc_raw * 0.25f;

    /* Bits 15-4: 12-bit signed internal (cold junction) temperature (0.0625°C) */
    int16_t cj_raw = (int16_t)((raw_data >> 4) & 0x0FFF);
    if (cj_raw & 0x0800) { cj_raw |= 0xF000; }
    *cj_temp = cj_raw * 0.0625f;

    /* Fault detection bits */
    if (raw_data & 0x0001) { *fault |= SENSOR_FAULT_OPEN_CIRCUIT; }
    if (raw_data & 0x0002) { *fault |= SENSOR_FAULT_SHORT_TO_GND; }
    if (raw_data & 0x0004) { *fault |= SENSOR_FAULT_SHORT_TO_VCC; }
    if (raw_data & 0x10000) { *fault |= SENSOR_FAULT_GENERAL; }
}

/* MAX31865 (PT100) raw data decode */
static void MAX31865_DecodeRaw(uint32_t raw_data, float *temp, uint8_t *fault) {
    *fault = SENSOR_FAULT_NONE;

    uint16_t rtd_raw = (uint16_t)(raw_data >> 1); /* Lower 15 bits, fault bit stripped */

    if (raw_data & 0x0001) { *fault |= SENSOR_FAULT_GENERAL; }      /* Fault bit D0 */
    if (raw_data & 0x0002) { *fault |= SENSOR_FAULT_OVER_VOLTAGE; }
    if (raw_data & 0x0004) { *fault |= SENSOR_FAULT_UNDER_VOLTAGE; }

    /* PT100 resistance from raw ADC value */
    float rtd_resistance = ((float)rtd_raw * 430.0f) / 32768.0f;

    /* PT100 Callendar-Van Dusen linear approx for 0-850°C range */
    /* R(T) = R0 * (1 + A*T + B*T^2), R0=100, A=3.9083e-3, B=-5.775e-7 */
    const float R0 = 100.0f;
    const float A  = 3.9083e-3f;
    const float B  = -5.775e-7f;

    /* Quadratic solve: B*T^2 + A*T + (R0 - R_meas)/R0 = 0 */
    float r_ratio = (rtd_resistance / R0) - 1.0f;
    float discriminant = (A * A) + (4.0f * B * r_ratio);
    if (discriminant >= 0.0f) {
        *temp = (-A + sqrtf(discriminant)) / (2.0f * B);
        if (*temp < -200.0f || *temp > 850.0f) {
            *temp = 0.0f;
            *fault |= SENSOR_FAULT_OPEN_CIRCUIT;
        }
    } else {
        *temp = 0.0f;
        *fault |= SENSOR_FAULT_GENERAL;
    }
}

SensorStatus_t MAX31855_Read(uint8_t channel_index, float *temperature, float *cold_junction, uint8_t *fault) {
    if (channel_index >= NUM_SENSOR_CHANNELS) {
        return SENSOR_ERR_INVALID;
    }

    uint8_t rx_buf[4] = {0};

    SPI_Drv_CS_Select(channel_index);
    SPI_DrvStatus_t status = SPI_Drv_Receive(rx_buf, MAX31855_DATA_SIZE, SPI_TIMEOUT_MS);
    SPI_Drv_CS_Deselect(channel_index);

    if (status != SPI_DRV_OK) {
        return SENSOR_ERR_TIMEOUT;
    }

    /* Combine 4 bytes into 32-bit value (big-endian from MAX31855) */
    uint32_t raw = ((uint32_t)rx_buf[0] << 24) | ((uint32_t)rx_buf[1] << 16) |
                   ((uint32_t)rx_buf[2] << 8)  | ((uint32_t)rx_buf[3]);

    MAX31855_DecodeRaw(raw, temperature, cold_junction, fault);

    if (*fault != SENSOR_FAULT_NONE) {
        return SENSOR_ERR_FAULT;
    }

    return SENSOR_OK;
}

SensorStatus_t MAX31865_Read(uint8_t channel_index, float *temperature, uint8_t *fault) {
    if (channel_index >= NUM_SENSOR_CHANNELS) {
        return SENSOR_ERR_INVALID;
    }

    /* MAX31865 requires writing config register before reading */
    uint8_t tx_buf[4] = {0x80, 0xC2, 0x00, 0x00}; /* Write config: Vbias ON, 1-shot, 3-wire RTD */
    uint8_t rx_buf[4] = {0};

    SPI_Drv_CS_Select(channel_index);
    if (SPI_Drv_TransmitReceive(tx_buf, rx_buf, MAX31865_DATA_SIZE, SPI_TIMEOUT_MS) != SPI_DRV_OK) {
        SPI_Drv_CS_Deselect(channel_index);
        return SENSOR_ERR_TIMEOUT;
    }
    SPI_Drv_CS_Deselect(channel_index);

    /* Wait for conversion (approx 52ms for 60Hz rejection) */
    HAL_Delay(55);

    /* Read RTD data register (0x01) */
    uint8_t tx_read[4] = {0x01, 0x00, 0x00, 0x00};
    uint8_t rx_data[4] = {0};

    SPI_Drv_CS_Select(channel_index);
    if (SPI_Drv_TransmitReceive(tx_read, rx_data, MAX31865_DATA_SIZE, SPI_TIMEOUT_MS) != SPI_DRV_OK) {
        SPI_Drv_CS_Deselect(channel_index);
        return SENSOR_ERR_TIMEOUT;
    }
    SPI_Drv_CS_Deselect(channel_index);

    uint32_t raw = ((uint32_t)rx_data[2] << 8) | ((uint32_t)rx_data[3]);
    MAX31865_DecodeRaw(raw, temperature, fault);

    if (*fault != SENSOR_FAULT_NONE) {
        return SENSOR_ERR_FAULT;
    }

    return SENSOR_OK;
}

void Sensor_Init(void) {
    for (uint8_t i = 0; i < NUM_SENSOR_CHANNELS; i++) {
        g_sys.sensors[i].temperature       = 0.0f;
        g_sys.sensors[i].cold_junction     = 0.0f;
        g_sys.sensors[i].fault_flags       = SENSOR_FAULT_NONE;
        g_sys.sensors[i].last_read_ms      = 0;
        g_sys.sensors[i].consecutive_errors = 0;
        g_sys.sensors[i].data_valid        = false;
    }
}

SensorStatus_t Sensor_ReadChannel(uint8_t channel_index) {
    if (channel_index >= NUM_SENSOR_CHANNELS) {
        return SENSOR_ERR_INVALID;
    }

    SensorChannel_t *ch = &g_sys.sensors[channel_index];
    SensorStatus_t result;

#if (ACTIVE_SENSOR_TYPE == SENSOR_TYPE_MAX31855)
    {
        uint8_t fault = SENSOR_FAULT_NONE;
        float temp = 0.0f, cj = 0.0f;
        result = MAX31855_Read(channel_index, &temp, &cj, &fault);

        if (result == SENSOR_OK) {
            ch->temperature   = temp;
            ch->cold_junction = cj;
            ch->fault_flags   = SENSOR_FAULT_NONE;
            ch->data_valid    = true;
            ch->consecutive_errors = 0;
        } else {
            ch->fault_flags |= fault;
            ch->consecutive_errors++;
            if (ch->consecutive_errors >= 3) {
                ch->data_valid = false;
            }
        }
    }
#elif (ACTIVE_SENSOR_TYPE == SENSOR_TYPE_MAX31865)
    {
        uint8_t fault = SENSOR_FAULT_NONE;
        float temp = 0.0f;
        result = MAX31865_Read(channel_index, &temp, &fault);

        if (result == SENSOR_OK) {
            ch->temperature   = temp;
            ch->cold_junction = 0.0f;
            ch->fault_flags   = SENSOR_FAULT_NONE;
            ch->data_valid    = true;
            ch->consecutive_errors = 0;
        } else {
            ch->fault_flags |= fault;
            ch->consecutive_errors++;
            if (ch->consecutive_errors >= 3) {
                ch->data_valid = false;
            }
        }
    }
#else
    #error "Invalid ACTIVE_SENSOR_TYPE: must be SENSOR_TYPE_MAX31855 or SENSOR_TYPE_MAX31865"
#endif

    ch->last_read_ms = g_sys.system_ticks_ms;
    return result;
}

void Sensor_ReadAll(void) {
    for (uint8_t i = 0; i < NUM_SENSOR_CHANNELS; i++) {
        Sensor_ReadChannel(i);
    }
}

void Sensor_ValidateData(uint8_t channel_index) {
    if (channel_index >= NUM_SENSOR_CHANNELS) return;

    SensorChannel_t *ch = &g_sys.sensors[channel_index];

    /* Check for absolute temperature limits */
    if (ch->temperature > MAX_SAFE_TEMPERATURE_C ||
        ch->temperature < MIN_SAFE_TEMPERATURE_C) {
        ch->data_valid = false;
        ch->fault_flags |= SENSOR_FAULT_GENERAL;
    }
}
