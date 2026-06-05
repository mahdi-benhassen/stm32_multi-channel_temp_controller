#include "spi_driver.h"
#include "data_structures.h"

static SPI_HandleTypeDef hspi;

/* GPIO/CS pin lookup table */
static const struct {
    GPIO_TypeDef *port;
    uint16_t pin;
} cs_pins[NUM_SENSOR_CHANNELS] = {
    { SPI_CS_GPIO_PORT_1, SPI_CS_PIN_1 },
    { SPI_CS_GPIO_PORT_2, SPI_CS_PIN_2 },
    { SPI_CS_GPIO_PORT_3, SPI_CS_PIN_3 },
    { SPI_CS_GPIO_PORT_4, SPI_CS_PIN_4 },
    { SPI_CS_GPIO_PORT_5, SPI_CS_PIN_5 },
    { SPI_CS_GPIO_PORT_6, SPI_CS_PIN_6 },
    { SPI_CS_GPIO_PORT_7, SPI_CS_PIN_7 },
    { SPI_CS_GPIO_PORT_8, SPI_CS_PIN_8 }
};

void SPI_Drv_Init(void) {
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_SPI1_CLK_ENABLE();

    /* Configure SPI GPIO: PA5=SCK, PA6=MISO, PA7=MOSI */
    GPIO_InitTypeDef gpio = {0};

    gpio.Mode  = GPIO_MODE_AF_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF5_SPI1;
    gpio.Pin   = GPIO_PIN_5 | GPIO_PIN_7;
    HAL_GPIO_Init(GPIOA, &gpio);

    gpio.Mode  = GPIO_MODE_AF_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Alternate = GPIO_AF5_SPI1;
    gpio.Pin   = GPIO_PIN_6;
    HAL_GPIO_Init(GPIOA, &gpio);

    /* Configure all CS pins as output push-pull, set HIGH (deselected) */
    for (uint8_t i = 0; i < NUM_SENSOR_CHANNELS; i++) {
        GPIO_InitTypeDef gpio_cs = {0};
        gpio_cs.Mode  = GPIO_MODE_OUTPUT_PP;
        gpio_cs.Pull  = GPIO_NOPULL;
        gpio_cs.Speed = GPIO_SPEED_FREQ_HIGH;
        gpio_cs.Pin   = cs_pins[i].pin;
        HAL_GPIO_Init(cs_pins[i].port, &gpio_cs);
        HAL_GPIO_WritePin(cs_pins[i].port, cs_pins[i].pin, GPIO_PIN_SET);
    }

    /* Initialize SPI1 */
    hspi.Instance            = SPI_INSTANCE;
    hspi.Init.Mode           = SPI_MODE_MASTER;
    hspi.Init.Direction      = SPI_DIRECTION_2LINES;
    hspi.Init.DataSize       = SPI_DATASIZE_8BIT;
    hspi.Init.CLKPolarity    = SPI_POLARITY_LOW;
    hspi.Init.CLKPhase       = SPI_PHASE_1EDGE;
    hspi.Init.NSS            = SPI_NSS_SOFT;
    hspi.Init.BaudRatePrescaler = SPI_BAUDRATE_PRESCALER;
    hspi.Init.FirstBit       = SPI_FIRSTBIT_MSB;
    hspi.Init.TIMode         = SPI_TIMODE_DISABLE;
    hspi.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi.Init.CRCPolynomial  = 7;

    HAL_SPI_Init(&hspi);
}

SPI_DrvStatus_t SPI_Drv_Transmit(uint8_t *tx_data, uint16_t size, uint32_t timeout) {
    HAL_StatusTypeDef status = HAL_SPI_Transmit(&hspi, tx_data, size, timeout);
    return (status == HAL_OK) ? SPI_DRV_OK : SPI_DRV_TIMEOUT;
}

SPI_DrvStatus_t SPI_Drv_Receive(uint8_t *rx_data, uint16_t size, uint32_t timeout) {
    HAL_StatusTypeDef status = HAL_SPI_Receive(&hspi, rx_data, size, timeout);
    return (status == HAL_OK) ? SPI_DRV_OK : SPI_DRV_TIMEOUT;
}

SPI_DrvStatus_t SPI_Drv_TransmitReceive(uint8_t *tx_data, uint8_t *rx_data, uint16_t size, uint32_t timeout) {
    HAL_StatusTypeDef status = HAL_SPI_TransmitReceive(&hspi, tx_data, rx_data, size, timeout);
    return (status == HAL_OK) ? SPI_DRV_OK : SPI_DRV_TIMEOUT;
}

void SPI_Drv_CS_Select(uint8_t sensor_index) {
    if (sensor_index < NUM_SENSOR_CHANNELS) {
        HAL_GPIO_WritePin(cs_pins[sensor_index].port, cs_pins[sensor_index].pin, GPIO_PIN_RESET);
    }
}

void SPI_Drv_CS_Deselect(uint8_t sensor_index) {
    if (sensor_index < NUM_SENSOR_CHANNELS) {
        HAL_GPIO_WritePin(cs_pins[sensor_index].port, cs_pins[sensor_index].pin, GPIO_PIN_SET);
    }
}

void SPI_Drv_CS_DeselectAll(void) {
    for (uint8_t i = 0; i < NUM_SENSOR_CHANNELS; i++) {
        HAL_GPIO_WritePin(cs_pins[i].port, cs_pins[i].pin, GPIO_PIN_SET);
    }
}
