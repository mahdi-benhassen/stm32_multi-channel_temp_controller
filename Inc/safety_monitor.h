#ifndef SAFETY_MONITOR_H
#define SAFETY_MONITOR_H

#include "data_structures.h"

#define THERMAL_ALARM_PID1_BIT       0x01
#define THERMAL_ALARM_PID2_BIT       0x02
#define THERMAL_ALARM_PID3_BIT       0x04
#define THERMAL_ALARM_PID4_BIT       0x08
#define THERMAL_ALARM_SENSOR_BIT     0x10
#define THERMAL_ALARM_EMERGENCY_BIT  0x20

typedef enum {
    SAFETY_OK           = 0,
    SAFETY_WARN_THERMAL = 1,
    SAFETY_ALARM_SENSOR = 2,
    SAFETY_EMERGENCY    = 3
} SafetyStatus_t;

void            SafetyMonitor_Init(void);
SafetyStatus_t  SafetyMonitor_Check(void);
void            SafetyMonitor_CheckThermalRunaway(uint8_t pid_channel);
void            SafetyMonitor_CheckSensorLimits(void);
void            SafetyMonitor_CheckSystem(void);
void            SafetyMonitor_TriggerEmergency(void);
void            SafetyMonitor_ClearAlarms(void);
uint16_t        SafetyMonitor_GetAlarmFlags(void);

#endif /* SAFETY_MONITOR_H */
