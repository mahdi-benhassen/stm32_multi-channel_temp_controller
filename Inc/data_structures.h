#ifndef DATA_STRUCTURES_H
#define DATA_STRUCTURES_H

#include "system_config.h"

/* ==================== Enumerations ==================== */

typedef enum {
    SENSOR_FAULT_NONE           = 0x00,
    SENSOR_FAULT_OPEN_CIRCUIT   = 0x01,
    SENSOR_FAULT_SHORT_TO_GND   = 0x02,
    SENSOR_FAULT_SHORT_TO_VCC   = 0x04,
    SENSOR_FAULT_GENERAL        = 0x08,
    SENSOR_FAULT_UNDER_VOLTAGE  = 0x10,
    SENSOR_FAULT_OVER_VOLTAGE   = 0x20,
    SENSOR_FAULT_COMM_ERROR     = 0x40
} SensorFaultFlag_t;

typedef enum {
    ATUNE_IDLE       = 0,
    ATUNE_RUNNING    = 1,
    ATUNE_COMPLETED  = 2,
    ATUNE_FAILED     = 3
} AutoTuneState_t;

typedef enum {
    OUTPUT_MODE_ANALOG  = 0,
    OUTPUT_MODE_PWM     = 1,
    OUTPUT_MODE_DISABLED = 2
} OutputMode_t;

typedef enum {
    PID_MODE_MANUAL    = 0,
    PID_MODE_AUTOMATIC = 1
} PIDMode_t;

typedef enum {
    CONTROLLER_STATE_INIT        = 0,
    CONTROLLER_STATE_IDLE        = 1,
    CONTROLLER_STATE_RUNNING     = 2,
    CONTROLLER_STATE_FAULT       = 3,
    CONTROLLER_STATE_EMERGENCY   = 4
} ControllerState_t;

/* ==================== Data Structures ==================== */

/* PID Channel State */
typedef struct {
    float   kp;                /* Proportional gain */
    float   ki;                /* Integral gain */
    float   kd;                /* Derivative gain */
    float   setpoint;          /* Target temperature in °C */
    float   integral;          /* Integral accumulator */
    float   prev_error;        /* Previous error for derivative */
    float   output;            /* Current PID output 0-100% */
    float   p_term;
    float   i_term;
    float   d_term;
    PIDMode_t mode;            /* Manual or Automatic */
    uint8_t  sensor_index;     /* Which sensor feeds this PID (0-7) */
    uint8_t  output_index;     /* Which output channel this drives (0-3) */
} PIDChannel_t;

/* Auto-Tune State Machine */
typedef struct {
    AutoTuneState_t  state;
    uint8_t          channel_index;    /* Which PID channel being tuned */
    uint32_t         start_time_ms;
    uint32_t         last_cross_time_ms;
    float            step_output;
    float            setpoint_original;
    float            amplitude;
    float            period;
    float            output_at_start;
    uint8_t          half_cycle_count;
    bool             peak_detected;
    float            prev_pv;
    float            peak_max;
    float            peak_min;
    float            ultimate_gain;
    float            ultimate_period;
} AutoTune_t;

/* Sensor Channel State */
typedef struct {
    float            temperature;      /* Current temperature in °C */
    float            cold_junction;    /* Cold junction temp (MAX31855) */
    SensorFaultFlag_t fault_flags;
    uint32_t         last_read_ms;
    uint8_t          consecutive_errors;
    bool              data_valid;
} SensorChannel_t;

/* Output Channel State */
typedef struct {
    OutputMode_t mode;
    float        duty_cycle;      /* 0.0-100.0 (%) */
    uint16_t     dac_value;       /* Raw 12-bit DAC value */
    uint16_t     pwm_compare;     /* Timer CCR value */
    bool         output_enabled;
} OutputChannel_t;

/* Modbus Register Map (Holding Registers) */
typedef enum {
    REG_PV_BASE         = 0x0000,  /* PV for sensors 1-8: 8 registers */
    REG_SP_BASE         = 0x0008,  /* SP for PID 1-4: 4 registers */
    REG_KP_BASE         = 0x000C,  /* Kp for PID 1-4: 4 registers */
    REG_KI_BASE         = 0x0010,  /* Ki for PID 1-4: 4 registers */
    REG_KD_BASE         = 0x0014,  /* Kd for PID 1-4: 4 registers */
    REG_OUTPUT_BASE     = 0x0018,  /* Output % for channels 1-4: 4 registers */
    REG_ATUNE_STATUS    = 0x001C,  /* Auto-tune status per channel: 4 registers */
    REG_SENSOR_FAULTS   = 0x0020,  /* Sensor fault flags: 8 registers */
    REG_OUTPUT_MODE     = 0x0028,  /* Output mode per channel: 4 registers */
    REG_CJ_TEMP_BASE    = 0x002C,  /* Cold junction temp: 8 registers */
    REG_PID_MODE_BASE   = 0x0034,  /* PID mode per channel: 4 registers */
    REG_SYSTEM_STATE    = 0x0038,  /* Controller state: 1 register */
    REG_THERMAL_ALARM   = 0x0039,  /* Thermal alarm flags: 1 register */
    REG_ATUNE_CMD       = 0x0040,  /* Auto-tune start command: 4 registers */
    REG_SENSOR_TYPE     = 0x0044,  /* Sensor type config: 8 registers */
    REG_SAMPLE_TIME     = 0x004C,  /* PID sample time config: 1 register */
    REG_COUNT           = 0x0050
} ModbusRegister_t;

/* Global System State - Singleton */
typedef struct {
    SensorChannel_t  sensors[NUM_SENSOR_CHANNELS];
    PIDChannel_t     pid_channels[NUM_PID_CHANNELS];
    OutputChannel_t  outputs[NUM_OUTPUT_CHANNELS];
    AutoTune_t       auto_tuner;
    ControllerState_t controller_state;
    uint32_t         system_ticks_ms;
    uint32_t         last_pid_sample_ms;
    uint32_t         last_sensor_read_ms;
    uint32_t         last_web_update_ms;
    uint16_t         modbus_holding_regs[REG_COUNT * 2]; /* *2 for float-as-two-ints storage */
    bool             modbus_regs_dirty;
    bool             emergency_stop;
    uint16_t         thermal_alarm_flags; /* Bitmask per PID channel */
    uint32_t         thermal_overlimit_start[4];
} SystemState_t;

/* External global system state accessor */
extern SystemState_t g_sys;

/* ==================== Function Prototypes ==================== */

void System_Init(void);
void System_Update_Modbus_Registers(void);
void System_Load_Modbus_Registers(void);
uint16_t Float_To_ModbusReg(float value, float scale);
float ModbusReg_To_Float(uint16_t reg, float scale);

#endif /* DATA_STRUCTURES_H */
