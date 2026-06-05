#include "system_config.h"
#include "data_structures.h"
#include "spi_driver.h"
#include "sensor_driver.h"
#include "output_driver.h"
#include "pid_controller.h"
#include "auto_tune.h"
#include "modbus_server.h"
#include "http_server.h"
#include "safety_monitor.h"

#include <string.h>

SystemState_t g_sys;

static void SystemClock_Config(void);
static void System_Init(void);
uint16_t Float_To_ModbusReg(float value, float scale);
float    ModbusReg_To_Float(uint16_t reg_val, float scale);

int main(void) {
    /* HAL Init */
    HAL_Init();
    SystemClock_Config();

    /* Initialize system state */
    memset(&g_sys, 0, sizeof(SystemState_t));
    g_sys.controller_state = CONTROLLER_STATE_INIT;

    /* Initialize drivers */
    SPI_Drv_Init();
    Sensor_Init();
    Output_Init();

    /* Initialize control modules */
    PID_Init();
    AutoTune_Init();
    SafetyMonitor_Init();

    /* Initialize communication */
    Modbus_Init();

    g_sys.controller_state = CONTROLLER_STATE_IDLE;

    /* Super-loop with fixed-period PID execution */
    uint32_t last_pid_time   = HAL_GetTick();
    uint32_t last_sensor_time = HAL_GetTick();
    uint32_t last_safety_time  = HAL_GetTick();
    uint32_t last_modbus_time  = HAL_GetTick();

    while (1) {
        g_sys.system_ticks_ms = HAL_GetTick();
        uint32_t now = g_sys.system_ticks_ms;

        /* ---- Sensor Read (Periodic) ---- */
        if ((now - last_sensor_time) >= SENSOR_READ_INTERVAL_MS) {
            last_sensor_time = now;
            Sensor_ReadAll();
            System_Update_Modbus_Registers();
        }

        /* ---- PID Loop (Fixed Sample Time) ---- */
        if ((now - last_pid_time) >= PID_SAMPLE_TIME_MS) {
            last_pid_time = now;

            /* Safety check before PID computation */
            SafetyStatus_t safety = SafetyMonitor_Check();

            if (safety == SAFETY_OK || safety == SAFETY_WARN_THERMAL) {
                /* Auto-tune process if running */
                if (g_sys.auto_tuner.state == ATUNE_RUNNING) {
                    uint8_t at_ch = g_sys.auto_tuner.channel_index;
                    uint8_t s_idx = g_sys.pid_channels[at_ch].sensor_index;
                    AutoTune_Process(g_sys.sensors[s_idx].temperature);
                }

                /* Compute PID for all channels */
                PID_ProcessAll();
            }

            /* Refresh modbus registers with latest output values */
            System_Update_Modbus_Registers();
        }

        /* ---- Safety Monitor (1 Hz) ---- */
        if ((now - last_safety_time) >= 1000) {
            last_safety_time = now;
            SafetyMonitor_Check();
            if (g_sys.controller_state != CONTROLLER_STATE_EMERGENCY &&
                g_sys.controller_state != CONTROLLER_STATE_FAULT) {
                g_sys.controller_state = CONTROLLER_STATE_RUNNING;
            }
        }

#if MODBUS_RTU_ENABLED
        /* ---- Modbus RTU Polling ---- */
        Modbus_RTU_Process();
#endif

        /* Yield to allow lwIP or other background tasks */
        HAL_Delay(1);
    }
}

/* ==================== System Helper Functions ==================== */

static void System_Init(void) {
    memset(&g_sys, 0, sizeof(SystemState_t));
    g_sys.controller_state = CONTROLLER_STATE_INIT;
}

