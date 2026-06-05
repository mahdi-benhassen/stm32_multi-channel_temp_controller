#ifndef SPI_DRIVER_H
#define SPI_DRIVER_H

#include "system_config.h"

typedef enum {
    SPI_DRV_OK        = 0,
    SPI_DRV_TIMEOUT   = 1,
    SPI_DRV_BUSY      = 2,
    SPI_DRV_ERROR     = 3
} SPI_DrvStatus_t;

void              SPI_Drv_Init(void);
SPI_DrvStatus_t   SPI_Drv_Transmit(uint8_t *tx_data, uint16_t size, uint32_t timeout);
SPI_DrvStatus_t   SPI_Drv_Receive(uint8_t *rx_data, uint16_t size, uint32_t timeout);
SPI_DrvStatus_t   SPI_Drv_TransmitReceive(uint8_t *tx_data, uint8_t *rx_data, uint16_t size, uint32_t timeout);
void              SPI_Drv_CS_Select(uint8_t sensor_index);
void              SPI_Drv_CS_Deselect(uint8_t sensor_index);
void              SPI_Drv_CS_DeselectAll(void);

#endif /* SPI_DRIVER_H */
