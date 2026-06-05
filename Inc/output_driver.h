#ifndef OUTPUT_DRIVER_H
#define OUTPUT_DRIVER_H

#include "data_structures.h"

typedef enum {
    OUTPUT_OK        = 0,
    OUTPUT_ERR_PARAM = 1,
    OUTPUT_ERR_HW    = 2
} OutputStatus_t;

void            Output_Init(void);
OutputStatus_t  Output_SetChannel(uint8_t channel_index, float duty_cycle_pct);
void            Output_SetMode(uint8_t channel_index, OutputMode_t mode);
void            Output_Enable(uint8_t channel_index, bool enable);
void            Output_EmergencyStop(void);
void            Output_ClearEmergency(void);

/* Internal helpers */
static void     Output_WriteDAC(uint8_t channel_index, float duty_cycle_pct);
static void     Output_WritePWM(uint8_t channel_index, float duty_cycle_pct);

#endif /* OUTPUT_DRIVER_H */
