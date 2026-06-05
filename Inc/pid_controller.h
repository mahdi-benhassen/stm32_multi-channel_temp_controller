#ifndef PID_CONTROLLER_H
#define PID_CONTROLLER_H

#include "data_structures.h"

void  PID_Init(void);
void  PID_InitChannel(uint8_t channel_index, float kp, float ki, float kd, uint8_t sensor_idx, uint8_t output_idx);
void  PID_SetTunings(uint8_t channel_index, float kp, float ki, float kd);
void  PID_SetSetpoint(uint8_t channel_index, float setpoint);
float PID_Compute(uint8_t channel_index, float pv);
void  PID_SetMode(uint8_t channel_index, PIDMode_t mode);
void  PID_ResetChannel(uint8_t channel_index);
void  PID_ProcessAll(void);

#endif /* PID_CONTROLLER_H */