void System_Update_Modbus_Registers(void) {
    /* Write sensor PV values (scaled: °C × 10) */
    for (uint8_t i = 0; i < NUM_SENSOR_CHANNELS; i++) {
        g_sys.modbus_holding_regs[REG_PV_BASE + i] =
            Float_To_ModbusReg(g_sys.sensors[i].temperature, 10.0f);
        g_sys.modbus_holding_regs[REG_CJ_TEMP_BASE + i] =
            Float_To_ModbusReg(g_sys.sensors[i].cold_junction, 10.0f);
        g_sys.modbus_holding_regs[REG_SENSOR_FAULTS + i] =
            (uint16_t)g_sys.sensors[i].fault_flags;
        g_sys.modbus_holding_regs[REG_SENSOR_TYPE + i] = ACTIVE_SENSOR_TYPE;
    }

    /* Write PID channel values */
    for (uint8_t i = 0; i < NUM_PID_CHANNELS; i++) {
        PIDChannel_t *p = &g_sys.pid_channels[i];
        g_sys.modbus_holding_regs[REG_SP_BASE + i]     = Float_To_ModbusReg(p->setpoint, 10.0f);
        g_sys.modbus_holding_regs[REG_KP_BASE + i]     = Float_To_ModbusReg(p->kp, 1000.0f);
        g_sys.modbus_holding_regs[REG_KI_BASE + i]     = Float_To_ModbusReg(p->ki, 1000.0f);
        g_sys.modbus_holding_regs[REG_KD_BASE + i]     = Float_To_ModbusReg(p->kd, 1000.0f);
        g_sys.modbus_holding_regs[REG_OUTPUT_BASE + i] = Float_To_ModbusReg(p->output, 10.0f);
        g_sys.modbus_holding_regs[REG_PID_MODE_BASE + i] = (p->mode == PID_MODE_AUTOMATIC) ? 1 : 0;

        AutoTune_t *at = &g_sys.auto_tuner;
        if (at->channel_index == i) {
            g_sys.modbus_holding_regs[REG_ATUNE_STATUS + i] = (uint16_t)at->state;
        }
    }

    /* Write output channel values */
    for (uint8_t i = 0; i < NUM_OUTPUT_CHANNELS; i++) {
        g_sys.modbus_holding_regs[REG_OUTPUT_MODE + i] =
            (g_sys.outputs[i].output_enabled ? g_sys.outputs[i].mode : OUTPUT_MODE_DISABLED);
    }

    g_sys.modbus_holding_regs[REG_SYSTEM_STATE] = (uint16_t)g_sys.controller_state;
    g_sys.modbus_holding_regs[REG_THERMAL_ALARM] = SafetyMonitor_GetAlarmFlags();
    g_sys.modbus_holding_regs[REG_SAMPLE_TIME]  = PID_SAMPLE_TIME_MS;
}

void System_Load_Modbus_Registers(void) {
    for (uint8_t i = 0; i < NUM_PID_CHANNELS; i++) {
        float sp = ModbusReg_To_Float(g_sys.modbus_holding_regs[REG_SP_BASE + i], 10.0f);
        float kp = ModbusReg_To_Float(g_sys.modbus_holding_regs[REG_KP_BASE + i], 1000.0f);
        float ki = ModbusReg_To_Float(g_sys.modbus_holding_regs[REG_KI_BASE + i], 1000.0f);
        float kd = ModbusReg_To_Float(g_sys.modbus_holding_regs[REG_KD_BASE + i], 1000.0f);
        uint16_t mode = g_sys.modbus_holding_regs[REG_PID_MODE_BASE + i];

        PID_SetSetpoint(i, sp);
        PID_SetTunings(i, kp, ki, kd);
        PID_SetMode(i, (mode == 1) ? PID_MODE_AUTOMATIC : PID_MODE_MANUAL);
    }
}

/* Convert float value to Modbus register (scaled integer) */
uint16_t Float_To_ModbusReg(float value, float scale) {
    float scaled = value * scale + 0.5f;
    if (scaled < 0) scaled -= 1.0f;
    int32_t int_val = (int32_t)scaled;
    if (int_val > 65535) int_val = 65535;
    if (int_val < 0) int_val = 0;
    return (uint16_t)int_val;
}

float ModbusReg_To_Float(uint16_t reg_val, float scale) {
    return (float)reg_val / scale;
}

/* ==================== System Clock Configuration ==================== */
static void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /* Configure the main internal regulator output voltage */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState       = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM       = 8;
    RCC_OscInitStruct.PLL.PLLN       = 336;
    RCC_OscInitStruct.PLL.PLLP       = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ       = 7;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    /* OverDrive */
    HAL_PWREx_EnableOverDrive();

    /* Initializes the CPU, AHB and APB busses clocks */
    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                       RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);

    /* Configure the Systick */
    HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq() / 1000);
    HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);
    HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
}
