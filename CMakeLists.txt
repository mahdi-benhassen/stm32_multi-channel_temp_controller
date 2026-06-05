cmake_minimum_required(VERSION 3.16)
project(stm32_temp_controller C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(MCU_FAMILY STM32F4xx)
set(MCU_MODEL  STM32F407VG)

set(MCU_FLAGS
    -mcpu=cortex-m4
    -mthumb
    -mfpu=fpv4-sp-d16
    -mfloat-abi=hard
    -Wall
    -Os
    -ffunction-sections
    -fdata-sections
)

set(CMAKE_C_FLAGS   "${MCU_FLAGS}")
set(CMAKE_ASM_FLAGS "${MCU_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "-specs=nosys.specs -specs=nano.specs -Wl,--gc-sections -T${CMAKE_SOURCE_DIR}/STM32F407VG_FLASH.ld")

add_definitions(-D${MCU_FAMILY} -DUSE_HAL_DRIVER)

# Source files
set(CORE_SOURCES
    Src/main.c
    Src/spi_driver.c
    Src/sensor_driver.c
    Src/output_driver.c
    Src/pid_controller.c
    Src/auto_tune.c
    Src/modbus_server.c
    Src/http_server.c
    Src/safety_monitor.c
    Src/webui_data.c
)

# HAL sources (include from STM32Cube package)
set(HAL_SOURCES
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_spi.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dac.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash.c
)

# Startup
set(STARTUP_SOURCES
    Drivers/CMSIS/Device/ST/STM32F4xx/Source/Templates/gcc/startup_stm32f407xx.s
)

set(SOURCES ${CORE_SOURCES} ${HAL_SOURCES} ${STARTUP_SOURCES})

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    Inc
    Drivers/STM32F4xx_HAL_Driver/Inc
    Drivers/CMSIS/Device/ST/STM32F4xx/Include
    Drivers/CMSIS/Include
)

target_link_libraries(${PROJECT_NAME} -lm)
