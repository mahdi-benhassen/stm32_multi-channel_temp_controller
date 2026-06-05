#include "pid_controller.h"
#include "output_driver.h"

/* Default PID tunings per channel (conservative defaults, overwritten by auto-tune) */
static const float default_kp[NUM_PID_CHANNELS] = { 2.0f, 2.0f, 2.0f, 2.0f };
static const float default_ki[NUM_PID_CHANNELS] = { 0.1f, 0.1f, 0.1f, 0.1f };
static const float default_kd[NUM_PID_CHANNELS] = { 0.05f, 0.05f, 0.05f, 0.05f };

void PID_Init(void) {
    for (uint8_t i = 0; i < NUM_PID_CHANNELS; i++) {
        PID_InitChannel(i, default_kp[i], default_ki[i], default_kd[i], i, i);
        g_sys.pid_channels[i].setpoint = 25.0f;
    }
}

void PID_InitChannel(uint8_t channel_index, float kp, float ki, float kd, uint8_t sensor_idx, uint8_t output_idx) {
    if (channel_index >= NUM_PID_CHANNELS) return;

    PIDChannel_t *ch = &g_sys.pid_channels[channel_index];
    ch->kp           = kp;
    ch->ki           = ki;
    ch->kd           = kd;
    ch->sensor_index = sensor_idx;
    ch->output_index = output_idx;
    ch->setpoint     = 0.0f;
    ch->integral     = 0.0f;
    ch->prev_error   = 0.0f;
    ch->output       = 0.0f;
    ch->p_term       = 0.0f;
    ch->i_term       = 0.0f;
    ch->d_term       = 0.0f;
    ch->mode         = PID_MODE_MANUAL;
}

void PID_SetTunings(uint8_t channel_index, float kp, float ki, float kd) {
    if (channel_index >= NUM_PID_CHANNELS) return;
    PIDChannel_t *ch = &g_sys.pid_channels[channel_index];

    /* Clamp Kp to reasonable range */
    if (kp < 0.0f) kp = 0.0f;
    if (kp > 1000.0f) kp = 1000.0f;

    ch->kp = kp;
    ch->ki = ki;
    ch->kd = kd;

    /* Reset integral to avoid windup on gain change */
    ch->integral = 0.0f;
    ch->prev_error = 0.0f;
}

void PID_SetSetpoint(uint8_t channel_index, float setpoint) {
    if (channel_index >= NUM_PID_CHANNELS) return;
    if (setpoint > MAX_SAFE_TEMPERATURE_C) setpoint = MAX_SAFE_TEMPERATURE_C;
    if (setpoint < MIN_SAFE_TEMPERATURE_C) setpoint = MIN_SAFE_TEMPERATURE_C;
    g_sys.pid_channels[channel_index].setpoint = setpoint;
}

float PID_Compute(uint8_t channel_index, float pv) {
    if (channel_index >= NUM_PID_CHANNELS) return 0.0f;

    PIDChannel_t *ch = &g_sys.pid_channels[channel_index];

    if (ch->mode == PID_MODE_MANUAL) {
        return ch->output;
    }

    /* Only accept valid sensor data */
    uint8_t s_idx = ch->sensor_index;
    if (s_idx < NUM_SENSOR_CHANNELS && !g_sys.sensors[s_idx].data_valid) {
        return ch->output;
    }

    float error = ch->setpoint - pv;

    /* ---------- Proportional Term ---------- */
    ch->p_term = ch->kp * error;

    /* ---------- Integral Term (with anti-windup clamping) ---------- */
    float dt = PID_SAMPLE_TIME_MS / 1000.0f;
    ch->integral += ch->ki * error * dt;

    /* Clamp integral to prevent windup */
    if (ch->integral > PID_INTEGRAL_MAX) ch->integral = PID_INTEGRAL_MAX;
    if (ch->integral < PID_INTEGRAL_MIN) ch->integral = PID_INTEGRAL_MIN;
    ch->i_term = ch->integral;

    /* ---------- Derivative Term (derivative on measurement to avoid kick) ---------- */
    float derivative = ch->kd * (pv - ch->prev_error) / dt;
    ch->d_term = -derivative; /* Derivative on PV means subtract */
    ch->prev_error = pv;

    /* ---------- Sum Terms ---------- */
    float raw_output = ch->p_term + ch->i_term + ch->d_term;

    /* ---------- Output Clamping (with back-calculation anti-windup) ---------- */
    if (raw_output > PID_OUTPUT_MAX) {
        ch->output = PID_OUTPUT_MAX;
        /* Back-calculate integral: freeze I term when saturated */
        ch->integral -= ch->ki * error * dt;
    } else if (raw_output < PID_OUTPUT_MIN) {
        ch->output = PID_OUTPUT_MIN;
        ch->integral -= ch->ki * error * dt;
    } else {
        ch->output = raw_output;
    }

    /* Re-clamp integral after back calculation */
    if (ch->integral > PID_INTEGRAL_MAX) ch->integral = PID_INTEGRAL_MAX;
    if (ch->integral < PID_INTEGRAL_MIN) ch->integral = PID_INTEGRAL_MIN;

    /* Safety: if emergency stop active or thermal runaway, force 0% output */
    if (g_sys.emergency_stop) {
        ch->output = 0.0f;
    }

    /* Write to output hardware */
    Output_SetChannel(ch->output_index, ch->output);

    return ch->output;
}

void PID_SetMode(uint8_t channel_index, PIDMode_t mode) {
    if (channel_index >= NUM_PID_CHANNELS) return;
    g_sys.pid_channels[channel_index].mode = mode;
    if (mode == PID_MODE_AUTOMATIC) {
        PID_ResetChannel(channel_index);
    }
}

void PID_ResetChannel(uint8_t channel_index) {
    if (channel_index >= NUM_PID_CHANNELS) return;
    PIDChannel_t *ch = &g_sys.pid_channels[channel_index];
    ch->integral   = 0.0f;
    ch->prev_error = 0.0f;
    ch->output     = 0.0f;
    ch->p_term     = 0.0f;
    ch->i_term     = 0.0f;
    ch->d_term     = 0.0f;
}

void PID_ProcessAll(void) {
    for (uint8_t i = 0; i < NUM_PID_CHANNELS; i++) {
        if (g_sys.pid_channels[i].mode == PID_MODE_AUTOMATIC) {
            uint8_t s_idx = g_sys.pid_channels[i].sensor_index;
            if (s_idx < NUM_SENSOR_CHANNELS) {
                PID_Compute(i, g_sys.sensors[s_idx].temperature);
            }
        }
    }
}
