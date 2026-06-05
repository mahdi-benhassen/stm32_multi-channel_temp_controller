#ifndef SENSOR_DRIVER_H
#define SENSOR_DRIVER_H

#include "data_structures.h"

typedef enum {
    SENSOR_OK           = 0,
    SENSOR_ERR_TIMEOUT  = 1,
    SENSOR_ERR_FAULT    = 2,
    SENSOR_ERR_INVALID  = 3
} SensorStatus_t;

void            Sensor_Init(void);
SensorStatus_t  Sensor_ReadChannel(uint8_t channel_index);

/* MAX31855 (Thermocouple) specific */
SensorStatus_t  MAX31855_Read(uint8_t channel_index, float *temperature, float *cold_junction, uint8_t *fault);

/* MAX31865 (PT100) specific */
SensorStatus_t  MAX31865_Read(uint8_t channel_index, float *temperature, uint8_t *fault);

void            Sensor_ReadAll(void);
void            Sensor_ValidateData(uint8_t channel_index);

#endif /* SENSOR_DRIVER_H */
