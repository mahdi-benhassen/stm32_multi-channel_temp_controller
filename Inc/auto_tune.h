#ifndef AUTO_TUNE_H
#define AUTO_TUNE_H

#include "data_structures.h"

typedef enum {
    ATUNE_METHOD_ZN_CLOSED = 0,  /* Ziegler-Nichols Closed Loop (Ultimate Gain) */
    ATUNE_METHOD_COHEN     = 1   /* Cohen-Coon Open Loop */
} AutoTuneMethod_t;

void            AutoTune_Init(void);
void            AutoTune_Start(uint8_t channel_index, AutoTuneMethod_t method);
void            AutoTune_Abort(void);
AutoTuneState_t AutoTune_GetState(void);
void            AutoTune_Process(float current_pv);

void            AutoTune_ZN_ClosedLoop(float pv);
void            AutoTune_CohenCoon_OpenLoop(float pv);
void            AutoTune_ApplyResults(void);
void            AutoTune_Calculate_ZN_PID(float ultimate_gain, float ultimate_period, float *kp, float *ki, float *kd);
void            AutoTune_Calculate_CohenCoon_PID(float process_gain, float time_constant, float dead_time, float *kp, float *ki, float *kd);

#endif /* AUTO_TUNE_H */
