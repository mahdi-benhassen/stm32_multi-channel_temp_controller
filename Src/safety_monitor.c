#include "safety_monitor.h"
#include "output_driver.h"

void SafetyMonitor_Init(void) {
    g_sys.thermal_alarm_flags = 0;
    g_sys.emergency_stop       = false;
    memset(g_sys.thermal_overlimit_start, 0, sizeof(g_sys.thermal_overlimit_start));
}

SafetyStatus_t SafetyMonitor_Check(void) {
    SafetyMonitor_CheckSensorLimits();
    SafetyMonitor_CheckSystem();

    if (g_sys.emergency_stop) {
        return SAFETY_EMERGENCY;
    }

    for (uint8_t i = 0; i < NUM_PID_CHANNELS; i++) {
        SafetyMonitor_CheckThermalRunaway(i);
    }

    if (g_sys.thermal_alarm_flags & THERMAL_ALARM_EMERGENCY_BIT) {
        SafetyMonitor_TriggerEmergency();
        return SAFETY_EMERGENCY;
    }

    if (g_sys.thermal_alarm_flags != 0) {
        return SAFETY_WARN_THERMAL;
    }

    return SAFETY_OK;
}

void SafetyMonitor_CheckThermalRunaway(uint8_t pid_channel) {
    if (pid_channel >= NUM_PID_CHANNELS) return;

    PIDChannel_t *pid = &g_sys.pid_channels[pid_channel];
    uint8_t s_idx = pid->sensor_index;

    if (s_idx >= NUM_SENSOR_CHANNELS) return;

    SensorChannel_t *sensor = &g_sys.sensors[s_idx];

    /* Skip if sensor data is invalid */
    if (!sensor->data_valid) {
        g_sys.thermal_alarm_flags |= (1 << pid_channel);
        return;
    }

    /* Skip if PID is in manual mode */
    if (pid->mode == PID_MODE_MANUAL) {
        g_sys.thermal_alarm_flags &= ~(1 << pid_channel);
        g_sys.thermal_overlimit_start[pid_channel] = 0;
        return;
    }

    float deviation = pid->setpoint - sensor->temperature;

    /* Check for thermal runaway: PV deviates too far from SP */
    if (fabsf(deviation) > THERMAL_RUNAWAY_THRESHOLD_C &&
        pid->output > 0.0f) {

        if (g_sys.thermal_overlimit_start[pid_channel] == 0) {
            g_sys.thermal_overlimit_start[pid_channel] = g_sys.system_ticks_ms;
        } else {
            uint32_t elapsed = g_sys.system_ticks_ms - g_sys.thermal_overlimit_start[pid_channel];
            if (elapsed > THERMAL_RUNAWAY_TIMEOUT_MS) {
                g_sys.thermal_alarm_flags |= (1 << pid_channel);
                g_sys.thermal_alarm_flags |= THERMAL_ALARM_EMERGENCY_BIT;
            }
        }
    } else {
        g_sys.thermal_overlimit_start[pid_channel] = 0;
        g_sys.thermal_alarm_flags &= ~(1 << pid_channel);
    }
}

void SafetyMonitor_CheckSensorLimits(void) {
    bool sensor_fault = false;
    bool over_temp    = false;

    for (uint8_t i = 0; i < NUM_SENSOR_CHANNELS; i++) {
        SensorChannel_t *s = &g_sys.sensors[i];

        if (!s->data_valid && s->consecutive_errors >= 5) {
            sensor_fault = true;
        }

        /* Absolute over-temperature check */
        if (s->temperature > MAX_SAFE_TEMPERATURE_C) {
            over_temp = true;
            s->fault_flags |= SENSOR_FAULT_OVER_VOLTAGE;
        }

        if (s->temperature < MIN_SAFE_TEMPERATURE_C) {
            s->fault_flags |= SENSOR_FAULT_UNDER_VOLTAGE;
        }
    }

    if (sensor_fault) {
        g_sys.thermal_alarm_flags |= THERMAL_ALARM_SENSOR_BIT;
    }

    if (over_temp) {
        g_sys.thermal_alarm_flags |= THERMAL_ALARM_EMERGENCY_BIT;
        SafetyMonitor_TriggerEmergency();
    }
}

void SafetyMonitor_CheckSystem(void) {
    /* Check if all required sensors for active PID channels have valid data */
    for (uint8_t i = 0; i < NUM_PID_CHANNELS; i++) {
        PIDChannel_t *pid = &g_sys.pid_channels[i];
        if (pid->mode == PID_MODE_AUTOMATIC) {
            uint8_t s_idx = pid->sensor_index;
            if (s_idx < NUM_SENSOR_CHANNELS && !g_sys.sensors[s_idx].data_valid) {
                /* Sensor feeding active PID is invalid - set PID to manual, 0% output */
                pid->mode = PID_MODE_MANUAL;
                Output_SetChannel(pid->output_index, 0.0f);
                g_sys.thermal_alarm_flags |= THERMAL_ALARM_SENSOR_BIT;
            }
        }
    }
}

void SafetyMonitor_TriggerEmergency(void) {
    g_sys.controller_state = CONTROLLER_STATE_EMERGENCY;
    g_sys.emergency_stop    = true;
    Output_EmergencyStop();
}

void SafetyMonitor_ClearAlarms(void) {
    g_sys.thermal_alarm_flags = 0;
    memset(g_sys.thermal_overlimit_start, 0, sizeof(g_sys.thermal_overlimit_start));

    if (g_sys.emergency_stop) {
        Output_ClearEmergency();
        g_sys.emergency_stop = false;
        g_sys.controller_state = CONTROLLER_STATE_IDLE;
    }
}

uint16_t SafetyMonitor_GetAlarmFlags(void) {
    return g_sys.thermal_alarm_flags;
}
