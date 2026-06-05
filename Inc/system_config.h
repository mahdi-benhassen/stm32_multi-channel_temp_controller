#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* ==================== Platform Selection ==================== */
#define STM32F407xx
#define HAL_FRAMEWORK

/* ==================== Sensor Configuration ==================== */
#define NUM_SENSOR_CHANNELS      8
#define SENSOR_TYPE_MAX31855     1    /* Thermocouple */
#define SENSOR_TYPE_MAX31865     2    /* PT100 RTD */
#define ACTIVE_SENSOR_TYPE       SENSOR_TYPE_MAX31855

/* ==================== PID Control Channels ==================== */
#define NUM_PID_CHANNELS         4
#define PID_SAMPLE_TIME_MS       100    /* 100ms PID loop period */
#define PID_OUTPUT_MIN           0.0f
#define PID_OUTPUT_MAX           100.0f
#define PID_INTEGRAL_MIN         -50.0f
#define PID_INTEGRAL_MAX         50.0f

/* ==================== SPI Bus Configuration ==================== */
#define SPI_INSTANCE             SPI1
#define SPI_BAUDRATE_PRESCALER   SPI_BAUDRATEPRESCALER_16
#define SPI_TIMEOUT_MS           100
#define SENSOR_READ_INTERVAL_MS  250

/* SPI Chip Select GPIO Port/Pins for sensors 1-8 */
/* Sensor 1-4 on GPIOB, Sensor 5-8 on GPIOC (example mapping) */
#define SPI_CS_GPIO_PORT_1       GPIOB
#define SPI_CS_PIN_1             GPIO_PIN_0
#define SPI_CS_GPIO_PORT_2       GPIOB
#define SPI_CS_PIN_2             GPIO_PIN_1
#define SPI_CS_GPIO_PORT_3       GPIOB
#define SPI_CS_PIN_3             GPIO_PIN_2
#define SPI_CS_GPIO_PORT_4       GPIOB
#define SPI_CS_PIN_4             GPIO_PIN_3
#define SPI_CS_GPIO_PORT_5       GPIOC
#define SPI_CS_PIN_5             GPIO_PIN_0
#define SPI_CS_GPIO_PORT_6       GPIOC
#define SPI_CS_PIN_6             GPIO_PIN_1
#define SPI_CS_GPIO_PORT_7       GPIOC
#define SPI_CS_PIN_7             GPIO_PIN_2
#define SPI_CS_GPIO_PORT_8       GPIOC
#define SPI_CS_PIN_8             GPIO_PIN_3

/* ==================== Output Configuration ==================== */
#define NUM_OUTPUT_CHANNELS      4
#define OUTPUT_MODE_AO          0    /* 0-10V Analog Output via DAC */
#define OUTPUT_MODE_PWM         1    /* PWM Digital Output */
#define ACTIVE_OUTPUT_MODE       OUTPUT_MODE_PWM

#define DAC_INSTANCE             DAC
#define DAC_CHANNEL_1            DAC_CHANNEL_1
#define DAC_CHANNEL_2            DAC_CHANNEL_2
#define DAC_VREF_MV              3300
#define DAC_OUTPUT_DIVIDER       3.3f  /* Voltage divider to scale 3.3V to 10V */

#define PWM_FREQUENCY_HZ         1000
#define PWM_TIMER_INSTANCE       TIM2
#define PWM_TIMER_CHANNEL_1      TIM_CHANNEL_1
#define PWM_TIMER_CHANNEL_2      TIM_CHANNEL_2
#define PWM_TIMER_CHANNEL_3      TIM_CHANNEL_3
#define PWM_TIMER_CHANNEL_4      TIM_CHANNEL_4
#define PWM_PERIOD               9999  /* ARR value for 1kHz with 72MHz CLK, prescaler 7 */

/* ==================== Networking Configuration ==================== */
#define ETH_MAC_ADDR0            0x00
#define ETH_MAC_ADDR1            0x80
#define ETH_MAC_ADDR2            0xE1
#define ETH_MAC_ADDR3            0x00
#define ETH_MAC_ADDR4            0x00
#define ETH_MAC_ADDR5            0x01

#define IP_ADDR0                 192
#define IP_ADDR1                 168
#define IP_ADDR2                 1
#define IP_ADDR3                 100

#define NETMASK_ADDR0            255
#define NETMASK_ADDR1            255
#define NETMASK_ADDR2            255
#define NETMASK_ADDR3            0

#define GW_ADDR0                 192
#define GW_ADDR1                 168
#define GW_ADDR2                 1
#define GW_ADDR3                 1

#define HTTP_SERVER_PORT         80
#define MODBUS_TCP_PORT          502
#define MODBUS_RTU_ENABLED       1
#define MODBUS_RTU_BAUDRATE      115200
#define MODBUS_RTU_USART         USART3
#define MODBUS_DEVICE_ID         1

/* ==================== Safety Limit Configuration ==================== */
#define THERMAL_RUNAWAY_THRESHOLD_C    10.0f   /* Max °C deviation before trip */
#define THERMAL_RUNAWAY_TIMEOUT_MS     5000   /* 5 seconds above threshold = trip */
#define MAX_SAFE_TEMPERATURE_C         450.0f  /* Absolute max temperature cut-off */
#define MIN_SAFE_TEMPERATURE_C         -50.0f

/* ==================== Auto-Tune Configuration ==================== */
#define ATUNE_HYSTERESIS_C             1.0f
#define ATUNE_TUNE_TIMEOUT_MS          300000 /* 5 minutes max tuning time */
#define ATUNE_MAX_CYCLES               5
#define ATUNE_STEP_OUTPUT_PCT          20.0f  /* 20% output bump for Z-N method */

/* ==================== System Timing ==================== */
#define SYS_TICK_MS                    (HAL_GetTick())

#endif /* SYSTEM_CONFIG_H */
