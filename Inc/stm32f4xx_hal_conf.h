#ifndef STM32F4XX_HAL_CONF_H
#define STM32F4XX_HAL_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/* Pull in device-specific CMSIS header (defines __IO, uint32_t, GPIO_TypeDef, etc.) */
#if defined(STM32F407xx)
  #include "stm32f407xx.h"
#elif defined(STM32F405xx)
  #include "stm32f405xx.h"
#elif defined(STM32F427xx)
  #include "stm32f427xx.h"
#elif defined(STM32F429xx)
  #include "stm32f429xx.h"
#elif defined(STM32F446xx)
  #include "stm32f446xx.h"
#else
  #error "Please define an STM32F4xx device: STM32F407xx, STM32F429xx, etc."
#endif

/* Pull in HAL common definitions (defines HAL_StatusTypeDef, etc.) */
#include "stm32f4xx_hal_def.h"

/* ---- HAL module enable defines ---- */
#define HAL_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_SPI_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED
#define HAL_DAC_MODULE_ENABLED
#define HAL_TIM_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED

/* ---- Clock config values ---- */
#define HSE_VALUE          8000000U
#define HSE_STARTUP_TIMEOUT 100U
#define HSI_VALUE          16000000U
#define LSE_VALUE          32768U
#define LSI_VALUE          32000U
#define EXTERNAL_CLOCK_VALUE 12288000U

#define VDD_VALUE          3300U
#define PREFETCH_ENABLE    1U
#define INSTRUCTION_CACHE_ENABLE  1U
#define DATA_CACHE_ENABLE         1U

/* ---- HAL callback options ---- */
#define USE_HAL_SPI_REGISTER_CALLBACKS 0U
#define USE_HAL_UART_REGISTER_CALLBACKS 0U
#define USE_HAL_TIM_REGISTER_CALLBACKS  0U
#define USE_HAL_DAC_REGISTER_CALLBACKS  0U

#define assert_param(expr) ((void)0U)

#ifdef __cplusplus
}
#endif

#endif /* STM32F4XX_HAL_CONF_H */
